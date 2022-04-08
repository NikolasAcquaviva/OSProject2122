/**
 * @file syscall.h
 * @author Enrico Emiro
 * @author Giulio Billi
 * @brief System call header
 * @date 2022-03-27
 */
#ifndef SYSCALL_H
#define SYSCALL_H

extern devregarea_t* deviceRegisters;

/**
 * @brief Crea un nuovo processo come figlio del chiamante. Il primo parametro
 * contiene lo stato che deve avere il processo.
 * Se la system call ha successo il valore di ritorno è il pid del processo
 * altrimenti è -1.
 *
 * @param state_ptr   puntatore alla struttura di stato del processo padre
 * @param prio        priorità del processo
 * @param support_ptr puntatore alla struttura di supporto del processo
 * @return int        pid del processo
 */
int createProcess(state_t* state_ptr, int prio, support_t* support_ptr);

/**
 * @brief Termina il processo indicato dal pid insieme a tutta la sua progenie.
 * Se il pid è 0 il bersaglio è il processo invocante.
 *
 * @param pid identificatore del processo da terminare
 */
void terminateProcess(int pid);

/**
 * @brief Operazione di richiesta di un semaforo. Il valore del semaforo è
 * memorizzato nella variabile di tipo intero passata per indirizzo. L’indirizzo
 * della variabile agisce da identificatore per il semaforo
 *
 * @param semAdd
 */
void passeren(int* semAdd);

/**
 * @brief Operazione di rilascio su un semaforo. Il valore del semaforo è
 * memorizzato nella variabile di tipo intero passata per indirizzo. L’indirizzo
 * della variabile agisce da identificatore per il semaforo.
 *
 * @param semAdd
 */
void verhogen(int* semAdd);

/**
 * @brief Effettua un’operazione di I/O scrivendo il comando cmdValue nel
 * registro cmdAdd, e mette in pausa il processo chiamante fino a quando non si
 * è conclusa. Il valore ritornato deve essere il contenuto del registro di
 * status del dispositivo.
 *
 * @param cmdAdd
 * @param cmdValue
 */
int doIODevice(int* cmdAdd, int cmdValue);

/**
 * @brief Quando invocata, restituisce il tempo di esecuzione (in microsecondi)
 * del processo che l’ha chiamata fino a quel momento. Questa System call
 * implica la registrazione del tempo passato durante l’esecuzione di un
 * processo.
 *
 * @return
 */
int getCPUTime();

/**
 * @brief Equivalente a una Passeren sul semaforo dell’Interval Timer.
 * Blocca il processo invocante fino al prossimo tick del dispositivo.
 */
void waitForClock();

/**
 * @brief Restituisce il puntatore alla struttura di supporto del processo
 * corrente. Se tale struttura non è stata assegnata durante la fase di
 * inizializzazione ritorna NULL.
 *
 * @return support_t* - puntatore alla struttura di supporto del processo
 *                      corrente
 */
support_t* getSupportData();

/**
 * @brief Restituisce l’identificatore del processo invocante se parent == 0,
 * quello del genitore del processo invocante altrimenti.
 *
 * @param parent
 * @return
 */
int getProcessID(int parent);

/**
 * @brief Il processo che invoca questa system call viene sospeso e messo in
 * fondo alla coda corrispondente dei processi ready.
 * Il processo che si è autosospeso, anche se rimane "ready", non può ripartire
 * immediatamente.
 */
void yield();

/**
 * @brief Processo di terminazione di un singolo processo
 *
 * @param processToTerminate
 */
void terminateOneProcess(pcb_t* processToTerminate);

/**
 * @brief Termina l'albero di figli del processo in input
 *
 * @param rootProcess root dell'albero da terminare
 *
 */
void terminateProgeny(pcb_t* rootProcess);

#endif
