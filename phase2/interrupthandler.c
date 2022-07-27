//toDo:             if(blocked!=currentProcess) => correggere (SU IT sul quale facciamo semplice passeren). Confusione della DOIO
//					inoltre gestore delle eccezioni è uno => le procedure per gestire le eccezioni sono svolte in modo atomico, quindi non è
//					possibile che mentre stavamo svolgendo una WAIT_FOR_CLOCK, è arrivato l'interrupt. Inoltre il current process nel caso viene annullato

//					potrebbe esserci nessun process unlocked (in fondo)


#include "../pandos_const.h"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "init.h"
#include "scheduler.h"
#include "exceptionhandler.h"
#include "interrupthandler.h"
#include <umps3/umps/libumps.h>

static memaddr* getInterruptLineAddr(int line){  //indirizzo linea 3. Per classe device con interrupt. parte da 0x10000040, e incrementiamo di una word 
    return (memaddr *) (0x10000040 + (0x04 * (line-3)));
}

/*cercare un bit a 1 nei registri relativi*/
/* MANAGING ALL INTERRUPTS */
void InterruptExceptionHandler(){
    //salva il tempo iniziale dell'interrupt
    STCK(interruptstarttime);
    state_t* iep_s = (state_t*) BIOSDATAPAGE;    //preleviamo l'exception state
    memaddr interruptmap = ((iep_s->cause & 0xFF00) >> 8); //0xFF00 = CAUSEMASK. Come con ExcCode, abbiamo hardcodato. Guarda figura 3.3 pops. Otteniamo Interrupt Pending => su quale linea l'interrupt is pending. CAUSEMASK azzera tutta la parte precedente 
	//CERCHIAMO, partendo dal basso, SU QUALE LINEA C'E' STATO L'INTERRUPT
    int line = getInterruptInt(&interruptmap); //calcolare la linea che ha richiesto l'interrupt
    if (line == 0) PANIC(); //caso inter- processor interrupts, disabilitato in umps3, monoprocessore
    else if (line == 1) { //PLT Interrupt
        setTIMER(TIME_CONVERT(NEVER)); // setting the timer to a high value, ack interrupt  (riscriviamo un nuovo valore all'interno del registro (tale che lo scheduler ha tempo di svolgere le sue operazioni?))
        /* SETTING OLD STATE ON CURRENT PROCESS */
        currentProcess->p_s = *iep_s; //update the current process state information
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
            if(blocked!=currentProcess){ //SBAGLIATO, sempre vera
                if (blocked->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, blocked);
                else if (blocked->p_prio == 0) insertProcQ(&LowPriorityReadyQueue, blocked);
            }
            softBlockCount--; 
        }
        
        //adjust the semaphore value
        deviceSemaphores[NoDEVICE-1] = 0;
        /*torna al processo in esecuzione se esiste, oppure rientro nello scheduler*/
        if (currentProcess == NULL) scheduler(); /* passing control to the current process (if there is one) */
        else LDST((state_t*) BIOSDATAPAGE);
    }

    else if(line > 2){ //controllo sulla linea che non sia un interrupt temporizzato
        /*DEVICE INTERRUPT */
        memaddr* device= getInterruptLineAddr(line);
        // in case its a line > 2 interrupt, we cycle through its devices to look for the one that we have to handle
        //Ciclo 8 volte (devices per interrupt line)
        int mask = 1;
        for(int i = 0; i < DEVPERINT; i++){
            /*Cerco il device corretto*/
            if(*device & mask) NonTimerHandler(line , i);
            mask *=2;
        }
    }
}

int getInterruptInt(memaddr* map){  //calcolare la linea che ha richiesto l'interrupt
    unsigned int p_map = *(map); 
    int check=1; //2^0 = 1. gestiamo il caso ove la linea è 1
    for(int i=0; i<8; i++){
        if(p_map & check) return i;
        check*=2;
    }
    return -1;
}

/* /1* Device register type for disks, flash devices and printers (dtp) *1/ */
/* typedef struct dtpreg { */
/* 	unsigned int status; */
/* 	unsigned int command; */
/* 	unsigned int data0; */
/* 	unsigned int data1; */
/* } dtpreg_t; */

/* /1* Device register type for terminals *1/ */
/* typedef struct termreg { */
/* 	unsigned int recv_status; */
/* 	unsigned int recv_command; */
/* 	unsigned int transm_status; */
/* 	unsigned int transm_command; */
/* } termreg_t; */

/* typedef union devreg { */
/* 	dtpreg_t dtp; */
/* 	termreg_t term; */
/* } devreg_t; */

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
        devreg->dtp.command = ACK; //ACK command code in proper register or writing a new command, in order to ack the interrupt
        /*Salvo lo status da ritornare*/
        status_toReturn = devreg->dtp.status;
    }
    /*Terminali*/
    else if (line == 7){
        /* CASTING TO TERMINAL REGISTER */
        termreg_t* termreg = (termreg_t*) devreg;
		
		//pops pg 54 pdf dice che dovremmo fare l'ack ad entrambi
        //Se non è pronto a ricevere. Prima di poter ricevere nuovi comandi, dobbiamo mandare l'ack
		//pops pg 54 pdf char recv'd/transmitted IS NOT YET ACKED AS MUCH AS READY STATUS!
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
        
    }

    /* FINDING DEVICE SEMAPHORE ADDRESS */
    int semAdd = (line - 3) * 8 + dev + 8*isReadTerm;
    /* UNBLOCKING PROCESS ON THE SEMAPHORE */
    pcb_PTR unlocked = removeBlocked(&deviceSemaphores[semAdd]); //potrebbe ritornare NULL! primo IMPORTANT POINT 3.6.1 student guide. Interrupt per segnalare fine op => come fa ad esserci nessun processo bloccato?

    /*Inserisco lo stato da ritornare nel registro v0*/
    unlocked->p_s.reg_v0 = status_toReturn;

    /*Calcolo il nuovo tempo del processo*/
    unlocked->p_time += (CURRENT_TOD - interruptstarttime);
    
    /*Diminuisco il numero di processi SoftBlocked*/
    softBlockCount--;
    
    /*Inserisco il processo sbloccato nella readyQueue*/
    if (unlocked->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, unlocked);
    else if (unlocked->p_prio == 0) insertProcQ(&LowPriorityReadyQueue, unlocked);  
    
    if (currentProcess == NULL) scheduler();
    else LDST((state_t*) BIOSDATAPAGE);
}
