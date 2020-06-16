#include <iostream>
#include <mpi/mpi.h>
#include <vector>
#include <stdlib.h>


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
std::vector<Flowerpot> potStatus;
//vector holding all toilet status
std::vector<Toilet> toilStatus;

/*
 * Mutexes
 */
pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * message structure: Lamport + PID + index of requested item + changeStamp of requested item
 * message type to requests: tag_flowerpots - to request flowerpot; tag_toilet - to request toilet.
 */
int main() {
    std::cout << "Hello, World!" << std::endl;
    return 0;
}
