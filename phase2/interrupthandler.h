#ifndef INTERRUPTHANDLER_H
#define INTERRUPTHANDLER_H

#include "../pandos_types.h"
#include "exceptionhandler.h"
cpu_t interruptstarttime, interruptendtime;
memaddr *getInterruptLineAddr(int line);
int getInterruptInt(memaddr* map);
void NonTimerHandler(int line,int dev);
#endif