#ifndef ASL_H

#define ASL_H

#include "../h/pandos_const.h" //accordarsi per stabilire una volta per tutte che tutti gli headers stanno in /h non ../h
#include "../h/pandos_types.h"
#include "../h/listx.h"
#include "../h/pcb.h"
#include "../h/asl.h"

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