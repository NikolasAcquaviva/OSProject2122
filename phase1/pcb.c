#include "../h/pandos_const.h"
#include "../h/pandos_types.h"
#include "../h/listx.h"
#include "../h/pcb.h"
#include "../h/asl.h"
//NON BASARSI SU QUESTE LIBRERIE!
#include <string.h>

void initPcbs(){
	INIT_LIST_HEAD(&pcbFree_h); //inizializza il nodo sentinella
	/*
	La funzione di aggiunta in testa alla lista aggiunge in realtà
	il nodo tra i primi 2 elementi,di conseguenza è scritta
	in modo che la lista bidirezionale venga gestita con un
	nodo sentinella(così il nodo aggiunto sarà sempre il secondo,
	ovvero il primo esclusa la sentinella). Inoltre la funzione di aggiunta
	in testa non aggiorna la testa del puntatore, ovvero accedendo
	a freepcb_h accederemo sempre al nodo sentinella.
	*/
	for(int i = 0; i < MAXPROC; i++) list_add(&pcbFree_table[i].p_list, &pcbFree_h);
	//con la prima riga di codice viene definito il nodo sentinella
	//ogni volta che entriamo nel for aggiungiamo un nodo che viene 
	//puntato dal next del nodo sentinella. Avremo quindi MAXPROC + 1
	//nodi, di cui uno(il primo) è la sentinella.
}

void freePcb(pcb_t *p){
	//controllo che p non sia già nella lista dei pcb liberi
	pcb_t *tmp; 
	list_for_each_entry(tmp,&pcbFree_h,p_list) if(tmp==p) break;
	if(&tmp->p_list==&pcbFree_h) list_add(&p->p_list,&pcbFree_h);//se sono qui non sono mai entrato nell'if precedente
	else exit(-1);
}

pcb_t *allocPcb(){
	if(list_empty(&pcbFree_h)) return NULL;
	else{
		//il primo vero nodo della lista dei pcb(ricordando che usiamo la sentinella)
		struct list_head *head = pcbFree_h.next;
		//l'istanza del primo pcb, quella che contiene il nodo \
		  puntato da head nel campo p_list
		pcb_t *tmp = container_of(&head,pcb_t,p_list);
		//inizializzare il blocco di memoria occupato \
		  da un'istanza di tipo pcb_t
		//NO MEMSET
		memset(&tmp,0,sizeof(pcb_t)); return tmp;									   		
	}
}

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

pcb_t *headProcQ(struct list_head* head){ //7
/*Restituisce l’elemento di testa della coda
dei processi da head, SENZA
RIMUOVERLO. Ritorna NULL se la coda
non ha elementi. */
	pcb_t *h = NULL;
	if(head->next != NULL){
		h = head->next;
	}
	return h;
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

int emptyChild(pcb_t *p) { //10
/*
Restituisce TRUE se il PCB puntato da p
non ha figli, FALSE altrimenti.
*/
if (list_empty(&p->p_child))  return TRUE;
else return FALSE;
}

void insertChild(pcb_t *prnt, pcb_t *p){ //11
/*
Inserisce il PCB puntato da p come figlio
del PCB puntato da prnt.
*/
list_add(&p->p_list, &prnt->p_child);
}


pcb_t* removeChild(pcb_t *p) { //12
/*
Rimuove il primo figlio del PCB puntato
da p. Se p non ha figli, restituisce NULL.
*/
if (list_empty(&p->p_child)) return NULL;
else {
pcb_t * uscita = &p->p_child;
list_del(&p->p_child);
return uscita;
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

/*
Commento di Matteo.
tmp e p sono puntatori di tipo pcb_t, ma sia list_prev e list_next accettano
come argomento struct list_head, e ritornano un pointer a questa struct, non al tipo pcb_t
Quando leggete struct list_head, leggetela in verità come "struct connettore", creata esclusivamente per creare strutture dati generiche
Infatti gli unici campi che hanno le struct "connettore" sono prev e next. Non memorizzano nessun'altro valore

tmp->next il tipo pcb_t non ha nessun campo next
*/
if (list_empty(&p->p_parent)) return NULL;
else {
	//per rimuovere p devi fare in modo di non lasciare la lista scollegata \
	devi usare la funzione di rimozione fornita da list e probabilmente \
	anche una funzione che itera la lista per fornire alla funzione \
	i puntatori che richiedono(forse quello precedente e successivo \
	a quello che devi eliminare) . Leggi un po' le funzioni che sono descritte precisamente.
    //PERCHE TMP PUNTA AL PRECEDENTE DI P? SBAGLIO O BASTA \
	MEMORIZZARE P IN UNA VARIABILE CHE SARA RITORNATA DOPO \
	AVERLO RIMOSSO DALLA LISTA DEI FIGLI DEL PADRE?? \
	NON SO PERCHÉ NON MI DA UN ERRORE A COMPILE TIME MA LA RIGA \
	SOTTO È PALESEMENTE UN ERRORE, LIST_PREV RITORNA UN LISTHEAD E \
	UN PUNTATORE A PCB NON PUO PUNTARCI QUINDI.
	pcb_t * tmp = p;
    list_del(p);
	return tmp;
}
}
