//cercare "DA CONTROLLARE"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>
#include "../pandos_const.h"
#include "../pandos_types.h"
#include "../phase2/init.h"
//#include "../phase2/interrupthandler.h"
//AGGIUNGERE LE SEGUENTI DUE FUNZIONI A interrupthandler.c
memaddr *getDevRegAddr(int line, int devNo){
	return (memaddr *) (0x10000054 + ((line - 3) * 0x80) + (devNo * 0x10));
}

int getDevSemIndex(int line, int devNo, int isReadTerm){
	return ((line - 3) * 8) + (line == 7 ? (isReadTerm * 8) + devNo : devNo);
}

//Master sempahore which wait for all processes to be concluded in order to terminate testing
extern int masterSem; //da initProc.c

#define TRUE 1
#define FALSE 0
#define DISABLEINTERRUPTS setSTATUS(getSTATUS() & (~IECON))
#define ENABLEINTERRUPTS setSTATUS(getSTATUS() | IECON)
#define POOLSTART (RAMSTART + (32 * PAGESIZE))
#define __GETVPN(T) (T & GETPAGENO) >> VPNSHIFT
#define GETVPN(T) ((T >= KUSEG && T < 0xBFFFF000) ? __GETVPN(T) : 31) //indirizzo ultimo frame dedicato a stack
/* Page Table Starting Address */
#define PAGETBLSTART 0x80000000


swap_t swapTable[UPROCMAX*2]; //ci consente una panoramica aggiornata sulla swapPool. MEMORIA FISICA
//DA CONTROLLARE QUESTA VARIABILE
int swapSem; //per accedere alla swapPool in mutex

void initSwap(){
	swapSem =  1;
	for (int i=0; i < UPROCMAX*2; i++){
		swapTable[i].sw_asid = -1;	
	}
}


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

void killProc(int *sem){

	clearSwap(currentProcess->p_supportStruct->sup_asid);

	if (sem != NULL) SYSCALL(VERHOGEN, (int) sem, 0, 0);

	SYSCALL(VERHOGEN, (int) &masterSem, 0, 0);
	SYSCALL(TERMPROCESS, 0, 0, 0);

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



//flashCmd(FLASHWRITE, victimPgAddr, devBlockNum, victimPgOwner); //scrivi il contenuto di victimPgAddr dentro blocco devBlockNum del dispositivo flash victimPgOwner
int flashCmd(int cmd, int block, int devBlockNum, int flashDevNum){
	//((line - 3) * 8) + (line == 7 ? (isRead * 8) + dev : dev);
	int semNo = getDevSemIndex(FLASHINT, flashDevNum, FALSE);		//(FLASHINT - 3)*8 + flashDevNum;
	//prende mutex sul device register associato al flash device
	SYSCALL(PASSEREN, (int) &deviceSemaphores[semNo], 0, 0);
	devreg_t* flashDevReg = (devreg_t*) getDevRegAddr(FLASHINT, flashDevNum);	//(memaddr*) (0x10000054 + ((FLASHINT - 3) * 0x80) + (flashDevNum * 0x10));
	
	/*carica data0 con il blocco da leggere o scrivere*/
	flashDevReg->dtp.data0 =  block;

	// inserting the command after writing into data
	flashDevReg->dtp.command = (devBlockNum << 8) | cmd;
	int devStatus = SYSCALL(DOIO, FLASHINT, flashDevNum, 0);
	SYSCALL(VERHOGEN, (int) &deviceSemaphores[semNo], 0, 0);
	
	if (devStatus != READY){
		return -1;
	}
	else return devStatus;
}


//pg fault
void pager(){
	support_t *currSup = (support_t*) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
	int cause = (currSup->sup_exceptState[0].cause & GETEXECCODE) >> CAUSESHIFT;
	//if the cause is a TLB mod exc => trap	
	if (cause != TLBINVLDL && cause != TLBINVLDS){
		killProc(NULL);
	}
	else {
		//swap pool mutex
		//DA CONTROLLARE
		SYSCALL(PASSEREN, (int) &swapSem, 0, 0);
		int missingPage = GETVPN(currSup->sup_exceptState[PGFAULTEXCEPT].entry_hi); //l'exception state della BIOSDATAPAGE è stato caricato qui in GeneralExceptionHandler
	
		int devStatus;

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

			// block of the flash device to write into (coincides with the page number)
			int devBlockNum = (swapTable[victimPgNum].sw_pte->pte_entryHI - PAGETBLSTART) >> VPNSHIFT;
			int victimPgOwner = (swapTable[victimPgNum].sw_asid) - 1; //un flash device associato ad ogni ASID. 0 based

			//update old owner's process backing storage
			devStatus = flashCmd(FLASHWRITE, victimPgAddr, devBlockNum, victimPgOwner);
			if (devStatus != READY){
				killProc(&swapSem);
			}
			
		}
		
		//Read the contents of the currentProcess's backing storage/flash device logical page p into frame i
		devStatus = flashCmd(FLASHREAD, victimPgAddr, missingPage, currSup->sup_asid);
		if (devStatus != READY){
			killProc(&swapSem);
		}
		
		//UPDATING SWAP POOL TABLE ENTRY TO REFLECT THE NEW CONTENTS
		swapTable[victimPgNum].sw_asid = currSup->sup_asid;
		swapTable[victimPgNum].sw_pageNo = missingPage;
		swapTable[victimPgNum].sw_pte = &(currSup->sup_privatePgTbl[missingPage]);

		DISABLEINTERRUPTS;
		
		/*accende il V bit, il D bit e setta PNF*/
		swapTable[victimPgNum].sw_pte->pte_entryLO = victimPgAddr | VALIDON | DIRTYON;
		updateTLB();	

		ENABLEINTERRUPTS;	
		

		/*rilascia la mutua esclusione*/
		SYSCALL(VERHOGEN, (int) &swapSem, 0, 0);
		
		/*ritorna il controllo a current e ritenta*/
		LDST((state_t *) &(currSup->sup_exceptState[PGFAULTEXCEPT]));

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
