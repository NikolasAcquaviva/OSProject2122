/**
 * @file syscall.c
 * @author Enrico Emiro (enrico.emiro@studio.unibo.it)
 * @author Giulio Billi (giulio.billi2@studio.unibo.it)
 * @brief Implementazione delle system call
 * @date 2022-03-27
 */
#include "phase_2/syscall.h"

#include "phase_1/asl.h"
#include "phase_1/pcb.h"
#include "phase_2/globals.h"
#include "phase_2/scheduler.h"
#include "utils/helpers.h"

devregarea_t* deviceRegisters = (devregarea_t*)RAMBASEADDR;

int createProcess(state_t* state_ptr, int prio, support_t* support_ptr) {
  pcb_t* process = allocPcb();

  // The process cannot be created due to lack of resources
  if (process == NULL) return -1;

  // Initialize process fields
  memCpy(&process->p_s, state_ptr, sizeof(state_t));
  process->p_prio = prio;
  process->p_supportStruct = support_ptr;
  process->p_pid = pid++;  // da rivedere
  process->p_time = 0;
  process->p_semAdd = NULL;

  // Place the process on the Ready Queue based on the priority
  if (process->p_prio == PROCESS_PRIO_LOW) insertProcQ(&readyQueueLo, process);
  else if (process->p_prio == PROCESS_PRIO_HIGH)
    insertProcQ(&readyQueueHi, process);

  // Make this process a child of the current process
  insertChild(currentProcess, process);

  processCount++;

  return process->p_pid;
}

void terminateProcess(int pid) {
  /**
   * The root of the sub-tree of terminated processes must be "orphaned" from
   * its parents; its parent can no longer have this pcb as one of its progeny.
   */
  pcb_t* process = NULL;

  if (pid == 0) process = currentProcess;
  else process = *((pcb_t**)pid);
  if (process == NULL) return PANIC();

  // Se il processo da terminare non ha padre vuol dire che è il processo
  // iniziale
  if (emptyChild(process)) {
    terminateOneProcess(process);
  } else {
    terminateProgeny(process);
  }
  currentProcess = NULL;
  return scheduler();

  /**
   * If a terminated process is blocked on a device semaphore, the semaphore
   * should NOT be adjusted. When the interrupt eventually occurs the semaphore
   * will get V’ed (and hence incremented) by the interrupt handler.
   */

  /**
   * The process count and soft-blocked variables need to be adjusted
   * accordingly.
   */

  /**
   * Processes (i.e. pcb’s) can’t hide. A pcb is either the Current Process
   * ("running"), sitting on the Ready Queue ("ready"), blocked on a device
   * semaphore ("blocked"), or blocked on a non-device semaphore ("blocked")
   */
}
void terminateOneProcess(pcb_t* processToTerminate) {
  pcb_t* readyQueueOut = NULL;

  // Rimuoviamo il processo dalla coda Ready, se è presente.
  if (processToTerminate->p_prio == PROCESS_PRIO_LOW)
    readyQueueOut = outProcQ(&readyQueueLo, processToTerminate);
  else if (processToTerminate->p_prio == PROCESS_PRIO_HIGH)
    readyQueueOut = outProcQ(&readyQueueHi, processToTerminate);
  // Se il processo da eliminare non era in Ready state sarà bloccato su un
  // semaforo
  if (readyQueueOut == NULL) {
    // Se è bloccato su un device semaphore non dobbiamo modificarlo
    if (&(deviceSemaphores[0]) <= processToTerminate->p_semAdd &&
        processToTerminate->p_semAdd <= &(deviceSemaphores[48])) {
      // la V dovrebbe essere eseguita dall'interrupt handler

    } else {
      // Se il processo non è su un device semaphore bisogna sistemare il
      // valore(essendo binario non sono sicuro di sta roba)
      (*(processToTerminate->p_semAdd)++);
    }
    // Il processo da terminare va tolto dalla lista dei bloccati del semaforo
    // su cui è bloccato
    outBlocked(processToTerminate);
  }
  // Liberiamo la struttura utilizzata per il processo
  freePcb(processToTerminate);
  // Decrementiamo il numero di processi attivi
  processCount--;
}

void terminateProgeny(pcb_t* rootProcess) {
  if (rootProcess == NULL) return;
  // Visita in profondità dell'albero e eliminazione di ogni nodo
  while (!emptyChild(rootProcess))
    terminateProgeny(removeChild(rootProcess));

  terminateOneProcess(rootProcess);
}

void passeren(int* semAdd) {
  // Se il semaforo è zero, dobbiamo bloccare il "currentProcess"
  if ((*semAdd) == 0) {
    // Sezione 3.5.13 (Blocking SYSCALLs)
    // Caso: Syscall bloccante
    memCpy(&currentProcess->p_s, (state_t*)BIOSDATAPAGE, sizeof(state_t));

    // Aggiorniamo il tempo del "currentProcess"
    currentProcess->p_time +=
        (STCK(endBlockingSyscallTime) - startBlockingSyscallTime);

    int hasProcessBeenBlocked = insertBlocked(semAdd, currentProcess) == FALSE;

    // Se non è possibile inserire il "currentProcess", PANIC
    if (!hasProcessBeenBlocked) return PANIC();

    // Incrementiamo il contatore dei processi avviati ma non ancora terminati
    softBlockCount++;

    // Invochiamo lo scheduler
    return scheduler();
  } else {
    if (emptyProcQ(&((semd_t*)semAdd)->s_procq) == TRUE) {
      (*semAdd)--;
    } else {
      pcb_t* unblockedProcess = removeBlocked(semAdd);
      softBlockCount--;
      if (unblockedProcess->p_prio == PROCESS_PRIO_LOW)
        insertProcQ(&readyQueueLo, unblockedProcess);
      else if (unblockedProcess->p_prio == PROCESS_PRIO_HIGH)
        insertProcQ(&readyQueueHi, unblockedProcess);

      return scheduler();
    }
  }
}

void verhogen(int* semAdd) {
  // Controlliamo se la risorsa è libera
  if ((*semAdd) == 1) {
    // Sezione 3.5.13 (Blocking SYSCALLs)
    // Caso: Syscall bloccante
    memCpy(&currentProcess->p_s, (state_t*)BIOSDATAPAGE, sizeof(state_t));

    // Aggiorniamo il tempo del "currentProcess"
    currentProcess->p_time +=
        (STCK(endBlockingSyscallTime) - startBlockingSyscallTime);

    // E lo inseriamo nei processi bloccati
    insertBlocked(semAdd, currentProcess);

    // Incrementiamo il contatore dei processi avviati ma non ancora terminati
    softBlockCount++;

    // Invochiamo lo scheduler
    return scheduler();
  } else {
    // Se la risorsa non è bloccata controlliamo se la lista dei pcb bloccati
    // dal semaforo è libera e nel caso incrementiamo il valore del semaforo
    if (emptyProcQ(&((semd_t*)semAdd)->s_procq) == TRUE) {
      (*semAdd)++;
    } else {
      // Se invece non è libera bisogna:
      // - Sbloccare il primo processo che è stato bloccato sul semaforo
      // - Ed inserirlo nella readyQueue
      pcb_t* unblockedProcess = removeBlocked(semAdd);
      softBlockCount--;
      if (unblockedProcess->p_prio == PROCESS_PRIO_LOW)
        insertProcQ(&readyQueueLo, unblockedProcess);
      else if (unblockedProcess->p_prio == PROCESS_PRIO_HIGH)
        insertProcQ(&readyQueueHi, unblockedProcess);

      return scheduler();
    }
  }
}

/*
int doIODevice(int* cmdAdd, int cmdValue) {
  // Il parametro "cmdAdd" non contiene un indirizzo valido
  if ((memaddr)cmdAdd < DEV_REG_START || (memaddr)cmdAdd > DEV_REG_END) PANIC();

  // Calcolo il numero del semaforo del device da bloccare
  unsigned int semIndex = ((memaddr)cmdAdd - DEV_REG_START) / DEV_REG_SIZE;
  unsigned int* sem = &deviceSemaphores[semIndex];

  // Corrisponde all'indirizzo: 0x1000.02C4
  memaddr terminalDevRegAddr = DEV_REG_ADDR(IL_TERMINAL, 0);

  // Caso 1: device è uno tra disk, flash o printer
  if ((memaddr)cmdAdd < terminalDevRegAddr) {
    dtpreg_t* nonTerminalDevice = (dtpreg_t*)cmdAdd;

    // Inseriamo il cmdValue nel campo "command" del device
    nonTerminalDevice->command = cmdValue;

    // Finchè non ha completato l'operazione dobbiamo bloccare il currentProcess
    while (nonTerminalDevice->status == BUSY)
      ;

    // Eseguo una P sul semaforo del device
    passeren(sem);

    return nonTerminalDevice->status;
  }

  // Caso 2: terminal device
  termreg_t* terminalDevice = (termreg_t*)cmdAdd;
}
*/

// DA SISTEMARE
int doIODevice(int* cmdAdd, int cmdValue) {
  //  // Il parametro "cmdAdd" non contiene un indirizzo valido
  //  if ((memaddr)cmdAdd < DEV_REG_START || (memaddr)cmdAdd > DEV_REG_END)
  //  PANIC(); int interruptLine; int deviceNumber; int semIndex; int status;
  //  // Troviamo l'interrupt line e il device number del device register
  //  if (isDiskDevice(cmdAdd)) {
  //    interruptLine = IL_DISK;
  //    deviceNumber = findDeviceNumber(cmdAdd, interruptLine);
  //    semIndex = getSemaphoreIndex(deviceNumber, interruptLine, 0);
  //    *cmdAdd = cmdValue;
  //    dtpreg_t* deviceReg = (dtpreg_t*)DEV_REG_ADDR(interruptLine,
  //    deviceNumber); status = deviceReg->status;
  //  }
  //  if (isFlashDevice(cmdAdd)) {
  //    interruptLine = IL_FLASH;
  //    deviceNumber = findDeviceNumber(cmdAdd, interruptLine);
  //    semIndex = getSemaphoreIndex(deviceNumber, interruptLine, 0);
  //    *cmdAdd = cmdValue;
  //    dtpreg_t* deviceReg = (dtpreg_t*)DEV_REG_ADDR(interruptLine,
  //    deviceNumber); status = deviceReg->status;
  //  }
  //
  //  if (isEthernetDevice(cmdAdd)) {
  //    interruptLine = IL_ETHERNET;
  //    deviceNumber = findDeviceNumber(cmdAdd, interruptLine);
  //    semIndex = getSemaphoreIndex(deviceNumber, interruptLine, 0);
  //    *cmdAdd = cmdValue;
  //    dtpreg_t* deviceReg = (dtpreg_t*)DEV_REG_ADDR(interruptLine,
  //    deviceNumber); status = deviceReg->status;
  //  }
  //
  //  if (isPrinterDevice(cmdAdd)) {
  //    interruptLine = IL_PRINTER;
  //    deviceNumber = findDeviceNumber(cmdAdd, interruptLine);
  //    semIndex = getSemaphoreIndex(deviceNumber, interruptLine, 0);
  //    *cmdAdd = cmdValue;
  //    dtpreg_t* deviceReg = (dtpreg_t*)DEV_REG_ADDR(interruptLine,
  //    deviceNumber); status = deviceReg->status;
  //  }
  //
  //  if (isTerminalDevice(cmdAdd)) {
  //    interruptLine = IL_TERMINAL;
  //    deviceNumber = findDeviceNumber(cmdAdd, interruptLine);
  //    int* deviceReg = DEV_REG_ADDR(interruptLine, deviceNumber);
  //    if (cmdAdd == deviceReg + 0x4) {
  //      semIndex = getSemaphoreIndex(deviceNumber, interruptLine, 0);
  //      termreg_t* termDev =
  //          (termreg_t*)DEV_REG_ADDR(interruptLine, deviceNumber);
  //      termDev->recv_command = cmdValue;
  //      status = termDev->recv_status;
  //    } else if (cmdAdd == deviceReg + 0xc) {
  //      semIndex = getSemaphoreIndex(deviceNumber, interruptLine, 1);
  //      termreg_t* termDev =
  //          (termreg_t*)DEV_REG_ADDR(interruptLine, deviceNumber);
  //      termDev->transm_command = cmdValue;
  //      status = termDev->transm_status;
  //    }
  //  }
  //
  //  int* semAddress = &(deviceSemaphores[semIndex]);
  //  passeren(semAddress);
  //  // Finchè non ha completato l'operazione dobbiamo bloccare il
  //  currentProcess
  //
  //  return status;
}

int getCPUTime() {
  cpu_t intervalEndTime;
  // salvo il valore del TOD
  STCK(intervalEndTime);
  // Calcolo la durata dell'intervallo tra inizio processo e chiamata della
  // SYSCALL
  accomulatedCPUTime = intervalEndTime - accomulatedCPUTime;
  // Calcolo e salvo il tempo accomulato dal processo
  currentProcess->p_time += accomulatedCPUTime;
  return currentProcess->p_time;
}

void waitForClock() {
  // Esegue una P sul semaforo del Pseudo-clock
  return passeren(&(deviceSemaphores[48]));
}

support_t* getSupportData() {
  return (support_t*)(currentProcess->p_supportStruct || NULL);
}

int getProcessID(int parent) {
  if (parent == 0) return currentProcess->p_pid;
  return currentProcess->p_parent->p_pid;
}

// Da modificare in modo che se se il processo che la esegue è l'unico ad alta
// priorità allora deve passare la CPU a uno a bassa priorità
void yield() {
  if (currentProcess != NULL) {
    if (currentProcess->p_prio == PROCESS_PRIO_LOW)
      insertProcQ(&readyQueueLo, currentProcess);
    else if (currentProcess->p_prio == PROCESS_PRIO_HIGH)
      insertProcQ(&readyQueueHi, currentProcess);

    currentProcess = NULL;

    return waitForClock();
  }

  return PANIC();
}

// Le system calls non bloccanti, devono caricare lo stato che si trova in
// p0_state e non lo stato (obsoleto) che si trova nel "currentProcess"
// LDST(p0_state);
