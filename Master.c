#include "Shared.h" 

#define PARAM_FILE_PATH "Parameters.txt"

//Macro Clean che usiamo in test_error() per pulire tutto prima di uscire
#undef CLEAN
#define CLEAN clean(local_map, SO_CAP_MAP, SO_SOURCES_PID_MAP, SO_TIMENSEC_MAP, so_taxis_pid_free, SO_TAXI_PID_ARRAY, shd_mem_id_source, shd_mem_source, shd_mem_id_taxi, shd_mem_taxi, my_message_queue, my_sem_set, sem_cells);

//dichiarazione dei metodi
void setup();
void read_parameters();
void test_parameters();
void sem_setup();           //da controllare il numero di semafori
void local_map_setup();
void shd_mem_setup_source(int size);
void shd_mem_setup_taxi(int size);
void shd_mem_setup_taxi_return(int size);   //controlla coerenza
int  holes_placement_check(int i, int j);
int check_cell_2be_inaccessible(int x, int y);
int first_source_available();
void children_PID_kill();
void execution();
void signal_actions();
void signal_sigusr1_actions();
void handle_signal(int sig);
void message_queue_setup(); // code di messagi 
void sources_exec();
void taxis_exec();
void aborted_taxi_reboot(pid_t taxi_pid);
void print_stat();
void print_local_map(int isTerminal);

//mappe
int **SO_CAP_MAP;           //contiene il valore massimo di taxi possibili per cella
pid_t **SO_SOURCES_PID_MAP; //mappa dei PID dei processi SOURCE

//array dei PID di TAXI
pid_t *SO_TAXI_PID_ARRAY;

//parametri letti da file
int SO_HOLES = -1;
int SO_TOP_CELLS = -1;
int SO_SOURCES = -1;
int SO_CAP_MIN = -1;
int SO_CAP_MAX = -1;
int SO_TAXI = -1;
long int SO_TIMENSEC_MIN = -1;
long int SO_TIMENSEC_MAX = -1;
int SO_TIMEOUT = -1;
int SO_DURATION = -1;

//shared memory
int shd_mem_id_source = 0;
int shd_mem_id_taxi = 0;
int shd_mem_id_taxi_return = 0;
int *shd_mem_source = 0;
map_utils *shd_mem_taxi;
taxi_return_stats *shd_mem_taxi_return;


//id della coda di messagi;
int my_message_queue = 0;
struct msqid_ds buf; // da vedere --> utilizzata per stampa della mappa

//Maschera per i segnali
sigset_t my_mask;

//Union per i semafori
union semun {
    int val;    // Value for SETVAL 
    struct semid_ds *buf;    // Buffer for IPC_STAT, IPC_SET 
    unsigned short  *array;  // Array for GETALL, SETALL 
    struct seminfo  *__buf;  // Buffer for IPC_INFO
};

union semun arg;

//Set dei semafori e celle
int my_sem_set = -1; 
int sem_cells = 0; 

//Variabili varie
int duration = 0; //contiene i secondi da cui il programma è in esecuzione
int incompleted_trips = 0;
int aborted_trips = 0;
int child_process_call; //figlio che prende in carico la richiesta da terminale
int status_sigquit = 0; //se è già stata inviata una sigquit (=1), altrimenti 0
int so_taxis_pid_free = 1;


int main(int argc, char *argv[]){
    int child_pid, status;
    //stampo PID Master per utilizzo di kill -SIGUSR1 "Master-PID"
    printf("Per comand     da vedere\n");
    dprintf(1, "\nMASTER PID: %d\n", getpid());

    setup();

    execution();

    clean(local_map, SO_CAP_MAP, SO_SOURCES_PID_MAP, SO_TIMENSEC_MAP, so_taxis_pid_free, SO_TAXI_PID_ARRAY, shd_mem_id_source, shd_mem_source, shd_mem_id_taxi, shd_mem_taxi, my_message_queue, my_sem_set, sem_cells);

    return 0;
}

void setup(){
    int i, j;
    
    read_parameters();
    test_parameters();

    //Alloco la memoria per la mappa
    local_map = (int **)malloc(SO_HEIGHT * sizeof(int *));  
    if (local_map == NULL){
        allocation_error("Master", "local_map");
    }  
    for (i = 0; i < SO_HEIGHT; i++){
        local_map[i] = malloc(SO_WIDTH * sizeof(int));
        if (local_map[i] == NULL) {
            allocation_error("Master", "local_map");
        }   
    }

    //Alloco la memoria per la matrice che conterrà la capacità massima di taxi di ciascuna cella
    SO_CAP_MAP = (int **)malloc(SO_HEIGHT * sizeof(int *)); 
    if (SO_CAP_MAP == NULL) {   
        allocation_error("Master", "SO_CAP_MAP");
    }
    for (i = 0; i < SO_HEIGHT; i++){
        SO_CAP_MAP[i] = malloc(SO_WIDTH * sizeof(int));
        if (SO_CAP_MAP[i] == NULL){
            allocation_error("Master", "SO_CAP_MAP");
        } 
    }
    //Caricamento della capacità massima di taxi per ciascuna cella
    for(i = 0; i < SO_HEIGHT; i++){
        for(j = 0; j < SO_WIDTH; j++){
            SO_CAP_MAP[i][j] = rand() % (SO_CAP_MAX + 1 - SO_CAP_MIN) + SO_CAP_MIN;
        }
    }

    //Allocazione della mappa dei tempi di attesa massimi per cella
    SO_TIMENSEC_MAP = (long int **)malloc(SO_HEIGHT * sizeof(long int *));
    if (SO_TIMENSEC_MAP == NULL){
        allocation_error("Master", "SO_TIMENSEC_MAP");
    }
    for (i = 0; i < SO_HEIGHT; i++){
        SO_TIMENSEC_MAP[i] = malloc(SO_WIDTH * sizeof(long int));
        if (SO_TIMENSEC_MAP[i] == NULL){
            allocation_error("Master", "SO_TIMENSEC_MAP");
        }    
    }
    //Caricamento dei tempi di attesa massimi per ciascuna cella
    for (i = 0; i < SO_HEIGHT; i++){
        for (j = 0; j < SO_WIDTH; j++){
            SO_TIMENSEC_MAP[i][j] = rand() % (SO_TIMENSEC_MAX + 1 - SO_TIMENSEC_MIN) + SO_TIMENSEC_MIN;
        }
    }

    //Allocazione della memoria della mappa dei PID delle sorgenti
    SO_SOURCES_PID_MAP = (pid_t **)malloc(SO_HEIGHT * sizeof(pid_t *));
    if (SO_SOURCES_PID_MAP == NULL){
        allocation_error("Master", "SO_SOURCES_PID_MAP");
    }
    for (i = 0; i < SO_HEIGHT; i++){
        SO_SOURCES_PID_MAP[i] = malloc(SO_WIDTH * sizeof(pid_t));
        if (SO_SOURCES_PID_MAP[i] == NULL){
            allocation_error("Master", "SO_SOURCES_PID_MAP");
        } 
    }
    //Inizializzazione a 0 dei PID di ogni cella delle sorgenti
    for (i = 0; i < SO_HEIGHT; i++){
        for (j = 0; j <SO_WIDTH; j++){
            SO_SOURCES_PID_MAP[i][j] = 0;
        }
    }
    SO_TAXI_PID_ARRAY = (pid_t *)malloc(SO_TAXI * sizeof(pid_t));
    so_taxis_pid_free = 1;
    
    //Generazione della mappa di gioco
    local_map_setup();

    //Generazione della memorie condivise: SOURCE, TAXI e TAXI_RETURN per le statistiche
    shd_mem_setup_source((SO_WIDTH*SO_HEIGHT) * sizeof(int));
    shd_mem_setup_taxi((SO_WIDTH*SO_HEIGHT) * sizeof(map_utils));
    shd_mem_setup_taxi_return(sizeof(taxi_return_stats));

    //Generazione della coda di messaggi per le comunicazioni tra processi
    message_queue_setup();
    
    //Generazione dei semafori
    sem_setup();

    //Generazione processi SOURCES
    sources_exec();

    //Generazione processi TAXI
    taxis_exec();


}

void read_parameters(){
    FILE *file;
    file = fopen(PARAM_FILE_PATH , "r");
    char value[20];
    char parameter[200];
    if((file) == NULL){
        printf("Errore durante l'apertura del file contenente i parametri\n");
		exit(EXIT_FAILURE);
    }

    while(fscanf(file, "%s = %s\n", parameter, value) != EOF){
        if (strcmp(parameter, "SO_HOLES") == 0){
            SO_HOLES = atoi(value);
        } else if (strcmp(parameter, "SO_TOP_CELLS") == 0){
            SO_TOP_CELLS = atoi(value);
        } else if (strcmp(parameter, "SO_SOURCES") == 0){
            SO_SOURCES = atoi(value);
        } else if (strcmp(parameter, "SO_CAP_MIN") == 0){
            SO_CAP_MIN = atoi(value);
        } else if (strcmp(parameter, "SO_CAP_MAX") == 0){
            SO_CAP_MAX = atoi(value);
        } else if (strcmp(parameter, "SO_TAXI") == 0){
            SO_TAXI = atoi(value);
        } else if (strcmp(parameter, "SO_TIMENSEC_MIN") == 0){
            SO_TIMENSEC_MIN = atoi(value);
        } else if (strcmp(parameter, "SO_TIMENSEC_MAX") == 0){
            SO_TIMENSEC_MAX = atoi(value);
        } else if (strcmp(parameter, "SO_TIMEOUT") == 0){
            SO_TIMEOUT = atoi(value);
        } else if (strcmp(parameter, "SO_DURATION") == 0){
            SO_DURATION = atoi(value);
        } else{
            printf("Errore nella lettura dei parametri da file: correggi i parametri\n");
		    exit(EXIT_FAILURE);
        }
    }
}

void test_parameters(){
    if(SO_HOLES < 0){
        printf("Errore nel caricamento del parametro SO_HOLES: parametro negativo\n");
    }
    if(SO_TOP_CELLS < 0){
        printf("Errore nel caricamento del parametro SO_TOP_CELLS: parametro negativo\n");
    }
    if(SO_SOURCES < 0){
        printf("Errore nel caricamento del parametro SO_SOURCES: parametro negativo\n");
    }
    if((SO_SOURCES + SO_HOLES) > (SO_WIDTH * SO_HEIGHT)){
        printf("Errore nei parametri della mappa: troppi SO_SOURCES e SO_HOLES per la grandezza della mappa data");
    }
    if(SO_CAP_MIN <= 0){
        printf("Errore nel caricamento del parametro SO_CAP_MIN: il parametro deve essere maggiore di zero\n");
    }
    if(SO_CAP_MAX <= 0){
        printf("Errore nel caricamento del parametro SO_CAP_MAX: il parametro deve essere maggiore di zero\n");
    }
    if(SO_CAP_MAX < SO_CAP_MIN){
        printf("Errore: differenza tra capacità massima e capacità minima non valida\n");
    }
    if(SO_TAXI < 0){
        printf("Errore nel caricamento del parametro SO_TAXI: parametro negativo\n");
    }
    if(SO_TAXI > (SO_CAP_MIN*SO_HEIGHT*SO_WIDTH)){
        printf("Errore: SO_TAXI supera la dimensione massima consentita\n");
    }
    if(SO_TIMENSEC_MIN <= 0){
        printf("Errore nel caricamento del parametro SO_TIMENSEC_MIN: il parametro deve essere maggiore di zero\n");
    }
    if(SO_TIMENSEC_MAX <= 0){
        printf("Errore nel caricamento del parametro SO_TIMENSEC_MAX: il parametro deve essere maggiore di zero\n");
    }
    if(SO_TIMENSEC_MAX < SO_TIMENSEC_MIN){
        printf("Errore: differenza tra attesa massima e minima non valida\n");
    }
    if(SO_TIMEOUT <= 0){
        printf("Errore nel caricamento del parametro SO_TIMEOUT: il parametro deve essere maggiore di zero\n");
    }
    if(SO_DURATION <= 0){
        printf("Errore nel caricamento del parametro SO_DURATION: il parametro deve essere maggiore di zero\n");
    }
}

void shd_mem_setup_source(int size){
    int offset = 0;
    shd_mem_id_source = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | 0600);
    test_error();
    
    shd_mem_source = shmat(shd_mem_id_source, NULL, 0);
    test_error();

    for(int i = 0; i < SO_HEIGHT; i++){
        for(int j = 0; j < SO_WIDTH; j++){
            shd_mem_source[offset] = local_map[i][j];
            offset++;
        }
    }

    shmctl(shd_mem_id_source, IPC_RMID, NULL);
}

void shd_mem_setup_taxi(int size){
    int offset = 0;
    shd_mem_id_taxi = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | 0600);
    test_error();
    
    shd_mem_taxi = shmat(shd_mem_id_taxi, NULL, 0);
    test_error();

    for(int i = 0; i < SO_HEIGHT; i++){
        for(int j = 0; j < SO_WIDTH; j++){
            (shd_mem_taxi+offset)->cell_value = local_map[i][j];
            (shd_mem_taxi+offset)->cell_timensec_value = SO_TIMENSEC_MAP[i][j];
           offset++;
        }
    }

    shmctl(shd_mem_id_taxi, IPC_RMID, NULL);
}

void shd_mem_setup_taxi_return(int size){
    shd_mem_id_taxi_return = shmget(IPC_PRIVATE, size, IPC_CREAT | IPC_EXCL | 0600);
    test_error();

    shd_mem_taxi_return = shmat(shd_mem_id_taxi_return, NULL, 0);
    test_error();

    //inizializzo a zero i valori della struct
    shd_mem_taxi_return->total_trips_completed = 0;
    shd_mem_taxi_return->longest_trip_cells = 0;
    shd_mem_taxi_return->longest_trip_time = 0;
    shd_mem_taxi_return->most_client = 0;

    for(int i = 0; i < SO_HEIGHT; i++){
        for(int j = 0; j < SO_WIDTH; j++){
            shd_mem_taxi_return->so_top_cells_map[i][j] = 0;
        }
    }

    shmctl(shd_mem_id_taxi_return, IPC_RMID, NULL);
}

void local_map_setup(){
    int i, j;
    int holes_placed = 0;           //holes_n++ da fare solo se piazzo
    int hole_not_placeble = 0;           //se non piazzo, hole_not_placeble++
    int source_placed = 0;
    int placed = 0;
    srand(getpid());

    //Generazione della mappa di gioco. Ogni cella viene inizializzata a 1, il valore "vergine"
    for (i = 0; i < SO_HEIGHT; i++){
        for (j = 0; j < SO_WIDTH; j++){
            local_map[i][j] = 1; 
        } 
    }

    //Generazione degli SO_HOLES
    while(holes_placed < SO_HOLES && hole_not_placeble < SO_HEIGHT * SO_WIDTH){
        i = rand() % SO_HEIGHT; 
        j = rand() % SO_WIDTH;

        if(local_map[i][j] != 0){         
            if(holes_placement_check(i, j)){
                local_map[i][j] = 0;
                holes_placed++;
            } else {
                hole_not_placeble++;
            }
        } 
    }
    if (holes_placed < SO_HOLES){
        printf("SO_HOLES: Parametro non valido; non è possibile piazzare tutti gli HOLES");
        free_matrix(0, local_map, SO_CAP_MAP, SO_SOURCES_PID_MAP, SO_TIMENSEC_MAP, so_taxis_pid_free, SO_TAXI_PID_ARRAY);
        exit(EXIT_FAILURE);
    }

    //Generazione degli SO_SOURCES    
    while(source_placed < SO_SOURCES){
        while(!placed){
            i = rand() % SO_HEIGHT;
            j = rand() % SO_WIDTH;
            if(local_map[i][j] == 1){
                local_map[i][j] = 2;
                placed = 1;
            }
        }
        placed = 0;
        source_placed++;
    }
}

int holes_placement_check(int x, int y){
    /*devo controllare le 8 celle confinanti per evitare barriere

    if (i > 0 && j > 0 && local_map[i-1][j-1] == 0){
        return 0;
    }

    if (j > 0 && local_map[i][j-1] == 0){
        return 0;
    }

    if (i < SO_HEIGHT && j > 0 && local_map[i+1][j-1] == 0){
        return 0;
    }

    if (i > 0 && local_map[i-1][j] == 0){
        return 0;
    }

    if (i > 0 && j < SO_WIDTH && local_map[i-1][j+1]== 0){
        return 0;
    }

    if (j < SO_WIDTH && local_map[i][j+1] == 0){
        return 0;
    }

    if (i < SO_HEIGHT && j < SO_WIDTH && local_map[i+1][j+1] == 0){
        return 0;
    }

    if (i < SO_HEIGHT && local_map[i+1][j] == 0){
        return 0;
    }
    
    return 1;*/

    int esito = 1; /* assumo che sia possibile rendere la cella inaccessibile */
    /* devo controllare che tutte le celle X siano (!= 0), dove 0 => cella inaccessibile
        X X X
        X o X
        X X X
    */

    if((x-1) >= 0){ /* check sull'INTERA RIGA SOPRA alla cella considerata, se non fuori dai bordi */
        if((y-1) >= 0){
            if(local_map[x-1][y-1] == 0) /* è già inaccessibile => CELLA NON UTILE */
                esito = 0;
        }

        if(local_map[x-1][y] == 0)
            esito = 0;

        if((y+1) < SO_WIDTH){
            if(local_map[x-1][y+1] == 0)
                esito = 0;
        }
    }

    if((esito == 1) && ((x+1) < SO_HEIGHT)){ /* check sull'INTERA RIGA SOTTO, SE la RIGA SOPRA era OK(esito rimasto 1)*/
        if((y-1) >= 0){
            if(local_map[x+1][y-1] == 0)
                esito = 0;
        }

        if(local_map[x+1][y] == 0)
            esito = 0;

        if((y+1) < SO_WIDTH){
            if(local_map[x+1][y+1] == 0)
                esito = 0;
        }
    }

    if((esito == 1) && ((y-1) >= 0)){ /* CELLA a SINISTRA */
        if(local_map[x][y-1] == 0)
            esito = 0;
    }

    if((esito == 1) && ((y+1) < SO_WIDTH)){ /* CELLA a DESTRA */
        if(local_map[x][y+1] == 0)
            esito = 0;
    } 

    return esito;
}

void sem_setup(){ 
    
    //creazione semaforo message queue
    my_sem_set = semget(IPC_PRIVATE, 3, IPC_CREAT|IPC_EXCL| S_IRUSR | S_IWUSR);
    test_error();

    arg.val = 1;         //sync dell'attesa iniziale di processi taxi e source
    semctl(my_sem_set, 0, SETVAL, arg);
    test_error();

    //Settiamo i valori dei semafori
    arg.val = SO_SOURCES + SO_TAXI;                            //accesso esclusivo alla coda di messaggi
    semctl(my_sem_set, 1, SETVAL, arg);
    test_error();

    arg.val = 1;                            //mutex di accesso alla struct dei return values. da controllare la necessità
    semctl(my_sem_set, 2, SETVAL, arg);
    test_error();
    
    //creazione semafori delle celle
    sem_cells = semget(IPC_PRIVATE, SO_HEIGHT*SO_WIDTH, IPC_CREAT|IPC_EXCL| S_IRUSR | S_IWUSR);
    test_error();

    for(int i = 0; i < SO_HEIGHT; i++){    
        for(int j = 0; j < SO_WIDTH; j++){
            arg.val = SO_CAP_MAP[i][j];
            semctl(sem_cells, i * SO_WIDTH + j, SETVAL, arg);
            test_error();
        }
    }
}

void message_queue_setup(){
    my_message_queue = msgget(IPC_PRIVATE, IPC_CREAT | IPC_EXCL | 0666);     //0666 = 400+200+40+20+4+2
    test_error();                                                      //User-read | User-write | Group-read |Group write | Other-read | Other_write
}

void sources_exec(){
    char *args_source[8];   //array degli argomenti da passare
    for (int x = 0; x < SO_HEIGHT; x++){
        for(int y = 0; y < SO_WIDTH; y++){
            if(local_map[x][y] == 2){
                switch(SO_SOURCES_PID_MAP[x][y] = fork()){
                    case -1: //Caso errore fork
                        dprintf(1,"Error Fork #%03d: %s\n", errno, strerror(errno));
                        exit(EXIT_FAILURE);
                        break;
                    case 0: //Caso processo figlio
                        args_source[0] = malloc(7 * sizeof(char));
                        strcpy(args_source[0], "Source");
                        args_source[1] = malloc(sizeof((char)x));
                        strcpy(args_source[1], (char)x);
                        args_source[2] = malloc(sizeof((char)y));
                        strcpy(args_source[2], y);
                        args_source[3] = malloc(sizeof((char)shd_mem_id_source));  //memoria condivisa
                        strcpy(args_source[3], shd_mem_id_source);
                        args_source[4] = malloc(sizeof((char)my_message_queue));  //coda di messaggi
                        strcpy(args_source[4], my_message_queue);
                        args_source[5] = malloc(sizeof((char)my_sem_set));  //semafori
                        strcpy(args_source[5], my_sem_set);
                        args_source[6] = NULL; 

                        execve("./Source", args_source, NULL);
                        //Stampa di errore
                        dprintf(1,"Error Execve #%03d: %s\n", errno, strerror(errno));
                        exit(EXIT_FAILURE);
                        break;
                    default: //Caso processo padre
                        break;
                }
            }
        }
    }
}

//statistiche di fine esecuzione
void print_stat(){ 
    printf("Durata totale di esecuzione: %d secondi\n", duration);
    void taxi_return_reserve_sem(int my_shdmem_sem);

    printf("Numero totali di viaggi eseguiti con successo: %d\n", shd_mem_taxi_return->total_trips_completed);
    printf("Numero totali di viaggi inevasi: %d\n", incompleted_trips);
    printf("Numero totali di viaggi abortiti: %d\n", aborted_trips);
    printf("Il taxi con PID %d ha percorso più celle di tutti, per un totale di %d celle\n", shd_mem_taxi_return->pid_longest_trip_cells, shd_mem_taxi_return->longest_trip_cells);
    printf("Il taxi con PID %d ha impiegato un tempo maggiore di tutti gli altri, per un totale di %d secondi\n", shd_mem_taxi_return->pid_longest_trip_time, shd_mem_taxi_return->longest_trip_time);
    printf("Il taxi con PID %d ha raccolto il numero maggiore di clienti, per un totale di %d richieste\n", shd_mem_taxi_return->pid_most_client, shd_mem_taxi_return->most_client);
    
    //get_top_cells();
    void taxi_return_release_sem(int my_shdmem_sem);
}

void print_local_map(int isTerminal){
    int i, k;
    double sec;

    if(isTerminal){
        print_stat();
    }

    /* cicla per tutti gli elementi della mappa */
    for(i = 0; i < SO_HEIGHT; i++){
        for(k = 0; k < SO_WIDTH; k++){
            switch (local_map[i][k])
            {
            /* CASO 0: cella invalida, quadratino nero */
            case 0:
                printf( BGRED KBLACK "|_|" RESET);
                break;
            /* CASO 1: cella di passaggio valida, non sorgente, quadratino bianco */
            case 1:
                if(SO_CAP_MAP[i][k] - semctl(sem_cells, (i*SO_WIDTH)+k ,GETVAL) > 0)
                        printf("|%d|" RESET,SO_CAP_MAP[i][k] - semctl(sem_cells, (i*SO_WIDTH)+k ,GETVAL));
                else
                    printf("|_|" RESET);
                break;
            /* CASO 2: cella sorgente, quadratino striato se stiamo stampando l'ultima mappa, altrimenti stampo una cella generica bianca*/
            case 2:
                if(isTerminal)
                    if(SO_CAP_MAP[i][k] - semctl(sem_cells, (i*SO_WIDTH)+k ,GETVAL) > 0)
                        printf(BGGREEN KBLACK "|%d|" RESET, SO_CAP_MAP[i][k] - semctl(sem_cells, (i*SO_WIDTH)+k ,GETVAL));
                    else
                        printf(BGGREEN KBLACK "|_|" RESET);
                else
                    if(SO_CAP_MAP[i][k] - semctl(sem_cells, (i*SO_WIDTH)+k ,GETVAL) > 0)
                        printf("|%d|", SO_CAP_MAP[i][k] - semctl(sem_cells, (i*SO_WIDTH)+k ,GETVAL));
                    else
                        printf("|_|");
                break;
            /* DEFAULT: errore o TOP_CELL se stiamo stampando l'ultima mappa, quadratino doppio */
            default:
                if(isTerminal)
                    if(SO_CAP_MAP[i][k] - semctl(sem_cells, (i*SO_WIDTH)+k ,GETVAL) > 0)
                        printf(BGMAGENTA KBLACK "|%d|" RESET,SO_CAP_MAP[i][k] - semctl(sem_cells, (i*SO_WIDTH)+k ,GETVAL));
                    else
                        printf(BGMAGENTA KBLACK "|_|" RESET);
                else
                    printf("E");
                break;
            }
        }
        /* nuova linea dopo aver finito di stampare le celle della linea i della matrice */
        printf("|\n");
    }
}

void taxis_exec(){

    struct sembuf sops_cells; 

    srand(time(NULL));
    int x, y, try;
    char *args_taxis[9];

    for (int i = 0; i < SO_TAXI; i++){
        try = 1;
        while(try){
            x = rand() % SO_HEIGHT;
            y = rand() % SO_WIDTH;
            if (local_map[x][y] != 0 && semctl(sem_cells, (x * SO_WIDTH) + y, GETVAL) > 0){
                sops_cells.sem_num = (x * SO_WIDTH ) + y; 
                sops_cells.sem_op = -1; 
                sops_cells.sem_flg = 0;
                if (semop(sem_cells, &sops_cells, 1) != -1){
                    try = 0;
                } else {
                    test_error();
                }
            }
        }

        switch(SO_TAXI_PID_ARRAY[i] = fork()){
            case -1: //caso errore fork
                dprintf(1, "\nFORK Error #%03d: %s\n", errno, strerror(errno));
                exit(EXIT_FAILURE);
                break;
            case 0: //Caso processo figlio

                args_taxis[0] = malloc(5 * sizeof(char));
                strcpy(args_taxis[0], "Taxi");
                args_taxis[1] = malloc(sizeof((char)x));
                strcpy(args_taxis[1], x);
                args_taxis[2] = malloc(sizeof((char)y));
                strcpy(args_taxis[2], y);
                args_taxis[3] = malloc(sizeof((char)shd_mem_taxi));
                strcpy(args_taxis[3], shd_mem_taxi);
                args_taxis[4] = malloc(sizeof((char)shd_mem_taxi_return));
                strcpy(args_taxis[4], shd_mem_taxi_return);
                args_taxis[5] = malloc(sizeof((char)my_message_queue));
                strcpy(args_taxis[5], my_message_queue);
                args_taxis[6] = malloc(sizeof((char)my_sem_set));
                strcpy(args_taxis[6], my_sem_set);
                args_taxis[7] = malloc(sizeof((char)sem_cells));
                strcpy(args_taxis[7], sem_cells);
                args_taxis[8] = malloc(sizeof((char)SO_TIMEOUT));
                strcpy(args_taxis[8], SO_TIMEOUT);
                args_taxis[9] = NULL;

                execve("./Taxi", args_taxis, NULL);
                //Stampa di errore
                dprintf(1,"Error Execve #%03d: %s\n", errno, strerror(errno));
                exit(EXIT_FAILURE);
                break;
            default: //Caso processo padre
                break;
        }
    }
}

void aborted_taxi_reboot(pid_t taxi_pid){
    struct sembuf sops_cells;
    
    srand(time(NULL));
    int try = 1, x, y, i = 0;
    char *args_taxis[9];

     while(try){
        x = rand() % SO_HEIGHT;
        y = rand() % SO_WIDTH;
        if (local_map[x][y] != 0 && semctl(sem_cells, (x * SO_WIDTH) + y, GETVAL) > 0){
            sops_cells.sem_num = (x * SO_WIDTH ) + y; 
            sops_cells.sem_op = -1; 
            sops_cells.sem_flg = 0;
            if (semop(sem_cells, &sops_cells, 1) != -1){
                try = 0;
            } else {
                test_error();
            }
        }
    }

    while(i < SO_TAXI && try == 0){
        if(taxi_pid ==  SO_TAXI_PID_ARRAY[i]){
            try = 1;
        }
        i++;
    }

    switch(SO_TAXI_PID_ARRAY[i] = fork()){
        case -1: //caso errore fork
            dprintf(1, "\nFORK Error #%03d: %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
            break;
        case 0: //Caso processo figlio
            args_taxis[0] = malloc(5 * sizeof(char));
            strcpy(args_taxis[0], "Taxi");
            args_taxis[1] = malloc(sizeof((char)x));
            strcpy(args_taxis[1], x);
            args_taxis[2] = malloc(sizeof((char)y));
            strcpy(args_taxis[2], y);
            args_taxis[3] = malloc(sizeof((char)shd_mem_taxi));
            strcpy(args_taxis[3], shd_mem_taxi);
            args_taxis[4] = malloc(sizeof((char)shd_mem_taxi_return));
            strcpy(args_taxis[4], shd_mem_taxi_return);
            args_taxis[5] = malloc(sizeof((char)my_message_queue));
            strcpy(args_taxis[5], my_message_queue);
            args_taxis[6] = malloc(sizeof((char)my_sem_set));
            strcpy(args_taxis[6], my_sem_set);
            args_taxis[7] = malloc(sizeof((char)sem_cells));
            strcpy(args_taxis[7], sem_cells);
            args_taxis[8] = malloc(sizeof((char)SO_TIMEOUT));
            strcpy(args_taxis[8], SO_TIMEOUT);
            args_taxis[9] = NULL;

            execve("./Taxi", args_taxis, NULL);
            //Stampa di errore
            dprintf(1,"Error Execve #%03d: %s\n", errno, strerror(errno));
            exit(EXIT_FAILURE);
            break;
        default: //Caso processo padre
            break;
    }
}



int check_cell_2be_inaccessible(int x, int y){ 
    int esito = 1; /* assumo che sia possibile rendere la cella inaccessibile */
    /* devo controllare che tutte le celle X siano (!= 0), dove 0 => cella inaccessibile
        X X X
        X o X
        X X X
    */

    if((x-1) >= 0){ /* check sull'INTERA RIGA SOPRA alla cella considerata, se non fuori dai bordi */
        if((y-1) >= 0){
            if(local_map[x-1][y-1] == 0) /* è già inaccessibile => CELLA NON UTILE */
                esito = 0;
        }

        if(local_map[x-1][y] == 0)
            esito = 0;

        if((y+1) < SO_WIDTH){
            if(local_map[x-1][y+1] == 0)
                esito = 0;
        }
    }

    if((esito == 1) && ((x+1) < SO_HEIGHT)){ /* check sull'INTERA RIGA SOTTO, SE la RIGA SOPRA era OK(esito rimasto 1)*/
        if((y-1) >= 0){
            if(local_map[x+1][y-1] == 0)
                esito = 0;
        }

        if(local_map[x+1][y] == 0)
            esito = 0;

        if((y+1) < SO_WIDTH){
            if(local_map[x+1][y+1] == 0)
                esito = 0;
        }
    }

    if((esito == 1) && ((y-1) >= 0)){ /* CELLA a SINISTRA */
        if(local_map[x][y-1] == 0)
            esito = 0;
    }

    if((esito == 1) && ((y+1) < SO_WIDTH)){ /* CELLA a DESTRA */
        if(local_map[x][y+1] == 0)
            esito = 0;
    } 

    return esito;
}

void execution(){
    struct sembuf sops_queue;
    int child_pid, status;

    signal_actions();

    dprintf(1, "\nLegenda della mappa: ");
    dprintf(1, "\n\tCelle INVALIDE:" BGRED "\t  " RESET "\n");
    dprintf(1, "\tCelle SORGENTI:" BGGREEN "\t  " RESET "\n");
    dprintf(1, "\tCelle TOP:" BGMAGENTA "\t  " RESET "\n\n");
    /* faccio un alarm di un secondo per far iniziare il ciclo di stampe ogni secondo */
    dprintf(1, "Attesa Sincronizzazione...\n");
    sops_queue.sem_num = 1;
    sops_queue.sem_op = 0;
    sops_queue.sem_flg = 0;

    semop(my_sem_set, &sops_queue, 1);

    signal_sigusr1_actions(); /* gestisco il segnale SIGUSR1 solo da questo punto in avanti (ovvero quando tutti i processi figli sono pronti all'esecuzione) cosi da poter poi commissionare la richiesta da terminale ad uno di loro */
    
    alarm(1);
    while ((child_pid = wait(&status)) > 0){
        if(WEXITSTATUS(status) == TAXI_NOT_COMPLETED_STATUS)
            incompleted_trips++;
        else if(WEXITSTATUS(status) == TAXI_ABORTED_STATUS){
            aborted_trips++;
            aborted_taxi_reboot(child_pid);
        }
    }

    printf("\n\n--------------------------------------\nFine: %d secondi di simulazione\n", duration);
    print_local_map(1);
}

void signal_actions(){
    struct sigaction restart, fail;

    bzero(&restart,sizeof(restart));
    bzero(&fail,sizeof(fail));

    //Imposta il gestore del segnale. Il gestore viene passato come puntatore
    restart.sa_handler = handle_signal;
    fail.sa_handler = handle_signal;

    restart.sa_flags = SA_RESTART; // Questo flag controlla cosa succede quando un segnale viene consegnato durante determinate primitive (come open, reado write) e il gestore del segnale ritorna normalmente. Esistono due alternative: la funzione di libreria può riprendere o può restituire un errore con codice di errore EINTR.
    fail.sa_flags = 0;

    //Uso della maschera per bloccare i segnali per bloccare i segnali durante l’esecuzione dell’handler
    sigaddset(&my_mask, SIGALRM);
    sigaddset(&my_mask, SIGINT);

    restart.sa_mask = my_mask;
    fail.sa_mask = my_mask;

    //!!set new handler to new (controllare se è corretto)(dovrebbe)!!
    sigaction(SIGALRM, &restart, NULL);
    test_error();
    sigaction(SIGINT, &fail, NULL);
    test_error();
}

void signal_sigusr1_actions(){
    struct sigaction user_signal;
    bzero(&user_signal, sizeof(user_signal));

    user_signal.sa_handler = handle_signal;
    user_signal.sa_flags = SA_RESTART; /*Sono segnali definiti dall'utente , quindi non vengono attivati ​​da alcuna azione particolare*/

    sigaddset(&my_mask, SIGUSR1);
    user_signal.sa_mask = my_mask;

    sigaction(SIGUSR1, &user_signal, NULL);
    test_error();
}

void handle_signal(int signal){
    lock_signals(my_mask);

    if (signal == SIGALRM){
        duration++;
        printf("Tempo: %d\n",duration);
        print_local_map(0);
        if(duration < SO_DURATION){
            alarm(1);
        }else{ 
            children_PID_kill();
        }
    } else if (signal == SIGINT){
        children_PID_kill();
        alarm(0);           //Non viene attivato nessun allarme e viene disabilitato l'eventuale allarme precedentemente impostato.
    } else if (signal == SIGUSR1){
        if((child_process_call = first_source_available())!=-1){         /* richiesta esplicita da terminale, fatta dall'utente. Handler associato solo dopo che tutti i processi sono sincronizzati */
            printf("Figlio scelto con PID : %d\n", child_process_call); //se si vuol mandare la richiesta direttamente al figlio <kill -SIGUSR PIDFIGLIO>. 
            kill(child_process_call, SIGUSR1);                          // Per vedere i PID dei figli ps -axjf | grep <PIDMASTER>*/
            test_error();
        }else{printf("Error SIGUSR1: la richiesta generata non è stata presa in carica da nessun processo figlio. Riprova\n"); }
            
    }else{
        printf("Il segnale %d non è presente nel handler", signal);
    }

    unlock_signals(my_mask);
}

int first_source_available(){
    int i = 0, j = 0;

    for (int i = 0; i < SO_WIDTH; i++){
        for (j = 0; j < SO_HEIGHT; j++){
            if(SO_SOURCES_PID_MAP[i][j] != 0){
                return SO_SOURCES_PID_MAP[i][j];
            }
        }
    }

    return -1;
}

void children_PID_kill(){
    status_sigquit = 1;

    for(int i=0; i<SO_TAXI; i++){
        if(SO_TAXI_PID_ARRAY[i] > 0)
            kill(SO_TAXI_PID_ARRAY[i], SIGQUIT);
    }

    for(int i=0; i<SO_HEIGHT; i++){
        for(int j=0; j<SO_WIDTH; j++){
            if(SO_SOURCES_PID_MAP[i][j] > 0)
                kill(SO_SOURCES_PID_MAP[i][j], SIGQUIT);
        }
    }
}
