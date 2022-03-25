#include "../pandos_const.h"
#include "../pandos_types.h"
#include <umps3/umps/libumps.h>
#include "exceptionhandler.h"
#include "init.h"

//progressive unique id for processes
int id = 1;

//Gestore generale delle eccezioni. Performa un branching basato sul codice dell'eccezione
void GeneralExceptionHandler(){
    memaddr Cause = getCAUSE(); //otteniamo il contenuto del registro cause
    int exCode = ((Cause & 127) >> 2); //codice eccezione dal registro cause
    
    if(exCode == 0) {
        currentProcess->p_supportStruct->stackPtr = KERNELSTACK;
        currentProcess->p_supportStruct->pc = InterruptExceptionHandler;
        reg_t9 = InterruptExceptionHandler;
    }
    else if(exCode <= 3) {
        currentProcess->p_supportStruct->stackPtr = KERNELSTACK
        currentProcess->p_supportStruct->pc = TLBExceptionHandler;
        reg_t9 = TLBExceptionHandler;
    }
    else if(exCode == 8) {
        currentProcess->p_supportStruct->stackPtr = KERNELSTACK;
        currentProcess->p_supportStruct->pc = SYSCALLExceptionHandler;
        reg_t9 = SYSCALLExceptionHandler;
    }
    else if (exCode <= 12) {
        currentProcess->p_supportStruct->stackPtr = KERNELSTACK;
        currentProcess->p_supportStruct->pc = TrapExceptionHandler;
        reg_t9 = TrapExceptionHandler;
    }
}

//Funzione Pass-up or Die per tutte le eccezioni diverse da Syscall con codice non positivo e Interrupts
//Serve l'indice per distinguere tra page fault o eccezione generale
void PassUp_Or_Die(int index){
    if(currentProcess->p_supportStruct == NULL){
        //Siamo nel caso die
        TERM_PROCESS();
        //rimuoviamo il processo terminato dalla lista dei figli del padre
        outChild(currentProcess);
        //numero processi cambia dopo la terminazione del processo corrente
        processCount--;
    }
    else{
        //salviamo lo stato nel giusto campo della struttura di supporto
        currentProcess->p_supportStruct->sup_exceptState[index] = *BIOSDATAPAGE;
        //carichiamo il context dal giusto campo della struttura di supporto
        context_t rightContext = currentProcess->p_supportStruct->sup_exceptContext[index];
        LDCXT(rightContext.stackPtr, rightContext.status, rightContext.pc);
    }
}

void TLBExceptionHandler(){

}

void TrapExceptionHandler(){

}

/*
Cose da fare in SYSCALL:
    2. Eseguire una delle 10 chiamate in base al codice passato come primo parametro
    3. Inserire il valore di ritorno nel registro v0 del processo chiamante
    4. Incrementare il PC di una word (4.0B) per proseguire il flusso (se bloccante)
*/
void SYSCALLExceptionHandler(){
        
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
    pcb_t* nuovo = allocPcb();
    if (nuovo != NULL){
        nuovo->p_s = *statep;
        nuovo->p_prio = prio;
        nuovo->p_supportStruct = supportp;
        nuovo->p_pid = id;
        id++;
        insertProcQ(prio, nuovo);
        insertChild(currentProcess, nuovo);
        nuovo->p_time = 0;
        nuovo->p_semAdd = NULL;
        return nuovo->p_pid;
    }
    else return -1;
}


void TERM_PROCESS(int pid, int a2, int a3){
    if (a2 == 0){
        3
    }
    else std::cout << 'sole'; //termina il processo con pid = a2
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
