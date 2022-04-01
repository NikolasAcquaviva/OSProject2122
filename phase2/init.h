#ifndef INIT_H
#define INIT_H

#define NoDEVICE 49 //numero di device
#define TRUE 1
#define FALSE 0

//toDO: lasciare le variabili che davvero devono essere globali
extern int pidCounter; //AGGIUNTO
extern int processCount;
extern int maxPid;
extern int softBlockCount;
extern struct list_head HighPriorityReadyQueue;
extern pcb_PTR lastProcessHasYielded;
extern struct list_head LowPriorityReadyQueue; 
extern pcb_PTR currentProcess;
extern int deviceSemaphores[NoDEVICE];
extern void scheduler();
extern void memcpy(void *dest, const void *src, int n);
#endif