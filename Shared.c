#include "Shared.h"

//Funzione di test per gli errori
void test_error(){
    if(errno){
        fprintf(stderr, "%s:%d: PID=%5d: Error #%03d: %s\n", __FILE__, __LINE__, getpid(), errno, strerror(errno));
        CLEAN;
        exit(EXIT_FAILURE);
    }
}

void allocation_error(char *file, char *structure){
	printf("Errore di allocazione in %s per la struttura %s", file, structure);
	exit(EXIT_FAILURE);
}

//Metodi per i segnali
void lock_signals(sigset_t my_mask){
	sigprocmask(SIG_BLOCK, &my_mask, NULL);
}

void unlock_signals(sigset_t my_mask){
	sigprocmask(SIG_UNBLOCK, &my_mask, NULL);
}

//Metodi per i semafori
void sem_reserve_msgqueue(int my_queue_sem){
    struct sembuf sops_queue;

	sops_queue.sem_num = 0;
	sops_queue.sem_op = -1;
	sops_queue.sem_flg = 0;
    semop(my_queue_sem, &sops_queue, 1);
}

void sem_release_msgqueue(int my_queue_sem){
    struct sembuf sops_queue;

    sops_queue.sem_num = 0;
	sops_queue.sem_op = 1;
	sops_queue.sem_flg = 0;
    semop(my_queue_sem, &sops_queue, 1);
}

void sem_syncronization(int my_sync_sem){
    struct sembuf sops_queue;
    int sem_value = semctl(my_sync_sem, 1, GETVAL);

    if(sem_value > 0){      //sem_value > 0 -> attendo tutti i processi source e taxi
        sops_queue.sem_num = 1;
        sops_queue.sem_op = -1;
        sops_queue.sem_flg = 0;
        semop(my_sync_sem, &sops_queue, 1);
        sops_queue.sem_num = 1;
        sops_queue.sem_op = 0;
        sops_queue.sem_flg = 0;
        semop(my_sync_sem, &sops_queue, 1);
    }
}

void taxi_return_reserve_sem(int my_shdmem_sem){
    struct sembuf sops_queue;

    sops_queue.sem_num = 2;
	sops_queue.sem_op = -1;
	sops_queue.sem_flg = 0;
    semop(my_shdmem_sem, &sops_queue, 1);
}

void taxi_return_release_sem(int my_shdmem_sem){
    struct sembuf sops_queue;
    
    sops_queue.sem_num = 2;
	sops_queue.sem_op = 1;
	sops_queue.sem_flg = 0;
    semop(my_shdmem_sem, &sops_queue, 1);
}
//memory_free method
void clean(int **local_map, int **SO_CAP_MAP, pid_t **SO_SOURCES_PID_MAP, long int **SO_TIMENSEC_MAP, int so_taxis_pid_free, pid_t *SO_TAXI_PID_ARRAY, int shd_mem_id_source, int *shd_mem_source, int shd_mem_id_taxi, map_utils *shd_mem_taxi, int message_queue, int sem_message_queue, int sem_cells){

    //libero matrici
    free_matrix(0, **local_map, **SO_CAP_MAP, **SO_SOURCES_PID_MAP, **SO_TIMENSEC_MAP, so_taxis_pid_free, *SO_TAXI_PID_ARRAY);
    
    //libero memoria condivisa 
        if(shd_mem_id_source){
            shmdt(shd_mem_source);
        }
        if(shd_mem_id_taxi){
            shmdt(shd_mem_taxi);
        }
    
    //libero la coda di messaggi e ottengo i messaggi rimasti non letti nella coda
    msgctl(message_queue, IPC_RMID, NULL);
 
    if(sem_message_queue != -1){
        semctl(sem_message_queue, 0, IPC_RMID);
        semctl(sem_message_queue, 1, IPC_RMID);
        semctl(sem_message_queue, 2, IPC_RMID);
    }
    if(sem_cells){
        for(int i=0;i<SO_HEIGHT*SO_WIDTH;i++){
            semctl(sem_cells, i, IPC_RMID);
        }
    }
    
}

void free_matrix(int value, int **local_map, int **SO_CAP_MAP, pid_t **SO_SOURCES_PID_MAP, long int **SO_TIMENSEC_MAP, int so_taxis_pid_free, pid_t *SO_TAXIS_PID_ARRAY){
    int i = 0;
    int *objectInt;
    long int *objectLong;
    pid_t *objectPid;

    while(i < SO_HEIGHT){
        objectInt = local_map[i];
        free(objectInt);
        switch(value){
            case 0:
                objectInt = SO_CAP_MAP[i];
                free(objectInt);
                objectPid = SO_SOURCES_PID_MAP[i];
                free(objectPid);
                break;
            case 1:
                objectLong = SO_TIMENSEC_MAP[1];
                free(objectLong);
                break;
            default:
                break;
        }
        i++;
    }

    switch(value){
        case 0:
            if(so_taxis_pid_free){
                free(SO_TAXIS_PID_ARRAY);
            }
            free(local_map);
            free(SO_CAP_MAP);
            free(SO_TIMENSEC_MAP);
            free(SO_SOURCES_PID_MAP);
            break;
        default:
            break;
    }
}