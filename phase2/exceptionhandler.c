#include "../pandos_const.h"
#include "../pandos_types.h"
#include <umps3/umps/libumps.h>
#include "exceptionhandler.h"

void exceptionHandler(){
    memaddr Cause = getCAUSE(); //otteniamo il contenuto del registro cause
    int exCode = ((Cause & 127) >> 2); //codice eccezione dal registro cause
    
    if(exCode == 0) InterruptExceptionHandler();
    else if(exCode <= 3) TLBExceptionHandler();
    else if(exCode == 8) SYSCALLExceptionHandler();
    else if (exCode <= 12) TrapExceptionHandler();
}

/*
Cose da fare in SYSCALL:
    1. Salvare lo stato del processo(dopo averlo interrotto) e passare al kernel
    2. Eseguire una delle 10 chiamate in base al codice passato come primo parametro
    3. Inserire il valore di ritorno nel registro v0 del processo chiamante
    4. Incrementare il PC di una word (4.0B) per proseguire il flusso
*/

void SYSCALLExceptionHandler(){
        
}


int CREATE_PROCESS(state_t *statep, int prio, support_t *supportp){

}

void TERM_PROCESS(int pid, int a2, int a3){

}

void _PASSEREN(int *semaddr, int a2, int a3){

}

void _VERHOGEN(int *semaddr, int a2, int a3){

}

int DO_IO(int *cmdAddr, int cmdValue, int a3){

}

int GET_CPU_TIME(int a1, int a2, int a3){

}

int WAIT_FOR_CLOCK(int a1, int a2, int a3){

}

support_t* GET_SUPPORT_DATA(int a1, int a2, int a3){

}

int GET_PROCESS_ID(int parent, int a2, int a3){

}

int _YIELD(int a1, int a2, int a3){

}
