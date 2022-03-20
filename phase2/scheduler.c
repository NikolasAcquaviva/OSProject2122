#include "../pandos_const.h"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "main.h"

extern struct list_head readyQueueLowPriority;
extern struct list_head readyQueueHighPriority;
extern int softBlockCount;
extern int processCount;
extern pcb_PTR currentProcess;

cpu_t startTime;

void scheduler() {
	if (currentProcess != NULL) {
		
	}
	//get next process to run, list_empty, flag se bassa/alta priorità
	if (currentProcess != NULL) {

	}
	else {

	}
}