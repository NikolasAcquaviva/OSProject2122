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
	head->next=NULL;
	head->prev=NULL;
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
	struct list_head* tail;
	if(head->next !=NULL){	//caso lista non vuota
		tail=head->prev;	//prende l'elemento precedente della lista
		tail->next=p;		//inserisce il pcb puntato da p
		p->p_list.prev=tail;	//riempie i campi di p
		p->p_list.next=head;
		head->prev=p;	//imposta p come ultimo elemento della lista
	}
	else{	//caso lista vuota
		head->next=p;	//mette p come precedente e successivo
		head->prev=p;
		p->p_list.prev=p;
		p->p_list.next=p;
	}
}

pcb_t headProcQ(struct list_head* head){ //7
/*Restituisce l’elemento di testa della coda
dei processi da head, SENZA
RIMUOVERLO. Ritorna NULL se la coda
non ha elementi. */
	pcb_t *h=NULL;
	if(head->next!=NULL){
		h=head->next;
	}
	return *h;
}

pcb_t* removeProcQ(struct list_head *head){ //8
/*Rimuove il primo elemento dalla coda dei
processi puntata da head. Ritorna NULL se la
coda è vuota. Altrimenti ritorna il puntatore
all’elemento rimosso dalla lista */
	pcb_t* p;
	pcb_t* pafter;

	if (head->next==NULL){ 	//controlla se la lista è vuota
		return NULL;
	}
	p=head->next;
	if (p->p_list.next==head) {	//controlla se ci sono altri elementi oltre al primo
		head->next=NULL;
		head->prev=NULL;
		return p;
	}
	else {
		pafter=p->p_list.next;
		pafter->p_list.prev=head;
		head->next=pafter;
		p->p_list.next=NULL;
		p->p_list.prev=NULL;

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

	if(head->next == p){ // controlla se il pcb da togliere è il primo
		rt=removeProcQ(head); //quindi richiama la funzione per rimuovere il primo elemento della coda
	}
	else{
		tmp=head->next;
		if(tmp->p_list.next==head) {	//controlla se ci sono altri elementi oltre al primo
			return rt;
		}
		else{
			while((tmp!=head)&&(tmp!=p)){	//scorre la lista fino a che non trova il pcb p o ritorna alla testa della lista
				tmp=tmp->p_list.next;
			}
			if(tmp==p){// se ha trovato l'elemento
				tmpbefore=tmp->p_list.prev;					//puntatore dell'elemento precedente a p
				tmpbefore->p_list.next=tmp->p_list.next;	//il campo next del pcb precedente a p, ora punta all'elemento successivo a p
				rt=tmp;										//rt ora punta a p
				tmp=tmp->p_list.next;						//tmp ora punta all'elemento successivo a p
				tmp->p_list.prev=tmpbefore;					//il campo prev del pcb successivo a p, ora punta all'elemento precedente a p
				rt->p_list.next=NULL;						//pulisco i campi di p che ho rimosso dalla lista
				rt->p_list.prev=NULL;
			}
		}

	}
	return rt;
}


/*
pcb_t* sentinel_listPCB(struct list_head *head){	//cast tra list_head e pcb_t
	pcb_t* h=allocPcb();	//alloca h che sarà il pcb di head
	h->p_list.next=head->next;
	h->p_list.prev=head->prev;
	return h;
}
*/
