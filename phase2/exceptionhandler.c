#include "../pandos_const.h"
#include "../pandos_types.h"
#include <umps3/umps/libumps.h>
#include "../phase1/pcb.h"
#include "../phase1/asl.h"
#include "scheduler.h"
#include "interrupthandler.h"
#include "exceptionhandler.h"
#include "init.h"


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
        klog_print("\nuser mode");
        exceptState.cause = 10;
        GeneralExceptionHandler();
    }
    else if(a0 > 0 && a0 <= 10) PassUp_Or_Die(GENERALEXCEPT);
    else{
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
            GET_SUPPORT_DATA(a1,a2,a3);
            break;
        case GETPROCESSID:
            exceptState.reg_v0 = GET_PROCESS_ID(a1,a2,a3);
            break;
        case YIELD:
            _YIELD(a1,a2,a3);
            break;
        default:
            //caso codice non valido, program trap settando excCode in cause a RI(code number 10), passare controllo al gestore
            klog_print("\ncodice non valido");
            exceptState.cause = 10;
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
        klog_print("\nfine handler\n");
        exceptState.pc_epc += 4; 
        LDST(&exceptState);
    }
}

//funzione che performa le azioni necessarie al tempo di terminazione di un processo
//Usata in casi di die portion of pass-up or die oppure nsys2
static void Die (pcb_t *p, int isRoot){
    int *startDevice, *endDevice; //memorizzano l'indirizzo di memoria 
    // di inizio e fine dell'array dei device semaphores
    int isDevice; //ci dice se un semaforo è un device semaphore o meno
    if(isRoot) outChild(p); 
    // rimuoviamo p dalla lista dei figli del padre solo se è la radice dell'albero di pcb da rimuovere
    
    //controlliamo il tipo del semaforo, isDevice è 1 se è un device semaphore
    startDevice = deviceSemaphores;
    endDevice = &(deviceSemaphores[NoDEVICE-1]) + 32; 
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
    //i pcb possono essere in running, ready o blocked state
    //il caso running è escluso, la funzione viene chiamata dalla nsys2
    //per terminare il processo che ha come id il valore pid
    //quando pid è pari a 0 non si entra qui ma si termina il current process
    //di conseguenza non ci resta che cercare nelle code ready o code dei semafori 

    pcb_PTR tmp;
    list_for_each_entry(tmp,&HighPriorityReadyQueue,p_list)
        if(tmp->p_pid == pid) return tmp;
    list_for_each_entry(tmp,&LowPriorityReadyQueue,p_list)
        if(tmp->p_pid == pid) return tmp;
    
    //cerchiamo in tutti i semafori se nei processi bloccati c'è quello che ci interessa
    for(int i = 0; i < MAX_PROC; i++){
        pcb_PTR itr;
        list_for_each_entry(itr,&semd_table[i].s_procq,p_list)
            if(itr->p_pid == pid) return itr;
    }

    //cerchiamo sui semafori dei device
    for(int i = 0; i < NoDEVICE; i++){
        //solo se il semaforo ha processi bloccati sulla sua coda
        if(deviceSemaphores[i]!=0){
            for(int j = 0; j < MAXPROC; j++)
                if(pcbFree_table[j].p_pid == pid) 
                    return &pcbFree_table[j];
        }
    }
    //NON SI PUÒ TERMINARE UN PROCESSO INESISTENTE
    PANIC();
    return NULL; //non si arriverà mai qui ovviamente, ma è buona norma
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
        if (prio ==  1) insertProcQ(&HighPriorityReadyQueue, nuovo); // decido in quale queue inserirlo
        else insertProcQ(&LowPriorityReadyQueue, nuovo);
        insertChild(currentProcess, nuovo); // lo inserisco come figlio del processo corrente
        nuovo->p_time = 0; // setto il tempo a 0
        nuovo->p_semAdd = NULL;
        return nuovo->p_pid;
    }
    else return -1;
}


void TERM_PROCESS(int pid, int a2, int a3){
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
    klog_print("\nentro in passeren");
    //Quando si entra qui, il processo si bloccherà sull'ASL se il valore del semaforo è negativo dopo il decremento
    state_t exceptState = *((state_t*) BIOSDATAPAGE); //save exception state
    
    //controlliamo che il semaforo si possa utilizzare
    if(insertBlocked(semaddr,currentProcess) == TRUE) PANIC();
    //se si può utilizzare lo rimuoviamo dal semaforo per poi reinserirlo
    //perché fare questo controllo implica un inserimento nel semaforo del 
    //pcb (se il semaforo è libero e non va in PANIC)
    outBlocked(currentProcess);
    //decrementiamo valore semaforo
    *semaddr -= 1;
    if (*semaddr < 0 || semaddr == (int*) INTERVALTMR){ //in questo caso si blocca il pcb sul semaforo
        klog_print("\nsono bloccato sulla asl");
        //incremento solo se c'è almeno un processo tra le due code
        //altrimenti lo scheduler non caricherà lo stato con ldst ma si tornerà a questo flusso
        //realizzando un doppio  incremento del pc(viene fatto un ulteriore incremento all'uscita del syscall handler)
        if(!(list_empty(&LowPriorityReadyQueue) && list_empty(&HighPriorityReadyQueue))){
            klog_print("\n a quanto pare ce n'è almeno uno in code");
            exceptState.pc_epc += 4; //increment pc by a word
            exceptState.reg_t9 = exceptState.pc_epc;
            }

        currentProcess->p_s = exceptState; //copiamo lo stato della bios data page nello stato del current
        GET_CPU_TIME(0, 0, 0); // settiamo il tempo accumulato di cpu usato dal processo
        if(semaddr == (int*) INTERVALTMR) softBlockCount++; // incrementiamo il numero di processi bloccati
        insertBlocked(semaddr,currentProcess); //blocchiamo il pcb sul semaforo
        scheduler(); // richiamiamo lo scheduler
    }
    klog_print("\nesco da passeren riottenendo il flusso");
    //altrimenti il flusso ritorna al currentprocess (oppure lo scheduler ha fatto il suo lavoro, NSYS5)
}

void _VERHOGEN(int *semaddr, int a2, int a3){
    (*currentProcess->p_semAdd) += 1; // eseguo una V operation sull'indirizzo ricevuto
    if (*semaddr-1 < 0){
        outBlocked(currentProcess); // rimuovo il processo dalla lista dei bloccati
        softBlockCount--;
        if (currentProcess->p_prio == 1) insertProcQ(&HighPriorityReadyQueue, currentProcess); // lo inserisco nella lista corretta
        else insertProcQ(&LowPriorityReadyQueue, currentProcess);
    }
}

int DO_IO(int *cmdAddr, int cmdValue, int a3){
    klog_print("\n\nsono nel doio"); 
    devregarea_t * deviceRegs = (devregarea_t*) RAMBASEADDR;
    dtpreg_t* dev; termreg_t* terminal;
    int line, numDevice, semIndex; 
    int isRecvTerm = 0; //per individuare se è di ricezione
    //politiche del devicesemaphores array
    //8 disk - 8 tape - 8 network - 8 printer - 8 transm term - 8 recv term - interval timer
    
    //*cmdAddr = cmdValue;
    for(int j = 0; j < 8; j++){
        //terminali su linea 4
        if(&(deviceRegs->devreg[4][j].term.transm_command) == (memaddr*) cmdAddr){
            klog_print("\ndentro if");
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
    if((list_empty(&HighPriorityReadyQueue) && list_empty(&LowPriorityReadyQueue))){
        exceptState.pc_epc += 4; //increment pc by a word
        exceptState.reg_t9 = exceptState.pc_epc;
    }
    currentProcess->p_s = exceptState; //copiamo lo stato della bios data page nello stato del current
    GET_CPU_TIME(0, 0, 0); // settiamo il tempo accumulato di cpu usato dal processo
    softBlockCount++; // incrementiamo il numero di processi bloccati
    insertBlocked(&deviceSemaphores[semIndex], currentProcess);
    scheduler(); // richiamiamo lo scheduler   
    klog_print("\nfinisco doio");
    if(&(dev->command) == (memaddr*) cmdAddr) return dev->status;
    else if(isRecvTerm == 1) return terminal->recv_status;
    else return terminal->transm_status;
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