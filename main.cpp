#include <iostream>
#include "mpi/mpi.h"
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include "utils.h"
#include "communication.h"

#define NOTREQUESTING 0
#define REQUESTING 1
#define FLOWERPOT 0
#define TOILET 1
#define BROKEN 0
#define REPAIRED 1

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

//0 -> process not requesting resources
//1 -> process requesting a resource
bool processStatus = NOTREQUESTING;

//lamport clock
int processLamport = 0;

//Requests to flowerpots
std::vector<Request> potsRequests;
//Requests to toilets
std::vector<Request> toilRequests;

//list of available flowerpots after repair/breaking
std::vector<int> potsToReturn;
//list of available toilets after repair/breaking
std::vector<int> toilToReturn;

//vector holding all flowerpots status
std::vector<Flowerpot *> potStatus;
//vector holding all toilet status
std::vector<Toilet *> toilStatus;

/*
 * Mutexes
 */
//for clock sync inside the process
pthread_mutex_t clock_mutex = PTHREAD_MUTEX_INITIALIZER;
//for flowerpot queue to sync inside the process
pthread_mutex_t potMutex = PTHREAD_MUTEX_INITIALIZER;
//for toilet queue to sync inside the process
pthread_mutex_t toiletMutex = PTHREAD_MUTEX_INITIALIZER;
//for flowerpot request queue.
pthread_mutex_t potRequestMutex = PTHREAD_MUTEX_INITIALIZER;
//for toilet request queue
pthread_mutex_t toiletRequestMutex = PTHREAD_MUTEX_INITIALIZER;

/*
 * function to recieve messages and proper status update for Benefactors 
 */
void *benefactorReciever(void *thread)
{
    while (run_program)
    {
        MPI_Status status;
        int data[5]; //for message -> sender id, id of item to fix,
    }
}
/*
 * function to recieve messages and proper status update for Thieves 
 */
void *thieveReciever(void *thread)
{
    while (run_program)
    {
    }
}

/*
    initData - initialize vectors, fill them with default data.
*/
void initData(int argc, char *argv[])
{
    F = atoi(argv[1]);
    W = atoi(argv[2]);

    if (F < 0)
    {
        printf("[ERROR] Bad number of flowerpots\n");
        exit(EXIT_FAILURE);
    }
    if (W < 0)
    {
        printf("[ERROR] Bad number of toilets\n");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < F; i++)
        potStatus.push_back(new Flowerpot());
    // printf("Dodano Doniczki do procesu: %d\n", myPID);
    for (int i = 0; i < W; i++)
        toilStatus.push_back(new Toilet());
    // printf("Dodano Toalety do procesu: %d\n", myPID);
}
/*
* checkThread - create and check new MPI thread. 
* if MPI_THREAD_MULTIPLE then OK else, exit program.
*/
void checkThread(int *argc, char **argv[])
{
    int status = 0;
    MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &status);

    if (debugMode)
    {
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

    if (status != MPI_THREAD_MULTIPLE)
    {
        printf("[ERROR] Not enough support for threads\n");
        MPI_Finalize();
        exit(EXIT_FAILURE);
    }
}

////////////////////////////////////////////
//////////// PROCESS FUNCTIONS /////////////
////////////////////////////////////////////

//choose to fix flowerpot or toilet
//0 -> flowerpot
//1 -> toilet
std::pair<int, bool> flowerpotOrToilet()
{
    //create broken flowerpots counter
    int brokenFlowerpotsCount = 0;
    //count
    //for each flowerpot in potStatus
    for (auto flowerpot : potStatus)
    {
        if (flowerpot->status == REPAIRED)
        {
            brokenFlowerpotsCount++;
        }
    }

    //create broken toilets counter
    int brokenToiletsCount = 0;
    //count
    //for each toilet in toilStatus
    for (auto toilet : toilStatus)
    {
        if (toilet->status == REPAIRED)
        {
            brokenToiletsCount++;
        }
    }

    //requests counts
    int flowerpotRequestsCount = potsRequests.size();
    int toiletRequestsCount = toilRequests.size();

    //calculate requests to resources ratio
    double flowerpotsRatio = (brokenFlowerpotsCount == 0) ? 0.0 : flowerpotRequestsCount / brokenFlowerpotsCount;
    double toiletsRatio = (brokenToiletsCount == 0) ? 0.0 : toiletRequestsCount / brokenToiletsCount;

    //choose smaller ratio
    bool choice;
    if (flowerpotsRatio < toiletsRatio)
    {
        choice = FLOWERPOT;
        std::cout << "Sending: " << flowerpotRequestsCount << " and " << choice << std::endl;
        return std::make_pair(brokenFlowerpotsCount, choice);
    }
    else
    {
        choice = TOILET;
        std::cout << "Sending: " << toiletRequestsCount << " and " << choice << std::endl;
        return std::make_pair(brokenToiletsCount, choice);
    }
}

//find an item to fix
//return item index from flowerpots/toilets vector
std::pair<int, int> findItemToFix()
{
    //choose to fix flowerpot or toilet
    //0 -> flowerpot
    //1 -> toilet
    std::pair<int, bool> choice = flowerpotOrToilet();

    //sign that there is no broken toilets and flowerpots so we have to wait some time for thieves to break something
    if (choice.first == 0)
    {
        return std::make_pair(-1, -1);
    }
    else
    {
        //flowerpot choosen
        if (choice.second == FLOWERPOT)
        {
            //hold smallest flowerpot changeStamp
            //set this to the first flowerpot changeStamp value
            Flowerpot *firstFlowerpot = potStatus[0];
            int smallestChangeStampValue = firstFlowerpot->changeStamp;
            int smallestChangeStampID = 0;

            //for each flowerpot in potStatus
            for (int flowerpotID = 0; flowerpotID < potStatus.size(); flowerpotID++)
            {
                //current flowerpot
                Flowerpot *flowerpot = potStatus[flowerpotID];

                //check if this flowerpot has no requests
                bool noRequests = true;
                //for each request in potsRequests
                for (Request request : potsRequests)
                {
                    //if request to this flowerpot exists break
                    if (request.rid == flowerpotID)
                    {
                        noRequests = false;
                        break;
                    }
                }

                //if flowerpot has no requests choose it as an item to fix
                if (noRequests)
                {
                    return std::make_pair(choice.second, flowerpotID);
                }
                //else keep looking for the smallest flowerpot changeStamp
                else if (flowerpot->changeStamp < smallestChangeStampValue)
                {
                    smallestChangeStampValue = flowerpot->changeStamp;
                    smallestChangeStampID = flowerpotID;
                }
            } //for

            //if every flowerpot have some requests
            //return the one with the smallest changeStamp
            return std::make_pair(choice.second, smallestChangeStampID);
        } //if

        //toilet choosen
        else if (choice.second == TOILET)
        {
            //hold smallest toilet changeStamp
            //set this to the first toilet changeStamp value
            Toilet *firstToilet = toilStatus[0];
            int smallestChangeStampValue = firstToilet->changeStamp;
            int smallestChangeStampID = 0;

            //for each toilet in toilets
            for (int toiletID = 0; toiletID < toilStatus.size(); toiletID++)
            {
                //current toilet
                Toilet *toilet = toilStatus[toiletID];

                //check if this toilet has no requests
                bool noRequests = true;
                //for each request in toiletRequests
                for (Request request : toilRequests)
                {
                    //if request to this toilet exists break
                    if (request.rid == toiletID)
                    {
                        noRequests = false;
                        break;
                    }
                }

                //if toilet has no requests choose it as an item to fix
                if (noRequests)
                {
                    return std::make_pair(choice.second, toiletID);
                }
                //else keep looking for the smallest toilet changeStamp
                else if (toilet->changeStamp < smallestChangeStampValue)
                {
                    smallestChangeStampValue = toilet->changeStamp;
                    smallestChangeStampID = toiletID;
                }
            } //for

            //if every toilet have some requests
            //return the one with the smallest changeStamp
            return std::make_pair(choice.second, smallestChangeStampID);
        } //else if
    }
    return std::make_pair(-1, -1);
} //int Benefactor::findItemToFix()

void runBenefactorLoop()
{

    while (run_program)
    {
        std::pair<int, int> choice = findItemToFix();

        printf("[Benefactor %d] Chosen %d item with id %d \n", myPID, choice.first, choice.second);

        sleep(4);
    }
}
void sendRequest()
{
}
bool waitForACK()
{
    return true;
}
void breakItem()
{
}
void runThieveLoop()
{

    while (run_program)
    {
        //should be findItemToBreak();
        std::pair<int, int> choice = findItemToFix();
        printf("[Thieve %d] Chosen %d item with id %d \n", myPID, choice.first, choice.second);
        sleep(4);

        //function which send request to others (add parameters)
        sendRequest();
        //wait untill you wont get all ACK needed (? check this in recieverLoop and increment variable ?) - return bool if we can or dont.
        bool canIEnter = waitForACK();

        if (canIEnter)
        {
            //enter critical section increment your clock, update item status and send proper message to others.
            breakItem();
        }
        else
        {
            //we couldnt break, no clock incrementation, just sleep for some time to decide what do I do next.
            sleep(5);
        }
    }
}

/*
 * detection ctrl+c -> clear memory and shut processes 
 */
void ctrl_c(int signum)
{
    std::cout << "Caught closing signal: " << signum << std::endl;
    run_program = false;

    exit(signum);
}

/*
 * message structure: Lamport + PID + index of requested item + changeStamp of requested item
 * message type to requests: tag_flowerpots - to request flowerpot; tag_toilet - to request toilet.
 *
 * argv[1] => liczba doniczek
 * argv[2] => liczba toalet
 */
int main(int argc, char *argv[])
{

    if (argc < 3)
    {
        perror("Za mało argumentów\n");
        exit(1);
    }

    signal(SIGINT, ctrl_c);

    checkThread(&argc, &argv);

    //Get ours PID and total processes.
    MPI_Comm_rank(MPI_COMM_WORLD, &myPID);
    MPI_Comm_size(MPI_COMM_WORLD, &totalProcesses);

    //initialize vectors with flowerpots and toilets
    initData(argc, argv);

    //seed for random based on our PID
    srand(myPID);
    pthread_t reciever;
    if (myPID % 2 == 0)
    {
        pthread_create(&reciever, NULL, thieveReciever, 0);
        runThieveLoop();
    }
    else
    {
        pthread_create(&reciever, NULL, benefactorReciever, 0);
        runBenefactorLoop();
    }

    //  std::cout << "Hello, World!" << std::endl;

    run_program = false;

    printf("Waiting for others to complete working\n");
    sleep(2);

    MPI_Finalize();
    return 0;
}
