//toDo: quando si controllano che gli indirizzi virtuali siano giusti, oltre a controllare che siano oltre il TLB floor address, controllare che siano nel range degli indirizzi virtuali di QUEL processo!
#include "../pandos_types.h"
#include "../pandos_const.h"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>
#include <umps3/umps/cp0.h>
#include <umps3/umps/arch.h>
#include <umps3/umps/types.h>
#include "vmSupport.h" //eg killProc
#include "initProc.h" //eg devSem
//esportate le funzioni getDevSemIndex, getDevRegAddr e devSem
#include "../phase2/init.h"


static int stranezza;

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
        klog_print("\nerrore write printer\n");
        killProc(NULL);
        return;
    }

    int printerNum = currSup->sup_asid - 1;
    int printerSem = getDevSemIndex(PRNTINT, printerNum, 0);
    devreg_t* devRegs = (devreg_t*) getDevRegAddr(PRNTINT, printerNum); 
    //void *devRegs = (void *)DEV_REG_ADDR(PRNTINT, printerNum);
    SYSCALL(PASSEREN, (int) &devSem[printerSem], 0, 0);

    int status;
    int i = 0;
    while ((i < strLen) && (i >= 0)){
        ((dtpreg_t *)devRegs)->data0 = *firstCharAddr;
        /* devRegs->dtp.data0 = *firstCharAddr; */
        //PRINTCHR = TRANSMITCHAR = RECEIVECHAR
        status = SYSCALL(DOIO, (int) &((dtpreg_t *)devRegs)->command, TRANSMITCHAR, 0);
        if ((status & 0x000000FF)== READY){ //TODO da chiarire! c'è anche da dire che sulla doc non c'è scritto quanti bit occupa lo status delle stampanti...
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
    if((memaddr)firstCharAddr < KUSEG || strLen < 0 || strLen > MAXSTRLENG){
        klog_print("\nerrore write terminal\n");
        killProc(NULL); //i ragazzi fanno direttamente una TERMINATE del current process
        return;
    }
    int termNum = currSup->sup_asid - 1;
    int termSem = getDevSemIndex(TERMINT, termNum, 0);
    devreg_t* devRegs = (devreg_t*) getDevRegAddr(TERMINT, termNum); 
    //void *devRegs = (void *)DEV_REG_ADDR(TERMINT, termNum);
    SYSCALL(PASSEREN, (int) &devSem[termSem], 0, 0);

    int status;
    int i = 0;
    while ((i < strLen) && (i >= 0)){
        status = SYSCALL(DOIO, (int) &((termreg_t *)devRegs)->transm_command, (unsigned int)*firstCharAddr << BYTELENGTH | TRANSMITCHAR, 0);
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
    if((memaddr)buf < KUSEG){
        klog_print("\nerrore read terminal cazzo\n");
        killProc(NULL);
        return;
    }
    int termNum = currSup->sup_asid - 1;
    //TODO: valutare idea di fare 3 array di semafori, ognuno lungo UPROCMAX
    int termSem = getDevSemIndex(TERMINT, termNum, 1);
    //termreg_t* devRegs = (termreg_t*) DEV_REG_ADDR(TERMINT, termNum);
    termreg_t* devRegs = (termreg_t*) getDevRegAddr(TERMINT, termNum); 
    SYSCALL(PASSEREN, (int) &devSem[termSem], 0, 0);

    int status;
    int readChar = 0; 
    // No fixed string length: we terminate reading a newline character.
    for (char r = EOS; r != '\n';){
        status = SYSCALL(DOIO, (int) &devRegs->recv_command, TRANSMITCHAR, 0);
        //OKCHARTRANS has same value of char received
        if ((status & 0x000000FF)  == OKCHARTRANS){ //TODO: capire perchè solo una f
            r = (status & 0x0000FF00) >> BYTELENGTH;
            *(buf + readChar++) = r;  

            //ci va EOS?
            /* if (*buf == EOS) break; */
            /* if (((status & 0x0000FF00) >> BYTELENGTH == '\n') || ((status & 0x0000FF00) >> BYTELENGTH == EOS)) break; */
        }
        else{
            klog_print("\nerrore read terminal cazzo2\n");
            killProc(&devSem[termSem]);
            currSup->sup_exceptState[GENERALEXCEPT].reg_v0 =  (status & 0x0000000F) * -1;
            return;
        }
    }
    SYSCALL(VERHOGEN, (int) &devSem[termSem], 0, 0);
    // We add a EOS terminating character after the newline character: "*\n\0" TODO: PERCHE'? Controllare se si verifica una sorta di ciclo altrimenti
    *(buf + readChar) = EOS;
    currSup->sup_exceptState[GENERALEXCEPT].reg_v0 = readChar;
}
void supGeneralExceptionHandler(){
	support_t *currSup = (support_t*) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
	/* int cause = ((currSup->sup_exceptState[GENERALEXCEPT].cause) & GETEXECCODE) >> CAUSESHIFT; */
    int cause = CAUSE_GET_EXCCODE(currSup->sup_exceptState[GENERALEXCEPT].cause);

    stranezza = cause;

	if (cause == SYSEXCEPTION){
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
                klog_print("errore strano 1");
				killProc(NULL);
		}
        currSup->sup_exceptState[GENERALEXCEPT].pc_epc += 4;
        currSup->sup_exceptState[GENERALEXCEPT].reg_t9 += WORDLEN; //me lo ero dimenticato...
        LDST(&(currSup->sup_exceptState[GENERALEXCEPT]));
    }else if (cause == 6){
        klog_print("\nentrato boh\n");
        readterminal(currSup);
        currSup->sup_exceptState[GENERALEXCEPT].pc_epc += 4;
        currSup->sup_exceptState[GENERALEXCEPT].reg_t9 += WORDLEN; //me lo ero dimenticato...
        LDST(&(currSup->sup_exceptState[GENERALEXCEPT]));
        
    }
	else if (currSup->sup_exceptState[GENERALEXCEPT].reg_a0 == READTERMINAL){ 
        klog_print("\nENTRATA STRANA\n");
        readterminal(currSup);
        currSup->sup_exceptState[GENERALEXCEPT].pc_epc += 4;
        currSup->sup_exceptState[GENERALEXCEPT].reg_t9 += WORDLEN; //me lo ero dimenticato...
        LDST(&(currSup->sup_exceptState[GENERALEXCEPT]));
    }
	else {
        klog_print_hex(currSup->sup_exceptState[GENERALEXCEPT].reg_a0);
        klog_print("errore strano 2");
        killProc(NULL);
        //gli altri ragazzi hanno semplicemente terminato, credo sia sbagliata la loro scelta
        /* SYSCALL(TERMINATE, 0, 0, 0); */
    }
    /* currSup->sup_exceptState[GENERALEXCEPT].pc_epc += WORDLEN; */
    /* currSup->sup_exceptState[GENERALEXCEPT].reg_t9 += WORDLEN; */
    /* LDST(&(currSup->sup_exceptState[GENERALEXCEPT])); */

}
