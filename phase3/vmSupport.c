/* perchè SOLO quando invalidiamo una entry della swap table ( swapTable[victimPgNum].sw_pte->pte_entryLO &= ~VALIDON;) oppure quando aggiorniamo la TLB (updateTLB) dobbiamo agire in mutex? */
/* Non dovremmo agire in mutex ogni volta che modifichiamo queste risorse condivise (eg quando facciamo: */
/*   //UPDATING SWAP POOL TABLE ENTRY TO REFLECT THE NEW CONTENTS */
/*   swapTable[victimPgNum].sw_asid = currSup->sup_asid; */
/*   swapTable[victimPgNum].sw_pageNo = invalidPageAddr; */
/*   swapTable[victimPgNum].sw_pte = &(currSup->sup_privatePgTbl[pageNum]); */
/* ) ? Grazie mille in anticipo */
/* quando dico "agire in mutex" mi riferisco agli important point del paragrafo 4.4.2 aggiornato della student guide */

/* In realta anche quello che hai indicato tu dovrebbe essere in mutua esclusione, mi rileggero' il manuale */

/* Alla fine abbiamo seguito la doc */
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

memaddr my_poolstart(){

#define CORE_TEXT_ADDR 0x20001014
#define CORE_DATA_ADDR 0x20001024 //.bss zeroed out
	unsigned int text_file_size = *(unsigned int *) CORE_TEXT_ADDR;
	unsigned int data_file_size = *(unsigned int *) CORE_DATA_ADDR;
	return  0x20001000 + text_file_size + data_file_size;
}

memaddr MY_POOLSTART;
int getDevSemIndex(int line, int devNo, int isReadTerm){
	return ((line - 3) * 8) + (line == 7 ? (isReadTerm * 8) + devNo : devNo);
}

swap_t swapTable[UPROCMAX*2]; //ci consente una panoramica aggiornata sulla swapPool. MEMORIA FISICA
int swapSem; //per accedere alla swapPool in mutex

void initSwap(){
	MY_POOLSTART = my_poolstart(); //messo qui poichè è la prima funzione che il main chiama
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

void updateTLB(pteEntry_t *sw_pte){
	setENTRYHI(sw_pte->pte_entryHI);
	TLBP();
	int index = getINDEX();
	int probe = (index >> 31); //prendo il bit probe del registro index (6.4 pops), se è 0 c'è un tlb hit
	if (!probe){
		setENTRYLO(sw_pte->pte_entryLO);
		TLBWI(); 	/*TLBP ha già aggiornato Index.TLB-Index */
	}
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
	if ((cause != TLBINVLDL) && (cause != TLBINVLDS)){
		//dobbiamo trattarlo come trap
		killProc(NULL);
	}
	else{
		//swap pool mutex
		SYSCALL(PASSEREN, (int) &swapSem, 0, 0);
		//ha associazione non valida
		int pageNum = GETVPN(currSup->sup_exceptState[PGFAULTEXCEPT].entry_hi); //l'exception state della BIOSDATAPAGE è stato caricato qui in GeneralExceptionHandler DA NOI student guide phase 2 3.7
		int invalidPageAddr = currSup->sup_exceptState[PGFAULTEXCEPT].entry_hi >> VPNSHIFT; //missing /TLBRefill vs invalid /TLBInvalid => dobbiamo inserirlo

		int devStatus;
		int victimPgNum = getVictimPage();
		memaddr victimPgAddr = MY_POOLSTART + victimPgNum*PAGESIZE; //indirizzo FISICO!

		//if it was occupied - going to replace its content
		if (swapTable[victimPgNum].sw_asid != -1){

			//anche se siamo in mutex, abbiamo interrupt abilitati nel livello di supporto, pertanto in un multiproc sys potremmo essere interrotti
			//"Since the Support Level runs in a fully concurrent mode (interrupts unmasked), each process needs its own location(s) for their saved exception states, and addresses to pass control to: The Support STRUCT"
			DISABLEINTERRUPTS;

			//invalidiamo associazione
			swapTable[victimPgNum].sw_pte->pte_entryLO &= ~VALIDON; //"PUNTATORE alla entry corrispondente nella tabella delle pagine del processo" => lo vedrà anche il processo!
			updateTLB(swapTable[victimPgNum].sw_pte); //aggiorniamo

			ENABLEINTERRUPTS;

			//Funzionano a blocchi di 4KB come tutto il resto
			int devBlockNum = swapTable[victimPgNum].sw_pageNo;
			int victimPgOwner = (swapTable[victimPgNum].sw_asid) - 1; //un flash device associato ad ogni ASID. 0 based

			if (swapTable[victimPgNum].sw_pte->pte_entryLO & DIRTYON){
				//update old owner's process backing storage. da RAM a flash dev. victimPgOwner != currSup->sup_asid -1 ! (togliamo il frame di un altro processo)
				devStatus = flashCmd(FLASHWRITE, victimPgAddr, devBlockNum, victimPgOwner);
				if (devStatus != READY){
					klog_print("pager devstatus!=READY(1)\n");
					killProc(&swapSem);
				}

			}
		}

		//blocco che dobbiamo/vogliamo inserire in ram
		devStatus = flashCmd(FLASHREAD, victimPgAddr, pageNum, currSup->sup_asid - 1);
		if (devStatus != READY){
			klog_print("pager devstatus!=READY(2)\n");
			killProc(&swapSem);
		}

		//UPDATING SWAP POOL TABLE ENTRY TO REFLECT THE NEW CONTENTS
		swapTable[victimPgNum].sw_asid = currSup->sup_asid;
		swapTable[victimPgNum].sw_pageNo = invalidPageAddr; //entryHI
		swapTable[victimPgNum].sw_pte = &(currSup->sup_privatePgTbl[pageNum]);


		DISABLEINTERRUPTS; //come indicato da documentazione e perchè risorse condivise critiche

		/*accende il V bit, il D bit e setta PNF*/
		// Update the Current Process’s Page Table entry as well
		currSup->sup_privatePgTbl[pageNum].pte_entryLO = victimPgAddr | VALIDON | DIRTYON;
		updateTLB(swapTable[victimPgNum].sw_pte);
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
	//recupero della entry mancante
	int vpn = GETVPN(currProc_s->entry_hi);
	//locate the correct Page Table entry and write into regs in order to perform TLBWR()
	setENTRYHI(currentProcess->p_supportStruct->sup_privatePgTbl[vpn].pte_entryHI);
	setENTRYLO(currentProcess->p_supportStruct->sup_privatePgTbl[vpn].pte_entryLO);
	//WRITING ON TLB
	TLBWR();
	//LOADING PREVIOUS STATE. ripeterà l'istruzione e questa volta troverà l'entry
	LDST(currProc_s);
}
