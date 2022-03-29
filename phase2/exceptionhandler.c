#include "../pandos_const.h"
#include "../pandos_types.h"
#include <umps3/umps/libumps.h>
#include "../phase1/pcb.h"
#include "exceptionhandler.h"
#include "init.h"
#include "scheduler.h"

#define CAUSEMASK    0xFF

//Gestore generale delle eccezioni. Performa un branching basato sul codice dell'eccezione
void GeneralExceptionHandler(){
    memaddr Cause = getCAUSE(); //otteniamo il contenuto del registro cause
    int exCode = ((Cause & CAUSEMASK) >> 2); //codice eccezione dal registro cause
    
    if(exCode == 0) {
        currentProcess->p_s.reg_sp = KERNELSTACK;
        currentProcess->p_s.pc_epc = InterruptExceptionHandler;
        currentProcess->p_s.reg_t9 = InterruptExceptionHandler;
    }
    else if(exCode <= 3) {
        currentProcess->p_s.reg_sp = KERNELSTACK;
        currentProcess->p_s.pc_epc = TLBExceptionHandler;
        currentProcess->p_s.reg_t9 = TLBExceptionHandler;
    }
    else if(exCode == 8) {
        currentProcess->p_s.reg_sp = KERNELSTACK;
        currentProcess->p_s.pc_epc = SYSCALLExceptionHandler;
        currentProcess->p_s.reg_t9 = SYSCALLExceptionHandler;
    }
    else if (exCode <= 12) {
        currentProcess->p_s.reg_sp = KERNELSTACK;
        currentProcess->p_s.pc_epc = TrapExceptionHandler;
        currentProcess->p_s.reg_t9 = TrapExceptionHandler;
    }
}

//Funzione Pass-up or Die per tutte le eccezioni diverse da Syscall con codice non positivo e Interrupts
//Serve l'indice per distinguere tra page fault o eccezione generale
void PassUp_Or_Die(int index){
    if(currentProcess->p_supportStruct == NULL){
        //Siamo nel caso die
        TERM_PROCESS(currentProcess->p_pid,0,0);
    }
    else{
        //salviamo lo stato nel giusto campo della struttura di supporto
        currentProcess->p_supportStruct->sup_exceptState[index] = *((state_t*) BIOSDATAPAGE);
        //carichiamo il context dal giusto campo della struttura di supporto
        context_t rightContext = currentProcess->p_supportStruct->sup_exceptContext[index];
        LDCXT(rightContext.stackPtr, rightContext.status, rightContext.pc);
    }
}

void TLBExceptionHandler(){
    PassUp_Or_Die(PGFAULTEXCEPT);
}

void TrapExceptionHandler(){
    PassUp_Or_Die(GENERALEXCEPT);
}

/*
Cose da fare in SYSCALL:
    2. Eseguire una delle 10 chiamate in base al codice passato come primo parametro
    3. Inserire il valore di ritorno nel registro v0 del processo chiamante
    4. Incrementare il PC di una word (4.0B) per proseguire il flusso (se bloccante)
*/
void SYSCALLExceptionHandler(){
    //se il codice della syscall è positivo(ma valido), pass-up or die
    //se il codice è nel range negativo con user mode oppure non valido simuliamo un program trap
    
    //prendiamo i registri
    int a0 = currentProcess->p_s.reg_a0,
        a1 = currentProcess->p_s.reg_a1,
        a2 = currentProcess->p_s.reg_a2,
        a3 = currentProcess->p_s.reg_a3;
    //check user mode
    int user = currentProcess->p_supportStruct->sup_exceptState[GENERALEXCEPT].status;
    user = (user << 28) >> 31;
    if(a0 <= -1 && a0 >= -10 && user == 1){
        currentProcess->p_s.cause = 10;
        currentProcess->p_s.reg_sp = KERNELSTACK;
        currentProcess->p_s.pc_epc = TrapExceptionHandler;
        currentProcess->p_s.reg_t9 = TrapExceptionHandler;
    }
    else if(a0 > 0 && a0 <= 10) PassUp_Or_Die(GENERALEXCEPT);
    else{
        switch (a0){
        case CREATEPROCESS:
            currentProcess->p_s.reg_v0 = CREATE_PROCESS(a1,a2,a3);
            break;
        case TERMPROCESS:
            TERM_PROCESS(a1,a2,a3);
            break;
        case PASSEREN:
            _PASSEREN(a1,a2,a3);
            break;
        case VERHOGEN:
            _VERHOGEN(a1,a2,a3);
            break;
        case DOIO:
            currentProcess->p_s.reg_v0 = DO_IO(a1,a2,a3);
            break;
        case GETTIME:
            currentProcess->p_s.reg_v0 = GET_CPU_TIME(a1,a2,a3);
            break;
        case CLOCKWAIT:
            WAIT_FOR_CLOCK(a1,a2,a3);
            break;
        case GETSUPPORTPTR:
            GET_SUPPORT_DATA(a1,a2,a3);
            break;
        case GETPROCESSID:
            currentProcess->p_s.reg_v0 = GET_PROCESS_ID(a1,a2,a3);
            break;
        case YIELD:
            _YIELD(a1,a2,a3);
            break;
        default:
            //caso codice non valido, program trap settando excCode in cause a RI(code number 10), passare controllo al gestore
            currentProcess->p_s.cause = 10;
            currentProcess->p_s.reg_sp = KERNELSTACK;
            currentProcess->p_s.pc_epc = TrapExceptionHandler;
            currentProcess->p_s.reg_t9 = TrapExceptionHandler;
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

        currentProcess->p_s.pc_epc += WORDLEN;
        LDST((STATE_PTR) BIOSDATAPAGE);
    }
    
}

//funzione che performa le azioni necessarie al tempo di terminazione di un processo
//Usata in casi di die portion of pass-up or die oppure nsys2
static void Die (pcb_t *p, int isRoot){
    int* startDevice, endDevice; //memorizzano l'indirizzo di memoria 
    // di inizio e fine dell'array dei device semaphores
    int isDevice; //ci dice se un semaforo è un device semaphore o meno
    if(isRoot) outChild(p); 
    // rimuoviamo p dalla lista dei figli del padre solo se è la radice dell'albero di pcb da rimuovere
    
    //controlliamo il tipo del semaforo, isDevice è 1 se è un device semaphore
    startDevice = deviceSemaphores;
    endDevice = deviceSemaphores[NoDEVICE-1]+32; 
    //32 è la dimensione del tipo int, noDevice è la lunghezza del vettore
    //calcoliamo l'indirizzo di fine vettore 

    //se p è bloccato incrementiamo il valore del semaforo
    //oppure decrementiamo il numero di processi softblock
    //se è bloccato rispettivamente su un semaforo binario o su un devicesemaphore
    if (p->p_semAdd != NULL){
        //se l'indirizzo è compreso tra startdevice ed enddevice è l'indirizzo di un device semaphore
        isDevice = (p->p_semAdd >= startDevice && p->p_semAdd <= endDevice);
        isDevice ? softBlockCount-- : *p->p_semAdd++; 
    }
    else{ //lo rimuoviamo dalla coda dei processi pronti
        if (p->p_prio == 1) outProcQ(&HighPriorityReadyQueue, p);
        else outProcQ(&LowPriorityReadyQueue, p);
    }
    processCount--;
}

//trova il pcb che ha come id un certo pid
static pcb_PTR FindProcess(int pid){

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
        nuovo->p_pid = pidCounter;
        pidCounter++;
        if (prio ==  1) insertProcQ(&HighPriorityReadyQueue, nuovo);
        else insertProcQ(&LowPriorityReadyQueue, nuovo);
        insertChild(currentProcess, nuovo);
        nuovo->p_time = 0;
        nuovo->p_semAdd = NULL;
        return nuovo->p_pid;
    }
    else return -1;
}


void TERM_PROCESS(int pid, int a2, int a3){
    /*if (!emptyChild(currentProcess)){
        if (pid == 0){
        TERM_PROCESS(currentProcess->p_pid, 0, 0);
        }
        else {
            pcb_t* tmp = currentProcess;
            while(tmp->p_pid != pid){
                tmp = container_of(&tmp->p_child.next, pcb_t, p_list);
            }
            pcb_t* fader = tmp;
            tmp = container_of(&tmp->p_child.next, pcb_t, p_list);
            Die(fader);
            if (tmp->p_semAdd != NULL){
                pcb_t *exist = outBlocked(tmp);
                if (&exist->p_semAdd < 0) *exist->p_semAdd++; 
            }
            pcb_t* fader = tmp;
            tmp = container_of(&tmp->p_child.next, pcb_t, p_list);
            Die(fader);
            TERM_PROCESS(tmp->p_pid, 0, 0);
        }
    }*/
    if(pid == 0){
        Die(currentProcess,1); //Die performa un'operazione speciale solo se il pcb è root (1)
        pcb_PTR tmpChild,tmpSib; // per iterare sulle liste di figli e fratelli
        list_for_each_entry(tmpChild,&currentProcess->p_child,p_child){
            list_for_each_entry(tmpSib,&tmpChild->p_sib,p_sib) Die(tmpSib,0);
            //per ogni pcb sulla lista dei child, solo dopo aver terminato
            //tutti i fratelli, terminiamo il child stesso
            Die(tmpChild,0);
        }     
    }
    else{
        pcb_PTR proc = FindProcess(pid); //proc->p_pid = pid
        Die(proc,1); 
        pcb_PTR tmpChild,tmpSib; 
        list_for_each_entry(tmpChild,&proc->p_child,p_child){
            list_for_each_entry(tmpSib,&tmpChild->p_sib,p_sib) Die(tmpSib,0);
            Die(tmpChild,0);
        }
    }
}

void _PASSEREN(int *semaddr, int a2, int a3){
    //controlliamo che il semaforo si possa utilizzare
    if(insertBlocked(semaddr,currentProcess)) PANIC();
    //se si può utilizzare lo rimuoviamo dal semaforo per poi reinserirlo
    //perché fare questo controllo implica un inserimento nel semaforo del 
    //pcb (se il semaforo è libero e non va in PANIC)
    outBlocked(currentProcess);
    //decrementiamo valore semaforo
    *currentProcess->p_semAdd--;
    
    if (*currentProcess->p_semAdd < 0){ //in questo caso si blocca il pcb sul semaforo
        int save = currentProcess->p_s.pc_epc; //prendiamo il valore del PC che andrà incrementato di una word (per evitare infinite syscall loop)
        currentProcess->p_s = *((state_t*) BIOSDATAPAGE); //salviamo lo stato della bios data page nello stato del current
        currentProcess->p_s.pc_epc = save + WORDLEN; //il PC lo settiamo a quello che c'era precedentemente incrementato di una word
        GET_CPU_TIME(0, 0, 0); // settiamo il tempo accumulato di cpu usato dal processo
        insertBlocked(semaddr,currentProcess); //blocchiamo il pcb sul semaforo
        if(currentProcess->p_prio) outProcQ(&HighPriorityReadyQueue, currentProcess); // in base alla priorità rimuoviamo il processo
        else outProcQ(&LowPriorityReadyQueue, currentProcess); //dalla coda dei processi pronti (perché sarà bloccato) con la giusta priorità
        softBlockCount++; // incrementiamo il numero di processi bloccati
        scheduler(); // incrementiamo lo scheduler
    }
    //altrimenti il flusso ritorna al currentprocess
}

void _VERHOGEN(int *semaddr, int a2, int a3){
    *currentProcess->p_semAdd++;
    if (*semaddr < 0){
        outBlocked(currentProcess);
        softBlockCount--;
        if (currentProcess->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, currentProcess);
        else insertProcQ(&LowPriorityReadyQueue, currentProcess);
    }
}

int DO_IO(int *cmdAddr, int cmdValue, int a3){

}

// We've to return the accumulated processor time in 
// microseconds used by the requesting process
int GET_CPU_TIME(int a1, int a2, int a3){
    cpu_t currentTime; 
    STCK(currentTime);
    currentProcess->p_time = currentTime-startTime;
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
    if(parent) return currentProcess->p_parent->p_pid;
    else return currentProcess->p_pid;
}

// take out the current process from its queue
// and reinsert enqueuing it in the queue
void _YIELD(int a1, int a2, int a3){
    if(currentProcess->p_prio){ //coda ad alta priorità
        outProcQ(&HighPriorityReadyQueue,currentProcess);
        insertProcQ(&HighPriorityReadyQueue,currentProcess);
        lastProcessHasYielded = currentProcess;
    }
    else{
        outProcQ(&LowPriorityReadyQueue,currentProcess);
        insertProcQ(&LowPriorityReadyQueue,currentProcess);
        lastProcessHasYielded = currentProcess;
    }
}
