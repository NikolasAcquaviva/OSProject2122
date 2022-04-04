#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "../pandos_types.h"
#define TIME_CONVERT(T) ((T) * (*((memaddr *) TIMESCALEADDR)))
#define CURRENT_TOD ((*((memaddr *)TODLOADDR)) / (*((cpu_t *)TIMESCALEADDR)))
extern cpu_t startTime;
extern cpu_t finishTime;

#endif