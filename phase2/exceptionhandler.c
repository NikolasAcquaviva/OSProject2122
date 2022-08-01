//toDo: puntatore BIOSDATAPAGE in SYSCALLExceptionHandler()
//		errore semaforo nella DOIO (anche giulio billi ha fatto così)
//		per coerenza, currentProcess = NULL; andrebbe messo all'inizio di scheduler(), dopo che abbiamo aggiornato il tempo d'uso (sempre nello scheduler) del processo corrente (come in progetto drif). Tutto questo per evitare  if(semaddr == &deviceSemaphores[NoDEVICE-1]) currentProcess = NULL in P()
//
//		quando avviene trap, solo RI come codice EcxCode? (3.5.11 student guide)

#include "../pandos_const.h"
#include "../pandos_types.h"
#include <umps3/umps/cp0.h>
#include <umps3/umps/libumps.h>
#include "../phase1/pcb.h"
#include "../phase1/asl.h"
#include "scheduler.h"
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
static void PassUp_Or_Die(int index){
    if(currentProcess->p_supportStruct == NULL){
        //Siamo nel caso die
        TERM_PROCESS(0,0,0);
    }
    else{
        //salviamo lo stato nel giusto campo della struttura di supporto
        state_t *exceptState = (state_t*)BIOSDATAPAGE;
		//[index] => handler of that exception, if specified. Switch context
        currentProcess->p_supportStruct->sup_exceptState[index] = *exceptState;
		//LDCXT allows a current process to change its operating mode/context - non carichiamo uno stato pieno di registri come con LDST
        LDCXT(currentProcess->p_supportStruct->sup_exceptContext[index].stackPtr,
              currentProcess->p_supportStruct->sup_exceptContext[index].status,
              currentProcess->p_supportStruct->sup_exceptContext[index].pc);
        }
}

//da GeneralExceptionHandler
void TLBExceptionHandler(){
    PassUp_Or_Die(PGFAULTEXCEPT);
}


//da GeneralExceptionHandler
void TrapExceptionHandler(){
    PassUp_Or_Die(GENERALEXCEPT);
}

void SYSCALLExceptionHandler(){
    //se il codice della syscall è positivo(ma valido), pass-up or die
    //se il codice è nel range negativo con user mode oppure non valido simuliamo un program trap

    state_t *exceptState = (state_t*) BIOSDATAPAGE; //saved exception state is stored at the start of the BIOS Data Page
    //prendiamo i registri
	//a0 => quale syscall viene eseguita
    int a0 = exceptState->reg_a0, 
        a1 = exceptState->reg_a1,
        a2 = exceptState->reg_a2,
        a3 = exceptState->reg_a3;
    //check user mode
    int user = exceptState->status;
    user = (user << 28) >> 31; //troviamo KUp
	//"process was in kernel mode and a0 contained a value in the
	//negative kernel's level range"
	//(+ => syscall da eseguire a livello di supporto (phase 3))
	//NON SIAMO IN KERNEL MODE => TRAP
    if(a0 <= -1 && a0 >= -10 && user == 1){
        exceptState->cause = (exceptState->cause & CAUSE_EXCCODE_MASK) | (EXC_RI << CAUSE_EXCCODE_BIT); //codice eccezione trap in posizione giusta in registro cause. SOLO EXC_RI?
        GeneralExceptionHandler();
    }
	//livello di supporto - TRAP
    else if(a0 > 0 && a0 <= 10) PassUp_Or_Die(GENERALEXCEPT);
    else{
        switch (a0){
            case CREATEPROCESS:
				//"a1 should contain a pointer to the initial processor state (state_t *) of newly created process"
                exceptState->reg_v0 = CREATE_PROCESS((state_t*) a1, a2, (support_t*) a3);
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
                exceptState->reg_v0 = DO_IO((int*)a1,a2,a3);
                break;
            case GETTIME:
                exceptState->reg_v0 = GET_CPU_TIME(a1,a2,a3);
                break;
            case CLOCKWAIT:
                WAIT_FOR_CLOCK(a1,a2,a3);
                break;
            case GETSUPPORTPTR:
                exceptState->reg_v0 = (memaddr) GET_SUPPORT_DATA(a1,a2,a3);
                break;
            case GETPROCESSID:
                exceptState->reg_v0 = GET_PROCESS_ID(a1,a2,a3);
                break;
            case YIELD:
                _YIELD(a1,a2,a3);
                break;
            default:
				//codice negativo ma inesistente
                //caso codice non valido, program trap settando excCode in cause a RI(code number 10), passare controllo al gestore
                exceptState->cause = (exceptState->cause & CAUSE_EXCCODE_MASK) | (EXC_RI << CAUSE_EXCCODE_BIT);
                GeneralExceptionHandler();
                break;
        }

        /*
            Siamo al punto di ritorno dalla syscall.
            Dobbiamo caricare lo stato salvato nella bios data page
            dopo aver incrementato di una word il program counter per 
            non avere il loop infinito di syscalls
        */
		//3.5.12 "["
        exceptState->pc_epc += 4; //prima di ricaricare lo stato del processo, dobbiamo incrementare il PC di una word, altrimenti ripete l'istruzione che chiama la syscall
        LDST(exceptState); //load back updated interrupted state

		//The saved exception state was the state of the process at the time the
		//SYSCALL was executed. The processor state in the Current Process’s pcb
		//was the state of the process at the start of it current time slice/quantum
    }
}

//check whether a semaphore is a device semaphore. Sem4s per accedere al dispositivo in mutex
static int isDevice(int* semaddr){
    for(int i = 0; i < NoDEVICE; i++){
        if(semaddr == &deviceSemaphores[i])
            return 1;
    }
    return 0;
}

//funzione che performa le azioni necessarie al tempo di terminazione di un processo
//Usata in casi di die portion of pass-up or die oppure nsys2
static void Die (pcb_t *p, int isRoot){
    if(isRoot==1) outChild(container_of(p->p_child.next,pcb_t,p_sib)); 
    // rimuoviamo p dalla lista dei figli del padre solo se è la radice dell'albero di pcb da rimuovere. Guarda TERM_PROCESS
    
    //se il pcb da terminare è bloccato, aggiustiamo il valore
    //del semaforo o la variabile softblock in base al tipo del semaforo
    if (p->p_semAdd != NULL){
        if(isDevice(p->p_semAdd) == 1) softBlockCount--;
        else if(*p->p_semAdd == 0 && headBlocked(p->p_semAdd) == NULL) *p->p_semAdd = 1; //il processo si era accaparrato la risorsa => il prossimo processo che la richiederà la otterrà subito.
		//IL SIGNALING E' EFFETTUATO IN INTERRUPT.C
        outBlocked(p); //se non è effettivamente bloccato, semplicemente ritorna NULL. ALtrimenti se stava aspettando per quella risorsa, si defila.
        p->p_semAdd = NULL;
    }
    //rimuoviamo il pcb dalle code di priorita altrimenti
    else if (p->p_prio == 1) outProcQ(&HighPriorityReadyQueue, p);
    else outProcQ(&LowPriorityReadyQueue, p);
    //decrementiamo il numero di processi e liberiamo il pcb
    processCount--;
    freePcb(p);
}

//rimozione di tutti i processi appartenenti al sottoalbero 
//di un dato processo, proc è il primo figlio di tale processo
static void RecursiveDie(pcb_PTR proc){
    struct list_head *head = &proc->p_parent->p_child; struct list_head *iter;
    list_for_each(iter,head){
        pcb_PTR tmp = container_of(iter,pcb_t,p_sib); //quindi si parte dal primo figlio
		//elimino prima i miei discendente, ricorsivamente
        if(!list_empty(&tmp->p_child)) 
            RecursiveDie(container_of(tmp->p_child.next,pcb_t,p_sib));
		//poi mi suicido
        Die(proc,0);
    }
    
}

//trova il pcb che ha come id un certo pid
static pcb_PTR FindProcess(int pid){
    //i pcb possono essere in running, ready o blocked state
    //il caso running è escluso, la funzione viene chiamata dalla nsys2
    //per terminare il processo che ha come id il valore pid
    //quando pid è pari a 0 non si entra qui ma si termina il current process
    //di conseguenza non ci resta che cercare nelle code ready o code dei semafori 
    pcb_PTR tmp;
	//p_list: nome del campo contenente la list_head
    list_for_each_entry(tmp,&HighPriorityReadyQueue,p_list){
        if(tmp->p_pid == pid) return tmp;
    }    
    list_for_each_entry(tmp,&LowPriorityReadyQueue,p_list){
        if(tmp->p_pid == pid) return tmp;
    }
    //cerchiamo in tutti i semafori se nei processi bloccati c'è quello che ci interessa
    for(int i = 0; i < MAX_PROC; i++){
        pcb_PTR itr;
        list_for_each_entry(itr,&semd_table[i].s_procq,p_list){
            if(itr->p_pid == pid) return itr;
        }
    }
	//Brute-force: i semafori dei device non sono nella ASL => cerco su tutti i pcb
    //cerchiamo sui semafori dei device //il check è pedante, potevamo semplicemente controllare solo il pid
    for(int i = 0; i < MAX_PROC; i++){
        for(int j = 0; j < NoDEVICE; j++){ //inserito per poter dire: "prima cerchiamo nella ASL, poi nei devices's sem4s"
            if(pcbFree_table[i].p_pid == pid && 
                pcbFree_table[i].p_semAdd == &deviceSemaphores[j])
                return &pcbFree_table[i];
        }
    }
    //se non troviamo il processo(i.e non esiste)
    PANIC();
    return NULL;
}

//controllo se figlio è un discendente di padre
static int __isMyRoot(pcb_PTR padre, pcb_PTR figlio){
    //casi base
    if (figlio->p_parent == NULL) return FALSE;
    else if (figlio->p_parent == padre) return TRUE;
    //ricorsione
    else return __isMyRoot(padre, figlio->p_parent);
}

//manca p_next e p_child (guarda student guide pg 12/28)
//Creo nuovo processo come figlio del processo invocante
//successo => restituisce l'id del processo creato; else -1
int CREATE_PROCESS(state_t *statep, int prio, support_t *supportp){
    pcb_t* nuovo = allocPcb(); // creo il processo
    if (nuovo != NULL){ // gli assegno lo stato, la prio, la support e il pid
        nuovo->p_s = *statep;
        nuovo->p_prio = prio;
        nuovo->p_supportStruct = supportp;
        nuovo->p_pid = pidCounter;
        pidCounter++;
        processCount++;
        nuovo->p_time = 0;
        nuovo->p_semAdd = NULL;
        insertChild(currentProcess,nuovo);
        if (prio == 1) insertProcQ(&HighPriorityReadyQueue, nuovo); // decido in quale queue inserirlo
        else insertProcQ(&LowPriorityReadyQueue, nuovo);  
        return nuovo->p_pid;
    }
    else return -1;
}

void TERM_PROCESS(int pid, int a2, int a3){
    //chiamiamo lo scheduler in tutti i casi tranne quello in cui
    //il currentprocess non viene terminato 
    //(pid != 0  && current non appartenente al sottoalbero del processo terminato) 
    int startScheduler = TRUE;
    if(pid == 0){
        
        if(emptyChild(currentProcess) == 0) {
            RecursiveDie(container_of(currentProcess->p_child.next,pcb_t,p_sib));
            Die(currentProcess,1);
        }
    }
    else{
        pcb_PTR proc = FindProcess(pid); //ritorna il processo, non il pid
        
		//elimino i miei figli nel caso ci siano. Parto dal fratello della sentinella
        if(emptyChild(proc) == 0){ 
            RecursiveDie(container_of(proc->p_child.next,pcb_t,p_sib));
            Die(proc,1);
		}
        //controllo se currentProcess è discendente di proc
        if (__isMyRoot(proc, currentProcess) == FALSE) startScheduler = FALSE;  
		//altrimenti anche currentProcess è stato eliminato
    }
    if (startScheduler == TRUE) scheduler();
    //altrimenti prosegue il processo corrente
}

//chiamata all'interval timer è sempre bloccante
void _PASSEREN(int *semaddr, int a2, int a3){   
    int isDev = isDevice(semaddr); //per contatore softBlockCount
    if((*semaddr) == 0 || semaddr == &deviceSemaphores[NoDEVICE-1]){
        state_t exceptState = *((state_t*) BIOSDATAPAGE);
        exceptState.pc_epc += 4; //incrementiamo per quando riprenderemo
        exceptState.reg_t9 = exceptState.pc_epc;
        currentProcess->p_s = exceptState;
        if(isDev == 1) softBlockCount++; // incrementiamo il numero di processi bloccati
        insertBlocked(semaddr,currentProcess); //blocchiamo il pcb sul semaforo
        GET_CPU_TIME(0, 0, 0); // settiamo il tempo accumulato di cpu usato dal processo
        if(semaddr == &deviceSemaphores[NoDEVICE-1]) currentProcess = NULL; //superfluo poichè dopo chiamiamo lo scheduler?
        scheduler();
    }
	//passaggio mutua esclusione
    else if(headBlocked(semaddr) != NULL){
        pcb_PTR exit = removeBlocked(semaddr);
        if(isDev == 1) softBlockCount--;
        if(exit->p_prio == 1) insertProcQ(&HighPriorityReadyQueue,exit);
        else insertProcQ(&LowPriorityReadyQueue,exit);
    }
    else (*semaddr)--;
}

void _VERHOGEN(int *semaddr, int a2, int a3){
    int isDev = isDevice(semaddr);
	//V() bloccante in bin sem con valore 1
    if((*semaddr) == 1){
        state_t exceptState = *((state_t*) BIOSDATAPAGE);
        exceptState.pc_epc += 4;
        exceptState.reg_t9 = exceptState.pc_epc;
        currentProcess->p_s = exceptState;
        if(isDev == 1) softBlockCount++; // incrementiamo il numero di processi bloccati
        insertBlocked(semaddr,currentProcess); //blocchiamo il pcb sul semaforo
        GET_CPU_TIME(0, 0, 0); // settiamo il tempo accumulato di cpu usato dal processo
        scheduler();
    }
	//il valore del sem è 0, proviamo prima a fare passaggio mutua esclusione
    else if(headBlocked(semaddr) != NULL){
        pcb_PTR exit = removeBlocked(semaddr);
        if(isDev == 1) softBlockCount--;
        if(exit->p_prio == 1) insertProcQ(&HighPriorityReadyQueue,exit);
        else insertProcQ(&LowPriorityReadyQueue,exit); 
    }
    else (*semaddr)++;   
}

//interrupt line   Device class
//0					Inter-processor - (abbiamo un solo processore...)
//1					Processor Local Timer (uno per ogni processore; serve per intervallare i processi a bassa priorità)
//2					Interval timer (Bus)
//3					Disk Devices
//4					Flash Devices					
//5					Network Devices
//6					Printer Devices
//7					Terminal Devices

int DO_IO(int *cmdAddr, int cmdValue, int a3){
    devregarea_t * deviceRegs = (devregarea_t*) RAMBASEADDR; /* INIZIO Bus register area */ //4.2 pops
	/* Device register type for disks, flash devices and printers (dtp) */
    dtpreg_t* dev;
	/* Device register type for terminals */
	termreg_t* terminal;
    int line, numDevice, semIndex; 
    int isRecvTerm = 0; //per individuare se è di ricezione
    //politiche del devicesemaphores array
    //8 disk - 8 tape - 8 network - 8 printer - 8 transm term - 8 recv term - interval timer
    
    for(int j = 0; j < 8; j++){ //8 per numero istanze. Potremmo breakare una volta entrati in un if?
        //terminali su linea 4 + 3
		//5 classi di devices; gli ultimi sono i terminali
        if(&(deviceRegs->devreg[4][j].term.transm_command) == (memaddr*) cmdAddr){
            line = 4; numDevice = j; 
			//"Given an interrupt line and a device number one can compute the starting address of the device's device register"
            terminal = (termreg_t*) (0x10000054 + (4 * 0x80) + (numDevice * 0x10));
            terminal->transm_command = cmdValue;
            break;
        }
        else if(&(deviceRegs->devreg[4][j].term.recv_command) == (memaddr*) cmdAddr){
            line = 4; numDevice = j; isRecvTerm = 1; 
            terminal = (termreg_t*) (0x10000054 + (4 * 0x80) + (numDevice * 0x10));
            terminal->recv_command = cmdValue;
            break;
        }

        //gli altri device fanno parte dello stesso gruppo
		//"dispositivi I/O dell'emulatore hanno per ogni loro istanza 4 registri, e tra questi c'è il registro nel quale viene scritto il codice
		//che viene interpretato dal dispositivo come il comando dell'operazione da eseguire"
        for(int i = 0; i < 4; i++){
            if(&(deviceRegs->devreg[i][j].dtp.command) == (memaddr*) cmdAddr){
                line = i; numDevice = j; 
                dev = (dtpreg_t*) (0x10000054 + (line * 0x80) + (numDevice* 0x10));
                dev->command = cmdValue;
                break;
            }
        }
    }
    
    //se è di ricezione siamo su linea 4, ma andiamo avanti di 8
    //perche abbiamo 16 device, i primi 8 di trasmissione
    if(isRecvTerm == 1) semIndex = line*8 + numDevice + 8;
    else semIndex = line*8 + numDevice;
    state_t exceptState = *((state_t*) BIOSDATAPAGE);
	//chiamata SEMPRE BLOCCANTE (da student guide)
    exceptState.pc_epc += 4; //increment pc by a word
    exceptState.reg_t9 = exceptState.pc_epc;    
    currentProcess->p_s = exceptState; //copiamo lo stato della bios data page nello stato del current
    softBlockCount++; // incrementiamo il numero di processi bloccati
    deviceSemaphores[semIndex] = 0; 
    GET_CPU_TIME(0, 0, 0); // settiamo il tempo accumulato di cpu usato dal processo
    _PASSEREN(&deviceSemaphores[semIndex], 0, 0);
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
	GET_CPU_TIME(0, 0, 0); //nella student guide (3.5.13) dice che anche questa syscall deve aggiornare il tempo di utilizzo
    _PASSEREN(&deviceSemaphores[NoDEVICE-1], 0, 0);
}

// We've to return the support data structure for 
// the current process, if isn't provided return NULL
support_t* GET_SUPPORT_DATA(int a1, int a2, int a3){
    return currentProcess->p_supportStruct;
}

// get pid of current process if parent is equal to 0
// get pid of the parent process otherwise
int GET_PROCESS_ID(int parent, int a2, int a3){
    if(parent!=0){
      if(currentProcess->p_parent!=NULL) return currentProcess->p_parent->p_pid;
      else return 0;
    }
    else return currentProcess->p_pid;
}

// take out the current process from its queue
// and reinsert enqueuing it in the queue
void _YIELD(int a1, int a2, int a3){
    state_t exceptState = *((state_t*)BIOSDATAPAGE);
    exceptState.pc_epc += 4;
    exceptState.reg_t9 = exceptState.pc_epc;  
    currentProcess->p_s = exceptState;
    if(currentProcess->p_prio==1) insertProcQ(&HighPriorityReadyQueue,currentProcess);
    else insertProcQ(&LowPriorityReadyQueue,currentProcess);
    lastProcessHasYielded = currentProcess;
    scheduler();
}