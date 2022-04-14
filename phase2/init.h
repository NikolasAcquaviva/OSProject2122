#ifndef INIT_H
#define INIT_H

#define NoDEVICE 49 //numero di device
#define TRUE 1
#define FALSE 0

#include "../pandos_const.h" 
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "scheduler.h"
#include <umps3/umps/libumps.h>
extern void scheduler();
extern int pidCounter;
extern int processCount;
//extern int maxPid; questa variabile non è mai utilizzata in tutto il progetto nè è presente in init.c
extern int softBlockCount;
extern struct list_head HighPriorityReadyQueue;
extern pcb_PTR lastProcessHasYielded;
extern struct list_head LowPriorityReadyQueue; 
extern pcb_PTR currentProcess;
extern int deviceSemaphores[NoDEVICE];
extern void memcpy(void *dest, const void *src, int n);
void klog_print(char *str); //funzione non globale bensì facente parte di un modulo

#endif