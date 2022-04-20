#include "../pandos_const.h"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "init.h"
#include "exceptionhandler.h"
#include <umps3/umps/libumps.h>

#define TIME_CONVERT(T) ((T) * (*((memaddr *) TIMESCALEADDR)))
#define CURRENT_TOD ((*((memaddr *)TODLOADDR)) / (*((cpu_t *)TIMESCALEADDR)))

//umps3 supporta solo I/O asincrono, se ci sono altri processi
//lo scheduler non li mandera anche se il currentprocess è bloccato
//su un semaforo, se quest'ultimo è di un device di I/O

//usata per prendere il codice di syscall in un punto specifico dell'esecuzione 

extern int codiceEccezione; 
cpu_t startTime;
cpu_t finishTime;
void scheduler() {
	unsigned int highPriorityProcessChosen = FALSE; 
	//introdotta per determinare il timer di ogni processo. Infatti i processi a bassa
	//priorità sono cadenzati dall'algoritmo roundRobin ogni x secondi. istanza x = 5ms
	
	//SCEGLIAMO IL PROSSIMO PROCESSO DA METTERE IN ESECUZIONE/SCHEDULARE
	//si controlla se l'ultimo processo era ad alta priorità e ha rilasciato le risorse con yield(), poichè bisogna evitare (best effort)
	//che tali processi riprendano immediatamente dopo l'operazione yield()
	if (lastProcessHasYielded != NULL) {
		if (lastProcessHasYielded->p_prio == 1){
			pcb_PTR headHighPriorityQueue = headProcQ(&HighPriorityReadyQueue);
			//e se è l'unico processo nella coda ad alta priorità pronto per essere eseguito
			//si prova a dispatchare un processo a bassa priorita, se esiste
			if (headHighPriorityQueue == lastProcessHasYielded) {
				if (!list_empty(&LowPriorityReadyQueue)) {
					currentProcess = removeProcQ(&LowPriorityReadyQueue);
					highPriorityProcessChosen = FALSE;
				}
				//best effort: fai partire il processo che aveva rilasciato le risorse
				else {
					currentProcess = removeProcQ(&HighPriorityReadyQueue); 
					highPriorityProcessChosen = TRUE;
				}
			}
			//altrimenti fai partire il secondo processo ad alta priorità ready
			else {
				currentProcess = removeProcQ(&HighPriorityReadyQueue);
			} 	
		}

		//se un processo a bassa priorita ha yieldato, a prescindere seguiamo la classica politica
		//poiché a prescindere che estraiamo dalla coda ad alta o bassa priorità
		//estrarremo lo stesso processo che ha yieldato solo se siamo costretti(unico processo attivo)
		else{
			if (!list_empty(&HighPriorityReadyQueue)){
				currentProcess = removeProcQ(&HighPriorityReadyQueue);
				highPriorityProcessChosen = TRUE;
			}
			else{
				currentProcess = removeProcQ(&LowPriorityReadyQueue);
				highPriorityProcessChosen = FALSE;
			}
		}
	}
	//altrimenti consuetudine
	//estraiamo un nuovo processo solo se non stiamo eseguendo I/O
	//operazioni di I/O sincrone
	else if(codiceEccezione!=DOIO){
		if (!list_empty(&HighPriorityReadyQueue)) {
			currentProcess = removeProcQ(&HighPriorityReadyQueue);
			highPriorityProcessChosen = TRUE;			
		}
		//coda ad alta priorità è vuota => prendo un processo da quella a bassa priorità sse non è vuota
		else if (!list_empty(&LowPriorityReadyQueue)) {
			currentProcess = removeProcQ(&LowPriorityReadyQueue); //se le rispettive code sono vuote, removeProcQ restituirà NULL
			highPriorityProcessChosen = FALSE; //pedante
		}
		else currentProcess = NULL;
	}
	//resetto la flag
	lastProcessHasYielded = NULL;
	//c'è effettivamente un processo che sta aspettando in una delle due code
	if (currentProcess != NULL) {
		//fisso il momento (in "clock tick") di partenza in cui parte
		STCK(startTime);
		//setto il process local timer
		if (highPriorityProcessChosen) setTIMER(TIME_CONVERT(NEVER)); 
		else setTIMER(TIME_CONVERT(TIMESLICE));

		//reset variabile, indifferentemente dal suo valore precedente
		highPriorityProcessChosen = FALSE;
		//ed INFINE carico lo stato del processo nel processore
		LDST(&currentProcess->p_s);
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