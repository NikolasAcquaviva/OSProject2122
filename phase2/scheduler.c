// cerca AGGIUNTO o MODIFICATO per vedere cosa ho modificato dopo il cambio di politica dei pid

//toDO: ricordarsi di settare la flag globale lastProcessHasYielded quando si esegue la syscall YIELD
//setSTATUS/TIMER pg 71/158
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
extern int codiceEccezione; //usata per prendere il codice di syscall in un punto specifico dell'esecuzione (quando parte la doio)

cpu_t startTime;
cpu_t finishTime;
void scheduler() {
	//flags
	unsigned int highPriorityProcessChosen = FALSE; 
	//introdotta per determinare il timer di ogni processo. Infatti i processi a bassa
	//priorità sono cadenzati dall'algoritmo roundRobin ogni x secondi. istanza x = 5ms
	//SCEGLIAMO IL PROSSIMO PROCESSO DA METTERE IN ESECUZIONE/SCHEDULARE
	//si controlla se l'ultimo processo era ad alta priorità e ha rilasciato le risorse con yield(), poichè bisogna evitare (best effort)
	//che tali processi riprendano immediatamente dopo l'operazione yield()
	if (lastProcessHasYielded != NULL) {
		if (lastProcessHasYielded->p_prio == 1){
			klog_print("\nalta priorita in yield");
			pcb_PTR headHighPriorityQueue = headProcQ(&HighPriorityReadyQueue); //testa NON rimossa (peek)
			//e se è il primo processo nella coda ad alta priorità pronto per essere eseguito (riconosciuto tramite suo puntatore pcb)
			if (headHighPriorityQueue == lastProcessHasYielded) {
				if (!list_empty(&LowPriorityReadyQueue)) {
					currentProcess = removeProcQ(&LowPriorityReadyQueue);
					highPriorityProcessChosen = FALSE;
					klog_print("\nestraggo da bassa in yield");
				}
				//best effort: fai partire il processo che aveva rilasciato le risorse
				else {
					klog_print("\nestraggo da alta, un solo processo, quello che ha yieldato");
					currentProcess = removeProcQ(&HighPriorityReadyQueue); //non utilizziamo headHighPriorityQueue perchè altrimenti
					highPriorityProcessChosen = TRUE;
				}
			}
			//altrimenti fai partire il secondo processo ad alta priorità ready
			else {
				klog_print("\estraggo un secondo dall'alta in yield");
				currentProcess = removeProcQ(&HighPriorityReadyQueue);
			} 	
		}

		else{
			klog_print("\nyield a bassa");
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
	else if(codiceEccezione != DOIO){
		if (!list_empty(&HighPriorityReadyQueue)) {
			currentProcess = removeProcQ(&HighPriorityReadyQueue);
			highPriorityProcessChosen = TRUE;			
		}
		//coda ad alta priorità è vuota => prendo un processo da quella a bassa priorità sse non è vuota
		else if (!list_empty(&LowPriorityReadyQueue)) {
			currentProcess = removeProcQ(&LowPriorityReadyQueue); //se le rispettive code sono vuote, removeProcQ restituirà NULL
			highPriorityProcessChosen = FALSE; //pedante
		}
	}
	//resetto la flag
	lastProcessHasYielded = NULL;
	//c'è effettivamente un processo che sta aspettando in una delle due code
	if (currentProcess != NULL) {
		//fisso il momento (in "clock tick") di partenza in cui parte
		STCK(startTime);
		//setto il process local timer
		if (highPriorityProcessChosen) setTIMER(1000000); 
		else setTIMER(TIMESLICE);

		//reset variabile, indifferentemente dal suo valore precedente
		highPriorityProcessChosen = FALSE;
		klog_print("\ncarico lo stato");
		//ed INFINE carico lo stato del processo nel processore
		LDST(&currentProcess->p_s);
	}
	else{
		if (processCount == 0) HALT();
		else if (processCount > 0 && softBlockCount > 0){
			setTIMER(1000000); //"either disable the PLT through the STATUS register or load it with a very large value" => 2)
			setSTATUS(IECON | IMON); //enabling interrupts
			WAIT(); //idle processor (waiting for interrupts)
		}
		else if (processCount > 0 && softBlockCount == 0) {if(codiceEccezione==TERMPROCESS) klog_print("\npanic in scheduler"); PANIC();} //Deadlock
	}
}