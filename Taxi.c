#include "Shared.h"

//array delle SO_TOP_CELLS per ritornare le celle più attraversate
top_cells_array *top_cells;

//Parametri passati da args
int X;
int Y;
int my_message_queue; 
int my_sem_set = -1;
int sem_cells = 0;

//mappe con valore e tempo di attesa passate in shd_mem da Master
map_utils *shd_mem_taxi;

int travelling = 0;
struct msgbuf my_msg_receiver;
struct timespec timeout;

//segnali
sigset_t  my_mask;
sigset_t signals;

//buffer coda di messaggi
struct msgbuf my_msg_receiver;

//accumulatori e variabili utili al ritorno delle stats
int completed_trips = 0;

//dichiarazione dei metodi
void local_map_setup();
void implemented_signals();
void handle_signal(int signal);
void look_for_passenger();
int receive_msg(int x, int y);
int coordinate_is_acceptable(int x, int y);
void start_a_trip(int x_destination, int y_destination);
void update_stats();

int main(int argc, char *argv[]){
    if(argc != 9){ 
        printf("Errore nel numero di parmetri passati a TAXI\n");
		exit(EXIT_FATAL);
    }
    
    //inizializzo il set di segnali
    sigfillset(&signals); 
    implemented_signals();

    X = atoi(argv[1]);
    Y = atoi(argv[2]);

    shd_mem_taxi = (map_utils *)shmat(atoi(argv[3]), NULL, 0);
    if(shd_mem_taxi == NULL){
        printf("Errore caricamento della memoria condivisa in SOURCE\n");
		exit(EXIT_FATAL);
    }

    local_map_setup();

    shd_mem_taxi_return = shmat(atoi(argv[4]), NULL, 0);
    if(shd_mem_taxi_return == NULL){
        printf("Errore caricamento della memoria condivisa in SOURCE\n");
		exit(EXIT_FATAL);
    }

    my_message_queue = atoi(argv[5]); 
    my_sem_set = atoi(argv[6]);
    sem_cells = atoi(argv[7]);

    timeout.tv_sec = atoi(argv[8]);

    sem_syncronization(my_sem_set);

    while (1){
        look_for_passenger(); 
    }
}

void local_map_setup(){
    int i, j, offset = 0;

    //allocata mappa locale
    local_map = (int **)malloc(SO_HEIGHT*sizeof(int *));
    if (local_map == NULL){
        allocation_error("Taxi", "local_map");
    }
    for (i = 0; i<SO_HEIGHT; i++){
        local_map[i] = malloc(SO_WIDTH*sizeof(int));
        if (local_map[i] == NULL){
            allocation_error("Taxi", "local_map");
        }
    }

    //alloco memoria per SO_TIMENSEC_MAP (mappa tempi di attesa)
    SO_TIMENSEC_MAP = (long int **)malloc(SO_HEIGHT*sizeof(long int *));
    if (local_map == NULL){
        allocation_error("Taxi", "local_map");
    }
    for (i = 0; i<SO_HEIGHT; i++){
        SO_TIMENSEC_MAP[i] = malloc(SO_WIDTH*sizeof(long int));
        if (local_map == NULL){
            allocation_error("Taxi", "local_map");
        }
    }

    for (i = 0; i < SO_HEIGHT; i++){
        for(j = 0; j < SO_WIDTH; j++){
            local_map[i][j] = (shd_mem_taxi+offset)->cell_value;
            SO_TIMENSEC_MAP[i][j] = (shd_mem_taxi+offset)->cell_timensec_value;
            offset++;
        }
    }
    shmdt(shd_mem_taxi);
}


void implemented_signals(){
    struct sigaction fail;

    bzero(&fail, sizeof(fail));

    sigaddset(&my_mask, SIGINT);
    sigaddset(&my_mask,SIGQUIT);

    //Sono segnali definiti dall'utente , quindi non vengono attivati ​​da alcuna azione particolare
    fail.sa_mask = my_mask;
    fail.sa_handler = handle_signal;

    sigaction(SIGINT, &fail, NULL);
    sigaction(SIGQUIT, &fail,NULL);
}

void handle_signal(int signal){
    lock_signals(my_mask);

    if (signal == SIGINT){
        raise(SIGQUIT);
        free_matrix(1, local_map, NULL, NULL, SO_TIMENSEC_MAP, 0, NULL);
    }else if(signal == SIGQUIT){
        update_stats();
        free_matrix(1, local_map, NULL, NULL, SO_TIMENSEC_MAP, 0, NULL);
        if (travelling){
            exit(TAXI_NOT_COMPLETED_STATUS);
        }else{
            exit(EXIT_SUCCESS);
        }
    }else{
        printf("Il segnale %d non è presente nel handler", signal);
    }
    unlock_signals(my_mask);
}

void look_for_passenger(){
    int found = 0;
    int random_choice;
    srand(getpid());

    while(!found){          //se non lo trovo mi guardo intorno. 8 caselle circostanti
        if (!receive_msg(X, Y)){
        if (coordinate_is_acceptable(X - 1, Y) && receive_msg(X - 1, Y)){
            start_a_trip(X - 1, Y);
            found = 1;
        } else if (coordinate_is_acceptable(X, Y - 1) && receive_msg(X, Y - 1)){
            start_a_trip(X, Y - 1);
            found = 1;
        } else if (coordinate_is_acceptable(X + 1, Y) && receive_msg(X + 1, Y)){
            start_a_trip(X + 1, Y);
            found = 1;
        } else if (coordinate_is_acceptable(X, Y + 1) && receive_msg(X, Y + 1)){
            start_a_trip(X, Y + 1);
            found = 1;
        }

        //se non ho trovato nulla nei dintorni, mi muovo casualmente
        if (!found){
            random_choice = rand() % 4;
        }
        switch(random_choice){
           case 1:
                X = X - 1;
                break;
            case 2:
                Y = Y - 1;
                break;
            case 3:
                X = X +1;
                break;
            case 0:
                Y = Y + 1;
                break;
            }
        } else {
            start_a_trip(atoi(my_msg_receiver.mtext) / SO_WIDTH, atoi(my_msg_receiver.mtext) % SO_WIDTH);
            found = 1;
        }            
    }
}

int coordinate_is_acceptable(int y, int x){
    if (x >= 0 && x < SO_WIDTH && y >= 0 && y < SO_HEIGHT){
        if (local_map[x][y] != 0){
            return 1;
        }
    }
    return 0;
}

int receive_msg(int x, int y){
    int num_bytes, msg_ready = 0;

    lock_signals(signals);
    sem_reserve_msgqueue(my_sem_set);

    num_bytes = msgrcv(my_message_queue, &my_msg_receiver, MSG_LEN, (y * SO_WIDTH ) + x + 1, IPC_NOWAIT);

    sem_release_msgqueue(my_sem_set);
    unlock_signals(signals);
    

    if (num_bytes > 0){
        msg_ready = 1;
    }
    return msg_ready;
}

void start_a_trip(int x_destination, int y_destination){
    int arrived = 0, lock_res = -1, op_flag = -1, trip_time = 0, crossed_cells = 0;
;
    struct timespec timer;
    struct sembuf sops_cells[2];

    timer.tv_sec = 0;
    
    sops_cells[0].sem_op = 1; 
    sops_cells[0].sem_flg = 0;

    sops_cells[1].sem_op = -1; 
    sops_cells[1].sem_flg = 0;

    while (!arrived) {
        sops_cells[0].sem_num = (X * SO_WIDTH) + Y;

        if (X == x_destination && Y == y_destination){
            arrived = 1;
        } else if (X < x_destination){
            if (coordinate_is_acceptable(X++, Y)){
                X++;
                op_flag = 0;
            }
        } else if (X > x_destination){
            if (coordinate_is_acceptable(X--, Y)){
                X--;
                op_flag = 1;
            }
        } else if (Y < y_destination){
            if (coordinate_is_acceptable(X, Y++)){
                Y++;
                op_flag = 2;
            }
        } else if (Y > y_destination){
            if (coordinate_is_acceptable(X, Y--)){
                Y--;
                op_flag = 3;
            }
        }

        if (!arrived) {
            sops_cells[1].sem_num = (X * SO_WIDTH) + Y;
            lock_res = semtimedop(sem_cells, sops_cells, 2, &timeout);
            if (lock_res == -1){
                if(errno != EINTR && errno != EAGAIN){
                    test_error();
                } else if (errno == EAGAIN) {
                    sops_cells[0].sem_num = (X * SO_WIDTH) + Y; sops_cells[0].sem_op = 1;
                    while(semop(sem_cells, sops_cells, 1) == -1 && errno == EINTR);
                    update_stats();
                    //free_mat(1, local_map, NULL, NULL, SO_TIMENSEC_MAP, 0, NULL);
                    exit(TAXI_ABORTED_STATUS);
                } else {
                    switch (op_flag){
                        case 0:
                            X--;
                            break;
                        case 1:
                            X++;
                            break;
                        case 2:
                            Y--;
                            break;
                        case 3:
                            Y++;
                            break;
                    }
                }
            } else {
                crossed_cells++;
                trip_time = trip_time + SO_TIMENSEC_MAP[X][Y];
                timer.tv_nsec = SO_TIMENSEC_MAP[X][Y];
                nanosleep(&timer, NULL);
            }
        }
    }
    //incrementa contatore completed_trips e longest time trip ----> STATS
    completed_trips++;
    if (crossed_cells > shd_mem_taxi_return->longest_trip_cells) {
        taxi_return_reserve_sem(my_sem_set);
        shd_mem_taxi_return->longest_trip_cells = crossed_cells;
        shd_mem_taxi_return->pid_longest_trip_cells = getpid();
        taxi_return_release_sem(my_sem_set);
    }
    if (trip_time > shd_mem_taxi_return->longest_trip_time) {
        taxi_return_reserve_sem(my_sem_set);
        shd_mem_taxi_return->longest_trip_time = trip_time;
        shd_mem_taxi_return->pid_longest_trip_time = getpid();
        taxi_return_release_sem(my_sem_set);
    }
}

void update_stats(){
    taxi_return_reserve_sem(my_sem_set);
    shd_mem_taxi_return->total_trips_completed = completed_trips;
    if (completed_trips > shd_mem_taxi_return->most_client){
        shd_mem_taxi_return->most_client = completed_trips;
        shd_mem_taxi_return->pid_most_client = getpid();
    }

    taxi_return_release_sem(my_sem_set);
}