#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../pandos_types.h"
#define TIME_CONVERT(T) ((T) * (*((memaddr *) TIMESCALEADDR)))
extern cpu_t startTime;

#endif