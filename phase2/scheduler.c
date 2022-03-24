#include "../pandos_const.h"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "main.h"

extern struct list_head LowPriorityReadyQueue;
extern struct list_head HighPriorityReadyQueue;
extern int softBlockCount;
extern int processCount;
extern pcb_PTR currentProcess;
unsigned int lastHighPriorityProcessHasYielded;
cpu_t startTime;

void scheduler() {
	if (currentProcess != NULL) { // => c'è già un processo in exec
		//pg 32/158 per capire STCK(x) populate x
	}
	//get next process to run, list_empty, flag se bassa/alta priorità ...
	if (currentProcess != NULL) {

		//se l'ultimo processo era ad alta priorità e ha rilasciato le risorse,
		if (lastHighPriorityProcessHasYielded != NULL) {

			pcb_PTR headHighPriorityQueue = headProcQ(&HighPriorityReadyQueue);

			//e se è il primo processo nella coda ad alta priorità pronto per essere eseguito (puntatore pcb = pid processo)
			if (headHighPriorityQueue == lastHighPriorityProcessHasYielded) {

				//controlla che non sia l'unico, ovvero che il suo next non sia la sentinella
				//ma poichè non sappiamo l'indirizzo della sentinella, controlliamo che il next del next
				//non sia il processo che ha rilasciato la risorsa
				if (container_of(headHighPriorityQueue->next.next, pcb_t, p_list) == headHighPriorityQueue) {

					//controlla che la coda low priority non sia vuota. Se non è vuota fai partire il suo primo processo in attesa
					if (list_empty(LowPriorityReadyQueue)){

					}
					//best effort: fai partire il processo che aveva rilasciato le risorse
					else
				}
				//altrimenti fai partire il secondo processo ad alta priorità ready
				else 
			}
			//altrimenti come consuetudine fai partire il processo con più alta priorità in attesa
			else 
		}
		//altrimenti come consuetudine fai partire il processo con più alta priorità in attesa
		else 
	}
	else{}
}