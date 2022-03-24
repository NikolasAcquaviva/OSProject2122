#include "../pandos_const.h" //per dubbi cerca "?"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "scheduler.h"

#define NoDEVICE 49 //numero di device
#define TRUE 1
#define FALSE 0

//toDO: lasciare le variabili che davvero devono essere globali
extern int processCount;
extern int softBlockCount;
extern struct list_head HighPriorityReadyQueue;
extern unsigned int lastHighPriorityProcessHasYielded = NULL;
extern struct list_head LowPriorityReadyQueue; 
extern pcb_PTR currentProcess;
extern int deviceSemaphores[NoDEVICE];