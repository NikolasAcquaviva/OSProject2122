/**
 * @file exceptions.c
 * @author Enrico Emiro
 * @author Giulio Billi
 * @brief Implementazione dell'exception handler
 * @date 2022-04-01
 */
#include "phase_2/exceptions.h"

#include "phase_1/asl.h"
#include "phase_2/globals.h"
#include "phase_2/interrupts.h"
#include "phase_2/scheduler.h"
#include "phase_2/syscall.h"

#include "utils/helpers.h"

HIDDEN void syscallHandler();
HIDDEN void passUpOrDie(int code);

/**
 * @brief Gestore delle eccezioni
 */
void exceptionHandler() {
  // Sezione 3.4 Exception Handling (Important Point)
  // Lo stato di eccezione (per il CP0) è salvato all'inizio del
  // BIOS Data Page (0x0FFF.F000)
  state_t* p0_state = (state_t*)BIOSDATAPAGE;

  // Prendo il codice dell'eccezione (situato nel registro Cause)
  int excCode = CAUSE_GET_EXCCODE(p0_state->cause);

  printStr("Exception Code: ");
  printDec(excCode);

  switch (excCode) {
    /** Interrupts */
    case EXC_INT:
      return interruptExceptionHandler();

    /**
     * TLB
     * - 1: Mod  (TLB-Modification Exception)
     * - 2: TLBL (TLB Invalid Exception on a Load or Fetch instruction)
     * - 3: TLBS (TLB Invalid Exception on a Store instruction)
     */
    case EXC_MOD:
    case EXC_TLBL:
    case EXC_TLBS:
      return passUpOrDie(PGFAULTEXCEPT);

    /** Syscall */
    case EXC_SYS:
      return syscallHandler();

    /**
     * Program Traps
     * - 4:  AdEL (Address Error Exception on a Load or Fetch instruction)
     * - 5:  AdES (Address Error Exception on a Store instruction)
     * - 6:  IBE  (Bus Error Exception on a Fetch instruction)
     * - 7:  DBE  (Bus Error Exception on a Load or Store data access)
     * - 9:  Bp   (Breakpoint exception)
     * - 10: RI   (Reserved Instruction Exception)
     * - 11: CpU  (Coprocessor Unusable Exception)
     * - 12: OV   (Arithmetic Overflow Exception)
     * - default  (Codice inesistente)
     */
    case EXC_ADEL:
    case EXC_ADES:
    case EXC_IBE:
    case EXC_DBE:
    case EXC_BP:
    case EXC_RI:
    case EXC_CPU:
    case EXC_OV:
    default:
      return passUpOrDie(GENERALEXCEPT);
  }
}

/**
 * @brief Si occupa di gestire le syscall
 */
HIDDEN void syscallHandler() {
  // Salva il tempo d'entrata nella funzione
  STCK(startBlockingSyscallTime);

  // CP0 state
  state_t* p0_state = (state_t*)BIOSDATAPAGE;

  // Prelevo il codice della syscall
  int syscallCode = p0_state->reg_a0;

  printStr("Syscall Code: ");
  printDec(syscallCode);

  // Sezione 3.5.11 (NSYS1, NSYS2, ... in User-Mode)
  // Sezione 3.7.1 (SYSCALL Exceptions Numbered by positive numbers)
  // Controllo:
  // - Se la syscall è stata chiamata in modalità utente
  // - Se il "syscallCode" non è valido (maggiore di 1)
  // Se si verifica almeno una delle condizioni viene generata una program trap
  if (((p0_state->status & USERPON) != ALLOFF) || syscallCode >= 1) {
    // Nota: bisogna shiftare di "CAUSESHIFT" poichè il campo ExcCode
    // inizia dal 2° bit (figura 3.3-pops)
    p0_state->cause |= (EXC_RI << CAUSESHIFT);
    return passUpOrDie(GENERALEXCEPT);
  }

  unsigned int arg1 = p0_state->reg_a1;
  unsigned int arg2 = p0_state->reg_a2;
  unsigned int arg3 = p0_state->reg_a3;

  // Puntatore all'indirizzo di memoria del caller v0
  unsigned int* regV0 = &currentProcess->p_s.reg_v0;

  // Incremento il PC di una WORD (32 bit = 4 byte)
  p0_state->pc_epc += WORD_SIZE;

  switch (syscallCode) {
    case CREATEPROCESS:
      *regV0 = createProcess((state_t*)arg1, (int)arg2, (support_t*)arg3);
      break;

    case TERMPROCESS:
      terminateProcess((int)arg1);
      break;

    case PASSEREN:
      passeren((int*)arg1);
      break;

    case VERHOGEN:
      verhogen((int*)arg1);
      break;

    case DOIO:
      *regV0 = doIODevice((int*)arg1, (int)arg2);
      break;

    case GETTIME:
      *regV0 = getCPUTime();
      break;

    case CLOCKWAIT:
      waitForClock();
      break;

    case GETSUPPORTPTR:
      *regV0 = (memaddr)getSupportData();
      break;

    case GETPROCESSID:
      *regV0 = getProcessID((int)arg1);
      break;

    case YIELD:
      yield();
      break;
  }
}

/**
 * @brief Nel caso avvenga un program trap o una syscall con valore positivo o
 * una TLB exception bisogna gestire l'eccezione dandola al livello
 * supporto (Pass Up) oppure terminando il processo (Die)
 *
 * @param code
 */
HIDDEN void passUpOrDie(int code) {
  // Il processo non ha la support struct, terminalo
  if (currentProcess->p_supportStruct == NULL) {
    terminateProcess(currentProcess->p_pid);
    return scheduler();
  }
  /** Il processo ha la support struct, passalo al livello superiore */
  else {
    // Viene inserito nel campo appropriato del excpState il saved exception
    // state dal BIOS Data Page
    memCpy((state_t*)BIOSDATAPAGE,
           &currentProcess->p_supportStruct->sup_exceptState[code],
           sizeof(state_t));

    // Si esegue una LDCXT con i campi appropriati per garantire la gestione
    // al livello superiore
    LDCXT(currentProcess->p_supportStruct->sup_exceptContext[code].stackPtr,
          currentProcess->p_supportStruct->sup_exceptContext[code].status,
          currentProcess->p_supportStruct->sup_exceptContext[code].pc);
  }
}
