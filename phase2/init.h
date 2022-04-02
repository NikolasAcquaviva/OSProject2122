#ifndef INIT_H
#define INIT_H

#define NoDEVICE 49 //numero di device
#define TRUE 1
#define FALSE 0

extern int pidCounter;
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
void klog_print(char *str);

#endif