#ifndef INTERRUPTHANDLER_H
#define INTERRUPTHANDLER_H

#include "../pandos_types.h"
#include "exceptionhandler.h"

cpu_t interruptstarttime, interruptendtime;
int getInterruptInt(memaddr* map);
void InterruptExcpetionHandler();
void NonTimerHandler(int line,int dev);
#endif