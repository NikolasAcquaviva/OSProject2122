#include "../pandos_const.h"
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "init.h"
#define TIME_CONVERT(T) ((T) * (*((memaddr *) TIMESCALEADDR)))
extern cpu_t startTime;