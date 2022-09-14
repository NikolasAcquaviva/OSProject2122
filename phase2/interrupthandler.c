#include "../pandos_const.h"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "init.h"
#include "scheduler.h"
#include "exceptionhandler.h"
#include "interrupthandler.h"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>
#include <umps3/umps/types.h>

//figura 4.1 pops .interrupt_dev posizione in array unsigned int interrupt_dev[5];
static memaddr* getInterruptLineAddr(int line){  //indirizzo linea 3. Per classe device con interrupt. parte da 0x10000040, e incrementiamo di una word
    return (memaddr *) (0x10000040 + (0x04 * (line-3)));
}

/*cercare un bit a 1 nei registri relativi*/
/* MANAGING ALL INTERRUPTS */
void InterruptExceptionHandler(){
    //salva il tempo iniziale dell'interrupt
    STCK(interruptstarttime);
    state_t* exceptionState = (state_t*) BIOSDATAPAGE;    //preleviamo l'exception state
    unsigned int interruptsPending = ((exceptionState->cause & 0xFF00) >> 8); //0xFF00 = CAUSEMASK. Come con ExcCode, abbiamo hardcodato. Guarda figura 3.3 pops. Otteniamo Interrupt Pending => su quale linea l'interrupt is pending. CAUSEMASK azzera tutta la parte precedente
                                                                              //CERCHIAMO, partendo dal basso, SU QUALE LINEA C'E' STATO L'INTERRUPT
    int line = getInterruptInt(&interruptsPending); //calcolare la linea che ha richiesto l'interrupt
    if (line == 0) PANIC(); //caso inter- processor interrupts, disabilitato in umps3, monoprocessore
    else if (line == 1) { //PLT Interrupt
        setTIMER(TIME_CONVERT(NEVER)); // setting the timer to a high value, ack interrupt  (riscriviamo un nuovo valore all'interno del registro (tale che lo scheduler ha tempo di svolgere le sue operazioni?))
        /* SETTING OLD STATE ON CURRENT PROCESS */
        currentProcess->p_s = *exceptionState; //update the current process state information
        currentProcess->p_time += (CURRENT_TOD - startTime);
        if (currentProcess->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, currentProcess);
        else insertProcQ(&LowPriorityReadyQueue, currentProcess); //re-insert the process in the readyqueue
        scheduler();
    }

    else if (line == 2) { //System wide interval timer
        LDIT(PSECOND); //100000 ACK
        /* unlocking all processes in the interval timer semaphore */
        while (headBlocked(&deviceSemaphores[NoDEVICE-1]) != NULL) {
            STCK(interruptendtime);
            pcb_PTR blocked = removeBlocked(&deviceSemaphores[NoDEVICE - 1]);
            blocked->p_semAdd = NULL;
            /* PROCESS NO LONGER BLOCKED ON A SEMAPHORE */
            blocked->p_time += (interruptendtime - interruptstarttime);
            if(blocked!=currentProcess){ //SBAGLIATO, sempre vera TODO
                if (blocked->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, blocked);
                else if (blocked->p_prio == 0) insertProcQ(&LowPriorityReadyQueue, blocked);
            }
            softBlockCount--;
        }

        //adjust the semaphore value //credo non sia mai cambiato TODO
        deviceSemaphores[NoDEVICE-1] = 0;
        /*torna al processo in esecuzione se esiste, oppure rientro nello scheduler*/
        if (currentProcess == NULL) scheduler(); /* passing control to the current process (if there is one) */
        else LDST((state_t *)BIOSDATAPAGE);
    }

    else if(line > 2){ //controllo sulla linea che non sia un interrupt temporizzato
        /*DEVICE INTERRUPT */
        memaddr* device = getInterruptLineAddr(line);
        // in case its a line > 2 interrupt, we cycle through its devices to look for the one that we have to handle
        //Ciclo 8 volte (devices per interrupt line)
        int mask = 1;
        for(int i = 0; i < DEVPERINT; i++){
            /*Cerco il device corretto*/
            if(*device & mask) NonTimerHandler(line , i); //dereferenzio per ottenere il contenuto...
            mask *=2;
        }
    }
}
// Linea degli interrupt con la massima priorità
int getInterruptInt(memaddr* map){  //calcolare la linea che ha richiesto l'interrupt
    unsigned int p_map = *(map);
    int check=1; //2^0 = 1. gestiamo il caso ove la linea è 1
    for(int i=0; i<8; i++){
        if(p_map & check) return i;
        check*=2;
    }
    return -1;
}

void NonTimerHandler(int line, int dev){
    //pg 28 (39/158) manuale Non student guide
    devreg_t* devreg = (devreg_t*) (0x10000054 + ((line - 3) * 0x80) + (dev * 0x10));
    //se il terminale riceve o trasmette un interrupt
    int isReadTerm = 0;

    /*Stato da ritornare a v0*/
    unsigned int status_toReturn;

    // if it's a normal device
    if (2 < line && line < 7){ //line > 2 sempre vera...
        /*Invio un ACK*/
        devreg->dtp.command = ACK; //ACK command code in proper register or writing a new command, in order to ack the interrupt
        /*Salvo lo status da ritornare*/
        status_toReturn = devreg->dtp.status;
    }
    /*Terminali*/
    else if (line == 7){
        /* CASTING TO TERMINAL REGISTER */
        termreg_t* termreg = (termreg_t*) devreg;
        //Prima di poter ricevere nuovi comandi, dobbiamo mandare l'ack
        //char received/transmitted (successful completion but the interrupt is not yet acked) and device ready (completion has been acked)
        //pops pg 54 pdf quando riceviamo l'interrupt è perchè è stato ricevuto/trasmesso un carattere ma il dispositivo non è ancora pronto a ricevere nuovi comandi => status NOT READY
        //TODO non vengono gestiti casi di errore, inoltre può capitare che bisogna effttuare l'ack ad entrambi (scritto sotto figura 5.12 pops)
        //sotto la figura 5.12 di pops c'è scirtto che in teoria dovremmo dare l'ack ad entrambi i subterminale se entrambi pendenti. In tal caso come si decide da quale semaforo estraiamo il processo (che precedentemente era bloccato per un'op di IO)? (in paragrafo 3.6 student guide "one should process only one interrupt at a time" Edge o Level triggered? se level triggered allora ok possiamo lasciare così)
        if(termreg->transm_status != READY){
            /*Salvo lo status da ritornare*/
            status_toReturn = termreg->transm_status;
            termreg->transm_command = ACK;
        }
        /* if (termreg->recv_status != READY){ //char transmitted/received (5) */
        else if (termreg->recv_status != READY){ //char transmitted/received (5)
            /*Salvo lo status da ritornare*/
            status_toReturn = termreg->recv_status;
            termreg->recv_command = ACK;
            /*Il terminale può ricevere interrupts*/
            isReadTerm = 1; /* SETTING RECEIVE INTERRUPT */
        }
    }

    /* FINDING DEVICE SEMAPHORE ADDRESS */
    int semAdd = (line - 3) * 8 + dev + 8*isReadTerm;
    /* UNBLOCKING PROCESS ON THE SEMAPHORE */
    pcb_PTR unlocked = removeBlocked(&deviceSemaphores[semAdd]);

    //important points paragrafo 3.6.1 student guide potrebbero avver killato un suo ancestor
    if (unlocked == NULL){
        if (currentProcess != NULL) LDST((state_t *)BIOSDATAPAGE);
        else scheduler();
    }
    else {

        /*Inserisco lo stato da ritornare nel registro v0*/
        unlocked->p_s.reg_v0 = status_toReturn;

        /*Calcolo il nuovo tempo del processo*/
        unlocked->p_time += (CURRENT_TOD - interruptstarttime);

        /*Diminuisco il numero di processi SoftBlocked*/
        softBlockCount--;

        //ad inizio pagina 3 student guide capitolo 3 rivisto, sembra che ci sia un'ulteriore prelazione dell'eventuale processo ad alta priorità sul current process
        /*Inserisco il processo sbloccato nella readyQueue*/
        if (unlocked->p_prio == 1){
            if (currentProcess->p_prio == 0){
                state_t exceptState = *((state_t*) BIOSDATAPAGE);
                /* exceptState.pc_epc += 4; //incrementiamo per quando riprenderemo */
                exceptState.reg_t9 = exceptState.pc_epc;
                currentProcess->p_s = exceptState;
                insertProcQ(&LowPriorityReadyQueue, currentProcess);
                currentProcess = NULL;
            }
            else insertProcQ(&HighPriorityReadyQueue, unlocked);
        }
        else if (unlocked->p_prio == 0) insertProcQ(&LowPriorityReadyQueue, unlocked);

        if (currentProcess == NULL) scheduler();
        else LDST((state_t *)BIOSDATAPAGE);
    }
}
