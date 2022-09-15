#include "../pandos_const.h"
#include "../pandos_types.h"
#define TRUE 1
#define FALSE 0

/* Page Table Starting Address */
#define PAGETBLSTART 0x80000000

#define DISABLEINTERRUPTS setSTATUS(getSTATUS() & (~IECON))
#define ENABLEINTERRUPTS setSTATUS(getSTATUS() | IECON)
/* #define POOLSTART (RAMSTART + (32 * PAGESIZE)) */
extern memaddr POOLSTART;
#define __GETVPN(T) (T & GETPAGENO) >> VPNSHIFT
#define GETVPN(T) ((T >= KUSEG && T < 0xBFFFF000) ? __GETVPN(T) : 31) //indirizzo ultimo frame dedicato a stack
//flashCmd(FLASHWRITE, victimPgAddr, devBlockNum, victimPgOwner); //scrivi il contenuto di victimPgAddr dentro blocco devBlockNum del dispositivo flash victimPgOwner
int flashCmd(int cmd, int block, int devBlockNum, int flashDevNum);
void updateTLB(pteEntry_t *sw_pte);
void killProc(int *sem);
extern memaddr *getDevRegAddr(int line, int devNo);
extern int getDevSemIndex(int line, int devNo, int isReadTerm);
void initSwap();
