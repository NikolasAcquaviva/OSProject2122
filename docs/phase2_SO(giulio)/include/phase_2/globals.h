#ifndef GLOBALS_H
#define GLOBALS_H

#define SEMNUM 49 /** Number of semaphores */

/**
 * @brief Integer indicating the number of started, but not yet terminated
 * processes.
 */
extern int processCount;

/**
 * @brief Number of started, but not terminated processes that are in the
 * "blocked" state due to an I/O or timer request.
 */
extern int softBlockCount;

/**
 * @brief Tail pointer to list of ready process with Low priority.
 */
extern struct list_head readyQueueLo;

/**
 * @brief Tail pointer to list of ready process with High priority.
 */
extern struct list_head readyQueueHi;

/**
 * @brief
 */
extern unsigned int pid;

/**
 * @brief Pointer to the pcb that is in the "running" state, i.e.the current
 * executing process.
 */
extern pcb_t* currentProcess;

/**
 * @brief One integer semaphore for each external (sub)device in μMPS3, plus one
 * additional semaphore to support the Pseudo-clock.
 */
extern int deviceSemaphores[SEMNUM];

/**
 * @brief Store the interval time from the start of a process to the call of
 * get CPU time
 */
extern cpu_t accomulatedCPUTime;

/**
 * @brief
 */
extern cpu_t startBlockingSyscallTime;

/**
 * @brief
 */
extern cpu_t endBlockingSyscallTime;

#endif
