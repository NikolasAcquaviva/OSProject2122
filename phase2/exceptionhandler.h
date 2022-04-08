#ifndef EXCEPTION_HANDLER_H
#define EXCEPTION_HANDLER_H

#define CAUSEMASK 0xFF
int codiceEccezione;
void GeneralExceptionHandler();
void PassUp_Or_Die(int index);
void TLBExceptionHandler();
void TrapExceptionHandler();
extern void InterruptExceptionHandler();
void SYSCALLExceptionHandler();
int CREATE_PROCESS(state_t *statep, int prio, support_t *supportp);
void TERM_PROCESS(int pid, int a2, int a3);
void _PASSEREN(int *semaddr, int a2, int a3);
void _VERHOGEN(int *semaddr, int a2, int a3);
int DO_IO(int *cmdAddr, int cmdValue, int a3);
int GET_CPU_TIME(int a1, int a2, int a3);
void WAIT_FOR_CLOCK(int a1, int a2, int a3);
support_t* GET_SUPPORT_DATA(int a1, int a2, int a3);
int GET_PROCESS_ID(int parent, int a2, int a3);
void _YIELD(int a1, int a2, int a3);
extern void scheduler();
#endif