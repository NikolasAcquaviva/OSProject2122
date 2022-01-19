/*
//semaphore descriptor (SEMD) data structure
typedef struct semd_t {
    
    // Semaphore key
    int *s_key;
    
    // Queue of PCBs blocked on the semaphore
    struct list_head s_procq;

    // Semaphore list
    struct list_head s_link;

} semd_t, *semd_PTR;

// process table entry type
typedef struct pcb_t {
    
    // process queue 
    struct list_head p_list;

    // process tree fields
    struct pcb_t    *p_parent; // ptr to parent
    struct list_head p_child;  // children list
    struct list_head p_sib;    // sibling list

    // process status information
    state_t p_s;    // processor state
    cpu_t   p_time; // cpu time used by proc

    // Pointer to the semaphore the process is currently blocked on
    int *p_semAdd;

} pcb_t, *pcb_PTR;
*/


#include "../h/pandos_const.h" //accordarsi per stabilire una volta per tutte che tutti gli headers stanno in /h non ../h
#include "../h/pandos_types.h"
#include "../h/listx.h"
#include "./pcb.h"
#include "./asl.h"

#define TRUE 1
#define FALSE 0
#define MAX_PROC 20
#define HIDDEN static //static globale => visibile solo nel file in cui compare la dichiarazione => è nascosta (hidden) altrove

static int *MAXINT = (int *)0x7FFFFFFF;//2^31 -1. Univoco addr. di memoria del valore del sema4 è identificativo di quest'ultimo 
static int *MININT = (int *)0x00000000;

HIDDEN semd_t semd_table[MAX_PROC + 2]; //Tabella Semafori forniti
HIDDEN semd_PTR semdFree_h; //Lista dei SEMD liberi o inutilizzati.
HIDDEN semd_PTR semd_h; //Lista dei semafori attivi. Questa lista ha i dummy nodes

/*
Viene inserito il PCB puntato da p nella coda dei
processi bloccati associata al SEMD con chiave
semAdd. Se il semaforo corrispondente non è
presente nella ASL, alloca un nuovo SEMD dalla
lista di quelli liberi (semdFree) e lo inserisce nella
ASL, settando I campi in maniera opportuna (i.e.
key e s_procQ). Se non è possibile allocare un
nuovo SEMD perché la lista di quelli liberi è vuota,
restituisce TRUE. In tutti gli altri casi, restituisce
FALSE.
*/
int insertBlocked(int *semAdd, pcb_t *p){

	//https://github.com/agente-drif/ProgettoSistemiOperativi/blob/master/src/phase1/asl.c
	if (semd_h == NULL){ //non c'è nessun semaforo attivo/utilizzato => sicuramente non ci sarà semAdd

	}
	semd_PTR index = semd_h; //copia puntatore temporaneo per scorrere. Si parte da MININT
	//creato per comodità per l'elif. Se semd_h ha solo un semaforo impegnato => questo sarà MAXINT il quale è l'ultimo
	semd_PTR nextIndex = container_of(index>s_link->next, "semd_t", "s_link"); //chiedere se va passata la stringa o il tipo

	while(index->s_key != MAXINT){

		if(index->s_key == semAdd){ //risorsa/semaforo impegnato già precedentemente

			p->p_semAdd = semAdd;   /* ptr to semaphore on which proc is blocked */
            insertProcQ(&(index->s_procq), p); //insertProcQ(struct list_head* head, pcb* p)
            return FALSE;
        }
        //altrimenti se soddisfatte condizioni => inserimento rispettando ordine non decrescente
        else if (nextIndex->s_semAdd > semAdd || nextIndex->s_semAdd == MAXINT){

        	//nuovo semaforo da allocare
        	if (semdFree_h == NULL) return TRUE;
        	
        	semd_PTR semToAdd = semdFree_h;
            semdFree_h = container_of(semdFree_h>s_link->next, "semd_t", "s_link");

            semToAdd->s_procq = mkEmptyProcQ();
            semToAdd->s_semAdd = semAdd; //proper init

            semToAdd->s_link->next = &(index->s_link->next); //ciò che puntava prima index
            semToAdd->s_link->prev = &(index->s_link); //oppure &(nextIndex->s_link->prev)

            index->s_link->next = &(semToAdd->s_link); //aggiusto nuovi nodi contigui
            nextIndex->s_link->prev = &(semToAdd->s_link);


            p->p_semAdd = semAdd; // Pointer to the semaphore the process is currently blocked on

            insertProcQ(&(semToAdd->s_procq), p);

            return FALSE;
        }
        //continuo a scorrere
        index = container_of(index>s_link->next, "semd_t", "s_link");;
	}
	return FALSE; //pedante, non lo raggiungerà mai
}

void initASL(){

	//inizializzazione di QUEGLI elementi
	for (int i = 0; i < MAX_PROC + 2; i++){ //https://github.com/leti15/project_pandOS/blob/main/phase1/asl.c
		semd_table[i].s_key = NULL; //NULL == 0? puntatore
		semd_table[i].s_link = NULL;
		semd_table[i].s_procq = NULL; //dalla documentazione sembra che debba mettere mkEmptyProcQ() (pg 23 pdf)
	}

	//Nodi dummy per lista dei semafori attivi (=> utilizzati). Lista circolare
	semd_h = &semd_table[0];
	semd_h->s_key = MININT;
	semd_h->s_procq = NULL; //dalla documentazione sembra che debba mettere mkEmptyProcQ() (pg 23 pdf)
	semd_h->s_link->next = &(semd_table[MAX_PROC+1].s_link);
	semd_h->s_link->prev = NULL; //NULL == 0? puntatore

	semd_PTR MAXINTDummyNode = &semd_table[MAX_PROC + 1];//container_of(semd_h->s_link->next, "semd_t", "s_link"); //puntatore alla struttura che contiene il prossimo connettore
	MAXINTDummyNode->s_key = MAXINT;
	MAXINTDummyNode->s_procq = NULL; //dalla documentazione sembra che debba mettere mkEmptyProcQ() (pg 23 pdf)
	MAXINTDummyNode->s_link->next = NULL; //NULL == 0? puntatore
	MAXINTDummyNode->s_link->prev = &(semd_table[0].s_link);

	semdFree_h = &semd_table[1]; //sentinella semafori non attivi.

	//Collego QUEI semafori tra loro tramite i connettori. Lista circolare (evitando di usare l'operatore modulo per costruirla)
	semd_table[1].s_link->next = &(semd_table[2].s_link);
	semd_table[1].s_link->prev = &(semd_table[MAX_PROC].s_link);
	for (int i = 2; i < MAX_PROC; i++){
		semd_table[i].s_link->next = &(semd_table[i+1].s_link);
		semd_table[i].s_link->prev = &(semd_table[i-1].s_link);
	}
	semd_table[MAX_PROC].s_link->next = &(semd_table[1].s_link);
	semd_table[MAX_PROC].s_link->prev = &(semd_table[MAX_PROC-1].s_link);

}
pcb_t* removeBlocked(int *semAdd){}
pcb_t* outBlocked(pcb_t *p){}
pcb_t* headBlocked(int *semAdd){}