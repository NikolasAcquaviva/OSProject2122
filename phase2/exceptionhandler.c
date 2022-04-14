#include "../pandos_const.h"
#include "../pandos_types.h"
#include <umps3/umps/cp0.h>
#include <umps3/umps/libumps.h>
#include "../phase1/pcb.h"
#include "../phase1/asl.h"
#include "scheduler.h"
#include "interrupthandler.h"
#include "exceptionhandler.h"
#include "init.h"


//serve per passare allo scheduler il caso in cui esso viene chiamato da una nsys5
void setExcCode(int exC){
    codiceEccezione = exC;
}
//Gestore generale delle eccezioni. Performa un branching basato sul codice dell'eccezione
void GeneralExceptionHandler(){
    memaddr Cause = ((state_t*) BIOSDATAPAGE)->cause; //otteniamo il contenuto del registro cause
    int exCode = ((Cause & CAUSEMASK) >> 2); //codice eccezione dal registro cause
    
    if(exCode == 0) InterruptExceptionHandler();
    else if(exCode <= 3) TLBExceptionHandler();
    else if(exCode == 8) SYSCALLExceptionHandler();
    else if (exCode <= 12) TrapExceptionHandler();
}

//Funzione Pass-up or Die per tutte le eccezioni diverse da Syscall con codice non positivo e Interrupts
//Serve l'indice per distinguere tra page fault o eccezione generale
void PassUp_Or_Die(int index){
    if(currentProcess->p_supportStruct == NULL){
        //Siamo nel caso die
        TERM_PROCESS(0,0,0);
    }
    else{
        //salviamo lo stato nel giusto campo della struttura di supporto
        state_t exceptState = *((state_t*)BIOSDATAPAGE);
        currentProcess->p_supportStruct->sup_exceptState[index] = exceptState;
        LDCXT(currentProcess->p_supportStruct->sup_exceptContext[index].stackPtr,
              currentProcess->p_supportStruct->sup_exceptContext[index].status,
              currentProcess->p_supportStruct->sup_exceptContext[index].pc);
        }
}

void TLBExceptionHandler(){
    PassUp_Or_Die(PGFAULTEXCEPT);
}

void TrapExceptionHandler(){
    PassUp_Or_Die(GENERALEXCEPT);
}

void SYSCALLExceptionHandler(){
    //se il codice della syscall è positivo(ma valido), pass-up or die
    //se il codice è nel range negativo con user mode oppure non valido simuliamo un program trap
    state_t exceptState = *((state_t*) BIOSDATAPAGE);
    //prendiamo i registri
    int a0 = exceptState.reg_a0,
        a1 = exceptState.reg_a1,
        a2 = exceptState.reg_a2,
        a3 = exceptState.reg_a3;
    //check user mode
    int user = exceptState.status;
    user = (user << 28) >> 31;
    if(a0 <= -1 && a0 >= -10 && user == 1){
        exceptState.cause = (exceptState.cause & CAUSE_EXCCODE_MASK) | (EXC_RI << CAUSE_EXCCODE_BIT);
        GeneralExceptionHandler();
    }
    else if(a0 > 0 && a0 <= 10) PassUp_Or_Die(GENERALEXCEPT);
    else{
        setExcCode(a0);
        //se viene sollevata un'operazione di I/O,setto il valore
        //lo scheduler lo usa per eseguire I/O sincrono
        //lo risetto ogni volta che viene sollevata un eccezione
        //altrimenti lo scheduler penserà che viene chiamato solo su operazioni I/O
        switch (a0){
            case CREATEPROCESS:
                exceptState.reg_v0 = CREATE_PROCESS((state_t*) a1, a2, (support_t*) a3);
                break;
            case TERMPROCESS:
                TERM_PROCESS(a1, a2, a3);
                break;
            case PASSEREN:
                _PASSEREN((int*) a1, a2, a3);
                break;
            case VERHOGEN:
                _VERHOGEN((int*) a1, a2, a3);
                break;
            case DOIO:
                exceptState.reg_v0 = DO_IO((int*)a1,a2,a3);
                break;
            case GETTIME:
                exceptState.reg_v0 = GET_CPU_TIME(a1,a2,a3);
                break;
            case CLOCKWAIT:
                WAIT_FOR_CLOCK(a1,a2,a3);
                break;
            case GETSUPPORTPTR:
                exceptState.reg_v0 = (memaddr) GET_SUPPORT_DATA(a1,a2,a3);
                break;
            case GETPROCESSID:
                exceptState.reg_v0 = GET_PROCESS_ID(a1,a2,a3);
                break;
            case YIELD:
                _YIELD(a1,a2,a3);
                break;
            default:
                //caso codice non valido, program trap settando excCode in cause a RI(code number 10), passare controllo al gestore
                exceptState.cause = (exceptState.cause & CAUSE_EXCCODE_MASK) | (EXC_RI << CAUSE_EXCCODE_BIT);
                GeneralExceptionHandler();
                break;
        }

        /*
            Se arrivo qui significa che sono entrato in uno dei casi
            in cui la syscall aveva un codice nel range negativo ed 
            inoltre non termina il processo.
            Dobbiamo caricare lo stato salvato nella bios data page
            e incrementare di una word il program counter per 
            non avere il loop infinito di syscalls
        */
        exceptState.pc_epc += 4; 
        LDST(&exceptState);
    }
}

//funzione che performa le azioni necessarie al tempo di terminazione di un processo
//Usata in casi di die portion of pass-up or die oppure nsys2
static void Die (pcb_t *p, int isRoot){
    if(isRoot==1 && p->p_parent!=NULL) outChild(p); 
    // rimuoviamo p dalla lista dei figli del padre solo se è la radice dell'albero di pcb da rimuovere
    int found = 0;
    if (p->p_semAdd != NULL){
        for(int i = 0; i < NoDEVICE; i++){
            if(p->p_semAdd==&deviceSemaphores[i]) {
                softBlockCount--;
                found = 1;
                break;
            }
        }
        if(found == 0 && (*p->p_semAdd)==0){
            if(headBlocked(p->p_semAdd) == NULL) (*p->p_semAdd)++;
            else removeBlocked(p->p_semAdd);
        }
        outBlocked(p);
    }
    else if(p != currentProcess){ //lo rimuoviamo dalla coda dei processi pronti
        if (p->p_prio == 1) outProcQ(&HighPriorityReadyQueue, p);
        else outProcQ(&LowPriorityReadyQueue, p);
    }
    freePcb(p);
    processCount--;
    if(processCount==0) currentProcess = NULL;  
}

//trova il pcb che ha come id un certo pid
static pcb_PTR FindProcess(int pid){
    //i pcb possono essere in running, ready o blocked state
    //il caso running è escluso, la funzione viene chiamata dalla nsys2
    //per terminare il processo che ha come id il valore pid
    //quando pid è pari a 0 non si entra qui ma si termina il current process
    //di conseguenza non ci resta che cercare nelle code ready o code dei semafori 
    pcb_PTR tmp, process;
    list_for_each_entry(tmp,&HighPriorityReadyQueue,p_list){
        if(tmp->p_pid == pid) process = tmp;
    }    
    list_for_each_entry(tmp,&LowPriorityReadyQueue,p_list){
        if(tmp->p_pid == pid) process = tmp;
    }
    
    //cerchiamo in tutti i semafori se nei processi bloccati c'è quello che ci interessa
    for(int i = 0; i < MAX_PROC; i++){
        pcb_PTR itr;
        list_for_each_entry(itr,&semd_table[i].s_procq,p_list){
            if(itr->p_pid == pid) process = itr;
        }
    }
    //cerchiamo sui semafori dei device
    for(int i = 0; i < MAX_PROC; i++){
        for(int j = 0; j < NoDEVICE; j++){
            if(pcbFree_table[i].p_pid == pid && 
                pcbFree_table[i].p_semAdd == &deviceSemaphores[j]){ 
                process = &pcbFree_table[i];
            }
        }
    }
    return process;
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
    pcb_t* nuovo = allocPcb(); // creo il processo
    if (nuovo != NULL){ // gli assegno lo stato, la prio, la support e il pid
        nuovo->p_s = *statep;
        nuovo->p_prio = prio;
        nuovo->p_supportStruct = supportp;
        nuovo->p_pid = pidCounter;
        pidCounter++;
        processCount++;
        insertChild(currentProcess,nuovo);
        if (prio == 1) insertProcQ(&HighPriorityReadyQueue, nuovo); // decido in quale queue inserirlo
        else insertProcQ(&LowPriorityReadyQueue, nuovo);  
        return nuovo->p_pid;
    }
    else return -1;
}

void RecursiveDie(pcb_PTR proc){
    //sappiamo che ha almeno un figlio ma il controllo va fatto per ricorsione
    struct list_head *head = &proc->p_parent->p_child; struct list_head *iter;
    list_for_each(iter,head){
        pcb_PTR tmp = container_of(iter,pcb_t,p_sib);
        if(!list_empty(&tmp->p_child)) 
            RecursiveDie(container_of(tmp->p_child.next,pcb_t,p_sib));
        Die(proc,0);
    }
    
}

void TERM_PROCESS(int pid, int a2, int a3){
    if(pid == 0){
        if(emptyChild(currentProcess) == 0) RecursiveDie(container_of(currentProcess->p_child.next,pcb_t,p_sib));
        Die(currentProcess,1);
    }
    else{
        pcb_PTR proc = FindProcess(pid); 
        if(emptyChild(proc) == 0) RecursiveDie(container_of(proc->p_child.next,pcb_t,p_sib));
        Die(proc,1);
    }   
    scheduler();
}

void _PASSEREN(int *semaddr, int a2, int a3){    
    if(insertBlocked(semaddr,currentProcess) == TRUE) PANIC();
    outBlocked(currentProcess);
    
    if((*semaddr) == 0){
        state_t exceptState = *((state_t*) BIOSDATAPAGE);
        exceptState.pc_epc += 4;
        exceptState.reg_t9 = exceptState.pc_epc;
        currentProcess->p_s = exceptState;
        if(semaddr >= deviceSemaphores && semaddr <= &(deviceSemaphores[NoDEVICE-1])+32)
            softBlockCount++; // incrementiamo il numero di processi bloccati
        insertBlocked(semaddr,currentProcess); //blocchiamo il pcb sul semaforo
        GET_CPU_TIME(0, 0, 0); // settiamo il tempo accumulato di cpu usato dal processo
        scheduler();
    }
    else if(headBlocked(semaddr) != NULL){
        pcb_PTR exit = removeBlocked(semaddr);
        if(semaddr >= deviceSemaphores && semaddr <= &(deviceSemaphores[NoDEVICE-1])+32)
            softBlockCount--;
        if(exit->p_prio == 1) insertProcQ(&HighPriorityReadyQueue,exit);
        else insertProcQ(&LowPriorityReadyQueue,exit);
    }
    else (*semaddr)--;
}

void _VERHOGEN(int *semaddr, int a2, int a3){
    if((*semaddr) == 1){
        state_t exceptState = *((state_t*) BIOSDATAPAGE);
        exceptState.pc_epc += 4;
        exceptState.reg_t9 = exceptState.pc_epc;
        currentProcess->p_s = exceptState;
        if(semaddr >= deviceSemaphores && semaddr <= &(deviceSemaphores[NoDEVICE-1])+32)
            softBlockCount++; // incrementiamo il numero di processi bloccati
        insertBlocked(semaddr,currentProcess); //blocchiamo il pcb sul semaforo
        GET_CPU_TIME(0, 0, 0); // settiamo il tempo accumulato di cpu usato dal processo
        scheduler();
    }
    else if(headBlocked(semaddr) != NULL){
        pcb_PTR exit = removeBlocked(semaddr);
        if(semaddr >= deviceSemaphores && semaddr <= &(deviceSemaphores[NoDEVICE-1])+32)
            softBlockCount--;
        if(exit->p_prio == 1) insertProcQ(&HighPriorityReadyQueue,exit);
        else insertProcQ(&LowPriorityReadyQueue,exit); 
    }
    else (*semaddr)++;   
}

int DO_IO(int *cmdAddr, int cmdValue, int a3){
    devregarea_t * deviceRegs = (devregarea_t*) RAMBASEADDR;
    dtpreg_t* dev; termreg_t* terminal;
    int line, numDevice, semIndex; 
    int isRecvTerm = 0; //per individuare se è di ricezione
    //politiche del devicesemaphores array
    //8 disk - 8 tape - 8 network - 8 printer - 8 transm term - 8 recv term - interval timer
    
    for(int j = 0; j < 8; j++){
        //terminali su linea 4
        if(&(deviceRegs->devreg[4][j].term.transm_command) == (memaddr*) cmdAddr){
            line = 4; numDevice = j;
            terminal = (termreg_t*) (0x10000054 + (4 * 0x80) + (numDevice * 0x10));
            terminal->transm_command = cmdValue;
        }
        if(&(deviceRegs->devreg[4][j].term.recv_command) == (memaddr*) cmdAddr){
            line = 4; numDevice = j; isRecvTerm = 1;
            terminal = (termreg_t*) (0x10000054 + (4 * 0x80) + (numDevice * 0x10));
            terminal->recv_command = cmdValue;
        }
        //gli altri device fanno parte dello stesso gruppo
        for(int i = 0; i < 4; i++){
            if(&(deviceRegs->devreg[i][j].dtp.command) == (memaddr*) cmdAddr){
                line = i; numDevice = j; 
                dev = (dtpreg_t*) (0x10000054 + (line * 0x80) + (numDevice* 0x10));
                dev->command = cmdValue;
            }
        }
    }
    
    //se è di ricezione siamo su linea 4, ma andiamo avanti di 8
    //perche abbiamo 16 device, i primi 8 di trasmissione
    if(isRecvTerm == 1) semIndex = line*8 + numDevice + 8;
    else semIndex = line*8 + numDevice;
    state_t exceptState = *((state_t*) BIOSDATAPAGE);
    
    exceptState.pc_epc += 4; //increment pc by a word
    exceptState.reg_t9 = exceptState.pc_epc;    
    currentProcess->p_s = exceptState; //copiamo lo stato della bios data page nello stato del current
    softBlockCount++; // incrementiamo il numero di processi bloccati
    deviceSemaphores[semIndex]--;
    insertBlocked(&deviceSemaphores[semIndex], currentProcess);
    GET_CPU_TIME(0, 0, 0); // settiamo il tempo accumulato di cpu usato dal processo
    scheduler(); // richiamiamo lo scheduler   
    
    if(&(dev->command) == (memaddr*) cmdAddr) return dev->status;
    else if(isRecvTerm == 1) return terminal->recv_status;
    else return terminal->transm_status;
}

// We've to return the accumulated processor time in 
// microseconds used by the requesting process
int GET_CPU_TIME(int a1, int a2, int a3){
    cpu_t currentTime; STCK(currentTime);
    currentProcess->p_time += currentTime - startTime;
    return currentProcess->p_time;
}

// Perform a P operation on the pseudo-clock semaphore, which is V-ed every 100ms
// The current process has to change its state from running to blocked
// It has to be blocked on the ASL, and then the scheduler has to be called
void WAIT_FOR_CLOCK(int a1, int a2, int a3){
    // A passeren on the interval-timer semaphore
    _PASSEREN((int*) INTERVALTMR, 0, 0);
}

// We've to return the support data structure for 
// the current process, if isn't provided return NULL
support_t* GET_SUPPORT_DATA(int a1, int a2, int a3){
    return currentProcess->p_supportStruct;
}

// get pid of current process if parent is equal to 0
// get pid of the parent process otherwise
int GET_PROCESS_ID(int parent, int a2, int a3){
    if(parent!=0) return currentProcess->p_parent->p_pid;
    else return currentProcess->p_pid;
}

// take out the current process from its queue
// and reinsert enqueuing it in the queue
void _YIELD(int a1, int a2, int a3){
    state_t exceptState = *((state_t*)BIOSDATAPAGE);
    exceptState.pc_epc += 4;
    exceptState.reg_t9 = exceptState.pc_epc;
    currentProcess->p_s = exceptState;
    if(currentProcess->p_prio==1){ //coda ad alta priorità
        outProcQ(&HighPriorityReadyQueue,currentProcess);
        insertProcQ(&HighPriorityReadyQueue,currentProcess);
        lastProcessHasYielded = currentProcess;
    }
    else{
        outProcQ(&LowPriorityReadyQueue,currentProcess);
        insertProcQ(&LowPriorityReadyQueue,currentProcess);
        lastProcessHasYielded = currentProcess;
    }
    scheduler();
}