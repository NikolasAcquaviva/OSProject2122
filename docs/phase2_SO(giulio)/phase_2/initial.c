/**
 * @file initial.c
 * @author Domenico Ciancio (domenico.ciancio@studio.unibo.it)
 * @author Enrico Emiro (enrico.emiro@studio.unibo.it)
 * @author Giulio Billi (giulio.billi2@studio.unibo.it)
 * @author Orazio Andrea Capone (orazioandrea.capone@studio.unibo.it)
 * @brief Nucleus Initialization
 * @date 2022-03-22
 */
#include "phase_1/asl.h"
#include "phase_1/pcb.h"

#include "phase_2/exceptions.h"
#include "phase_2/globals.h"
#include "phase_2/scheduler.h"

extern void uTLB_RefillHandler();
extern void test();

int main() {
  // 2. Populate the Processor 0 Pass Up Vector
  passupvector_t* passupvector = (passupvector_t*)PASSUPVECTOR;
  passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;
  passupvector->tlb_refill_stackPtr = (memaddr)KERNELSTACK;
  passupvector->exception_handler = (memaddr)exceptionHandler;
  passupvector->exception_stackPtr = (memaddr)KERNELSTACK;

  // 3. Initialize phase 1 data structures
  initPcbs();
  initASL();

  // 4. Initialize nucleus lists
  mkEmptyProcQ(&readyQueueLo);
  mkEmptyProcQ(&readyQueueHi);

  // 5. Load the system-wide Interval Timer with 100 milliseconds
  LDIT(PSECOND);

  // 6. Allocate single process
  pcb_t* process = allocPcb();
  if (process == NULL) PANIC();

  // Initialize process field
  process->p_prio = 0;
  process->p_supportStruct = NULL;
  process->p_s.status = ALLOFF | IEPON | IMON | TEBITON;
  process->p_s.pc_epc = (memaddr)test;
  process->p_s.reg_t9 = (memaddr)test;
  RAMTOP(process->p_s.reg_sp);

  // Insert the process and increment the counter
  insertProcQ(&readyQueueLo, process);
  processCount++;

  // 7. Call the scheduler
  scheduler();

  return 0;
}
