#include <umps3/umps/libumps.h>
#include "../pandos_const.h"
#include "../pandos_types.h"
#include "../phase2/init.h"

#define __GETVPN(T) (T & GETPAGENO) >> VPNSHIFT
#define GETVPN(T) ((T >= KUSEG && T < 0xBFFFF000) ? __GETVPN(T) : 31) //indirizzo ultimo frame dedicato a stack

swap_t swapTable[UPROCMAX*2]; //ci consente una panoramica aggiornata sulla swapPool
int swapSem; //per accedere alla swapPool in mutex

void initSwap(){
	swapSem = 1;
	for (int i=0; i < UPROCMAX*2; i++){
		swapTable[i].sw_asid = -1;	
	}
}

void killProc(){} 

 /*re-inizializza tutte le entry del processo che sta per essere ucciso*/
void clearSwap(int asid){
    for (int i = 0; i < UPROCMAX * 2; i++)
    {
        if (swapTable[i].sw_asid == asid)
        {
            swapTable[i].sw_asid = -1;
        }  
    }
}

//pg fault
void pager(){
	support_t *currSup = (support_t*) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
	int cause = (currSup->sup_exceptState[0].cause & GETEXECCODE) >> CAUSESHIFT;
	//if the cause is a TLB mod exc => trap	
	if (cause != TLBINVLDL && cause != TLBINVLDS){
		killProc();
	}
	else {
		//swap pool mutex
		SYSCALL(PASSEREN, (int) &swapSem, 0, 0);
		int missingPage = GETVPN(currSup->sup_exceptState[PGFAULTEXCEPT].entry_hi); //l'exception state della BIOSDATAPAGE è stato caricato qui in GeneralExceptionHandler
		
		int freedIndex = getVictimPage();
		//if it was occupied...
		if (swapTable[freedIndex].sw_asid != -1){
			
		}

	}

}

//FIFO replacement algo se prima non è stata trovata alcuna pagina libera as suggested
int getVictimPage(){
	static int frame = 0;
	for (int i=0; i < UPROCMAX*2; i++){
		if (swapTable[i].sw_asid == -1) {
			frame = i;
			return frame;
		}
	}
	/*altrimenti passa al prossimo*/
    frame = (frame + 1) % (UPROCMAX * 2);
    return frame;
}


//entry non trovata in TLB => la recuperiamo dalla tabella delle pagine del processo corrente
void uTLB_RefillHandler(){
	 /*prende l'inizio di BIOSDATAPAGE*/ // retrieving the exception state
	state_t* currProc_s = (state_t*) BIOSDATAPAGE;

	int vpn = GETVPN(currProc_s->entry_hi);
	//currentProcess esportata includendo init.h
	setENTRYHI(currentProcess->p_supportStruct->sup_privatePgTbl[vpn].pte_entryHI);
    setENTRYLO(currentProcess->p_supportStruct->sup_privatePgTbl[vpn].pte_entryLO);
    //WRITING ON TLB
    TLBWR();        
    //LOADING PREVIOUS STATE. ripeterà l'istruzione e questa volta troverà l'entry
	LDST(currProc_s);
}
