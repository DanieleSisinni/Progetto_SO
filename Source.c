//Source management here

#include "Shared.h"

//Parametri passati da argv
int x;
int y;
int *shd_mem_source;
int my_message_queue = 0;
int my_sem_set = -1;

//segnali
sigset_t my_mask;
sigset_t signals;
int calls = 0;

//dichiarazione dei metodi
void local_map_setup();
void implemented_signals();
void handle_signal(int signal);
void call_for_a_taxi();

struct msgbuf my_msg_sender;

int main(int argc, char *argv[]){
    if(argc != 6){ 
        printf("Errore nel numero di parmetri passati a SOURCE\n");
		exit(EXIT_FATAL);
    }

    //inizializzo il set di segnali
    sigfillset(&signals); 
    implemented_signals();

    //leggo i parametri passati dal Master
    x = atoi(argv[1]);
    y = atoi(argv[2]);

    shd_mem_source = (int *)shmat(atoi(argv[3]), NULL, 0);
    if(shd_mem_source == NULL){
        printf("Errore caricamento della memoria condivisa in SOURCE\n");
		exit(EXIT_FATAL);
    }

    local_map_setup();

    my_message_queue = atoi(argv[4]);
    my_sem_set = atoi(argv[5]);
    
    sem_syncronization(my_sem_set);
    raise(SIGALRM);

    while (1)
    {
        pause();
    }
}

void local_map_setup(){
    int i, j, offset = 0;
    
    //allocazione mappa locale e caricamento dei valori da memoria condivisa
    local_map = (int **)malloc(SO_HEIGHT*sizeof(int *));
    if (local_map == NULL){
        allocation_error("Source", "local_map");
    }
    for (i = 0; i<SO_HEIGHT; i++){
        local_map[i] = malloc(SO_WIDTH*sizeof(int));
        if (local_map[i] == NULL){
            allocation_error("Source", "local_map");
        }
        for (j = 0; j < SO_WIDTH; j++){
            local_map[i][j] = shd_mem_source[offset]; 
            offset++;
        }
    }
    shmdt(shd_mem_source);
}

void call_for_a_taxi(){
    int arrival_x, arrival_y, arrival_ok = 0;
    srand(getpid());

    while (!arrival_ok){
        arrival_x = rand() % SO_HEIGHT;
        arrival_y = rand() % SO_WIDTH;

        if (local_map[arrival_x][arrival_y] && x != arrival_x && y != arrival_y){
            arrival_ok = 1;
        }
    }
    my_msg_sender.mtype = (x * SO_WIDTH) + y;
    if (my_msg_sender.mtype <= 0){
        printf("Errore: mtype inserito < 0");
        exit(EXIT_FATAL);
    }
    sprintf(my_msg_sender.mtext, "%d", (arrival_x * SO_WIDTH) + arrival_y);
    lock_signals(signals);
    sem_reserve_msgqueue(my_sem_set);


    msgsnd(my_message_queue, &my_msg_sender, MSG_LEN, IPC_NOWAIT);
    test_error();

    sem_release_msgqueue(my_sem_set);
    unlock_signals(signals);
}

void implemented_signals(){
    struct sigaction restart, fail;

    bzero(&restart,sizeof(restart));
    bzero(&fail,sizeof(fail));

    //Imposta il gestore del segnale. Il gestore viene passato come puntatore
    restart.sa_handler = handle_signal;
    fail.sa_handler = handle_signal;

    //Questo flag (SA_RESTART) controlla cosa succede quando un segnale viene consegnato durante determinate primitive (come open, reado write) e il gestore del segnale ritorna normalmente. Esistono due alternative: la funzione di libreria può riprendere o può restituire un errore con codice di errore EINTR.
    restart.sa_flags = SA_RESTART; 
    fail.sa_flags = 0;

    //Uso della maschera per bloccare i segnali per bloccare i segnali durante l’esecuzione dell’handler
    sigaddset(&my_mask, SIGALRM);
    sigaddset(&my_mask, SIGQUIT);
    sigaddset(&my_mask, SIGINT);
    sigaddset(&my_mask, SIGUSR1);

    restart.sa_mask = my_mask;
    fail.sa_mask = my_mask;

    //!!set new handler to new (controllare se è corretto)(dovrebbe)!!
    sigaction(SIGALRM, &restart, NULL);
    sigaction(SIGINT, &fail, NULL);
    sigaction(SIGQUIT, &fail, NULL);
    sigaction(SIGUSR1, &restart, NULL);
    
    test_error();
}


void handle_signal(signal){
    lock_signals(my_mask);

    if (signal == SIGALRM){
        call_for_a_taxi();
        //tempo di attesa per la prossima richiesta
        alarm(rand() % 5 + 1);
    } else if(signal == SIGQUIT){
        alarm(0);               
        free_matrix(2, local_map, NULL, NULL, NULL, 0, NULL);
        exit(EXIT_SUCCESS);     
    } else if (signal == SIGUSR1){
        call_for_a_taxi();
        printf("Chiamata al taxi da terminale"); 
    }else if (signal == SIGINT) {
        raise(SIGQUIT);
    }else{
        printf("Il segnale %d non è presente nel handler", signal);
    }
    unlock_signals(my_mask);
}
