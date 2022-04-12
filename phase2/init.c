#include "../pandos_const.h" 
#include "../pandos_types.h"
#include "pcb.h"
#include "asl.h"
#include "listx.h"
#include "scheduler.h"
#include <umps3/umps/libumps.h>

#define NoDEVICE 49 //numero di device
#define TRUE 1
#define FALSE 0

#define KLOG_LINES     64     // Number of lines in the buffer. Adjustable, only limited by available memory
#define KLOG_LINE_SIZE 42     // Length of a single line in characters


static void next_line(void);
static void next_char(void);


unsigned int klog_line_index                         = 0;       // Index of the next line to fill
unsigned int klog_char_index                         = 0;       // Index of the current character in the line
char         klog_buffer[KLOG_LINES][KLOG_LINE_SIZE] = {0};     // Actual buffer, to be traced in uMPS3


// Print str to klog
void klog_print(char *str) {
    while (*str != '\0') {
        // If there is a newline skip to the next one
        if (*str == '\n') {
            next_line();
            str++;
        } 
        // Otherwise just fill the current one
        else {
            klog_buffer[klog_line_index][klog_char_index] = *str++;
            next_char();
        }
    }
}


// Princ a number in hexadecimal format (best for addresses)
void klog_print_hex(unsigned int num) {
    const char digits[] = "0123456789ABCDEF";

    do {
        klog_buffer[klog_line_index][klog_char_index] = digits[num % 16];
        num /= 16;
        next_char();
    } while (num > 0);
}


// Move onto the next character (and into the next line if the current one overflows)
static void next_char(void) {
    if (++klog_char_index >= KLOG_LINE_SIZE) {
        klog_char_index = 0;
        next_line();
    }
}


// Skip to next line
static void next_line(void) {
    klog_line_index = (klog_line_index + 1) % KLOG_LINES;
    klog_char_index = 0;
    // Clean out the rest of the line for aesthetic purposes
    for (unsigned int i = 0; i < KLOG_LINE_SIZE; i++) {
        klog_buffer[klog_line_index][i] = ' ';
    }
}

//dichiarazione delle  variabili globali
//come politica dei pid è stato scelto un contatore
int pidCounter;
/* Number of started, but not yet terminated processes. */
int processCount;
/* Number of started, but not terminated processes that in are the
“blocked” state  due to an I/O or timer request */
int softBlockCount;



/* Tail pointer to a queue of pcbs (related to high priority processes) 
that are in the “ready” state. */
//ricordarsi che quando un processo ad alta priorità esegue una yield(), bisogna
//cercare di far eseguire gli altri, anche se è il primo della high priority q
struct list_head HighPriorityReadyQueue;

/* Tail pointer to a queue of pcbs (related to low priority processes) 
that are in the “ready” state. */
struct list_head LowPriorityReadyQueue;

pcb_PTR lastProcessHasYielded = NULL; //puntatore al pcb del processo associato.

//NULL = unsigned int 0 il quale indirizzo non potrà/dovrà esistere
//quando un processo rilascia, ricordarsi di inserire il puntatore al pcb corrispondente dentro lastProcessHasYielded

/* Pointer to the current pcb that is in running state */
//the current (and only) executing process 
pcb_PTR currentProcess;
//Array for device semaphores
int deviceSemaphores[NoDEVICE];
//extern => declare a var in one object file but then use it in another one
//variable could be defined somewhere else => specifichiamo che provengono dall'esterno


//funzioni che prendo dall'esterno

extern void test();
extern void uTLB_RefillHandler();
extern void GeneralExceptionHandler();
extern void print(char* msg);

int main() {
	passupvector_t *passUpVector = (passupvector_t *) PASSUPVECTOR;
	//popolare gestore eccezioni. popolare = inserire PC e SP adeguati nei registri
	passUpVector->tlb_refill_handler = (memaddr) uTLB_RefillHandler; /*in Memory related constants */
	passUpVector->tlb_refill_stackPtr = (memaddr) KERNELSTACK;
	passUpVector->exception_stackPtr = (memaddr) KERNELSTACK;
	passUpVector->exception_handler = (memaddr) GeneralExceptionHandler;

	softBlockCount = 0;
	processCount = 0;
	currentProcess = NULL;

	mkEmptyProcQ(&LowPriorityReadyQueue);
	mkEmptyProcQ(&HighPriorityReadyQueue);

	initPcbs();
	initASL();

	for (int i = 0; i < NoDEVICE; i++) deviceSemaphores[i] = 0;

	//load interval timer globale. (Interval) timer è un vero e proprio dispositivo fisico che fa svolgere al kernel il context switch
	LDIT(PSECOND); //100000

	pcb_PTR initProc = allocPcb(); 
	
	initProc->p_supportStruct = NULL; //slide pg 24/48 => dobbiamo indicare un suo gestore? Chi?
	initProc->p_prio = 0; //poichè viene inserito in una coda a bassa priorità
	
	pidCounter=1; 
	initProc->p_pid = pidCounter; 
	pidCounter++;
	processCount+=1;

	/*init first process state */
	state_t initState;
	STST(&initState); //salvare stato processore in una struttura pcb_t

	RAMTOP(initState.reg_sp); //SP a ramtop

    initState.pc_epc = (memaddr) test; /*(exec) PC all'indirizzo della funzione test di p2test */
    initState.reg_t9 = (memaddr) test; //idiosincrasia dell'architettura

    /*process needs to have interrupts enabled, the processor Local Timer enabled, kernel-mode on => Status register constants */
    initState.status = IEPON | IMON | TEBITON;  //messo in fondo

    initProc->p_s = initState;

    insertProcQ(&LowPriorityReadyQueue, initProc);
	scheduler();

	return 0;
}

void memcpy(void *dest, const void *src, int n){
    for (int i = 0; i < n; i++) {
    	((char *)dest)[i] = ((char *)src)[i];
    }
}