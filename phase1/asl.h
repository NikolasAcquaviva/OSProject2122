#ifndef ASL_H

#define ASL_H

#include "../h/pandos_const.h"
#include "../h/pandos_types.h"
#include "../h/listx.h"

#define HIDDEN static
#define MAX_PROC 20

int insertBlocked(int *semAdd, pcb_t *p);
pcb_t* removeBlocked(int *semAdd);
pcb_t* outBlocked(pcb_t *p);
pcb_t* headBlocked(int *semAdd);
void initASL();

#endif