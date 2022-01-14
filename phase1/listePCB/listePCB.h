/*
 * listePCB.h
 *
 *  Created on: 8 gen 2022
 *      Author: MM
 */

#ifndef LISTEPCB_H_
#define LISTEPCB_H_

#include "listx.h"

/*
//parte da commentare perchè contenuta in pandos_type.h
typedef signed int   cpu_t;
typedef unsigned int memaddr;
typedef signed int   state_t;	///contenuto in type.h

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


//semaphore descriptor (SEMD) data structure
typedef struct semd_t {
    // Semaphore key
    int *s_key;
    // Queue of PCBs blocked on the semaphore
    struct list_head s_procq;

    // Semaphore list
    struct list_head s_link;
} semd_t, *semd_PTR;
//fine parte da commentare
*/

#define TRUE 1
#define FALSE 0
//#define NULL ((pcb_t*) 0)

void mkEmptyProcQ(struct list_head *head); //4
/*Crea una lista di PCB, inizializzandola
come lista vuota */

int emptyProcQ(struct list_head *head); //5
/*Restituisce TRUE se la lista puntata da
head è vuota, FALSE altrimenti. */

void insertProcQ(struct list_head* head, pcb_t* p); //6
/*Inserisce l’elemento puntato da p nella
coda dei processi puntata da head*/

pcb_t headProcQ(struct list_head* head); //7
/*Restituisce l’elemento di testa della coda
dei processi da head, SENZA
RIMUOVERLO. Ritorna NULL se la coda
non ha elementi. */

pcb_t* removeProcQ(struct list_head *head); //8
/*Rimuove il primo elemento dalla coda dei
processi puntata da head. Ritorna NULL se la
coda è vuota. Altrimenti ritorna il puntatore
all’elemento rimosso dalla lista */

pcb_t* outProcQ(struct list_head *head, pcb_t *p); //9
/*Rimuove il PCB puntato da p dalla coda dei
processi puntata da head. Se p non è presente
nella coda, restituisce NULL. (NOTA: p può
trovarsi in una posizione arbitraria della coda). */


#endif /* LISTEPCB_H_ */
