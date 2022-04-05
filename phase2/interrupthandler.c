#include "../pandos_const.h"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "init.h"
#include "scheduler.h"
#include "exceptionhandler.h"
#include "interrupthandler.h"
#include <umps3/umps/libumps.h>

cpu_t interruptstarttime, interruptendtime;


memaddr *getInterruptLineAddr(int line){   //restituisce l'indirizzo del device con l'interrupt attivo
    return (memaddr *) (0x10000040 + (0x04 * (line-3)));
}

/*cercare un bit a 1 nei registri relativi*/
/* MANAGING ALL INTERRUPTS */
void InterruptExceptionHandler(){

    //salva il tempo iniziale dell'interrupt
    STCK(interruptstarttime);

    state_t* interrupt_state= (state_t*) BIOSDATAPAGE;
    //interruptmap = exception state's Cause register
    unsigned int interruptmap=((interrupt_state->cause & CAUSEMASK) >> 8); //0xFF00 = CAUSEMASK

    int line = getInterruptInt(interruptmap); //calcolare la linea che ha richiesto l'interrupt

    if (line == 0) PANIC(); //caso inter- processor interrupts, disabilitato in umps3, monoprocessore
    else if (line == 1) { //PLT Interrupt
        
        //currentProcess e startTime variabili globali
        setTIMER(TIME_CONVERT(NEVER)); // setting the timer to a high value, ack interrupt
        /* SETTING OLD STATE ON CURRENT PROCESS */
        currentProcess->p_s = *((state_t*) BIOSDATAPAGE); //update the current process state information
        currentProcess->p_time += (CURRENT_TOD - startTime); 
        if (currentProcess->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, currentProcess);
        else if (currentProcess->p_prio == 0) insertProcQ(&LowPriorityReadyQueue, currentProcess); //re-insert the process in the readyqueue
        scheduler();
    
    }

    else if (line == 2) { //System wide interval timer
        
        LDIT(PSECOND); //100000
        /* unlocking all processes in the interval timer semaphore */
        while (headBlocked(&deviceSemaphores[NoDEVICE-1]) != NULL) {

            STCK(interruptendtime);
            pcb_PTR blocked = removeBlocked(&(deviceSemaphores[NoDEVICE - 1]));

            if (blocked != NULL)  // equivalent of performing a continuos V operation on the interval timer semaphore
            {
                /* PROCESS NO LONGER BLOCKED ON A SEMAPHORE */
                blocked->p_semAdd = NULL;
                blocked->p_time += (interruptendtime - interruptstarttime);
                if (currentProcess->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, blocked);
                else if (currentProcess->p_prio == 0) insertProcQ(&LowPriorityReadyQueue, blocked);
                softBlockCount--;
            }
        }
        
        //adjust the semaphore value
        deviceSemaphores[NoDEVICE-1] = 0;

        /*torna al processo in esecuzione se esiste, oppure rientro nello scheduler*/
        if (currentProcess == NULL) scheduler(); /* passing control to the current process (if there is one) */
        else LDST((state_t*) BIOSDATAPAGE);
    }

    else if(line >2){ //controllo sulla linea che non sia un interrupt temporizzato
        /*DEVICE INTERRUPT */
        memaddr* device= getInterruptLineAddr(line);
        // in case its a line > 2 interrupt, we cycle through its devices to look for the one that we have to handle
        //Ciclo 8 volte (devices per interrupt line)
        int mask =1;
        for(int i = 0; i < DEVPERINT; i++){
            /*Cerco il device corretto*/
            if(*device & mask) NonTimerHandler(line , i);
            mask *=2;
        }
    }
}

int getInterruptInt(int map){  //calcolare la linea che ha richiesto l'interrupt
    int check=1; //2^0 = 1. gestiamo il caso ove la linea è 1
    for(int i=0; i<8; i++){
        if(map & check) return i;
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
    if (2 < line && line < 7){
        /*Invio un ACK*/
        devreg->dtp.command = ACK;
        /*Salvo lo status da ritornare*/
        status_toReturn = devreg->dtp.status;
    }
    /*Terminali*/
    else if (line == 7){

        /* CASTING TO TERMINAL REGISTER */
        termreg_t* termreg = (termreg_t*) devreg;

        //Se non è pronto a ricevere
        if (termreg->recv_status != READY){
            
            /*Salvo lo status da ritornare*/
            status_toReturn = termreg->recv_status;
            termreg->recv_command = ACK;
            /*Il terminale può ricevere interrupts*/
            isReadTerm = 1; /* SETTING RECEIVE INTERRUPT */
        }
        else
        {
            /*Salvo lo status da ritornare*/
            status_toReturn = termreg->transm_status;
            termreg->transm_command = ACK;
        }

        /*Trovo il numero del device*/
        dev = 2*dev + isReadTerm;
    }

    /* FINDING DEVICE SEMAPHORE ADDRESS */
    int semAdd = (line - 3) * 8 + dev;

    /* INCREASING DEVICE SEMAPHORE VALUE */ /*Operazione V sul semaforo del device*/
    deviceSemaphores[semAdd]++;

    /* UNBLOCKING PROCESS ON THE SEMAPHORE */
    pcb_PTR unlocked = removeBlocked(&deviceSemaphores[semAdd]);

    /*Se c'era almeno un processo bloccato*/
    if (unlocked != NULL){

        /*Inserisco lo stato da ritornare nel registro v0*/
        unlocked->p_s.reg_v0 = status_toReturn;

        /*Il processo non è più bloccato da un semaforo*/
        unlocked->p_semAdd = NULL;
        
        /*Calcolo il nuovo tempo del processo*/
        unlocked->p_time += (CURRENT_TOD - interruptstarttime);
        
        /*Diminuisco il numero di processi SoftBlocked*/
        softBlockCount -= 1;
        
        /*Inserisco il processo sbloccato nella readyQueue*/
        if (unlocked->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, unlocked);
        else if (unlocked->p_prio == 0) insertProcQ(&LowPriorityReadyQueue, unlocked);
    }

    /*Se nessun processo era in esecuzione chiamo lo Scheduler*/
    if (currentProcess == NULL) scheduler();
    /*Se il processo sbloccato ha priotità maggiore del processo in esecuzione*/
    else if(unlocked->prio > currentProcess->prio){
        currentProcess->p_s= (state_t*) BIOSDATAPAGE;   //copio lo stato del processore nel pcb del processo corrente
        /*posiziono il processo nella coda Ready*/
        if (currentProcess->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, currentProcess);
        else if (currentProcess->p_prio == 0) insertProcQ(&LowPriorityReadyQueue, currentProcess);
        /*chiamo lo Scheduler*/
        scheduler();
    }
    /*Altrimenti carico il vecchio stato*/
    else LDST((state_t *) BIOSDATAPAGE);
}
