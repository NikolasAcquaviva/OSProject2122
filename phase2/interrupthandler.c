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

void interruptHandler(){
    STCK(interruptstarttime);  //salva il tempo iniziale dell'interrupt
    state_t* interrupt_state= (state_t*) BIOSDATAPAGE;
    int interruptmap=((interrupt_state->cause & CAUSEMASK) >>8);
    unsigned int line=getInterruptInt(interruptmap); //calcolare la linea che ha richiesto l'interrupt
    /*
    *altro codice da scrivere
    */
    if (line == 0) PANIC(); //caso inter- processor interrupts, disabilitato in umps3, monoprocessore
    if (line == 1) PLTTimerInterrupt();
    else if (line == 2) IntervalTimer();
    else if(line >2){//controllo sulla linea che non sia un interrupt temporizzato
        memaddr* device= getInterruptLineAddr(line);
        int mask =1;
        for(int i =0; i < DEVPERINT; i++){
            if(*device & mask) NonTimerHandler(line , i);
            mask +=2;
        }
    }
    
memaddr* getInterruptLineAddr(int line){   //restituisce l'indirizzo del device con l'interrupt attivo
return (memaddr*) (0x10000040 + (0x04 * (line-3)));
}

void getInterruptInt(int map){  //calcolare la linea che ha richiesto l'interrupt
    int check=2;
    for(int i=0; i<8; i++){
        if(map & check) return i;
        check*=2;
    }
    return -1;
}

int getInterruptPrio(memaddr* line_addr){    //decidere la priorità dell' interrupt 
//Interrupt con numero di linea più bassa hanno priorità più alta, e dovrebbero essere gestiti per primi.

}

void PLTTimerInterrupt(){

}

void IntervalTimer(){

}

void NonTimerHandler(){

}
