#include "../pandos_const.h"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "scheduler.h"

#define NoDEVICE 49 //numero di device
#define TRUE 1
#define FALSE 0

//dichiarazione delle  variabili globali
/* Number of started, but not yet terminated processes. */
int processCount;
/* Number of started, but not terminated processes that in are the
“blocked” state  due to an I/O or timer request */
int softBlockCount;
/* Tail pointer to a queue of pcbs (related to high priority processes) 
that are in the “ready” state. */
//ricordarsi che quando un processo ad alta priorità esegue una yield(), bisogna
//cercare di far eseguire gli altri, anche se è il primo della high priority q
struct list_head readyQueueHighPriority;
unsigned int lastProcessHasYielded[1][1]; //solo per processi ad alta priorità. unsigned int perchè lo è memaddr. nel primo ci sarà il pid,
//nel secondo ci sarà un booleano
/* Tail pointer to a queue of pcbs (related to low priority processes) 
that are in the “ready” state. */
struct list_head readyQueueLowPriority;
/* Pointer to the current pcb that is in running state */
//the current (and only) executing process 
pcb_PTR currentProcess;
//Array for device semaphores
int deviceSemaphores[NoDEVICE];
//extern => declare a var in one object file but then use it in another one
//variable could be defined somewhere else => specifichiamo che provengono dall'esterno
extern void test();
extern void uTLB_RefillHandler();
extern void exceptionHandler();


int main() {

	passupvector_t *passUpVector = (passupvector_t *) PASSUPVECTOR;
	//popolare gestore eccezioni. popolare = inserire PC e SP adeguati nei registri
	passUpVector->tlb_refill_handler = (memaddr) uTLB_RefillHandler; /*in Memory related constants */
	passUpVector->tlb_refill_stackPtr = (memaddr) KERNELSTACK;
	passUpVector->exception_stackPtr = (memaddr) KERNELSTACK;
	passUpVector->exception_handler = (memaddr) exceptionHandler;

	softBlockCount = 0;
	processCount = 0;
	currentProcess = NULL;

	mkEmptyProcQ(&readyQueueLowPriority);
	mkEmptyProcQ(&readyQueueHighPriority);

	initPcbs();
	initASL();

	for (int i = 0; i < NoDEVICE; i++) deviceSemaphores[i] = 0;

	//load interval timer globale. pg 21 pdf capitolo rivisto
	LDIT(PSECOND); //100000

	pcb_PTR initProc = AllocPcb();
	processCount+=1;

	scheduler();
}