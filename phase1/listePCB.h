#ifndef LISTEPCB_H_
#define LISTEPCB_H_

#include "listx.h"
#include "pandos_types.h"
#include "pandos_const.h"

#define TRUE 1
#define FALSE 0

/*pcbFree_h: lista dei PCB che 
sono liberi o inutilizzati*/
LIST_HEAD(pcbFree_h);
/*pcbFree_table[MAXPROC]: array
di PCB con dimensione massima MAXPROC*/
pcb_t pcbFree_table[MAXPROC];

void initPcbs();
/*Inizializza la lista pcbFree in modo da 
contenere tutti gli elementi della pcbFree_table.
Questo metodo deve essere chiamato una volta sola in fase
di inizializzazione della struttura dati.*/

void freePcb(pcb_t *p);
/*Inserisce il PCB puntato da p nella
lista dei PCB liberi (pcbFree_h).*/

pcb_t *allocPcb();
/*Restituisce NULL se la pcbFree_h è vuota.
Altrimenti rimuove un elemento dalla pcbFree,
inizializza tutti i campi (NULL/0) e
restituisce l'elemento rimosso.*/

void mkEmptyProcQ(struct list_head *head); //4
/*Crea una lista di PCB, inizializzandola
come lista vuota */

int emptyProcQ(struct list_head *head); //5
/*Restituisce TRUE se la lista puntata da
head è vuota, FALSE altrimenti. */

void insertProcQ(struct list_head* head, pcb_t* p); //6
/*Inserisce l’elemento puntato da p nella
coda dei processi puntata da head*/

pcb_t *headProcQ(struct list_head* head); //7
/*Restituisce l’elemento di testa della coda
dei processi da head, SENZA
RIMUOVERLO. Ritorna NULL se la coda
non ha elementi. */

pcb_t *removeProcQ(struct list_head *head); //8
/*Rimuove il primo elemento dalla coda dei
processi puntata da head. Ritorna NULL se la
coda è vuota. Altrimenti ritorna il puntatore
all’elemento rimosso dalla lista */

pcb_t *outProcQ(struct list_head *head, pcb_t *p); //9
/*Rimuove il PCB puntato da p dalla coda dei
processi puntata da head. Se p non è presente
nella coda, restituisce NULL. (NOTA: p può
trovarsi in una posizione arbitraria della coda). */


#endif /* LISTEPCB_H_ */