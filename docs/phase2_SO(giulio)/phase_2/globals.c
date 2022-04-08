#include "phase_2/globals.h"

int processCount = 0;
int softBlockCount = 0;
unsigned int pid = 0;
struct list_head readyQueueLo;
struct list_head readyQueueHi;
pcb_t* currentProcess = (pcb_t*)NULL;
int deviceSemaphores[SEMNUM] = {0};
cpu_t accomulatedCPUTime = 0;
cpu_t startBlockingSyscallTime = 0;
cpu_t endBlockingSyscallTime = 0;
