#include "vmSupport.h" //prende initSwap
#include <umps3/umps/libumps.h>
#include <umps3/umps/types.h>
#include "../phase2/init.h"
#define IL_MASK_ALL 0x0000FF00

//Master sempahore which wait for all processes to be concluded in order to terminate testing
int masterSem;
int devSem[49];
//A static array of support structures to allocate
static support_t supPool[UPROCMAX + 1]; //+1 perchè ASID 0 is reserved for kernel daemons

extern void pager(); //TLB Exception handler
extern void supGeneralExceptionHandler();

//CREATE A PROCESS USING ITS ID (PROCESS ASID)
static void createUProc(int id){
    memaddr ramTop;
    RAMTOP(ramTop);
    memaddr stackTop = ramTop - (2*id*PAGESIZE);

    state_t newState;
    newState.entry_hi = id << ASIDSHIFT; //figura 6.6 pops
    newState.reg_sp = 0xC0000000;
    newState.pc_epc = newState.reg_t9 = 0x800000B0;
    newState.status = USERPON | IEPON | TEBITON | IMON | IL_MASK_ALL; //IEPON => check if IMON

    supPool[id].sup_asid = id;

    // initialization of the process private PageTable
	for (int j = 0; j < MAXPAGES - 1; j++){ //-1 perchè l'ultima entry è dedicata allo stack'
        supPool[id].sup_privatePgTbl[j].pte_entryHI = 0x80000000 + (j << VPNSHIFT) + (id << ASIDSHIFT);
        supPool[id].sup_privatePgTbl[j].pte_entryLO = DIRTYON; //bisognerebbe fare check di quali aree fanno parte di .text
	}
    supPool[id].sup_privatePgTbl[MAXPAGES - 1].pte_entryHI = 0xBFFFF000 + (id << ASIDSHIFT);
    supPool[id].sup_privatePgTbl[MAXPAGES - 1].pte_entryLO = DIRTYON;

    supPool[id].sup_exceptContext[PGFAULTEXCEPT].stackPtr =(memaddr) (stackTop + PAGESIZE);
    supPool[id].sup_exceptContext[PGFAULTEXCEPT].status =  IEPON | TEBITON | IMON | IL_MASK_ALL;
    supPool[id].sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr) pager;
    /* supPool[id].sup_exceptContext[PGFAULTEXCEPT].stackPtr =(memaddr) &(supPool[id].sup_exceptContext[PGFAULTEXCEPT].sup_stack) ; */

    supPool[id].sup_exceptContext[GENERALEXCEPT].stackPtr =(memaddr) stackTop; ;
    supPool[id].sup_exceptContext[GENERALEXCEPT].status =  IEPON | TEBITON | IMON | IL_MASK_ALL;
    supPool[id].sup_exceptContext[GENERALEXCEPT].pc = (memaddr) supGeneralExceptionHandler;

    int status = SYSCALL(CREATEPROCESS, (int) &newState, PROCESS_PRIO_LOW, (int) &(supPool[id]));
    if (status == -1){ //CREATEPROCESS se errore ritorna -1
        SYSCALL(TERMPROCESS, 0, 0, 0);
    }
}


//entrypoint per la creazione dei processi di test
/*
Inizializza Swap Pool table e semafori, crea gli 8 processi e ne gestisce la terminazione
Il nome è rimasto 'test' dalla fase 2
*/
void test() {
    initSwap();
    //init mutex sem4s vs sync sem4s in phase2
    for(int i=0; i < 49; i++) devSem[i]=1; 
    for (int id=1; id <= UPROCMAX; id++) createUProc(id);
    //second choice
    masterSem = 0;
    for (int j = 0; j < UPROCMAX; j++){
        SYSCALL(PASSEREN, (int) &masterSem, 0, 0);
        klog_print("ricevuto un ack su UPROCMAX\n");
    }
    //killing test process ?
    SYSCALL(TERMPROCESS, 0, 0, 0);
}
