//cercare "DA CONTROLLARE"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>
#include "../pandos_const.h"
#include "../pandos_types.h"
#include "../phase2/init.h"

#define DISABLEINTERRUPTS setSTATUS(getSTATUS() & (~IECON))
#define ENABLEINTERRUPTS setSTATUS(getSTATUS() | IECON)
#define POOLSTART (RAMSTART + (32 * PAGESIZE))
#define __GETVPN(T) (T & GETPAGENO) >> VPNSHIFT
#define GETVPN(T) ((T >= KUSEG && T < 0xBFFFF000) ? __GETVPN(T) : 31) //indirizzo ultimo frame dedicato a stack

swap_t swapTable[UPROCMAX*2]; //ci consente una panoramica aggiornata sulla swapPool. MEMORIA FISICA
//DA CONTROLLARE QUESTA VARIABILE
int swapSem; //per accedere alla swapPool in mutex

void initSwap(){
	swapSem =  1;
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

void updateTLB(){
	TLBCLR();
}

void flashCmd(int cmd, int block, int devBlockNum, int flashDevNum){
	//((line - 3) * 8) + (line == 7 ? (isRead * 8) + dev : dev);
	int semNo = (FLASHINT - 3)*8 + flashDevNum;
	//prende mutex sul device register associato al flash device
	SYSCALL(PASSEREN, (int) &deviceSemaphores[semNo], 0, 0);
	(memaddr*) (0x10000054 + ((FLASHINT - 3) * 0x80) + (flashDevNum * 0x10));
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
		//DA CONTROLLARE
		SYSCALL(PASSEREN, &swapSem, 0, 0);
		int missingPage = GETVPN(currSup->sup_exceptState[PGFAULTEXCEPT].entry_hi); //l'exception state della BIOSDATAPAGE è stato caricato qui in GeneralExceptionHandler
		
		int victimPgNum = getVictimPage();
		memaddr victimPgAddr = POOLSTART + victimPgNum*PAGESIZE; //indirizzo FISICO!

		//if it was occupied
		if (swapTable[victimPgNum].sw_asid != -1){
			//anche se siamo in mutex, abbiamo interrupt abilitati nel livello di supporto, pertanto in un multiproc sys potremmo essere interrotti (?)
			DISABLEINTERRUPTS;
			
			//invalidiamo associazione
			swapTable[victimPgNum].sw_pte->pte_entryLO &= ~VALIDON; //"puntatore alla entry corrispondente nella tabella delle pagine del processo" => lo vedrà anche l'altro processo!
			//update TLB
			updateTLB();	
			
			ENABLEINTERRUPTS;

			int 
			int victimPgOwner = swapTable[victimPgNum].sw_asid; //un flash device associato ad ogni ASID

			//update old owner's process backing storage
			flashCmd(FLASHWRITE, victimPgAddr, , victimPgOwner);


		}

	}

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
