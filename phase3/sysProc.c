//toDo: quando si controllano che gli indirizzi virtuali siano giusti, oltre a controllare che siano oltre il TLB floor address, controllare che siano nel range degli indirizzi virtuali di QUEL processo! già controllato con MAXSTRLENG?
#include "../pandos_types.h"
#include "../pandos_const.h"
#include <umps3/umps/const.h>
#include <umps3/umps/libumps.h>
#include <umps3/umps/cp0.h>
#include <umps3/umps/arch.h>
#include <umps3/umps/types.h>
#include "vmSupport.h" //eg killProc
#include "initProc.h" //eg devSem
#include "../phase2/init.h"

int my_strlen(char *str){
    char *s;
    for (s = str;*s != EOS; s++){
    }
    return (s - str);
}


#define PROC_TOP 0x8001E000 //le 31 pagine di ogni processo
/* Support level SYS calls */
#define GET_TOD			1
#define TERMINATE		2
#define WRITEPRINTER	        3
#define WRITETERMINAL 	        4
#define READTERMINAL	        5
#define EOS '\0'
signed int gettod(support_t *currSup){
    cpu_t tod;
    STCK(tod);
    currSup->sup_exceptState[GENERALEXCEPT].reg_v0 = tod;
    return currSup->sup_exceptState[GENERALEXCEPT].reg_v0;
}

void terminate(){
    killProc(NULL);
}

//controllare se l'utente passa indirizzi sbagliati o stringhe mal formattate!
int writeprinter(support_t *currSup){
    //indirizzo virtuale del primo char della str da trasmettere
    char *firstCharAddr = (char *) currSup->sup_exceptState[GENERALEXCEPT].reg_a1;
    int strLen = currSup->sup_exceptState[GENERALEXCEPT].reg_a2;
    if((int)firstCharAddr < KUSEG || strLen < 0 || strLen > MAXSTRLENG || (int)&(firstCharAddr[strLen-1]) >= PROC_TOP || my_strlen(firstCharAddr) != strLen){
        klog_print("\nerrore write printer\n");
        killProc(NULL);
        return -1;
    }

    int printerNum = currSup->sup_asid - 1;
    int printerSem = getDevSemIndex(PRNTINT, printerNum, 0);
    SYSCALL(PASSEREN, (int) &devSem[printerSem], 0, 0);

    //device registers
    dtpreg_t* devRegs = (dtpreg_t*) getDevRegAddr(PRNTINT, printerNum); //per specificare su quale dispositivo dobbiamo rilasciare i comandi

    int status;
    int i = 0;
    while ((i < strLen) && (i >= 0)){
        devRegs->data0 = *firstCharAddr;
        //PRINTCHR = TRANSMITCHAR = RECEIVECHAR IMP!
        status = SYSCALL(DOIO, (int) &((dtpreg_t *)devRegs)->command, TRANSMITCHAR, 0);
        if ((status & 0xFF)== READY){ //1-5 => 2 bit
            i++;
            firstCharAddr++;
        }
        else i = status*-1;
    }
    SYSCALL(VERHOGEN, (int) &devSem[printerSem], 0, 0);
    currSup->sup_exceptState[GENERALEXCEPT].reg_v0 = i; //ritorniamo numero di caratteri trasmessi
    return currSup->sup_exceptState[GENERALEXCEPT].reg_v0;
}
int writeterminal(support_t *currSup){
    //indirizzo virtuale del primo char della str da trasmettere
    char *firstCharAddr = (char *) currSup->sup_exceptState[GENERALEXCEPT].reg_a1;
    int strLen = currSup->sup_exceptState[GENERALEXCEPT].reg_a2;
    if((memaddr)firstCharAddr < KUSEG || strLen < 0 || strLen > MAXSTRLENG){
        klog_print("\nerrore write terminal\n");
        killProc(NULL);
        return -1;
    }
    int termNum = currSup->sup_asid - 1;
    int termSem = getDevSemIndex(TERMINT, termNum, 0);
    SYSCALL(PASSEREN, (int) &devSem[termSem], 0, 0);

    termreg_t* devRegs = (termreg_t*) getDevRegAddr(TERMINT, termNum);

    int status;
    int i = 0;
    while ((i < strLen) && (i >= 0)){
        status = SYSCALL(DOIO, (int) &devRegs->transm_command, ((unsigned int)*firstCharAddr << BYTELENGTH) | TRANSMITCHAR, 0);
        //OKCHARTRANS has same value of char received
        //PRINTCHR = TRANSMITCHAR = RECEIVECHAR IMP!

        if ((status & 0xFF)  == OKCHARTRANS){
            i++;
            firstCharAddr++;
        }
        else i = status*-1;
    }
    SYSCALL(VERHOGEN, (int) &devSem[termSem], 0, 0);
    currSup->sup_exceptState[GENERALEXCEPT].reg_v0 = i;
    return currSup->sup_exceptState[GENERALEXCEPT].reg_v0;
}

int readterminal(support_t *currSup){
    char *buf = (char *) currSup->sup_exceptState[GENERALEXCEPT].reg_a1;
    if((memaddr) buf < KUSEG){
        klog_print("fuori kuseg\n");
        killProc(NULL);
        return -1;
    }

    int termNum = currSup->sup_asid - 1;
    int termSem = getDevSemIndex(TERMINT, termNum, 1); //isReadTerm = 1
    termreg_t* devRegs = (termreg_t*) getDevRegAddr(TERMINT, termNum);
    int status = 0;
    int readChar = 0;
    char r = '^'; //init

    SYSCALL(PASSEREN, (int) &devSem[termSem], 0, 0);
    // No fixed string length: we terminate reading a newline character.
    while(r != '\n' && status >= 0){
        status = SYSCALL(DOIO, (int) &devRegs->recv_command, TRANSMITCHAR, 0);
        //OKCHARTRANS has same value of char received         //PRINTCHR = TRANSMITCHAR = RECEIVECHAR IMP!
        if ((status & 0xFF) == OKCHARTRANS){
            klog_print("status good\n");
            r = (status & 0xFF00) >> BYTELENGTH; //annulliamo anche i bit precedenti (da 16 a 31 (0-based))
            *buf = r;
            buf++;
            readChar++;
        }
        else{
            status =  ((status & 0xFF00) >> BYTELENGTH) * -1;
        }
    }
    SYSCALL(VERHOGEN, (int) &devSem[termSem], 0, 0);
    if (status >= 0) currSup->sup_exceptState[GENERALEXCEPT].reg_v0 = readChar;
    else             currSup->sup_exceptState[GENERALEXCEPT].reg_v0 = status;
    return currSup->sup_exceptState[GENERALEXCEPT].reg_v0;
}

void supGeneralExceptionHandler(){
    support_t *currSup = (support_t*) SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    int cause = CAUSE_GET_EXCCODE(currSup->sup_exceptState[GENERALEXCEPT].cause);
    if (cause == SYSEXCEPTION){
        switch(currSup->sup_exceptState[GENERALEXCEPT].reg_a0){
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
                klog_print("default\n");
                killProc(NULL);
                break;
        }
        currSup->sup_exceptState[GENERALEXCEPT].pc_epc += 4;
        currSup->sup_exceptState[GENERALEXCEPT].reg_t9 += WORDLEN; //me lo ero dimenticato...
        LDST(&(currSup->sup_exceptState[GENERALEXCEPT]));
    }
    else {
        klog_print("exccode!=8\n");
        killProc(NULL);
    }
}
