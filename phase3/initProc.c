#include <umps3/umps/libumps.h>
#include "../pandos_const.h"
#include "../pandos_types.h"

//Master sempahore which wait for all processes to be concluded in order to terminate testing
int masterSem;

//A static array of support structures to allocate
static support_t supPool[UPROCMAX + 1]; //+1 perchè ASID 0 is reserved for kernel daemons

//CREATE A PROCESS USING ITS ID (PROCESS ASID)
void createUProc(int id){
	// initialization of the process private PageTable
	for (int j = 0; j < MAXPAGES - 1; j++){ //-1 perchè l'ultima entry è dedicata allo stack'

	}
}





//entrypoint per la creazione dei processi di test
/*
Inizializza Swap Pool table e semafori, crea gli 8 processi e ne gestisce la terminazione
Il nome è rimasto 'test' dalla fase 2
*/
void test() {}
