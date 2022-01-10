/*
 * listePCB.c
 *
 *  Created on: 8 gen 2022
 *      Author: MM
 */

#include "listePCB.h"



void mkEmptyProcQ(struct list_head *head){ //4
/*Crea una lista di PCB, inizializzandola
come lista vuota */
	pcb_t *q=head;
	q->p_next=NULL;
	q->p_parent=NULL;
	q->p_child=NULL;
	q->p_sib=NULL;
	q->p_s=0;
	q->p_time=0;
	q->p_semAdd=NULL;

	//aggiungere in caso altri campi da inizializzare head->"campo"=0;

}

int emptyProcQ(struct list_head *head){ //5
/*Restituisce TRUE se la lista puntata da
head è vuota, FALSE altrimenti. */
	if(head== NULL) return TRUE;
	else return FALSE;
}

void insertProcQ(struct list_head* head, pcb_t* p){ //6
/*Inserisce l’elemento puntato da p nella
coda dei processi puntata da head*/
	pcb_t q;
	q=head;
	while(q->p_next!=NULL){ //scorre la lista fino alla fine
		q=q->p_next;
	}
	q->p_next = p;	//p2->next = new pcb_t;
}

pcb_t headProcQ(struct list_head* head){ //7
/*Restituisce l’elemento di testa della coda
dei processi da head, SENZA
RIMUOVERLO. Ritorna NULL se la coda
non ha elementi. */
	if(head!=NULL) return head;
	else return NULL;
}

pcb_t* removeProcQ(struct list_head *head){ //8
/*Rimuove il primo elemento dalla coda dei
processi puntata da head. Ritorna NULL se la
coda è vuota. Altrimenti ritorna il puntatore
all’elemento rimosso dalla lista */
	pcb_t *p;
	pcb_t *newhead;
	p=head;	//salva il puntatore alla testa in un puntatore p temporaneo
	if (p==NULL) return NULL;	//controlla se la lista è vuota
	else if (p->p_next==NULL) {	//controlla se ci sono altri elementi oltre la testa
		head=NULL;
		return p;
	}
	else {
		newhead=p->p_next;	//salva il puntatore al elemento successivo alla testa, che sarà la nuova testa
		head=newhead;
		return p;
}

pcb_t* outProcQ(struct list_head *head, pcb_t *p){ //9
/*Rimuove il PCB puntato da p dalla coda dei
processi puntata da head. Se p non è presente
nella coda, restituisce NULL. (NOTA: p può
trovarsi in una posizione arbitraria della coda). */
	pcb_t *tmp;
	pcb_t *tmpbefore;
	pcb_t *rt=NULL;
	tmp=head;
	if(tmp == p){ // controlla se il pcb da togliere è in testa
		rt=removeProcQ(p); //quindi richiama la funzione per rimuovere il primo elemento della coda

	}
	else{
		if(tmp->p_next!=NULL){	//controlla che ci siano elementi dopo il primo
			tmp=tmp->p_next;	//puntatore da fare scorrere della lista
			tmpbefore = head;	//puntatore al elemento precedente a quello puntato da tmp
			while(tmp->p_next!=NULL){ //controlla se il PCB da togliere è in mezzo alla coda

				if(tmp==p){
					tmpbefore->p_next = tmp->p_next; //fa puntare il blocco precedente a p a quello successivo a p
					tmp->p_next=NULL;	//toglie il puntatore al blocco successivo a p
					rt=p;
				}
				tmpbefore = tmpbefore->p_next; //scorrono i puntatori per far andare avanti il ciclo
				tmp=tmp->p_next;
			}
		}
	}
	return rt;
}
