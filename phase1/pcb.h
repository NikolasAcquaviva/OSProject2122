#ifndef PCB_H_
#define PCB_H_

#include "../h/pandos_const.h"
#include "../h/pandos_types.h"
#include "../h/listx.h"
#include "asl.h"


#define TRUE 1
#define FALSE 0

/*pcbFree_h: lista dei PCB che 
sono liberi o inutilizzati*/
LIST_HEAD(pcbFree_h);
/*pcbFree_table[MAXPROC]: array
di PCB con dimensione massima MAXPROC*/
pcb_t pcbFree_table[MAXPROC];

extern void initPcbs();
/*Inizializza la lista pcbFree in modo da 
contenere tutti gli elementi della pcbFree_table.
Questo metodo deve essere chiamato una volta sola in fase
di inizializzazione della struttura dati.*/

extern void freePcb(pcb_t *p);
/*Inserisce il PCB puntato da p nella
lista dei PCB liberi (pcbFree_h).*/

extern pcb_t *allocPcb();
/*Restituisce NULL se la pcbFree_h è vuota.
Altrimenti rimuove un elemento dalla pcbFree,
inizializza tutti i campi (NULL/0) e
restituisce l'elemento rimosso.*/

extern void mkEmptyProcQ(struct list_head *head); //4
/*Crea una lista di PCB, inizializzandola
come lista vuota */

extern int emptyProcQ(struct list_head *head); //5
/*Restituisce TRUE se la lista puntata da
head è vuota, FALSE altrimenti. */

extern void insertProcQ(struct list_head *head, pcb_t *p); //6
/*Inserisce l’elemento puntato da p nella
coda dei processi puntata da head*/

extern pcb_t *headProcQ(struct list_head *head); //7
/*Restituisce l’elemento di testa della coda
dei processi da head, SENZA
RIMUOVERLO. Ritorna NULL se la coda
non ha elementi. */

extern pcb_t *removeProcQ(struct list_head *head); //8
/*Rimuove il primo elemento dalla coda dei
processi puntata da head. Ritorna NULL se la
coda è vuota. Altrimenti ritorna il puntatore
all’elemento rimosso dalla lista */

extern pcb_t *outProcQ(struct list_head *head, pcb_t *p); //9
/*Rimuove il PCB puntato da p dalla coda dei
processi puntata da head. Se p non è presente
nella coda, restituisce NULL. (NOTA: p può
trovarsi in una posizione arbitraria della coda). */

extern int emptyChild(pcb_t *p); //10
/*
Restituisce TRUE se il PCB puntato da p
non ha figli, FALSE altrimenti.
*/

extern void insertChild(pcb_t *prnt, pcb_t *p); //11
/*
Inserisce il PCB puntato da p come figlio
del PCB puntato da prnt.
*/

extern pcb_t *removeChild(pcb_t *p); //12
/* 
Rimuove il primo figlio del PCB puntato
da p. Se p non ha figli, restituisce NULL.
*/

extern pcb_t *outChild(pcb_t *p); //13
/*
Rimuove il PCB puntato da p dalla lista
dei figli del padre. Se il PCB puntato da
p non ha un padre, restituisce NULL,
altrimenti restituisce l’elemento
rimosso (cioè p). A differenza della
removeChild, p può trovarsi in una
posizione arbitraria (ossia non è
necessariamente il primo figlio del
padre).
*/

#endif /* PCB_H_ */