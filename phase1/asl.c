#include "../h/pandos_const.h"
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

/*Se non è possibile allocare un
nuovo SEMD perché la lista di quelli liberi è vuota,
restituisce TRUE. In tutti gli altri casi, restituisce FALSE*/
int insertBlocked(int *semAdd, pcb_t *p){

	//valutare idea Drif dove non c'è nessun semaforo attivo/utilizzato => sicuramente non ci sarà semAdd
	//https://github.com/agente-drif/ProgettoSistemiOperativi/blob/master/src/phase1/asl.c
	if (semdFree_h == NULL) return TRUE;
	return FALSE;


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