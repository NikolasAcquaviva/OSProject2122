#include "../pandos_const.h"
#include "../pandos_types.h"
#include "../phase2/init.h" //esporta currentProcess
#define TRUE 1
#define FALSE 0
#define DISABLEINTERRUPTS setSTATUS(getSTATUS() & (~IECON))
#define ENABLEINTERRUPTS setSTATUS(getSTATUS() | IECON)
#define POOLSTART (RAMSTART + (32 * PAGESIZE))
#define __GETVPN(T) (T & GETPAGENO) >> VPNSHIFT
#define GETVPN(T) ((T >= KUSEG && T < 0xBFFFF000) ? __GETVPN(T) : 31) //indirizzo ultimo frame dedicato a stack
void killProc(int *sem);
extern memaddr *getDevRegAddr(int line, int devNo);
extern int getDevSemIndex(int line, int devNo, int isReadTerm);
void initSwap();
