#ifndef EXCEPTION_HANDLER_H
#define EXCEPTION_HANDLER_H

#include "../pandos_types.h"

int CREATE_PROCESS(state_t *statep, int prio, support_t *supportp);
void TERM_PROCESS(int pid, int a2=0, int a3=0);
void _PASSEREN(int *semaddr, int a2=0, int a3=0);
void _VERHOGEN(int *semaddr, int a2=0, int a3=0);
int DO_IO(int *cmdAddr, int cmdValue, int a3=0);
int GET_CPU_TIME(int a1=0, int a2=0, int a3=0);
int WAIT_FOR_CLOCK(int a1=0, int a2=0, int a3=0);
support_t* GET_SUPPORT_DATA(int a1=0, int a2=0, int a3=0);
int GET_PROCESS_ID(int parent, int a2=0, int a3=0);
int _YIELD(int a1=0, int a2=0, int a3=0);

#endif