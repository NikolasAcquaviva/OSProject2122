#include "../h/pandos_const.h"
#include "../h/pandos_types.h"
#include "../h/listx.h"
#include "../h/pcb.h"
#include "../h/asl.h"

#define TRUE 1
#define FALSE 0
#define MAX_PROC 20
#define HIDDEN static //static globale => visibile solo nel file in cui compare la dichiarazione => è nascosta (hidden) altrove

static int *MAXINT = (int *)0x7FFFFFFF; //2^31 -1. Univoco addr. di memoria del valore del sema4 è identificativo di quest'ultimo 
static int *MININT = (int *)0x00000000;

//per fugare dubbi: intersezione tra semdFree_h e semd_h è l'insieme vuoto => puoi usare pointer diretti anzichè copie
//non esisterà un semaforo che punti da un lato ai liberi e dall'altro agli occupati
HIDDEN semd_t semd_table[MAX_PROC + 2]; //Tabella Semafori forniti
HIDDEN semd_PTR semdFree_h; //Lista DISORDINATA dei SEMD liberi o inutilizzati. E' circolare. (L'importante è prendere un semaforo libero)
HIDDEN semd_PTR semd_h; //Lista ORDINATA dei semafori attivi. Questa lista ha i dummy nodes. Testa di semd_h rimane sempre MININT 
//non ha senso che semd_h sia circolare poichè ci sono MININT e MAXINT che sono gli estremi che non possono essere sorpassati e rimossi
//in ogni caso sarebbe bastato un gioco di pointers tra MININT e MAXINT

/*
Ritorna il primo PCB dalla coda dei processi
bloccati (s_procq) associata al SEMD della
ASL con chiave semAdd. Se tale descrittore
non esiste nella ASL, restituisce NULL.
Altrimenti, restituisce l’elemento rimosso. Se
la coda dei processi bloccati per il semaforo
diventa vuota, rimuove il descrittore
corrispondente dalla ASL e lo inserisce nella
coda dei descrittori liberi (semdFree_h).
*/
pcb_t* removeBlocked(int *semAdd){

	//pedanteria assoluta
	if (semAdd == NULL) return NULL;

	semd_PTR index = semd_h;

    while(index->s_key != MAXINT){ //cerchiamo tra sema4 attivi

    	if(index->s_semAdd == semAdd){

    		//Rimuove il primo elemento dalla coda dei processi puntata da head. Ritorna NULL se la
			//coda è vuota. Altrimenti ritorna il puntatore all’elemento rimosso dalla lista.
    		pcb_PTR toReturn = removeProcQ(&(index->s_procq), p);

    		//se dopo aver tolto p non ci sono più processi in attesa su quel semaforo => libero il semaforo
    		if (emptyProcQ(&(index->s_procq))){

    			//lo stacca dalla ASL... per staccare/eliminare si intende "scucire"
    			__list_del(&(index->s_link.prev), &(index>s_link.next));

    			//...e lo attacca (o "cuce") alla testa di semdFree_h, facendo puntare ad index altro
    			__list_add(&(index->s_link), &(semdFree_h->s_link.prev), &(semdFree_h->s_link));
    		}

    		if (toReturn != NULL) toReturn->p_semAdd = NULL;
    		return toReturn;

    	}
    	else index = container_of(&(index>s_link.next), "semd_t", "s_link");
	}
	return NULL;
}


/*
Rimuove il PCB puntato da p dalla coda del semaforo
su cui è bloccato (indicato da p->p_semAdd). Se il PCB
non compare in tale coda, allora restituisce NULL
(condizione di errore). Altrimenti, restituisce p. Se la
coda dei processi bloccati per il semaforo diventa
vuota, rimuove il descrittore corrispondente dalla ASL
e lo inserisce nella coda dei descrittori liberi
*/
pcb_t* outBlocked(pcb_t *p){

	//pedanteria assoluta - qui NULL corrisponde a condizione di errore
	if (p == NULL || p->p_semAdd == NULL) return NULL; //anche se il semaforo di p non esiste è condizione d'errore

	int *semAdd = p->p_semAdd;  //indirizzo del semaforo da cercare

    semd_PTR index = semd_h;

    while(index->s_key != MAXINT){ //cerchiamo tra sema4 attivi

    	if(index->s_semAdd == semAdd){

    		//Rimuove il PCB puntato da p dalla coda dei processi puntata da head. Se p non è presente nella coda, restituisce NULL.
    		pcb_PTR toReturn = outProcQ(&(index->s_procq), p); //abbiamo la garanzia che il sema4 abbia almeno un processo che aspetta per la risorsa(altrimenti sarebbe stato rimosso)
    		
    		//se dopo aver tolto p non ci sono più processi in attesa su quel semaforo => libero il semaforo
    		if (emptyProcQ(&(index->s_procq))){

    			//lo stacca dalla ASL...
    			__list_del(&(index>s_link.prev), &(index>s_link.next));

    			//...e lo attacca alla testa di semdFree_h, facendo puntare ad index altro
    			__list_add(&(index->s_link), &(semdFree_h->s_link.prev), &(semdFree_h->s_link));
    		}

    		if (toReturn != NULL) toReturn->p_semAdd = NULL;
    		return toReturn;
    	}

    	else index = container_of(&(index>s_link.next), "semd_t", "s_link");
    }
    return NULL;
}


/*
Restituisce (senza rimuovere) il puntatore al PCB che
si trova in testa alla coda dei processi associata al
SEMD con chiave semAdd. Ritorna NULL se il SEMD
non compare nella ASL oppure se compare ma la sua
coda dei processi è vuota.
*/
//chiedere se hanno implementato liste di pcb_t in modo circolare e double-linked
//si suppone che le liste di pcb_t non abbiano un MININT e un MAXINT
//list_head è sempre un connettore => container_of()

pcb_t* headBlocked(int *semAdd){

	//pedanteria assoluta
	if (semAdd == NULL) return NULL;

	semd_PTR index = semd_h;

	//controllo se semAdd compare nella ASL
	while(index->s_key != MAXINT){

		//Restituisce l’elemento di testa della coda dei processi da head, SENZA RIMUOVERLO. Ritorna NULL se la coda non ha elementi.
		if(index->s_key == semAdd) return headProcQ(&(index->s_procq));

        index = container_of(&(index>s_link.next), "semd_t", "s_link");
	}

	//il SEMD non compare nella ASL
	return NULL;
}


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

	//pedanteria assoluta
	if (p == NULL || semAdd == NULL) return FALSE;

	semd_PTR index = semd_h; //copia puntatore temporaneo per scorrere. Si parte da MININT

	while(index->s_key != MAXINT){

		//creato per comodità per l'elif e scorrimento. Se semd_h ha solo un semaforo impegnato => questo sarà MAXINT il quale è l'ultimo
		semd_PTR nextIndex = container_of(&(index>s_link.next), "semd_t", "s_link"); //chiedere se va passata la stringa o il tipo

		if(index->s_key == semAdd){ //risorsa/semaforo impegnato già precedentemente

			p->p_semAdd = semAdd;   /* ptr to semaphore on which proc is blocked */
            insertProcQ(&(index->s_procq), p); //insertProcQ(struct list_head* head, pcb* p)
            return FALSE;
        }

        /*
        https://github.com/agente-drif/ProgettoSistemiOperativi/blob/master/src/phase1/asl.c
		ci sarà sempre un semaforo attivo/utilizzato anche se fittizio (MAXINT e MININT) => rimaneggiare idea drif come segue:
		if (index->s_key == MININT && nextIndex->s_key == MAXINT){ //ASL vuota con due nodi fittizi -> questa condizione vale nel seguente blocco else if
        */

        //altrimenti se soddisfatte condizioni => inserimento rispettando ordine non decrescente
        else if (nextIndex->s_key > semAdd || nextIndex->s_key == MAXINT){

        	//nuovo semaforo da allocare => facciamo check prima
        	if (semdFree_h == NULL) return TRUE;
        	
        	//riguardo semdFree_h
			semd_PTR semToAdd = semdFree_h; //stacco/salvo la testa di semdFree_h la quale è circolare (prendo il primo sema4 libero)
			__list_del(&(semdFree_h->s_link.prev), &(semdFree_h>s_link.next));

			//sema4 init
			//no assegnamento poichè mkEmptyProcQ è void
            mkEmptyProcQ(&(semToAdd->s_procq)); //no NULL perchè altrimenti la lista VUOTA non viene riconosciuta come tale (poichè qui una lista è vuota se entrambi i suoi campi puntano alla lista stessa)
            semToAdd->s_key = semAdd; //proper init

            //riguardo semd_h
            __list_add(&(semToAdd->s_link), &(index->s_link), &(index->s_link.next));

            p->p_semAdd = semAdd; // Pointer to the semaphore the process is currently blocked on

            insertProcQ(&(semToAdd->s_procq), p);

            return FALSE;
        }
        //altrimenti continuo a scorrere
       	else index = nextIndex;	//index = container_of(&(index>s_link.next), "semd_t", "s_link");;
	}

	//pedante, non lo raggiungerà mai. Al massimo si fermerà prima di MAXINT
	return FALSE;
}

void initASL(){

	//inizializzazione di QUEGLI elementi
	for (int i = 0; i < MAX_PROC + 2; i++){ //https://github.com/leti15/project_pandOS/blob/main/phase1/asl.c
		semd_table[i].s_key = NULL; //NULL == 0? puntatore
		//se risulterà che semd_table[i].s_link è una struttura gia' esistente => INIT_LIST_HEAD(&(semd_table[i].s_link)) la quale è VOID => no assegnamento ma comando
		semd_table[i].s_link = LIST_HEAD_INIT(&(semd_table[i].s_link)); //altrimenti se avessi messo NULL NON RICONOSCIUTA COME "LISTA VUOTA" (ovvero che punta ambo i lati a se stessa) 
		mkEmptyProcQ(&(semd_table[i].s_procq)); //dalla documentazione sembra che debba mettere mkEmptyProcQ() anzichè NULL (pg 23 pdf)
	}

	//Inizializzazione Nodi dummy per lista dei semafori attivi (=> utilizzati). Lista circolare
	semd_h = &semd_table[0];
	semd_h->s_key = MININT;
	mkEmptyProcQ(&(semd_h->s_procq));
	semd_h->s_link.next = &(semd_table[MAX_PROC+1].s_link);
	semd_h->s_link.prev = NULL; //NULL == 0? puntatore

	semd_PTR MAXINTDummyNode = &semd_table[MAX_PROC + 1];//container_of(&(semd_h->s_link.next), "semd_t", "s_link"); //puntatore alla struttura che contiene il prossimo connettore
	MAXINTDummyNode->s_key = MAXINT;
	mkEmptyProcQ(&(MAXINTDummyNode->s_procq));
	MAXINTDummyNode->s_link.next = NULL; //NULL == 0? puntatore
	MAXINTDummyNode->s_link.prev = &(semd_table[0].s_link);

	//sentinella semafori non attivi => pronti per essere usati.
	semdFree_h = &semd_table[1];

	//Collego QUEI semafori tra loro tramite i connettori. Lista circolare (evitando di usare l'operatore modulo per costruirla)
	semd_table[1].s_link.next = &(semd_table[2].s_link);
	semd_table[1].s_link.prev = &(semd_table[MAX_PROC].s_link);
	for (int i = 2; i < MAX_PROC; i++){
		semd_table[i].s_link.next = &(semd_table[i+1].s_link);
		semd_table[i].s_link.prev = &(semd_table[i-1].s_link);
	}
	semd_table[MAX_PROC].s_link.next = &(semd_table[1].s_link);
	semd_table[MAX_PROC].s_link.prev = &(semd_table[MAX_PROC-1].s_link);

}