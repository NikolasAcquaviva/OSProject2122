#ifndef ASL_H

#define ASL_H

#include "../pandos_const.h"
#include "../pandos_types.h"
#include "../listx.h"
#include "pcb.h"

#define TRUE 1
#define FALSE 0
#define MAX_PROC 20
#define HIDDEN static


int insertBlocked(int *semAdd, pcb_t *p);
pcb_t* removeBlocked(int *semAdd);
pcb_t* outBlocked(pcb_t *p);
pcb_t* headBlocked(int *semAdd);
void initASL();

#endif