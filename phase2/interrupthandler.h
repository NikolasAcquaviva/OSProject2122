#ifndef INTERRUPTHANDLER_H
#define INTERRUPTHANDLER_H

#include "../pandos_types.h"
#include "exceptionhandler.h"

memaddr *getInterruptLineAddr(int line);
int getInterruptInt(int map);
void NonTimerHandler(int line,int dev);
#endif