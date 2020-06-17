#include <iostream>
#include "mpi/mpi.h"
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>


#include "utils.h"
#include "communication.h"
#include "benefactor.h"

//Globals
int lamport_clock = 0;

//number of flowerpots
int F = 5;
//number of WC
int W = 0;

//Number of Benefactors
int B = 3;
//Number of Thieves
int T = 3;
//number of processes
int totalProcesses;
//my ID
int myPID;
//parameten needed to proper threads managment
bool run_program = true;

//debug mode - show additional messages while working
bool debugMode = true;

//wysłanie requesta = request + indeks + changeStamp.

/*
 * Vectors needed.
 */

//Requests to flowerpots
std::vector<Request> potsRequests;
//Requests to toilets
std::vector<Request> toilRequests;

//list of available flowerpots after repair/breaking
std::vector<int> potsToReturn;
//list of available toilets after repair/breaking
std::vector<int> toilToReturn;

//vector holding all flowerpots status
std::vector<Flowerpot*> potStatus;
//vector holding all toilet status
std::vector<Toilet*> toilStatus;

/*
 * Mutexes
 */
pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
    initData - initialize vectors, fill them with default data.
*/
void initData(int argc, char *argv[]){
    F = atoi(argv[1]);
    W = atoi(argv[2]);

    if(F <0){
        printf("[ERROR] Bad number of flowerpots\n");
        exit(EXIT_FAILURE);
    }
    if(W < 0){
         printf("[ERROR] Bad number of toilets\n");
        exit(EXIT_FAILURE);
    }
    for(int i = 0; i < F;i++) potStatus.push_back(new Flowerpot());
    printf("Dodano Doniczki do procesu: %d\n", myPID );
    for(int i = 0; i < W;i++) toilStatus.push_back(new Toilet());
    printf("Dodano Toalety do procesu: %d\n", myPID );

}
/*
* checkThread - create and check new MPI thread. 
* if MPI_THREAD_MULTIPLE then OK else, exit program.
*/
void checkThread(int *argc, char **argv[]){
    int status = 0;
    MPI_Init_thread(argc,argv,MPI_THREAD_MULTIPLE,&status);


    if(debugMode){
        switch (status)
        {
        case MPI_THREAD_SINGLE:
            printf("[INFO] Thread level supported: MPI_THREAD_SINGLE\n");
            break;
        case MPI_THREAD_FUNNELED:
            printf("[INFO] Thread level supported: MPI_THREAD_FUNNELED\n");
            break;
        case MPI_THREAD_SERIALIZED:
            printf("[INFO] Thread level supported: MPI_THREAD_SERIALIZED\n");
            break;
        case MPI_THREAD_MULTIPLE:
            printf("[INFO] Thread level supported: MPI_THREAD_MULTIPLE\n");
            break;
        default:
            printf("[INFO] Thread level supported: UNRECOGNIZED\n");
            exit(EXIT_FAILURE);
        }
    }

    if(status != MPI_THREAD_MULTIPLE){
        printf("[ERROR] Not enough support for threads\n");
        MPI_Finalize();
        exit(EXIT_FAILURE);
    }

}


/*
 * message structure: Lamport + PID + index of requested item + changeStamp of requested item
 * message type to requests: tag_flowerpots - to request flowerpot; tag_toilet - to request toilet.
 *
 * argv[1] => liczba doniczek
 * argv[2] => liczba toalet
 */
int main(int argc, char  *argv[]) {


    if(argc < 3){
        perror("Za mało argumentów\n");
        exit(1);
    }
   
    checkThread(&argc, &argv);

    //Get ours PID and total processes.
    MPI_Comm_rank(MPI_COMM_WORLD, &myPID);
    MPI_Comm_size(MPI_COMM_WORLD, &totalProcesses);

    //initialize vectors with flowerpots and toilets
    initData(argc, argv);

    //seed for random based on our PID
    srand(myPID);

    std::cout << "Hello, World!" << std::endl;

    printf("Waiting for others to complete working\n");
    sleep(2);

    MPI_Finalize();
    return 0;
}
