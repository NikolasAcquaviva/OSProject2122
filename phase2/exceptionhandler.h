#ifndef EXCEPTION_HANDLER_H
#define EXCEPTION_HANDLER_H

#include "../pandos_types.h"

void GeneralExceptionHandler();
static void PassUp_Or_Die(int index);
static void TLBExceptionHandler();
static void TrapExceptionHandler();
extern void InterruptExceptionHandler();
static void SYSCALLExceptionHandler();
static int CREATE_PROCESS(state_t *statep, int prio, support_t *supportp);
static void TERM_PROCESS(int pid, int a2, int a3);
static void _PASSEREN(int *semaddr, int a2, int a3);
static void _VERHOGEN(int *semaddr, int a2, int a3);
static int DO_IO(int *cmdAddr, int cmdValue, int a3);
static int GET_CPU_TIME(int a1, int a2, int a3);
static void WAIT_FOR_CLOCK(int a1, int a2, int a3);
static support_t* GET_SUPPORT_DATA(int a1, int a2, int a3);
static int GET_PROCESS_ID(int parent, int a2, int a3);
static void _YIELD(int a1, int a2, int a3);

#endif