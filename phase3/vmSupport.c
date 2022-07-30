//cercare "DA CONTROLLARE"
#include "initProc.h"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>
#include <umps3/umps/cp0.h>
#include <umps3/umps/arch.h>
#include <umps3/umps/types.h>
#include "../pandos_const.h"
#include "../pandos_types.h"
#include "../phase2/init.h"
#include "vmSupport.h"
extern pcb_PTR currentProcess;

//#include "../phase2/interrupthandler.h"
//AGGIUNGERE LE SEGUENTI DUE FUNZIONI A interrupthandler.c
memaddr *getDevRegAddr(int line, int devNo){
	return (memaddr *) (0x10000054 + ((line - 3) * 0x80) + (devNo * 0x10));
}

int getDevSemIndex(int line, int devNo, int isReadTerm){
	return ((line - 3) * 8) + (line == 7 ? (isReadTerm * 8) + devNo : devNo);
}

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
    //TODO controllare se asid detiene If the specified U-Proc is holding hostage the Swap Pool Table,
    //https://github.com/lucat1/unibo_08574_progetto/blob/6a98e14a18b875e8444339dce4c451a34e54b83e/phase3/src/pager.c
    //riga 140
    //idea principale è di tenere un array che rappresenta lo stato del sys
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

void updateTLB(pteEntry_t *sw_pte, int isCurr){
	/*abbiamo 2 casi di updateTLB nel pager:
		1. dopo aver invalidato la entry della page victim, bisogna aggiornare quella entry anche nella TLB,se esiste
		2. dopo aver inserito la pagina che ora è considerata valid per il current process (iscurr=1)
	*/

	setENTRYHI(sw_pte->pte_entryHI);
	setENTRYLO(sw_pte->pte_entryLO);
	TLBP(); //in registro index abbiamo primo bit che indica se esiste in cache la entry data dai registri cp0 entryHI/LO
	int index = getINDEX(); //nei bit da 8 a 15 abbiamo l'index del tlb nel quale scrivere i registri hi/lo tramite TLBWI()
	int probe = (index >> 31); //prendo il bit probe del registro index (6.4 pops), se è 0 c'è un tlb hit
	if(probe == 0)
		TLBWI();
	else if(isCurr==1) TLBWR(); // nel caso in cui la pagina invalidata (isCurr=0) non fosse presente in cache allora non facciamo niente

}

//flashCmd(FLASHWRITE, victimPgAddr, devBlockNum, victimPgOwner); //scrivi il contenuto di victimPgAddr dentro blocco devBlockNum del dispositivo flash victimPgOwner
int flashCmd(int cmd, int block, int devBlockNum, int flashDevNum){
	int semNo = getDevSemIndex(FLASHINT, flashDevNum, FALSE);		//(FLASHINT - 3)*8 + flashDevNum;
	//prende mutex sul device register associato al flash device
	SYSCALL(PASSEREN, (int) &devSem[semNo], 0, 0);
	dtpreg_t* flashDevReg = (dtpreg_t*) getDevRegAddr(FLASHINT, flashDevNum);	//(memaddr*) (0x10000054 + ((FLASHINT - 3) * 0x80) + (flashDevNum * 0x10));
	
	/*carica data0 con il blocco da leggere o scrivere*/
	flashDevReg->data0 =  block;

	// inserting the command after writing into data
	unsigned int value = (devBlockNum << 8) | cmd;
	flashDevReg->command = value; 
	int devStatus = SYSCALL(DOIO, (int) &flashDevReg->command, value, 0);
	SYSCALL(VERHOGEN, (int) &devSem[semNo], 0, 0);
	return devStatus;
}

//pg fault
void pager(){
	support_t *currSup = (support_t*) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
	//if the cause is a TLB mod exc => trap	
    
    int cause = (currSup->sup_exceptState[0].cause & GETEXECCODE) >> CAUSESHIFT;
	if (cause == 1){
		killProc(NULL);
	}
	else{
		//swap pool mutex
		//DA CONTROLLARE
		SYSCALL(PASSEREN, (int) &swapSem, 0, 0);
		int missingPageNum = GETVPN(currSup->sup_exceptState[PGFAULTEXCEPT].entry_hi); //l'exception state della BIOSDATAPAGE è stato caricato qui in GeneralExceptionHandler
		int missingPageAddr = currSup->sup_exceptState[PGFAULTEXCEPT].entry_hi >> VPNSHIFT;
        
		int devStatus;
		int victimPgNum = getVictimPage();
		memaddr victimPgAddr = POOLSTART + victimPgNum*PAGESIZE; //indirizzo FISICO!

		//if it was occupied
		if (swapTable[victimPgNum].sw_asid != -1){
			
			//anche se siamo in mutex, abbiamo interrupt abilitati nel livello di supporto, pertanto in un multiproc sys potremmo essere interrotti (?)
			DISABLEINTERRUPTS;
			
			//invalidiamo associazione
			swapTable[victimPgNum].sw_pte->pte_entryLO &= ~VALIDON; //"puntatore alla entry corrispondente nella tabella delle pagine del processo" => lo vedrà anche l'altro processo!
			updateTLB(swapTable[victimPgNum].sw_pte,0);

			ENABLEINTERRUPTS;

			// block of the flash device to write into (coincides with the page number)
			
			int devBlockNum = (swapTable[victimPgNum].sw_pte->pte_entryHI >> VPNSHIFT) - PAGETBLSTART;
			int victimPgOwner = (swapTable[victimPgNum].sw_asid) - 1; //un flash device associato ad ogni ASID. 0 based

			//update old owner's process backing storage
			devStatus = flashCmd(FLASHWRITE, victimPgAddr, devBlockNum, victimPgOwner);
			if (devStatus != READY){
				killProc(&swapSem);
			}
			
		}
		
		//Read the contents of the currentProcess's backing storage/flash device logical page p into frame i
		devStatus = flashCmd(FLASHREAD, victimPgAddr, missingPageNum, currSup->sup_asid - 1);
		if (devStatus != READY){
			killProc(&swapSem);
		}
		
		//UPDATING SWAP POOL TABLE ENTRY TO REFLECT THE NEW CONTENTS
		swapTable[victimPgNum].sw_asid = currSup->sup_asid;
		swapTable[victimPgNum].sw_pageNo = missingPageAddr;
		swapTable[victimPgNum].sw_pte = &(currSup->sup_privatePgTbl[missingPageNum]);

        // Update the Current Process’s Page Table entry.
        currSup->sup_privatePgTbl[missingPageNum].pte_entryLO = victimPgAddr | VALIDON | DIRTYON;

		DISABLEINTERRUPTS;
		
		/*accende il V bit, il D bit e setta PNF*/
		swapTable[victimPgNum].sw_pte->pte_entryLO = victimPgAddr | VALIDON | DIRTYON;
		updateTLB(swapTable[victimPgNum].sw_pte,1);	

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
	setENTRYHI(currentProcess->p_supportStruct->sup_privatePgTbl[vpn].pte_entryHI);
    setENTRYLO(currentProcess->p_supportStruct->sup_privatePgTbl[vpn].pte_entryLO);
    //WRITING ON TLB
    TLBWR();        
    //LOADING PREVIOUS STATE. ripeterà l'istruzione e questa volta troverà l'entry
	LDST(currProc_s);
}
