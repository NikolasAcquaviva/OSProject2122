#include "../pandos_const.h"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "init.h"
#include "scheduler.h"
#include "exceptionhandler.h"

//variabili globali
extern int processCount;
extern int softBlockCount;
extern pcb_PTR currentProcess;
extern list_head HighPriorityReadyQueue;
extern list_head LowPriorityReadyQueue;
extern int deviceSemaphores[NoDEVICE];
cpu_t interruptstarttime;

/*cercare un bit a 1 nei registri relativi*/
void interruptHandler(){
    //salva il tempo iniziale dell'interrupt
    STCK(interruptstarttime);

    state_t* interrupt_state= (state_t*) BIOSDATAPAGE;
    //interruptmap = exception state's Cause register
    unsigned int interruptmap=((interrupt_state->cause & CAUSEMASK) >>8); //0xFF00 = CAUSEMASK

    int line=getInterruptInt(interruptmap); //calcolare la linea che ha richiesto l'interrupt

    if (line == 0) PANIC(); //caso inter- processor interrupts, disabilitato in umps3, monoprocessore
    else if (line == 1) PLTTimerInterrupt(); //PLT Interrupt

    else if (line == 2) IntervalTimer(); //System wide interval timer
    
    else if(line >2){//controllo sulla linea che non sia un interrupt temporizzato
        memaddr* device= getInterruptLineAddr(line);
        int mask =1;
        for(int i =0; i < DEVPERINT; i++){
            if(*device & mask) NonTimerHandler(line , i);
            mask +=2;
        }
    }
}


memaddr* getInterruptLineAddr(int line){   //restituisce l'indirizzo del device con l'interrupt attivo
    return (memaddr*) (0x10000040 + (0x04 * (line-3)));
}

void getInterruptInt(int map){  //calcolare la linea che ha richiesto l'interrupt
    int check=1; //2^0 = 1. gestiamo il caso ove la linea è 1
    for(int i=0; i<8; i++){
        if(map & check) return i;
        check*=2;
    }
    return -1;
}

int getInterruptPrio(memaddr* line_addr){    //decidere la priorità dell' interrupt 
//Interrupt con numero di linea più bassa hanno priorità più alta, e dovrebbero essere gestiti per primi.

}

void PLTTimerInterrupt(){ //currentProcess e startTime variabili globali
    
    setTIMER(TIME_CONVERT(NEVER)); // setting the timer to a high value, ack interrupt
    currentProcess->p_s = *((state_t*) BIOSDATAPAGE); //update the current process state information
    currentProcess->p_time += (CURRENT_TOD - startTime); 
    if (currentProcess->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, currentProcess);
    else if (currentProcess->p_prio == 0) insertProcQ(&LowPriorityReadyQueue, currentProcess); //re-insert the process in the readyqueue
    scheduler();
}

void IntervalTimer(){

    LDIT(PSECOND); //100000
    /* unlocking all processes in the interval timer semaphore */
    while (headBlocked(&device_semaphores[NoDEVICE-1]) != NULL)
}

void NonTimerHandler(){

}
