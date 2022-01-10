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
	if (head==NULL) return NULL;	//controlla se la lista è vuota
	else if (head->p_next==NULL) return head;	//controlla se ci sono altri elementi oltre la testa
	else {
		p = head ;	//salva il puntatore alla testa in un puntatore p temporaneo
		newhead=p->p_next;	//salva il puntatore al elemento successivo alla testa, che sarà la nuova testa
		p=p->p_next;
		while(p->p_next!=NULL){	//assegna l'indirizo alla nuova testa a tutti i nodi della coda
			p->head=newhead;
			p=p->p_next;
		}
		head->p_next=NULL;	//pulisce i campi del nodo rimosso
		head->p_head=NULL;

		return head;
}

pcb_t* outProcQ(struct list_head *head, pcb_t *p){ //9
/*Rimuove il PCB puntato da p dalla coda dei
processi puntata da head. Se p non è presente
nella coda, restituisce NULL. (NOTA: p può
trovarsi in una posizione arbitraria della coda). */
	list_head *tmp;
	list_head *tmpbefore;
	list_head *rt=NULL;
	if(head == p){ // controlla se il pcb da togliere è in testa
		rt=removeProcQ(p); //quindi richiama la funzione per rimuovere il primo elemento della coda
		
	}
	else{
		if(head->p_next!=NULL){	//controlla che ci siano elementi dopo il primo
			tmp=head->p_next;	//puntatore da fare scorrere della lista
			tmpbefore = head;	//puntatore al elemento precedente a quello puntato da tmp
			while(tmp->p_next!=NULL){ //controlla se il PCB da togliere è in mezzo alla coda

				if(tmp==p){
					tmpbefore->p_next = tmp->p_next; //fa puntare il blocco precedente a p a quello successivo a p
					tmp->p_next=NULL;	//toglie il puntatore al blocco successivo a p
					tmp->p_head=NULL;	//toglie il puntatore alla testa della lista
					rt=p;
				}
				tmpbefore = tmpbefore->p_next; //scorrono i puntatori per far andare avanti il ciclo
				tmp=tmp->p_next;
			}
		}
	}
	return rt;
}
