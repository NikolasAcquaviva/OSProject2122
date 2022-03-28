// cerca AGGIUNTO o MODIFICATO per vedere cosa ho modificato dopo il cambio di politica dei pid

//toDO: ricordarsi di settare la flag globale lastHighPriorityProcessHasYielded quando si esegue la syscall YIELD
//setSTATUS/TIMER pg 71/158
#include "../pandos_const.h"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "init.h"

/*
extern struct list_head LowPriorityReadyQueue;
extern struct list_head HighPriorityReadyQueue;
extern int softBlockCount;
extern int processCount;
extern pcb_PTR currentProcess;
*/

#define TIME_CONVERT(T) ((T) * (*((memaddr *) TIMESCALEADDR)))

//flags
unsigned int highPriorityProcessChosen = FALSE; //introdotta per determinare il timer di ogni processo. Infatti i processi a bassa
//priorità sono cadenzati dall'algoritmo roundRobin ogni x secondi. x = 5ms

cpu_t startTime;
cpu_t finishTime;

void scheduler() {
	if (currentProcess != NULL) { // => c'è già un processo in exec
		//pg 32/158 per capire STCK(x) populate x
		//TOD = counter incremented by one after every processor cycle = tempo di vita del processore
		//STCK(x) => TOD/time scale
		STCK(finishTime); //"ferma il cronometro e popola x"
		currentProcess->p_time += finishTime - startTime; 
	}
	//default values
	currentProcess = NULL;

	//SCEGLIAMO IL PROSSIMO PROCESSO DA METTERE IN ESECUZIONE/SCHEDULARE
	//si controlla se l'ultimo processo era ad alta priorità e ha rilasciato le risorse con yield(), poichè bisogna evitare (best effort)
	//che tali processi riprendano immediatamente dopo l'operazione yield()
	if (lastHighPriorityProcessHasYielded != NULL) {

		pcb_PTR headHighPriorityQueue = headProcQ(&HighPriorityReadyQueue); //testa NON rimossa (peek)

		//e se è il primo processo nella coda ad alta priorità pronto per essere eseguito (riconosciuto tramite suo puntatore pcb)
		if (headHighPriorityQueue == lastHighPriorityProcessHasYielded) {

			//controlla che non sia l'unico, ovvero che il suo next non sia la sentinella
			//ma poichè non sappiamo l'indirizzo della sentinella, controlliamo che il next del next
			//non sia il processo che ha rilasciato la risorsa (ovvero il processo stesso)
			if (container_of(headHighPriorityQueue->p_list.next->next, pcb_t, p_list) == headHighPriorityQueue) { //

				//controlla che la coda low priority NON sia vuota. Se non è vuota fai partire il suo primo processo in attesa
				if (!list_empty(&LowPriorityReadyQueue)) {
					currentProcess = removeProcQ(&LowPriorityReadyQueue);
					highPriorityProcessChosen = FALSE;
				}

				//best effort: fai partire il processo che aveva rilasciato le risorse
				else {
					currentProcess = removeProcQ(&HighPriorityReadyQueue); //non utilizziamo headHighPriorityQueue perchè altrimenti
					//non rimuoveremmo l'elemento dalla coda!
					highPriorityProcessChosen = TRUE;
				}
			}
			//altrimenti fai partire il secondo processo ad alta priorità ready
			else {
				currentProcess = container_of(headHighPriorityQueue->p_list.next, pcb_t, p_list);
				//ricordarsi di rimuovere il secondo elemento (se è diverso da NULL)
				if (currentProcess != NULL) { //pedante
					outProcQ(&HighPriorityReadyQueue, currentProcess);
					highPriorityProcessChosen = TRUE;
				}
			}
		}
		//altrimenti come consuetudine fai partire il processo con più alta priorità in attesa
		//la coda ad alta priorità è necessariamente non vuota, poichè c'è almeno il processo che ha svolto la YIELD() => non può essere null
		//quindi dal momento che abbiamo scartato l'ipotesi che il primo ed eventuale unico elemento sia il processo che ha svolto la yield,
		//possiamo tranquillamente prendere il primo dalla high priority ready queue
		else {
			currentProcess = removeProcQ(&HighPriorityReadyQueue);
			highPriorityProcessChosen = TRUE;
		}
	}
	//altrimenti consuetudine
	else {
		if (list_empty(&HighPriorityReadyQueue)) {
			currentProcess = removeProcQ(&LowPriorityReadyQueue); //se le rispettive code sono vuote, removeProcQ restituirà NULL
			highPriorityProcessChosen = FALSE; //pedante
		}
		else {
			currentProcess = removeProcQ(&HighPriorityReadyQueue);
			highPriorityProcessChosen = TRUE;
		}
	}
	
	//resetto la flag
	lastHighPriorityProcessHasYielded = NULL;

	//c'è effettivamente un processo che sta aspettando in una delle due code
	if (currentProcess != NULL) {

		//fisso il momento (in "clock tick") di partenza in cui parte
		STCK(startTime);

		//setto il process local timer
		if (highPriorityProcessChosen) { setTIMER(TIME_CONVERT(NEVER)); }
		else setTIMER(TIME_CONVERT(TIMESLICE));

		//reset variabile, indifferentemente dal suo valore precedente
		highPriorityProcessChosen = FALSE;

		//gli assegno un pid (?)
		currentProcess->p_pid = pidCounter;
		pidCounter += 1;

		//ed INFINE carico lo stato del processo nel processore
		LDST(&(currentProcess->p_s));

	}
	else{
		if (processCount == 0) HALT();
		else if (processCount > 0 && softBlockCount > 0){

			setTIMER(TIME_CONVERT(NEVER)); //"either disable the PLT through the STATUS register or load it with a very large value" => 2)
			setSTATUS(IECON | IMON); //enabling interrupts
			WAIT(); //idle processor (waiting for interrupts)
		}
		else if (processCount > 0 && softBlockCount == 0) PANIC(); //Deadlock
	}
}