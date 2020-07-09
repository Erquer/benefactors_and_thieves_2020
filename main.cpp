#include <iostream>
#include "mpi/mpi.h"
#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>

#include "tags.h"
#include "utils.h"
#include "tags.h"
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
        int data[4]; //for message -> clock,senderID,  id of item to fix, changeStamp fixed item.
        
        recieve(lamport_clock, data, status, MPI_ANY_TAG, myPID, MPI_ANY_SOURCE);
        int senderID = status.MPI_SOURCE;
        int senderClock = data[0];
        int senderMessage = data[1];
        int senderChangeStamp = data[2];

        switch (status.MPI_TAG)
        {
        case TAG_REPAIR_TOILET:
            printf("[Benefactor %d] got access to repair toilet with id %d \n", myPID, data[1]);
            break;

        case TAG_REPAIR_POT:
            printf("[Benefactor %d] got access to repair pot with id %d \n", myPID, data[1]);
            break;

        case TAG_TOILET_TO_BREAK:
            printf("[Benefactor %d] got request for toilet with id %d to break\n", myPID, data[1]);
            break;

        case TAG_TOILET_TO_REPAIR:
            printf("[Benefactor %d] got request for toilet with id %d to repair\n", myPID, data[1]);


            //benefactor is requesting
            if(processStatus == REQUESTING)
            {
                //for each request in toilRequests
                for(Request request : toilRequests)
                {
                    //if it is our benefactor request
                    if(request.pid == myPID)
                    {
                        //requested toilet id
                        int toilID = senderMessage;

                        //if same toilet requested
                        if (request.rid == toilID)
                        {
                            //if our benefactor clock is lower (higher priority)
                            if (processLamport < senderClock)
                            {
                                //send MY_TURN
                            }
                            //if our benefactor clock is equal
                            else if (processLamport == senderClock)
                            {
                                //if our benefactor id is smaller
                                if (myPID <= senderID)
                                {
                                    //send MY_TURN
                                }
                                //if our benefactor id is higher
                                else if (senderID < myPID)
                                {
                                    //send ACK
                                }
                            }
                            //if our benefactor clock is higher (lower priority)
                            else if (senderClock < processLamport)
                            {
                                //send ACK
                            }
                        }
                        //ur process is requesting something else
                        else
                        {
                            //send ACK
                        }
                    }
                }
            }
            //benefactor is not requesting
            else if (processStatus == NOTREQUESTING)
            {
                //send ACK
            }

            break;

        case TAG_POT_TO_BREAK:
            printf("[Benefactor %d] got request for flowerpot with id %d to break\n", myPID, data[1]);
            break;

        case TAG_POT_TO_REPAIR:
            printf("[Benefactor %d] got request for flowerpot with id %d to repair\n", myPID, data[1]);
            break;

        case TAG_TOILET_BROKEN:
            printf("[Benefactor %d] got info that toilet with id %d has been broken \n", myPID, data[1]);
            break;

        case TAG_TOILET_REPAIRED:
            printf("[Benefactor %d] got info that toilet with id %d has been repaired \n", myPID, data[1]);
            break;

        case TAG_POT_BROKEN:
            printf("[Benefactor %d] got info that pot with id %d has been broken \n", myPID, data[1]);
            break;

        case TAG_POT_REPAIRED:
            printf("[Benefactor %d] got info that pot with id %d has been repaired \n", myPID, data[1]);
            break;

        default:
            break;
        }
    }
}
/*
 * function to recieve messages and proper status update for Thieves 
 */
void *thieveReciever(void *thread)
{
    while (run_program)
    {
        MPI_Status status;
        int data[4]; //for message -> clock,senderID,  id of item to fix, changeStamp fixed item.

        recieve(lamport_clock, data, status, MPI_ANY_TAG, myPID, MPI_ANY_SOURCE);
        int senderID = status.MPI_SOURCE;
        switch (status.MPI_TAG)
        {
        case TAG_BREAK_TOILET:
            printf("[Benefactor %d] got access to break toilet with id %d \n", myPID, data[1]);
            break;

        case TAG_BREAK_POT:
            printf("[Benefactor %d] got access to break pot with id %d \n", myPID, data[1]);
            break;

        case TAG_TOILET_TO_BREAK:
            printf("[Benefactor %d] got request for toilet with id %d to break\n", myPID, data[1]);
            break;

        case TAG_TOILET_TO_REPAIR:
            printf("[Benefactor %d] got request for toilet with id %d to repair\n", myPID, data[1]);
            break;

        case TAG_POT_TO_BREAK:
            printf("[Benefactor %d] got request for flowerpot with id %d to break\n", myPID, data[1]);
            break;

        case TAG_POT_TO_REPAIR:
            printf("[Benefactor %d] got request for flowerpot with id %d to repair\n", myPID, data[1]);
            break;

        case TAG_TOILET_BROKEN:
            printf("[Benefactor %d] got info that toilet with id %d has been broken \n", myPID, data[1]);
            break;

        case TAG_TOILET_REPAIRED:
            printf("[Benefactor %d] got info that toilet with id %d has been repaired \n", myPID, data[1]);
            break;

        case TAG_POT_BROKEN:
            printf("[Benefactor %d] got info that pot with id %d has been broken \n", myPID, data[1]);
            break;

        case TAG_POT_REPAIRED:
            printf("[Benefactor %d] got info that pot with id %d has been repaired \n", myPID, data[1]);
            break;

        default:
            break;
        }
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

//choose to change flowerpot or toilet
//0 -> flowerpot
//1 -> toilet
std::pair<int, bool> flowerpotOrToilet(bool toRepair)
{
    //create broken flowerpots counter
    int brokenFlowerpotsCount = 0;
    //count
    //for each flowerpot in potStatus
    for (auto flowerpot : potStatus)
    {
        if (flowerpot->status == toRepair)
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
        if (toilet->status == toRepair)
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
        return std::make_pair(brokenFlowerpotsCount, choice);
    }
    else
    {
        choice = TOILET;
        return std::make_pair(brokenToiletsCount, choice);
    }
}

//find an item to fix
//return item index from flowerpots/toilets vector
std::pair<int, int> findItemToChange(bool toRepair)
{
    //choose to fix flowerpot or toilet
    //0 -> flowerpot
    //1 -> toilet
    std::pair<int, bool> choice = flowerpotOrToilet(toRepair);

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

//function which send request to others
void sendRequest(std::pair<int, int> item)
{
    //flowerpot choosen
    if (item.first == FLOWERPOT)
    {
        //find flowerpot by id
        int flowerpotID = item.second;
        Flowerpot *flowerpot = potStatus[flowerpotID];

        //create request
        Request request(processLamport, myPID, flowerpotID, flowerpot->changeStamp);
        //store request in global memory
        potsRequests.push_back(request);

        //send req broadcast to other
        broadcast(processLamport, flowerpotID, flowerpot->changeStamp, TAG_POT_TO_REPAIR, totalProcesses, myPID);
    }
    //toilet choosen
    else if (item.first == TOILET)
    {
        //find toilet by id
        int toiletID = item.second;
        Toilet *toilet = toilStatus[toiletID];

        //create request
        Request request(processLamport, myPID, toiletID, toilet->changeStamp);
        //store request in global memory
        toilRequests.push_back(request);

        //send req broadcast to other
        broadcast(processLamport, toiletID, toilet->changeStamp, TAG_TOILET_TO_REPAIR, totalProcesses, myPID);
    }
}

//wait untill you wont get all ACK needed
//(? check this in recieverLoop and increment variable ?) -
//return bool if we can or dont.
bool waitForACK(int &gottenACK, bool &stillWaiting)
{
    if (debugMode)
    {
        printf("[Process %d] Waiting for D - 1 ACK\n", myPID);
    }

    //wait for access
    while (gottenACK <= totalProcesses - 1 && stillWaiting)
    {
        break;
    }

    //0 -> no access
    //1 -> access
    return stillWaiting;
}

void fixItem(std::pair<int, int> item)
{
    //take some time to fix item
    sleep(2);

    //flowerpot choosen
    if (item.first == FLOWERPOT)
    {
        //increment process clock
        processLamport++;

        //find flowerpot by id
        int flowerpotID = item.second;
        Flowerpot *flowerpot = potStatus[flowerpotID];

        //update item status
        flowerpot->status = REPAIRED;

        //increment item changeStamp
        flowerpot->changeStamp++;

        //send fixed broadcast to others
        broadcast(processLamport, flowerpotID, flowerpot->changeStamp, TAG_POT_REPAIRED, totalProcesses, myPID);
    }
    //toilet choosen
    else if (item.first == TOILET)
    {
        //increment process clock
        processLamport++;

        //find toilet by id
        int toiletID = item.second;
        Toilet *toilet = toilStatus[toiletID];

        //update item status
        toilet->status = REPAIRED;

        //increment item changeStamp
        toilet->changeStamp++;

        //send fixed broadcast to others
        broadcast(processLamport, toiletID, toilet->changeStamp, TAG_TOILET_REPAIRED, totalProcesses, myPID);
    }
}

void runBenefactorLoop()
{
    while (run_program)
    {
        std::pair<int, int> choice = findItemToChange(true);

        if(debugMode){
            if (choice.first == FLOWERPOT){
                printf("[Benefactor %d] Flowerpot with id %d chosen \n", myPID, choice.second);
            }
            else if (choice.first == TOILET){
                printf("[Benefactor %d] Toilet with id %d chosen \n", myPID, choice.second);
            }
            else{
                printf("[Benefactor %d] Nothing to fix \n", myPID);
                sleep(2);
                printf("[Benefactor %d] Starting new loop \n", myPID);
                continue;
            }
        }

        //process requesting resource now
        processStatus = REQUESTING;
        //function which send request to others
        sendRequest(choice);

        //ACK counter
        int gottenACK = 0;
        //set this to false when we won't get access to resource
        bool stillWaiting = true;
        //wait untill you wont get all ACK needed (? check this in recieverLoop and increment variable ?) - return bool if we can or dont.
        bool canIEnter = waitForACK(gottenACK, stillWaiting);

        //enter critical section
        if (canIEnter)
        {
            if (choice.first == FLOWERPOT){
                printf("[Benefactor %d] Fixing flowerpot with id %d \n", myPID, choice.second);
            }
            else if (choice.first == TOILET){
                printf("[Benefactor %d] Fixing Toilet with id %d \n", myPID, choice.second);
            }

            fixItem(choice);

            //process in not requesting resource now
            processStatus = NOTREQUESTING;
        }
        else
        {
            //we couldnt break, no clock incrementation, just sleep for some time to decide what do I do next.
            //you didnt enter critical section, you dont increment your clock.
            printf("[Benefactor %d] Couldn't enter critical section", myPID);
            sleep(2);
        }
        printf("[Benefactor %d] Starting new loop \n", myPID);
    }
}

void breakItem(std::pair<int, int> item)
{
    //take some time to break item
    sleep(2);

    //flowerpot choosen
    if (item.first == FLOWERPOT)
    {
        //increment process clock
        processLamport++;

        //find flowerpot by id
        int flowerpotID = item.second;
        Flowerpot *flowerpot = potStatus[flowerpotID];

        //update item status
        flowerpot->status = BROKEN;

        //increment item changeStamp
        flowerpot->changeStamp++;

        //send broken broadcast to others
        broadcast(processLamport, flowerpotID, flowerpot->changeStamp, TAG_POT_BROKEN, totalProcesses, myPID);
    }
    //toilet choosen
    else if (item.first == TOILET)
    {
        //increment process clock
        processLamport++;

        //find toilet by id
        int toiletID = item.second;
        Toilet *toilet = toilStatus[toiletID];

        //update item status
        toilet->status = BROKEN;

        //increment item changeStamp
        toilet->changeStamp++;

        //send broken broadcast to others
        broadcast(processLamport, toiletID, toilet->changeStamp, TAG_TOILET_BROKEN, totalProcesses, myPID);
    }
}

void runThieveLoop()
{
    while (run_program)
    {
        //should be findItemToBreak();
        std::pair<int, int> choice = findItemToChange(false);

        if (choice.first == FLOWERPOT){
            printf("[Thieve %d] Flowerpot with id %d chosen \n", myPID, choice.second);
        }
        else if (choice.first == TOILET){
            printf("[Thieve %d] Toilet with id %d chosen \n", myPID, choice.second);
        }
        else{
            printf("[Thieve %d] Nothing to break \n", myPID);
            sleep(2);
            printf("[Thieve %d] Starting new loop \n", myPID);
            continue;
        }

        //function which send request to others (add parameters)
        sendRequest(choice);

        //ACK counter
        int gottenACK = 0;
        //set this to false when we won't get access to resource
        bool stillWaiting = true;
        //wait untill you wont get all ACK needed (? check this in recieverLoop and increment variable ?) - return bool if we can or dont.
        bool canIEnter = waitForACK(gottenACK, stillWaiting);

        if (canIEnter)
        {
            if (choice.first == FLOWERPOT){
                printf("[Thieve %d] Breaking flowerpot with id %d \n", myPID, choice.second);
            }
            else if (choice.first == TOILET){
                printf("[Thieve %d] Breaking Toilet with id %d \n", myPID, choice.second);
            }

            breakItem(choice);
        }
        else
        {
            //we couldnt break, no clock incrementation, just sleep for some time to decide what do I do next.
            printf("[Thieve %d] Couldn't enter critical section", myPID);
            sleep(2);
        }
        printf("[Thieve %d] Starting new loop \n", myPID);
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

    run_program = false;

    printf("Waiting for others to complete working\n");
    sleep(2);

    MPI_Finalize();
    return 0;
}
