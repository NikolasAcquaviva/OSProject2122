// cerca AGGIUNTO o MODIFICATO per vedere cosa ho modificato dopo il cambio di politica dei pid

#include "../pandos_const.h" //per dubbi cerca "?"
#include "../pandos_types.h"
#include "init.h"
#include "pcb.h"
#include "asl.h"
#include "scheduler.h"
#include <umps3/umps/libumps.h>

#define NoDEVICE 49 //numero di device
#define TRUE 1
#define FALSE 0

//dichiarazione delle  variabili globali
//come politica dei pid è stato scelto un contatore
int pidCounter = 1; //AGGIUNTO
/* Number of started, but not yet terminated processes. */
int processCount;
/* Number of started, but not terminated processes that in are the
“blocked” state  due to an I/O or timer request */
int softBlockCount;

/* Tail pointer to a queue of pcbs (related to high priority processes) 
that are in the “ready” state. */
//ricordarsi che quando un processo ad alta priorità esegue una yield(), bisogna
//cercare di far eseguire gli altri, anche se è il primo della high priority q
struct list_head HighPriorityReadyQueue;// = LIST_HEAD_INIT(HighPriorityReadyQueue); //capire TAIL - coda dove gli elementi vengono semplicemente inseriti in fondo?

//MODIFICATO
pcb_PTR lastProcessHasYielded = NULL; //puntatore al pcb del processo associato.
//unsigned int perchè lo è memaddr
//NULL = unsigned int 0 il quale indirizzo non potrà/dovrà esistere
//quando un processo rilascia, ricordarsi di inserire il puntatore al pcb corrispondente dentro lastProcessHasYielded

/* Tail pointer to a queue of pcbs (related to low priority processes) 
that are in the “ready” state. */
struct list_head LowPriorityReadyQueue;// = LIST_HEAD_INIT(LowPriorityReadyQueue);
/* Pointer to the current pcb that is in running state */
//the current (and only) executing process 
pcb_PTR currentProcess;
//Array for device semaphores
int deviceSemaphores[NoDEVICE];
//extern => declare a var in one object file but then use it in another one
//variable could be defined somewhere else => specifichiamo che provengono dall'esterno
extern void test();
extern void uTLB_RefillHandler();
extern void GeneralExceptionHandler();


int main() {

	passupvector_t *passUpVector = (passupvector_t *) PASSUPVECTOR;
	//popolare gestore eccezioni. popolare = inserire PC e SP adeguati nei registri
	passUpVector->tlb_refill_handler = (memaddr) uTLB_RefillHandler; /*in Memory related constants */
	passUpVector->tlb_refill_stackPtr = (memaddr) KERNELSTACK;
	passUpVector->exception_stackPtr = (memaddr) KERNELSTACK;
	passUpVector->exception_handler = (memaddr) GeneralExceptionHandler;

	softBlockCount = 0;
	processCount = 0;
	currentProcess = NULL;

	mkEmptyProcQ(&LowPriorityReadyQueue);
	mkEmptyProcQ(&HighPriorityReadyQueue);

	initPcbs();
	initASL();

	for (int i = 0; i < NoDEVICE; i++) deviceSemaphores[i] = 0;

	//load interval timer globale. pg 21 pdf capitolo rivisto. (Interval) timer è un vero e proprio dispositivo fisico che fa svolgere al kernel il context switch
	LDIT(PSECOND); //100000 - ?"scrivendolo nel registro corrispondente" ci serve un indirizzo? DEV2ON 0x00000004 (dubbio sorto dalle slide,
	// sul libro è sciallato e non c'è nessun accenno a ciò).
	//bisogna convertire anche qui? non credo

	pcb_PTR initProc = allocPcb(); //init anche di p_list? avviene/avvenuto già in pcb.c? sembra di no...
	initProc->p_time = 0;
	initProc->p_semAdd = NULL;
	initProc->p_supportStruct = NULL;
	initProc->p_parent = NULL; //"set all the process Tree fields to NULL"
	INIT_LIST_HEAD(&(initProc->p_list)); //INIT_LIST_HEAD anzichè LIST_HEAD_INIT perchè la seconda restituisce una struct list head nuova, mentre noi dobbiamo inizializzare le preesistenti
	INIT_LIST_HEAD(&(initProc->p_child)); //commento di nikolas: impossibile assegnare un valore di tipo (void*) a struct list head
	INIT_LIST_HEAD(&(initProc->p_sib));   //usare la funzione di lista per inizializzazione di lista vuota. riguardare come avevamo fatto nei moduli di fase 1 per praticita
	initProc->p_prio = 0; //poichè viene inserito in una coda a bassa priorità. gestire già la politica di assegnamento dei pid? (puntatore
	// univoco a struttura pcb_t)
	
	//prima
	//initProc->p_pid = (int)&initProc; //identificativo univoco ovvero suo indirizzo logico di memoria. Gli anni scorsi non c'era questo campo
	//ora
	initProc->p_pid = pidCounter; //MODIFICATO
	pidCounter+=1;

	processCount+=1;

	/*init first process state */
	state_t initState;
	STST(&initState); //salvare stato processore in una struttura pcb_t

	RAMTOP(initState.reg_sp); //SP a ramtop

    /*process needs to have interrupts enabled, the processor Local Timer enabled, kernel-mode on => Status register constants */
    initState.status = IEPON | IMON | TEBITON; //parte non capita appieno https://github.com/AndreSkin/progetto_PandOs/blob/main/Fase_2/initial.c
    //2.3 nel libro (non student guide). cosa significano?

    initState.pc_epc = (memaddr) test; /*(exec) PC all'indirizzo della funzione test di p2test */
    initState.reg_t9 = (memaddr) test; //idiosincrasia dell'architettura

    initProc->p_s = initState;

    insertProcQ(&(LowPriorityReadyQueue), initProc);

	scheduler();

	return 0;
}