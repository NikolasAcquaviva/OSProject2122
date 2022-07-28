//cercare "DA CONTROLLARE"
#include "initProc.h"
#include <umps3/umps/libumps.h>
#include <umps3/umps/cp0.h>
#include <umps3/umps/arch.h>
#include <umps3/umps/types.h>
#include "../pandos_const.h"
#include "../pandos_types.h"
#include "../phase2/init.h"

extern pcb_PTR currentProcess;

//#include "../phase2/interrupthandler.h"
//AGGIUNGERE LE SEGUENTI DUE FUNZIONI A interrupthandler.c
memaddr *getDevRegAddr(int line, int devNo){
	return (memaddr *) (0x10000054 + ((line - 3) * 0x80) + (devNo * 0x10));
}

int getDevSemIndex(int line, int devNo, int isReadTerm){
	return ((line - 3) * 8) + (line == 7 ? (isReadTerm * 8) + devNo : devNo);
}

//Master sempahore which wait for all processes to be concluded in order to terminate testing
/* extern int masterSem; //da initProc.c */

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

void updateTLB(){
	TLBCLR();
}



//flashCmd(FLASHWRITE, victimPgAddr, devBlockNum, victimPgOwner); //scrivi il contenuto di victimPgAddr dentro blocco devBlockNum del dispositivo flash victimPgOwner
int flashCmd(int cmd, int block, int devBlockNum, int flashDevNum){
	//((line - 3) * 8) + (line == 7 ? (isRead * 8) + dev : dev);
	int semNo = getDevSemIndex(FLASHINT, flashDevNum, FALSE);		//(FLASHINT - 3)*8 + flashDevNum;
	//prende mutex sul device register associato al flash device
	SYSCALL(PASSEREN, (int) &devSem[semNo], 0, 0);
	dtpreg_t *flashDevReg = (dtpreg_t *)DEV_REG_ADDR(FLASHINT, flashDevNum);	//(memaddr*) (0x10000054 + ((FLASHINT - 3) * 0x80) + (flashDevNum * 0x10));
	
	/*carica data0 con il blocco da leggere o scrivere*/
	/* flashDevReg->dtp.data0 =  (memaddr)block; */
    flashDevReg->data0 = (memaddr) block; //dest se FLASHREAD, src se FLASHWRITE

	// inserting the command after writing into data
	unsigned int value;
    //fare if/else se cmd = FLASHREAD? figura 5.12 pops
    if (cmd == FLASHWRITE) value = (devBlockNum << 8) | cmd;
    else if (cmd == FLASHREAD) value = cmd;
    /* value = cmd | devBlockNum << 8; */

	//int devStatus = SYSCALL(DOIO, FLASHINT, flashDevNum, 0);
	int devStatus = SYSCALL(DOIO, (int) &flashDevReg->command, value, 0);
	SYSCALL(VERHOGEN, (int) &devSem[semNo], 0, 0);
	
	if (devStatus != READY) return -1;
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
			setENTRYHI(swapTable[victimPgNum].sw_pte->pte_entryHI);
			setENTRYLO(swapTable[victimPgNum].sw_pte->pte_entryLO);
			TLBP(); //in registro index abbiamo primo bit che indica se esiste in cache la entry data dai registri cp0 entryHI/LO
			int index = getINDEX(); //nei bit da 8 a 15 abbiamo l'index del tlb nel quale scrivere i registri hi/lo tramite TLBWI()
			int probe = (index >> 31); //prendo il bit probe del registro index (6.4 pops), se è 0 c'è un tlb hit
			if(probe == 0)
				TLBWI();
			//updateTLB();	
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
		//un errore risolto: qui nella riga sotto non veniva fatto lo shift del frame con evidenti conseguenze. Inoltre qui viene acceso anche il bit dirty, ma sulla guida non mi sembra che lo richieda. In ogni caso allo stato attuale, che venga acceso o meno il risultato non cambia.
		swapTable[victimPgNum].sw_pte->pte_entryLO = victimPgAddr  | VALIDON | DIRTYON;
		setENTRYHI(swapTable[victimPgNum].sw_pte->pte_entryHI);
		setENTRYLO(swapTable[victimPgNum].sw_pte->pte_entryLO);
		//di nuovo probe, se esiste riscrivo il valore aggiornato nello stesso indice, altrimenti utilizzo un indice random per inserirlo (TLBWR)
		TLBP();
		int index = getINDEX(); 
		int probe = (index >> 31); 
		if(probe == 0)
			TLBWI();
		else
			TLBWR();
		//la differenza è che tlbwi scrive i registri cp0 HI/LO nella entry con indice index(nel registro index, popolato opportunamente da TLBP, invece se TLBP risulta in TLB miss, l'indice sul quale scrivere è generato pseudorandomicamente)
		//updateTLB();	

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
