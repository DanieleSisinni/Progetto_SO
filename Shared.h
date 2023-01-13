//Libreria condivisa per le risorse comuni
#ifndef SHARED_H
#define SHARED_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/sem.h>

//Dimensioni della matrice
#define SO_WIDTH 20 	
#define SO_HEIGHT 10
//Valori di ritorno dei taxi, utili per raccogliere statistiche alla morte del processo
#define TAXI_ABORTED_STATUS 12
#define TAXI_NOT_COMPLETED_STATUS 63
#define EXIT_FATAL -1


//numero cellule più attraversate
#define SO_TOP_CELLS_NUMBER 5

// colori di foreground e background 
#define RESET "\x1B[0m"
#define KBLACK "\x1B[30m"
#define KRED "\x1B[31m"
#define KWHT  "\x1B[37m"
#define BGRED "\x1B[101m"
#define BGGREEN "\x1B[102m"
#define BGWHITE "\x1B[107m"
#define BGMAGENTA "\x1B[105m"

//Varie macro
#define VALUES_TO_SOURCE 0
#define VALUES_TO_TAXI 1
#define MSG_LEN 128
#define GOTO_SOURCE 1
#define CLEAN 

//mappa locale usata in Master, Taxi, Source
int **local_map; 

//mappa dei tempi di attesa per cella generati tra MIN e MAX. Spostata in Shared perché utilizzata anche da Taxi
long int **SO_TIMENSEC_MAP; 

// struct per mappare la Map passata dalla shdmem in un array locale
typedef struct{
    int cell_value; /* mapping della matrice map: contiene i valori di ogni cella della matrice */
} source_map_utils;

//usata per passare le mappe con i valori delle celle e con i valori di attesa per cella
typedef struct{
	int cell_value;  
	long int cell_timensec_value; 
} map_utils;

//usata per salvare i valori di ritorno dei taxi
typedef struct{
	int total_trips_completed;

	pid_t pid_longest_trip_cells;
	int longest_trip_cells;

	pid_t pid_longest_trip_time;
	int longest_trip_time;

	pid_t pid_most_client;
	int most_client;

	int so_top_cells_map[SO_HEIGHT][SO_WIDTH];    
} taxi_return_stats;

taxi_return_stats *shd_mem_taxi_return;

//usato per salvare le coordinate delle SO_TOP_CELLS
typedef struct{
	int x;
	int y;
} top_cells_array;

#define MSG_LEN 128     	//sizeof(long) per il msg.buffer

//Funzione di test per gli errori
void test_error();
void allocation_error(char *file, char *structure);

//Metodi per i segnali
void lock_signals(sigset_t my_mask);
void unlock_signals(sigset_t my_mask);

//Metodi per i semafori
void sem_reserve_msgqueue(int my_queue_sem);
void sem_release_msgqueue(int my_queue_sem);
void sem_syncronization(int my_sync_sem);

//Metodo per i taxi
void taxi_return_reserve_sem(int my_shdmem_sem);
void taxi_return_release_sem(int my_shdmem_sem);

//memory_free method
void clean(int **local_map, int **SO_CAP_MAP, pid_t **SO_SOURCES_PID_MAP, long int **SO_TIMENSEC_MAP, int so_taxis_pid_free, pid_t *SO_TAXI_PID_ARRAY, int shd_mem_id_source, int *shd_mem_source, int shd_mem_id_taxi, map_utils *shd_mem_taxi, int message_queue, int sem_message_queue, int sem_cells);
void free_matrix(int value, int **local_map, int **SO_CAP_MAP, pid_t **SO_SOURCES_PID_MAP, long int **SO_TIMENSEC_MAP, int so_taxis_pid_free, pid_t *SO_TAXIS_PID_ARRAY);

#endif
