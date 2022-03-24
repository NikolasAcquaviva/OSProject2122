#include "../pandos_const.h"
#include "../pandos_types.h"
#include <umps3/umps/libumps.h>
#include "exceptionhandler.h"
/*
Cose da fare in SYSCALL:
    1. Salvare lo stato del processo(dopo averlo interrotto) e passare al kernel
    2. Eseguire una delle 10 chiamate in base al codice passato come primo parametro
    3. Inserire il valore di ritorno nel registro v0 del processo chiamante
    4. Incrementare il PC di una word (4.0B) per proseguire il flusso
*/
int id = 1;

void exceptionHandler(){
    
}

unsigned int SYSCALL(unsigned int number,unsigned int arg1,
unsigned int arg2,unsigned int arg3){
        
}


int CREATE_PROCESS(state_t *statep, int prio, support_t *supportp){
/*
creo il processo:
decido se alta o bassa prio;
genero il puntatore alla struttura di supporto;
gli assegno un id;
;
lo inserisco come figlio del current;
ritorno l'id del processo;
*/
    pcb_t* nuovo;
    freePcb(nuovo);
    nuovo->p_s = statep; // Appunto personale: cercare la funzione di change
    nuovo->p_prio = prio;
    nuovo->p_supportStruct = supportp;
    nuovo->p_pid = id;
    id++;
    insertChild(currentProcess, nuovo); 
    // Ho messo "currentProcess" perché serve il processo 
    // corrente, ma se non è usabile troverò un altro modo.
    return nuovo->p_pid;
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
