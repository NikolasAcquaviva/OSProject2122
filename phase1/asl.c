#include "../h/pandos_const.h"
#include "../h/pandos_types.h"
#include "../h/listx.h"
#include "./pcb.h"
#include "./asl.h"


#define MAX_PROC 20
#define HIDDEN static //static globale => visibile solo nel file in cui compare la dichiarazione => è nascosta (hidden) altrove

static int *MAXINT = (int *)0x7FFFFFFF;//2^31 -1. Univoco addr. di memoria del valore del sema4 è identificativo di quest'ultimo 
static int *MININT = (int *)0x00000000;

HIDDEN semd_t semd_table[MAX_PROC + 2]; //Tabella Semafori forniti
HIDDEN semd_PTR semdFree_h; //Lista dei SEMD liberi o inutilizzati.
HIDDEN semd_PTR semd_h; //Lista dei semafori attivi. Questa lista ha i dummy noded

void initASL(){

	//inizializzazione di QUEGLI elementi
	for (int i = 0; i < MAX_PROC + 2; i++){ //https://github.com/leti15/project_pandOS/blob/main/phase1/asl.c
		semd_table[i].s_key = NULL;
		semd_table[i].s_link = NULL;
		semd_table[i].s_procq = NULL;
	}

	//Nodi dummy per lista dei semafori attivi (=> utilizzati). Lista circolare
	semd_h = &semd_table[0];
	semd_h->s_key = MININT;
	semd_h->s_procq = NULL;
	semd_h->s_link->next = &(semd_table[MAX_PROC+1].s_link);
	semd_h->s_link->prev = NULL;

	semd_PTR MAXINTDummyNode = &semd_table[MAX_PROC + 1]//container_of(semd_h->s_link->next, "semd_t", "s_link"); //puntatore alla struttura che contiene il prossimo connettore
	MAXINTDummyNode->s_key = MAXINT;
	MAXINTDummyNode->s_procq = NULL;
	MAXINTDummyNode->s_link->next = NULL;
	MAXINTDummyNode->s_link->prev = &(semd_table[0].s_link);

	semdFree_h = &semd_table[1]; //sentinella semafori non attivi.

	//Collego QUEI semafori tra loro tramite i connettori. Lista circolare (evitando di usare l'operatore modulo per costruirla)
	semd_table[1].s_link->next = &(semd_table[2].s_link);
	semd_table[1].s_link->prev = &(semd_table[MAXPROC].s_link);
	for (int i = 2; i < MAXPROC; i++){ //+1 e ->prev => lista circolare
		semd_table[i].s_link->next = &(semd_table[i+1].s_link);
		semd_table[i].s_link->prev = &(semd_table[i-1].s_link);
	}
	semd_table[MAXPROC].s_link->next = &(semd_table[1].s_link);
	semd_table[MAXPROC].s_link->prev = &(semd_table[MAXPROC-1].s_link);

}


int insertBlocked(int *semAdd,pcb_t *p){}
pcb_t* removeBlocked(int *semAdd){}
pcb_t* outBlocked(pcb_t *p){}
pcb_t* headBlocked(int *semAdd){}