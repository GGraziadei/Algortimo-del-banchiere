#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#define PROTOCOLLO_REGISTRAZIONE "TIDP:%d"

typedef struct classe_risorse_struct{
    int disponibilita;
    int totali;
    int id;
    pthread_mutex_t * lock;
}classe_risorse;

typedef struct th_struct{
    pthread_t tid;
    char c;
    int * assegnate;
    int * massimo;
    int * necessita;
    int numRisorse;
    int *registrazione;
    pid_t processoMaster;
    classe_risorse * risorse;
} th;

void * task ( void * arg );

int * richiestaRandom(classe_risorse * risorse, int numRisorse);

void masterTask();

void signalRegistrazioneProcesso();

void registraProcesso(th *pStruct);

int pipeRegistrazione[2];

int main(int argc, char ** argv) {
    //Parametri: N (num processi da generare)
    // [ argv[2].. ] Vettore indica disponibilità per ogni classe di  risorse da utilizzare

    assert( argc > 2 );
    int i,numProcessi = atoi(argv[1]),  numRisorse = ( argc - 2 ) ;
    assert( numProcessi >= 1 && numRisorse >= 1);
    fprintf(stderr, "Numero processi: %d\nNumero risorse: %d\n ",numProcessi, numRisorse);

    classe_risorse * risorse = malloc( numRisorse * sizeof (classe_risorse));
    th * processi = malloc( numProcessi * sizeof (th));
    srand( getpid() );

    for( i = 0; i<numRisorse; i += 1 ){
        risorse[i].id = i ;
        fprintf(stderr, "Cardinalita classe %d : %s \n",risorse[i] .id, argv[i + 2] ) ;
        risorse[i].lock = malloc( sizeof  ( pthread_mutex_t));
        pthread_mutex_init(risorse[i].lock , NULL);
        risorse[i].disponibilita = risorse[i].totali = atoi(argv[i + 2]);
        assert(risorse[i].disponibilita >= 1);
    }

    pipe(pipeRegistrazione);
    pid_t processoMaster ;
    if(  ( processoMaster = fork() ) == 0 )
        masterTask();
    else if (processoMaster == -1 )
        exit(1);
    else {
        for ( i = 0; i< numProcessi; i += 1 ){
            processi[i].c = (char )  ( 'a' +  ( rand() % 24 + 1 ) ); //assegno una lettera a caso
            processi[i].risorse = risorse;
            processi[i].assegnate = malloc( sizeof (int) * numRisorse);
            processi[i].massimo = malloc( sizeof (int) * numRisorse);
            processi[i].necessita = malloc( sizeof (int) * numRisorse);
            processi[i].numRisorse = numRisorse;
            processi[i].processoMaster = processoMaster;
            pthread_create(&processi[i].tid, NULL, task , (void *) &processi[i]);
        }

        for ( i = 0; i< numProcessi; i += 1 ){
            pthread_join(processi[i].tid, NULL);
            free ( processi[i].assegnate);
            free ( processi[i].massimo );
            free ( processi[i].necessita );
        }

        for( i = 0; i<numRisorse; i += 1 ){
            pthread_mutex_destroy(risorse[i].lock );
        }

        free(risorse);
        free(processi);

        // Attendo la terminazione del processo che si occupa di applicare algortimo del banchiere
        waitpid( processoMaster, NULL, 0 );
    }

    return 0;
}

void  signalRegistrazioneProcesso(int signal) {
    if ( signal != SIGUSR1 )
        return;
    else printf("Registrazione avviata con successo");
}

void masterTask() {
    fprintf(stderr, "Processo master avviato PID %d\n", getpid());
    int processiRegistrati = 0, n = 2;
    char BUFFER[256];
    th  *tmp_th_p;
    th ** processi = malloc( n * sizeof (th *));
    signal(SIGUSR1, signalRegistrazioneProcesso );
    close(pipeRegistrazione[1]);
    pause();
    while (1 ){
        if ( read(pipeRegistrazione[0], BUFFER, sizeof (BUFFER) ) >= 1){
            sprintf(BUFFER, PROTOCOLLO_REGISTRAZIONE, (th *) tmp_th_p );
            printf(" %s \n", BUFFER );
            if ( processiRegistrati + 1 <= n ){
                n = 2 * n;
                realloc(processi, n * sizeof (th *));
            }
            processi[processiRegistrati++] = tmp_th_p;
        }
        pause();
    }

    free(processi);
    exit(0);
}


void * task ( void * arg ){
    th * processo = (th *) arg;
    if(processo->tid != pthread_self() )
        pthread_exit(NULL);
    printf("Processo %c PID:%d operativo \n", processo->c, pthread_self() );
    int i,j,verifica, *richiesta;

    //! Il processo deve dichiarare le massime risorse da lui utilizzabili, vengono inizializzati i valori allo stato iniziale
    richiesta = richiestaRandom(processo->risorse, processo->numRisorse);
    for( i = 0; i<processo->numRisorse; i +=1 ){
        processo->massimo[i] = richiesta[i];
        processo->assegnate[i] = 0 ;
        processo->necessita[i] = processo->massimo[i];
    }

    registraProcesso(processo);

    //while ( 1 ){
        richiesta = richiestaRandom(processo->risorse, processo->numRisorse);
       // verifica = registraRichiesta();
        free(richiesta);
        sleep(5);
    //}

    pthread_exit(NULL);
}

void registraProcesso(th *processo) {
    kill(processo->processoMaster, SIGUSR1);
    th * tmp = processo;
    write(pipeRegistrazione[1], &tmp, sizeof (th *));
}

int * richiestaRandom(classe_risorse * risorse, int numRisorse){
    int i, *richiesta;
    richiesta = malloc( sizeof (int ) * numRisorse);
    printf("Richiesta: \t");
    for( i= 0; i<numRisorse; i+= 1) {
        richiesta[i] = rand() % risorse[i].totali + 1;
        printf("%d\t", richiesta[i]);
    }
    printf("\n");
    return richiesta;
}