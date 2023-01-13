# Progetto_SO
The Taxicab game

1 Descrizione
  Si intende realizzare un sistema in cui sono presenti vari taxi (simulati da processi) che si spostano all’interno di una città.
  
1.1 Mappa della città
  Le strade della città sono rappresentate da una griglia di larghezza SO_WIDTH e altezza SO_HEIGHT. La griglia ha SO_HOLES celle inaccessibili disposte in           posizione casuale. Non ci possono essere “barriere” di celle inaccessibili:
    per ogni cella inaccessibile, le otto celle adiacenti non possono essere inaccessibili. Figura 1 illustra un esempio di posizionamento legale di celle               inaccessibili, mentre Figura 2 mostra alcuni casi di celle inaccessibili.
Ogni cella è caratterizzata da
  • un tempo di attraversamento estratto casualmente fra SO_TIMENSEC_MIN e SO_TIMENSEC_MAX (in nanosecondi)
  • una capacità estratta casualmente fra SO_CAP_MIN, SO_CAP_MAX che rappresenta il numero massimo di taxi che possono essere ospitati contemporaneamente nella         cella.
Le caratteristiche della cella di cui sopra sono generate casualmente all’atto della creazione della mappa e non variano per tutta la durata di una simulazione.


1.2 Richieste di servizio taxi
   Le richieste di taxi sono originate da SO_SOURCES processi dedicati. Ognuno di essi `e legato ad una cella della mappa da cui la richiesta si genera. Ogni          richiesta di servizio taxi `e caratterizzata da:
      • la cella di partenza (la cella fra le SO_SOURCES ove la richiesta `e generata);
      • la cella di destinazione determinata in modo casuale fra una qualunque delle celle della mappa tra quelle accessibili, diversa da quella di partenza.
   Le celle SO_SOURCES di origine delle richieste sono posizionate casualmente sulla mappa senza sovrapposizione.
   Le richieste originatesi in ognuna delle SO_SOURCES sorgenti sono generate da SO_SOURCES processi che immettono tali richieste in una coda di messaggi oppure una    pipe (la scelta se avere una singola coda/pipe o una diversa per ogni sorgente `e libera). I processi taxi prelevano le richieste, e le richieste possono essere    servite soltanto da un taxi che si trova nella cella di origine della richiesta.
   Le richieste sono create da SO_SOURCES processi secondo un pattern configurabile a propria scelta, per esempio attraverso un alarm periodico oppure con un          intervallo variabile. Inoltre, ognuno degli SO_SOURCES processi deputati alla generazione di richieste deve generare una richiesta ogni volta che riceve un          segnale (a scelta dello sviluppatore), per esempio da terminale.
   
1.3 Movimento dei taxi
  All’atto della creazione i taxi sono posizionati in posizione casuale. I taxi sono abilitati alla ricerca di una richiesta da servire soltanto dopo che tutti gli   altri processi taxi sono stati creati e inizializzati.
  I taxi si possono muovere orizzontalmente e verticalmente, ma non in diagonale. Il tempo di attraversamento di una cella si simula con una nanosleep(...) di         durata pari al tempo di attraversamento della cella.
  Situazione di stallo Se un taxi non si muove entro SO_TIMEOUT secondi termina rilasciando eventuali risorse possedute. Se stava effettuando una corsa per un         cliente, la richiesta viene marcata come “aborted” e quindi eliminata. Quando un processo taxi termina a causa del timeout, allora un nuovo processo taxi viene     creato in una posizione casuale.
  
1.4 Inizio e terminazione della simulazione
  La simulazione si avvia dopo che tutti i processi sorgente e processi taxi sono stati creati e inizializzati. Dal momento dell’avvio, la simulazione ha una durata   SO_DURATION secondi che sar`a un alarm per il processo master.


1.5 Stampa
  E presente un processo “master” che raccoglie le statistiche delle varie richieste eseguite. Ogni secondo viene stampato a terminale lo stato di occupazione delle   varie celle.
  Alla fine della simulazione vengono stampati:
      • numero viaggi (eseguiti con successo, inevasi e abortiti)
      • la mappa con evidenziate le SO_SOURCES sorgenti e le SO_TOP_CELLS celle pi`u attraversate
      • il processo taxi che
          1. ha fatto pi`u strada (numero di celle) di tutti
          2. ha fatto il viaggio pi`u lungo (come tempo) nel servire una richiesta
          3. ha raccolto pi`u richieste/clienti
          
2 Configurazione
  I seguenti parametri sono letti a tempo di esecuzione, da file, da variabili di ambiente, o da stdin (a discrezione degli studenti):
      • SO_TAXI, numero di taxi presenti
      • SO_SOURCES, numero di punti sulla mappa di origine dei clienti (si immagini che siano le stazioni, gli aereoporti, etc.)
      • SO_HOLES, numero di celle della mappa inaccessibili. Si ricordi: ogni cella inaccessibile non pu`o averne altre nelle otto celle che la circondano
      • SO_TOP_CELLS, numero di celle maggiormente attraversate
      • SO_CAP_MIN, SO_CAP_MAX, capacit`a minima e massima di ogni cella
      • SO_TIMENSEC_MIN, SO_TIMENSEC_MAX, tempo minimo e massimo necessario per l’attraversamento di ogni cella (in nanosecondi).
      • SO_TIMEOUT, tempo di inattivit`a del taxi dopo il quale il taxi muore
      • SO_DURATION, durata della simulazione

  Un cambiamento dei precedenti parametri non deve determinare una nuova compilazione dei sorgenti.
  Inoltre, i seguenti parametri sono letti a tempo di compilazione:
      • SO_WIDTH, larghezza della mappa;
      • SO_HEIGHT, altezza della mappa.
      
  La Tabella 1 elenca valori “dense” e “large” per alcune configurazione di esempio da testare. Si tenga presente che il progetto deve poter funzionare anche con     altri parametri fattibili. Se i parametri specificati, che possono essere scelti anche non tra quelli indicati in tabella, non permettono di generare una citt`a      valida, la simulazione deve terminare indicando la causa del problema nella generazione.


  Tabella 1: Esempio di valori di configurazione.
  parametro “dense” “large”
        SO_WIDTH 20 60
        SO_HEIGHT 10 20
        SO_HOLES 10 50
        SO_TOP_CELLS 40 40
        SO_SOURCES SO_WIDTH×SO_HEIGHT−SO_HOLES 10
        SO_CAP_MIN 1 3
        SO_CAP_MAX 1 5
        SO_TAXI SO_SOURCES/2 1000
        SO_TIMENSEC_MIN [nsec] 100000000 10000000
        SO_TIMENSEC_MAX [nsec] 300000000 100000000
        SO_TIMEOUT [sec] 1 3
        SO_DURATION [sec] 20 20
        

3 Requisiti implementativi
  Il progetto deve
      • essere realizzato sfruttando le tecniche di divisione in moduli del codice,
      • essere compilato mediante l’utilizzo dell’utility make
      • massimizzare il grado di concorrenza fra processi
      • deallocare le risorse IPC che sono state allocate dai processi al termine del gioco
      • essere compilato con almeno le seguenti opzioni di compilazione: gcc -std=c89 -pedantic
      • poter eseguire correttamente su una macchina (virtuale o fisica) che presenta parallelismo (due o pi`u processori).
