#include "phase_2/scheduler.h"

#include "phase_1/pcb.h"
#include "phase_2/globals.h"
#include "utils/helpers.h"

/**
 * @brief Caso 1: readyQueueHi non è vuota
 */
HIDDEN void hiQueue() {
  //
  STCK(accomulatedCPUTime);

  // Rimuovo il processo dalla ready queue dei processi con priorità alta
  currentProcess = removeProcQ(&readyQueueHi);

  // Carico lo stato del processo
  LDST(&currentProcess->p_s);
}

/**
 * @brief Caso 2: readyQueueLo non è vuota
 */
HIDDEN void loQueue() {
  //
  STCK(accomulatedCPUTime);

  // Rimuovo il processo dalla ready queue dei processi con priorità bassa
  currentProcess = removeProcQ(&readyQueueLo);

  // Setto il PLT con 5 millisecondi
  setPLT(TIMESLICE);

  // Carico lo stato del processo
  LDST(&currentProcess->p_s);
}

/**
 * @brief Caso 3: readyQueueHi e readyQueueLo sono vuote
 */
HIDDEN void hiLoEmptyQueue() {
  // Non ci sono più processi da eseguire, arrestiamo
  if (processCount == 0) return HALT();

  //
  if (processCount > 0 && softBlockCount > 0) {
    setSTATUS(ALLOFF | IEPON | IMON | TEBITON);
    setPLT(0xFFFFFFFF);
    return WAIT();
  }

  //
  if (processCount > 0 && softBlockCount == 0) return PANIC();
}

void scheduler() {
  int isQueueHiEmpty = emptyProcQ(&readyQueueHi);
  int isQueueLoEmpty = emptyProcQ(&readyQueueLo);

  if (!isQueueHiEmpty) return hiQueue();
  else if (!isQueueLoEmpty) return loQueue();
  else if (isQueueHiEmpty && isQueueLoEmpty) return hiLoEmptyQueue();
}
