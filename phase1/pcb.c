#include "asl.h"
#include "../pandos_const.h"
#include "../pandos_types.h"
#include "../listx.h"

/*pcbFree_h: lista dei PCB che 
sono liberi o inutilizzati*/
HIDDEN struct list_head pcbFree_h = LIST_HEAD_INIT(pcbFree_h);
/*pcbFree_table[MAXPROC]: array
di PCB con dimensione massima MAXPROC*/
pcb_t pcbFree_table[MAXPROC];

void initPcbs(){
	//inizializzazione sentinella
	INIT_LIST_HEAD(&pcbFree_h); 
	//aggiunta dei pcb della table alla lista dei pcb liberi e aggiornamento dei campi
	//p_list di ognuno perché puntino ai nodi della lista dei pcb liberi
	for(int i = 0; i < MAXPROC; i++) {
		pcbFree_table[i].p_list = *pcbFree_h.next;
		list_add(&pcbFree_table[i].p_list, &pcbFree_h);
	}
}

void freePcb(pcb_t *p){
	//controllo che p non sia già nella lista dei pcb liberi
	pcb_t *tmp; 
	list_for_each_entry(tmp,&pcbFree_h,p_list) if(tmp==p) break;
	if(&tmp->p_list==&pcbFree_h) list_add(&p->p_list,&pcbFree_h);//se sono qui non sono mai entrato nell'if precedente
}

pcb_t *allocPcb(){
	//i campi p_list di ogni pcb sono inizializzati in initpcbs()
	//la lista dei pcb liberi cosi come, per ogni pcb, la lista 
	//dei figli e dei fratelli sono liste bidirezionali con sentinella
	if(list_empty(&pcbFree_h)) return NULL;
	else{
		//il primo vero nodo della lista dei pcb(ricordando che usiamo la sentinella)
		struct list_head *head = pcbFree_h.next;
		//l'istanza del primo pcb, quella che contiene il nodo
		//puntato da head nel campo p_list
		pcb_t *tmp = container_of(head,pcb_t,p_list);
		
		tmp->p_parent = NULL;
		tmp->p_child.next = tmp->p_child.prev = &tmp->p_child;	
		tmp->p_sib.next = tmp->p_sib.prev = &tmp->p_sib; 
		tmp->p_semAdd = NULL;
		tmp->p_time = 0;
		list_del(&tmp->p_list);
		return tmp;
		}
}

void mkEmptyProcQ(struct list_head *head){ //4
/*Crea una lista di PCB, inizializzandola
come lista vuota */
	INIT_LIST_HEAD(head);
}

int emptyProcQ(struct list_head *head){ //5
/*Restituisce TRUE se la lista puntata da
head è vuota, FALSE altrimenti. */
	if(list_empty(head)) return TRUE;
	else return FALSE;
}

void insertProcQ(struct list_head* head, pcb_t* p){ //6
/*Inserisce l’elemento puntato da p nella
coda dei processi puntata da head*/
	list_add_tail(&p->p_list, head);		//inserisce il pcb puntato da p in coda
}

pcb_t* headProcQ(struct list_head* head){ //7
/*Restituisce l’elemento di testa della coda
dei processi da head, SENZA
RIMUOVERLO. Ritorna NULL se la coda
non ha elementi. */
	pcb_t* h = NULL;
	if(head->next != head){	// controlla se c'è un elemento dopo head
		h = container_of(head->next, pcb_t, p_list);
	}
	return h;
}

pcb_t* removeProcQ(struct list_head *head){ //8
/*Rimuove il primo elemento dalla coda dei
processi puntata da head. Ritorna NULL se la
coda è vuota. Altrimenti ritorna il puntatore
all’elemento rimosso dalla lista */

	if (list_empty(head)){ 	//controlla se la lista è vuota
		return NULL;
	}
	pcb_t *p = container_of(head->next, pcb_t, p_list); //primo elemento della coda dei processi
	list_del(&p->p_list);
	return p;
}

pcb_t* outProcQ(struct list_head *head, pcb_t *p){ //9
/* Rimuove il PCB puntato da p dalla coda dei
processi puntata da head. Se p non è presente
nella coda, restituisce NULL. (NOTA: p può
trovarsi in una posizione arbitraria della coda). */
	pcb_t *tmp;

	list_for_each_entry(tmp, head, p_list){
		if(tmp==p){//p è nella coda dei processi puntata da head
			list_del(&tmp->p_list);
			return tmp;
		}
	}
	return NULL;
}

int emptyChild(pcb_t *p) { //10
/*
Restituisce TRUE se il PCB puntato da p
non ha figli, FALSE altrimenti.
*/
if (list_empty(&p->p_child)) return TRUE;
else return FALSE;
}

void insertChild(pcb_t *prnt, pcb_t *p){ //11
/*
Inserisce il PCB puntato da p come figlio
del PCB puntato da prnt.
*/
	list_add(&p->p_list,&prnt->p_child);
	p->p_parent = prnt;
}



pcb_t* removeChild(pcb_t *p) { //12
/*
Rimuove il primo figlio del PCB puntato
da p. Se p non ha figli, restituisce NULL.
*/
	if (list_empty(&p->p_child)) return NULL;
	else{
		pcb_t *tmp = container_of(p->p_child.next, pcb_t, p_list);
		// tmp = pcb primo figlio
		if (list_empty(&tmp->p_sib)){
			list_del(&tmp->p_list);
			return tmp;
		}
		else{
			struct list_head *toRemove = &tmp->p_list;
			p->p_child.next = tmp->p_sib.next; //primo figlio di p diventa il primo fratello del precedente primo figlio
			pcb_t * tmp_sib = container_of(tmp->p_sib.next, pcb_t, p_list); // accediamo al next perché la lista dei fratelli parte da tmp,ovvero tmp non è fratello destro di nessun nodo e il primo fratello è la sentinella della lista dei fratelli
			tmp_sib->p_child = tmp->p_child; //il nuovo figlio ha come figlio il figlio del precedente
			pcb_t * tmp_child = container_of(&tmp->p_child, pcb_t, p_list);
			tmp_child->p_parent = tmp_sib;
			pcb_t * iter;
			list_for_each_entry(iter, &tmp_child->p_sib, p_sib){
				//assegno a tutti i figli a 2 livelli da p il parent che sia 
				//il nuovo primo figlio(tmp_sib) e non quello che deve essere cancellato
				iter->p_parent = tmp_sib;
			}
			list_del(toRemove);
			return tmp;
		}
	}
}


pcb_t *outChild(pcb_t* p) { //13
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
	if (p->p_parent == NULL) return NULL;
	else if (&p->p_parent->p_child == &p->p_list) return removeChild(p->p_parent);
	else{
		pcb_t *exit = p;
		list_del(&p->p_list);
		return exit;
	}
}
