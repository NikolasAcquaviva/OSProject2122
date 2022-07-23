//toDo: quando si controllano che gli indirizzi virtuali siano giusti, oltre a controllare che siano oltre il TLB floor address, controllare che siano nel range degli indirizzi virtuali di QUEL processo!
#include "../pandos_types.h"
#include "../pandos_const.h"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>
#include <umps3/umps/cp0.h>
#include <umps3/umps/arch.h>
#include <umps3/umps/types.h>
#include "vmSupport.h"
#include "initProc.h"
//esportate le funzioni getDevSemIndex, getDevRegAddr e devSem



/* Support level SYS calls */
#define GET_TOD			1
#define TERMINATE		2
#define WRITEPRINTER	        3
#define WRITETERMINAL 	        4
#define READTERMINAL	        5
#define EOS '\0'



void gettod(support_t *currSup){
    cpu_t tod;
    STCK(tod);
    currSup->sup_exceptState[GENERALEXCEPT].reg_v0 = tod;
}

void terminate(){
    killProc(NULL);
}

//controllare se l'utente passa indirizzi sbagliati o stringhe mal formattate!
void writeprinter(support_t *currSup){
    //indirizzo virtuale del primo char della str da trasmettere
    char *firstCharAddr = (char *) currSup->sup_exceptState[GENERALEXCEPT].reg_a1;
    int strLen = currSup->sup_exceptState[GENERALEXCEPT].reg_a2;
    if((int)firstCharAddr < KUSEG || strLen < 0 || strLen > MAXSTRLENG){
        killProc(NULL);
        return;
    }

    int printerNum = currSup->sup_asid - 1;
    int printerSem = getDevSemIndex(PRNTINT, printerNum, 0);
    devreg_t* devRegs = (devreg_t*) getDevRegAddr(PRNTINT, printerNum);

    SYSCALL(PASSEREN, (int) &devSem[printerSem], 0, 0);

    int status;
    int i = 0;
    while ((i < strLen) && (i >= 0)){
        devRegs->dtp.data0 = *firstCharAddr;
        //PRINTCHR = TRANSMITCHAR = RECEIVECHAR
        SYSCALL(DOIO, (int) &devRegs->dtp.command, TRANSMITCHAR, 0);
        status = devRegs->dtp.status;
        if ((status & 0x000000FF)== READY){
            i++;
            firstCharAddr++;
        }
        else i = status*-1;
    }
    SYSCALL(VERHOGEN, (int) &devSem[printerSem], 0, 0);
    currSup->sup_exceptState[GENERALEXCEPT].reg_v0 = i;
}
void writeterminal(support_t *currSup){
    //indirizzo virtuale del primo char della str da trasmettere
    char *firstCharAddr = (char *) currSup->sup_exceptState[GENERALEXCEPT].reg_a1;
    int strLen = currSup->sup_exceptState[GENERALEXCEPT].reg_a2;
    if((int)firstCharAddr < KUSEG || strLen < 0 || strLen > MAXSTRLENG){
        killProc(NULL);
        return;
    }
    int termNum = currSup->sup_asid - 1;
    int termSem = getDevSemIndex(TERMINT, termNum, 0);
    devreg_t* devRegs = (devreg_t*) getDevRegAddr(TERMINT, termNum);
    SYSCALL(PASSEREN, (int) &devSem[termSem], 0, 0);

    int status;
    int i = 0;
    while ((i < strLen) && (i >= 0)){
        SYSCALL(DOIO, (int) &devRegs->term.transm_command, *firstCharAddr << BYTELENGTH | TRANSMITCHAR, 0);
        status = devRegs->dtp.status;
        //OKCHARTRANS has same value of char received
        if ((status & 0x000000FF)  == OKCHARTRANS){
            i++;
            firstCharAddr++;
        }
        else i = status*-1;
    }
    SYSCALL(VERHOGEN, (int) &devSem[termSem], 0, 0);
    currSup->sup_exceptState[GENERALEXCEPT].reg_v0 = i;
}
void readterminal(support_t *currSup){
    //toEDIT
    char *buf = (char *) currSup->sup_exceptState[GENERALEXCEPT].reg_a1;
    if((int)buf < KUSEG){
        killProc(NULL);
        return;
    }
    int termNum = currSup->sup_asid - 1;
    int termSem = getDevSemIndex(TERMINT, termNum, 1);
    devreg_t* devRegs = (devreg_t*) getDevRegAddr(TERMINT, termNum);
    SYSCALL(PASSEREN, (int) &devSem[termSem], 0, 0);

    int status;
    int i = 0;
    while (TRUE){
        SYSCALL(DOIO, (int) &devRegs->term.recv_command, TRANSMITCHAR, 0);
        status = devRegs->dtp.status;
        //OKCHARTRANS has same value of char received
        if ((status & 0x000000FF)  == OKCHARTRANS){
            i++;
            *buf = (status & 0x0000FF00) >> BYTELENGTH;
            buf++;
            //ci va EOS?
            if ((status & 0x0000FF00) >> BYTELENGTH == '\n') break;
        }
        else{
            i = status*-1;
            break;
        }
    }
    SYSCALL(VERHOGEN, (int) &devSem[termSem], 0, 0);
    currSup->sup_exceptState[GENERALEXCEPT].reg_v0 = i;
}

void supGeneralExceptionHandler(){

	support_t *currSup = (support_t*) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
	int cause = ((currSup->sup_exceptState[GENERALEXCEPT].cause) & GETEXECCODE) >> CAUSESHIFT;

	if (cause == SYSEXCEPTION){
		currSup->sup_exceptState[GENERALEXCEPT].pc_epc += 4;
		switch(currSup->sup_exceptState[GENERALEXCEPT].reg_a0)
		{
			case GET_TOD:
                gettod(currSup);
                break;
            case TERMINATE:
                terminate();
                break;
            case WRITEPRINTER:
                writeprinter(currSup);
                break;
            case WRITETERMINAL:
                writeterminal(currSup);
                break;
            case READTERMINAL:
                readterminal(currSup);
                break;
			default:
				killProc(NULL);
		}
        LDST(&(currSup->sup_exceptState[GENERALEXCEPT]));
	}
	else killProc(NULL);

}
