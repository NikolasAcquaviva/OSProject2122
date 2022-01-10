/*
 * listePCB.c
 *
 *  Created on: 8 gen 2022
 *      Author: Pc
 */

#include "listePCB.h"



void mkEmptyProcQ(struct list_head *head){ //4
/*Crea una lista di PCB, inizializzandola
come lista vuota */
	head->p_next=NULL;
	head->p_head=head;
	//head->p_tail=head;

	//aggiungere in caso altri campi da inizioalizzare head->"campo"=0;

}

int emptyProcQ(struct list_head *head){ //5
/*Restituisce TRUE se la lista puntata da
head è vuota, FALSE altrimenti. */
	if(head->p_next == NULL) return TRUE;
	else return FALSE;
}

void insertProcQ(struct list_head* head, pcb_t *p){ //6
/*Inserisce l’elemento puntato da p nella
coda dei processi puntata da head*/
	head->p_next = p;	//p2->next = new pcb_t;
	//head->p_tail = p;
}

pcb_t headProcQ(struct list_head* head){ //7
/*Restituisce l’elemento di testa della coda
dei processi da head, SENZA
RIMUOVERLO. Ritorna NULL se la coda
non ha elementi. */
	if(head->p_head!=NULL) return head->p_head;
	else return NULL;
}

pcb_t* removeProcQ(struct list_head *head){ //8
/*Rimuove il primo elemento dalla coda dei
processi puntata da head. Ritorna NULL se la
coda è vuota. Altrimenti ritorna il puntatore
all’elemento rimosso dalla lista */
	list_head *p;
	list_head *newhead;
	if (head==NULL) return NULL;
	else if (head->p_next==NULL) return head;
	else {
		p = head ;
		newhead=p->p_next;
		p=p->p_next;
		while(p->p_next!=NULL){
			p->head=newhead;
			p=p->p_next;
		}
		head->p_next=NULL;
		head->p_head=NULL;

		return head;
}

pcb_t* outProcQ(struct list_head *head, pcb_t *p){ //9
/*Rimuove il PCB puntato da p dalla coda dei
processi puntata da head. Se p non è presente
nella coda, restituisce NULL. (NOTA: p può
trovarsi in una posizione arbitraria della coda). */
	if(head == p){
		removeProcQ(p);
		return p;
	}
	else{

	}

}
