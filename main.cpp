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
#define INCRITICALSECTION 2
#define FLOWERPOT 0
#define TOILET 1
#define BROKEN 0
#define FIXED 1
#define BREAK 0
#define FIX 1
#define THIEVE 0
#define BENEFACTOR 1

//number of flowerpots
int F;
//number of WC
int W;
//number of thieves
int thievesCount;
//number of benefactors
int benefactorsCount;
//number of processes
int totalProcesses;
//my ID
int myPID;
//parameten needed to proper threads managment
bool run_program = true;
//debug mode - show additional messages while working
bool debugMode = false;

/*
 * Process variables
 */
//process lamport clock
int lamport_clock = 0;
//0 -> process not requesting resources
//1 -> process requesting a resource
int processStatus = NOTREQUESTING;

/*
 * Process variables when waiting for ACK
 */
//ACK counter
int gottenACK = 0;
//set this to false when we won't get access to resource
bool stillWaiting = true;

//Requests to flowerpots
std::vector<Request> potsRequests;
//Requests to toilets
std::vector<Request> toilRequests;

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
        int resourceID = data[1];
        int requestedResourceChangeStamp = data[2];

        switch (status.MPI_TAG)
        {
        case TAG_TOILET_TO_REPAIR:
        {
            if (debugMode)
            {
                printf("[Benefactor %d] got request for toilet with id %d to repair\n", myPID, data[1]);
            }

            //find requested toilet
            Toilet * toilet = toilStatus[resourceID];

            //if request is from the future
            if(toilet->changeStamp < requestedResourceChangeStamp)
            {
                //delete all other requests to this toilet from the past
                //for each request in toilRequests
                for (int i = 0; i < toilRequests.size(); i++)
                {
                    Request request = toilRequests[i];

                    //if same toilet requested
                    if (request.rid == resourceID)
                    {
                        //if it is our benefactor request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //delete request
                        toilRequests.erase(toilRequests.begin() + i);
                    }
                }

                //update toilet changeStamp
                toilet->changeStamp = requestedResourceChangeStamp;

                //add sender request to toilRequests
                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                toilRequests.push_back(senderRequest);

                //send ACK
                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
            }
            //if request is from the present
            else if(toilet->changeStamp == requestedResourceChangeStamp)
            {
                //benefactor is not requesting
                if (processStatus == NOTREQUESTING)
                {
                    //add sender request to toilRequests
                    Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                    toilRequests.push_back(senderRequest);

                    //send ACK
                    send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                }
                //benefactor is requesting
                else if (processStatus == REQUESTING)
                {
                    //true if we already found respond
                    bool isRespondFound = false;

                    //for each request in toilRequests
                    for (int i = 0; i < toilRequests.size(); i++)
                    {
                        Request request = toilRequests[i];

                        //if it is our benefactor request
                        if (request.pid == myPID)
                        {
                            //if same toilet requested
                            if (request.rid == resourceID)
                            {
                                //if our benefactor clock is lower (higher priority)
                                if (lamport_clock < senderClock)
                                {
                                    isRespondFound = true;

                                    //send MY_TURN
                                    send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                                }
                                //if our benefactor clock is equal
                                else if (lamport_clock == senderClock)
                                {
                                    //if our benefactor id is smaller
                                    if (myPID <= senderID)
                                    {
                                        isRespondFound = true;

                                        //send MY_TURN
                                        send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                                    }
                                    //if our benefactor id is higher
                                    else if (senderID < myPID)
                                    {
                                        //add sender request to toilRequests
                                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                        toilRequests.push_back(senderRequest);

                                        isRespondFound = true;

                                        //send ACK
                                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                                    }
                                }
                                //if our benefactor clock is higher (lower priority)
                                else if (senderClock < lamport_clock)
                                {
                                    //add sender request to toilRequests
                                    Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                    toilRequests.push_back(senderRequest);

                                    isRespondFound = true;

                                    //send ACK
                                    send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                                }
                            }
                            //ur benefactor is requesting something else
                            else
                            {
                                //add sender request to toilRequests
                                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                toilRequests.push_back(senderRequest);

                                isRespondFound = true;

                                //send ACK
                                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                            }

                            //we found our process request
                            break;
                        }
                    }

                    if (!isRespondFound)
                    {
                        //add sender request to toilRequests
                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                        toilRequests.push_back(senderRequest);

                        isRespondFound = true;

                        //send ACK
                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                    }
                }
                //benefactor is fixing something
                else if (processStatus == INCRITICALSECTION)
                {
                    //true if we already found respond
                    bool isRespondFound = false;

                    //for each request in toilRequests
                    for (int i = 0; i < toilRequests.size(); i++)
                    {
                        Request request = toilRequests[i];

                        //if it is our benefactor request
                        if (request.pid == myPID)
                        {
                            //if same toilet requested
                            if (request.rid == resourceID)
                            {
                                isRespondFound = true;

                                //send MY_TURN
                                send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                            }
                            //ur benefactor is fixing something else
                            else
                            {
                                //add sender request to toilRequests
                                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                toilRequests.push_back(senderRequest);

                                isRespondFound = true;

                                //send ACK
                                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                            }

                            //we found our process request
                            break;
                        }
                    }
                
                    if (!isRespondFound)
                    {
                        //add sender request to toilRequests
                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                        toilRequests.push_back(senderRequest);

                        isRespondFound = true;

                        //send ACK
                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                    }
                }
            }
            //if request is from the past
            if(requestedResourceChangeStamp < toilet->changeStamp)
            {
                
            }

            break;
        }

        case TAG_POT_TO_REPAIR:
        {
            if (debugMode)
            {
                printf("[Benefactor %d] got request for flowerpot with id %d to repair\n", myPID, data[1]);
            }

            //find requested flowerpot
            Flowerpot * flowerpot = potStatus[resourceID];

            //if request is from the future
            if(flowerpot->changeStamp < requestedResourceChangeStamp)
            {
                //delete all other requests to this flowerpot from the past
                //for each request in potsRequests
                for (int i = 0; i < potsRequests.size(); i++)
                {
                    Request request = potsRequests[i];

                    //if same flowerpot requested
                    if (request.rid == resourceID)
                    {
                        //if it is our benefactor request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //delete request
                        potsRequests.erase(potsRequests.begin() + i);
                    }
                }

                //update flowerpot changeStamp
                flowerpot->changeStamp = requestedResourceChangeStamp;

                //add sender request to potsRequests
                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                potsRequests.push_back(senderRequest);

                //send ACK
                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
            }
            //if request is from the present
            else if(flowerpot->changeStamp == requestedResourceChangeStamp)
            {
                //benefactor is not requesting
                if (processStatus == NOTREQUESTING)
                {
                    //add sender request to potsRequests
                    Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                    potsRequests.push_back(senderRequest);

                    //send ACK
                    send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                }
                //benefactor is requesting
                else if (processStatus == REQUESTING)
                {
                    //true if we already found respond
                    bool isRespondFound = false;

                    //for each request in potsRequests
                    for (int i = 0; i < potsRequests.size(); i++)
                    {
                        Request request = potsRequests[i];

                        //if it is our benefactor request
                        if (request.pid == myPID)
                        {
                            //if same flowerpot requested
                            if (request.rid == resourceID)
                            {
                                //if our benefactor clock is lower (higher priority)
                                if (lamport_clock < senderClock)
                                {
                                    isRespondFound = true;

                                    //send MY_TURN
                                    send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                                }
                                //if our benefactor clock is equal
                                else if (lamport_clock == senderClock)
                                {
                                    //if our benefactor id is smaller
                                    if (myPID <= senderID)
                                    {
                                        isRespondFound = true;

                                        //send MY_TURN
                                        send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                                    }
                                    //if our benefactor id is higher
                                    else if (senderID < myPID)
                                    {
                                        //add sender request to potsRequests
                                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                        potsRequests.push_back(senderRequest);

                                        isRespondFound = true;

                                        //send ACK
                                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                                    }
                                }
                                //if our benefactor clock is higher (lower priority)
                                else if (senderClock < lamport_clock)
                                {
                                    //add sender request to potsRequests
                                    Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                    potsRequests.push_back(senderRequest);

                                    isRespondFound = true;

                                    //send ACK
                                    send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                                }
                            }
                            //ur benefactor is requesting something else
                            else
                            {
                                //add sender request to potsRequests
                                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                potsRequests.push_back(senderRequest);

                                isRespondFound = true;

                                //send ACK
                                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                            }

                            //we found our process request
                            break;
                        }
                    }
                
                    if (!isRespondFound)
                    {
                        //add sender request to potsRequests
                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                        potsRequests.push_back(senderRequest);

                        isRespondFound = true;

                        //send ACK
                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                    }
                }
                //benefactor is fixing something
                else if (processStatus == INCRITICALSECTION)
                {
                    //true if we already found respond
                    bool isRespondFound = false;

                    //for each request in potsRequests
                    for (int i = 0; i < potsRequests.size(); i++)
                    {
                        Request request = potsRequests[i];

                        //if it is our thieve request
                        if (request.pid == myPID)
                        {
                            //if same flowerpot requested
                            if (request.rid == resourceID)
                            {
                                isRespondFound = true;

                                //send MY_TURN
                                send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                            }
                            //ur thieve is fixing something else
                            else
                            {
                                //add sender request to potsRequests
                                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                potsRequests.push_back(senderRequest);

                                isRespondFound = true;

                                //send ACK
                                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                            }

                            //we found our process request
                            break;
                        }
                    }
                
                    if (!isRespondFound)
                    {
                        //add sender request to potsRequests
                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                        potsRequests.push_back(senderRequest);

                        isRespondFound = true;
                        
                        //send ACK
                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                    }
                }
            }
            //if request is from the past
            if(requestedResourceChangeStamp < flowerpot->changeStamp)
            {
                
            }

            break;
        }

        case TAG_TOILET_BROKEN:
        {
            if (debugMode)
            {
                printf("[Benefactor %d] got info that toilet with id %d has been broken \n", myPID, data[1]);
            }

            //find toilet by id
            int resourceID = resourceID;
            Toilet *toilet = toilStatus[resourceID];

            //if toilet changeStamp is correct
            if (toilet->changeStamp < requestedResourceChangeStamp)
            {

                //update toilet status
                toilet->status = BROKEN;

                //increment item changeStamp
                toilet->changeStamp = requestedResourceChangeStamp;

                //iterate throu toilRequests
                for (int i = 0; i < toilRequests.size(); i++)
                {
                    //take next request
                    Request request = toilRequests[i];

                    //if it is same toilet
                    if (request.rid == resourceID)
                    {
                        //if it is our process request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //remove request from toilRequests
                        toilRequests.erase(toilRequests.begin() + i);
                    }
                }
            }
            //old message or error
            else if (debugMode)
            {
                printf("[Benefactor %d] Got message with wrong changeStamp. Got %d expected %d \n",
                        myPID, requestedResourceChangeStamp, toilet->changeStamp + 1);
            }

            break;
        }

        case TAG_TOILET_REPAIRED:
        {
            if (debugMode)
            {
                printf("[Benefactor %d] got info that toilet with id %d has been repaired \n", myPID, data[1]);
            }

            //find toilet by id
            int toiletID = resourceID;
            Toilet *toilet = toilStatus[toiletID];

            //if toilet changeStamp is correct
            if (toilet->changeStamp < requestedResourceChangeStamp)
            {
                //update toilets status
                toilet->status = FIXED;

                //increment toilet changeStamp
                toilet->changeStamp = requestedResourceChangeStamp;

                //iterate throu toilRequests
                for (int i = 0; i < toilRequests.size(); i++)
                {
                    //take next request
                    Request request = toilRequests[i];

                    //if it is same toilet
                    if (request.rid == toiletID)
                    {
                        //if it is our process request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //remove request from toilRequests
                        toilRequests.erase(toilRequests.begin() + i);
                    }
                }
            }
            //old message or error
            else if (debugMode)
            {
                printf("[Benefactor %d] Got message with wrong changeStamp. Got %d expected %d \n",
                        myPID, requestedResourceChangeStamp, toilet->changeStamp + 1);
            }

            break;
        }

        case TAG_POT_BROKEN:
        {
            if (debugMode)
            {
                printf("[Benefactor %d] got info that pot with id %d has been broken \n", myPID, data[1]);
            }

            //find pot by id
            int potID = resourceID;
            Flowerpot *pot = potStatus[potID];

            //if pot changeStamp is correct
            if (pot->changeStamp < requestedResourceChangeStamp)
            {
                //update pots status
                pot->status = BROKEN;

                //increment pot changeStamp
                pot->changeStamp = requestedResourceChangeStamp;

                //iterate throu potsRequests
                for (int i = 0; i < potsRequests.size(); i++)
                {
                    //take next request
                    Request request = potsRequests[i];

                    //if it is same pot
                    if (request.rid == potID)
                    {
                        //if it is our process request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //remove request from potsRequests
                        potsRequests.erase(potsRequests.begin() + i);
                    }
                }
            }
            //old message or error
            else if (debugMode)
            {
                printf("[Benefactor %d] Got message with wrong changeStamp. Got %d expected %d \n",
                        myPID, requestedResourceChangeStamp, pot->changeStamp + 1);
            }

            break;
        }

        case TAG_POT_REPAIRED:
        {
            if (debugMode)
            {
                printf("[Benefactor %d] got info that pot with id %d has been repaired \n", myPID, data[1]);
            }

            //find pot by id
            int potID = resourceID;
            Flowerpot *pot = potStatus[potID];

            //if pot changeStamp is correct
            if (pot->changeStamp < requestedResourceChangeStamp)
            {
                //update pots status
                pot->status = FIXED;

                //increment pot changeStamp
                pot->changeStamp = requestedResourceChangeStamp;

                //iterate throu potsRequests
                for (int i = 0; i < potsRequests.size(); i++)
                {
                    //take next request
                    Request request = potsRequests[i];

                    //if it is same pot
                    if (request.rid == potID)
                    {
                        //if it is our process request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //remove request from potsRequests
                        potsRequests.erase(potsRequests.begin() + i);
                    }
                }
            }
            //old message or error
            else if (debugMode)
            {
                printf("[Benefactor %d] Got message with wrong changeStamp. Got %d expected %d \n",
                        myPID, requestedResourceChangeStamp, pot->changeStamp + 1);
            }

            break;
        }

        case TAG_MY_TURN:
        {
            if (debugMode)
            {
                printf("[Benefactor %d] got info to stop requesting \n", myPID, data[1]);
            }

            //stop waiting for ack and stop requesting
            stillWaiting = false;
            processStatus = NOTREQUESTING;

            //delete our process request
            bool requestFound = false;
            //for each request in potsRequests
            for (int i = 0; i < potsRequests.size(); i++)
            {
                Request request = potsRequests[i];

                //if it is our benefactor request
                if (request.pid == myPID)
                {
                    potsRequests.erase(potsRequests.begin() + i);   
                    requestFound = true;
                    break;
                }
            }
            if (!requestFound)
            {
                //for each request in toilRequests
                for (int i = 0; i < toilRequests.size(); i++)
                {
                    Request request = toilRequests[i];

                    //if it is our benefactor request
                    if (request.pid == myPID)
                    {
                        toilRequests.erase(toilRequests.begin() + i);   
                        break;
                    }
                }
            }

            break;
        }

        case TAG_ACK:
        {
            if (debugMode)
            {
                printf("[Benefactor %d] got ack from process%d \n", myPID, senderID);
            }

            //increase ack count by 1
            gottenACK++;

            break;
        }

        default:
        {
            break;
        }

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
        int senderClock = data[0];
        int resourceID = data[1];
        int requestedResourceChangeStamp = data[2];

        switch (status.MPI_TAG)
        {
        case TAG_TOILET_TO_BREAK:
        {
            if (debugMode)
            {
                printf("[Thieve %d] got request for toilet with id %d to break\n", myPID, data[1]);
            }

            //find requested toilet
            Toilet * toilet = toilStatus[resourceID];

            //if request is from the future
            if(toilet->changeStamp < requestedResourceChangeStamp)
            {
                //delete all other requests to this toilet from the past
                //for each request in toilRequests
                for (int i = 0; i < toilRequests.size(); i++)
                {
                    Request request = toilRequests[i];

                    //if same toilet requested
                    if (request.rid == resourceID)
                    {
                        //if it is our thieve request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //delete request
                        toilRequests.erase(toilRequests.begin() + i);
                    }
                }

                //update toilet changeStamp
                toilet->changeStamp = requestedResourceChangeStamp;

                //add sender request to toilRequests
                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                toilRequests.push_back(senderRequest);

                //send ACK
                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
            }
            //if request is from the present
            else if(toilet->changeStamp == requestedResourceChangeStamp)
            {
                //thieve is not requesting
                if (processStatus == NOTREQUESTING)
                {
                    //add sender request to toilRequests
                    Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                    toilRequests.push_back(senderRequest);

                    //send ACK
                    send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                }
                //thieve is requesting
                else if (processStatus == REQUESTING)
                {
                    //true if we already found respond
                    bool isRespondFound = false;

                    //for each request in toilRequests
                    for (int i = 0; i < toilRequests.size(); i++)
                    {
                        Request request = toilRequests[i];

                        //if it is our thieve request
                        if (request.pid == myPID)
                        {
                            //if same toilet requested
                            if (request.rid == resourceID)
                            {
                                //if our thieve clock is lower (higher priority)
                                if (lamport_clock < senderClock)
                                {
                                    isRespondFound = true;

                                    //send MY_TURN
                                    send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                                }
                                //if our thieve clock is equal
                                else if (lamport_clock == senderClock)
                                {
                                    //if our thieve id is smaller
                                    if (myPID <= senderID)
                                    {
                                        isRespondFound = true;

                                        //send MY_TURN
                                        send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                                    }
                                    //if our thieve id is higher
                                    else if (senderID < myPID)
                                    {
                                        //add sender request to toilRequests
                                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                        toilRequests.push_back(senderRequest);

                                        isRespondFound = true;

                                        //send ACK
                                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                                    }
                                }
                                //if our thieve clock is higher (lower priority)
                                else if (senderClock < lamport_clock)
                                {
                                    //add sender request to toilRequests
                                    Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                    toilRequests.push_back(senderRequest);

                                    isRespondFound = true;

                                    //send ACK
                                    send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                                }
                            }
                            //ur thieve is requesting something else
                            else
                            {
                                //add sender request to toilRequests
                                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                toilRequests.push_back(senderRequest);

                                isRespondFound = true;

                                //send ACK
                                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                            }

                            //we found our process request
                            break;
                        }
                    }
                
                    if (!isRespondFound)
                    {
                        //add sender request to toilRequests
                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                        toilRequests.push_back(senderRequest);

                        isRespondFound = true;

                        //send ACK
                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                    }
                }
                //thieve is breaking something
                else if (processStatus == INCRITICALSECTION)
                {
                    //true if we already found respond
                    bool isRespondFound = false;

                    //for each request in toilRequests
                    for (int i = 0; i < toilRequests.size(); i++)
                    {
                        Request request = toilRequests[i];

                        //if it is our thieve request
                        if (request.pid == myPID)
                        {
                            //if same toilet requested
                            if (request.rid == resourceID)
                            {
                                isRespondFound = true;

                                //send MY_TURN
                                send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                            }
                            //ur thieve is breaking something else
                            else
                            {
                                //add sender request to toilRequests
                                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                toilRequests.push_back(senderRequest);

                                isRespondFound = true;

                                //send ACK
                                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                            }

                            //we found our process request
                            break;
                        }
                    }
                
                    if (!isRespondFound)
                    {
                        //add sender request to toilRequests
                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                        toilRequests.push_back(senderRequest);

                        isRespondFound = true;

                        //send ACK
                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                    }
                }
            }
            //if request is from the past
            if(requestedResourceChangeStamp < toilet->changeStamp)
            {
                
            }

            break;
        }

        case TAG_POT_TO_BREAK:
        {
            if (debugMode)
            {
                printf("[Thieve %d] got request for flowerpot with id %d to break\n", myPID, data[1]);
            }
            
            //find requested flowerpot
            Flowerpot * flowerpot = potStatus[resourceID];

            //if request is from the future
            if(flowerpot->changeStamp < requestedResourceChangeStamp)
            {
                //delete all other requests to this flowerpot from the past
                //for each request in potsRequests
                for (int i = 0; i < potsRequests.size(); i++)
                {
                    Request request = potsRequests[i];

                    //if same flowerpot requested
                    if (request.rid == resourceID)
                    {
                        //if it is our thieve request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //delete request
                        potsRequests.erase(potsRequests.begin() + i);
                    }
                }

                //update flowerpot changeStamp
                flowerpot->changeStamp = requestedResourceChangeStamp;

                //add sender request to potsRequests
                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                potsRequests.push_back(senderRequest);

                //send ACK
                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
            }
            //if request is from the present
            else if(flowerpot->changeStamp == requestedResourceChangeStamp)
            {
                //thieve is not requesting
                if (processStatus == NOTREQUESTING)
                {
                    //add sender request to potsRequests
                    Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                    potsRequests.push_back(senderRequest);

                    //send ACK
                    send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                }
                //thieve is requesting
                else if (processStatus == REQUESTING)
                {
                    //true if we already found respond
                    bool isRespondFound = false;

                    //for each request in potsRequests
                    for (int i = 0; i < potsRequests.size(); i++)
                    {
                        Request request = potsRequests[i];

                        //if it is our thieve request
                        if (request.pid == myPID)
                        {
                            //if same flowerpot requested
                            if (request.rid == resourceID)
                            {
                                //if our thieve clock is lower (higher priority)
                                if (lamport_clock < senderClock)
                                {
                                    isRespondFound = true;

                                    //send MY_TURN
                                    send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                                }
                                //if our thieve clock is equal
                                else if (lamport_clock == senderClock)
                                {
                                    //if our thieve id is smaller
                                    if (myPID <= senderID)
                                    {
                                        isRespondFound = true;

                                        //send MY_TURN
                                        send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                                    }
                                    //if our thieve id is higher
                                    else if (senderID < myPID)
                                    {
                                        //add sender request to potsRequests
                                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                        potsRequests.push_back(senderRequest);

                                        isRespondFound = true;

                                        //send ACK
                                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                                    }
                                }
                                //if our thieve clock is higher (lower priority)
                                else if (senderClock < lamport_clock)
                                {
                                    //add sender request to potsRequests
                                    Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                    potsRequests.push_back(senderRequest);

                                    isRespondFound = true;

                                    //send ACK
                                    send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                                }
                            }
                            //ur thieve is requesting something else
                            else
                            {
                                //add sender request to potsRequests
                                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                potsRequests.push_back(senderRequest);

                                isRespondFound = true;

                                //send ACK
                                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                            }

                            //we found our process request
                            break;
                        }
                    }

                    if (!isRespondFound)
                    {
                        //add sender request to potsRequests
                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                        potsRequests.push_back(senderRequest);

                        isRespondFound = true;

                        //send ACK
                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                    }
                }
                //thieve is breaking something
                else if (processStatus == INCRITICALSECTION)
                {
                    //true if we already found respond
                    bool isRespondFound = false;

                    //for each request in potsRequests
                    for (int i = 0; i < potsRequests.size(); i++)
                    {
                        Request request = potsRequests[i];

                        //if it is our thieve request
                        if (request.pid == myPID)
                        {
                            //if same flowerpot requested
                            if (request.rid == resourceID)
                            {
                                isRespondFound = true;

                                //send MY_TURN
                                send(lamport_clock, 0, 0, TAG_MY_TURN, senderID, myPID);
                            }
                            //ur thieve is fixing something else
                            else
                            {
                                //add sender request to potsRequests
                                Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                                potsRequests.push_back(senderRequest);

                                isRespondFound = true;

                                //send ACK
                                send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                            }

                            //we found our process request
                            break;
                        }
                    }
                
                    if (!isRespondFound)
                    {
                        //add sender request to potsRequests
                        Request senderRequest(senderClock, senderID, resourceID, requestedResourceChangeStamp);
                        potsRequests.push_back(senderRequest);

                        isRespondFound = true;

                        //send ACK
                        send(lamport_clock, 0, 0, TAG_ACK, senderID, myPID);
                    }
                }
            }
            //if request is from the past
            else if(requestedResourceChangeStamp < flowerpot->changeStamp)
            {
                
            }

            break;
        }

        case TAG_TOILET_BROKEN:
        {
            if (debugMode)
            {
                printf("[Thieve %d] got info that toilet with id %d has been broken \n", myPID, data[1]);
            }

            //find toilet by id
            int toiletID = resourceID;
            Toilet *toilet = toilStatus[toiletID];

            //if toilet changeStamp is correct
            if (toilet->changeStamp < requestedResourceChangeStamp)
            {
                //update toilets status
                toilet->status = BROKEN;

                //increment toilet changeStamp
                toilet->changeStamp = requestedResourceChangeStamp;

                //iterate throu toilRequests
                for (int i = 0; i < toilRequests.size(); i++)
                {
                    //take next request
                    Request request = toilRequests[i];

                    //if it is same toilet
                    if (request.rid == toiletID)
                    {
                        //if it is our process request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //remove request from toilRequests
                        toilRequests.erase(toilRequests.begin() + i);
                    }
                }
            }
            //old message or error
            else if (debugMode)
            {
                printf("[Benefactor %d] Got message with wrong changeStamp. Got %d expected %d \n",
                        myPID, requestedResourceChangeStamp, toilet->changeStamp + 1);
            }

            break;
        }

        case TAG_TOILET_REPAIRED:
        {
            if (debugMode)
            {
                printf("[Thieve %d] got info that toilet with id %d has been repaired \n", myPID, data[1]);
            }

            //find toilet by id
            int resourceID = resourceID;
            Toilet *toilet = toilStatus[resourceID];

            //if toilet changeStamp is correct
            if (toilet->changeStamp < requestedResourceChangeStamp)
            {
                //update toilet status
                toilet->status = FIXED;

                //increment item changeStamp
                toilet->changeStamp = requestedResourceChangeStamp;

                //iterate throu toilRequests
                for (int i = 0; i < toilRequests.size(); i++)
                {
                    //take next request
                    Request request = toilRequests[i];

                    //if it is same toilet
                    if (request.rid == resourceID)
                    {
                        //if it is our process request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //remove request from toilRequests
                        toilRequests.erase(toilRequests.begin() + i);
                    }
                }
            }
            //old message or error
            else if (debugMode)
                {
                    printf("[Benefactor %d] Got message with wrong changeStamp. Got %d expected %d \n",
                           myPID, requestedResourceChangeStamp, toilet->changeStamp + 1);
                }
            
            break;
        }

        case TAG_POT_BROKEN:
        {
            if (debugMode)
            {
                printf("[Thieve %d] got info that pot with id %d has been broken \n", myPID, data[1]);
            }

            //find pot by id
            int potID = resourceID;
            Flowerpot *pot = potStatus[potID];

            //if pot changeStamp is correct
            if (pot->changeStamp < requestedResourceChangeStamp)
            {
                //update pots status
                pot->status = FIXED;

                //increment pot changeStamp
                pot->changeStamp = requestedResourceChangeStamp;

                //iterate throu potsRequests
                for (int i = 0; i < potsRequests.size(); i++)
                {
                    //take next request
                    Request request = potsRequests[i];

                    //if it is same pot
                    if (request.rid == potID)
                    {
                        //if it is our process request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //remove request from potsRequests
                        potsRequests.erase(potsRequests.begin() + i);
                    }
                }
            }
            //old message or error
            else if (debugMode)
                {
                    printf("[Benefactor %d] Got message with wrong changeStamp. Got %d expected %d \n",
                           myPID, requestedResourceChangeStamp, pot->changeStamp + 1);
                }
        
            break;
        }

        case TAG_POT_REPAIRED:
        {
            if (debugMode)
            {
                printf("[Thieve %d] got info that pot with id %d has been repaired \n", myPID, data[1]);
            }

            //find pot by id
            int potID = resourceID;
            Flowerpot *pot = potStatus[potID];

            //if pot changeStamp is correct
            if (pot->changeStamp < requestedResourceChangeStamp)
            {
                //update pots status
                pot->status = FIXED;

                //increment pot changeStamp
                pot->changeStamp = requestedResourceChangeStamp;

                //iterate throu potsRequests
                for (int i = 0; i < potsRequests.size(); i++)
                {
                    //take next request
                    Request request = potsRequests[i];

                    //if it is same pot
                    if (request.rid == potID)
                    {
                        //if it is our process request
                        if (request.pid == myPID)
                        {
                            processStatus = NOTREQUESTING;
                        }

                        //remove request from potsRequests
                        potsRequests.erase(potsRequests.begin() + i);
                    }
                }
            }
            //old message or error
            else if (debugMode)
            {
                printf("[Benefactor %d] Got message with wrong changeStamp. Got %d expected %d \n",
                        myPID, requestedResourceChangeStamp, pot->changeStamp + 1);
            }

            break;
        }

        case TAG_MY_TURN:
        {
            if (debugMode)
            {
                printf("[Thieve %d] got info to stop requesting \n", myPID, data[1]);
            }

            //stop waiting for ack and stop requesting
            stillWaiting = false;
            processStatus = NOTREQUESTING;

            //delete our process request
            bool requestFound = false;
            //for each request in potsRequests
            for (int i = 0; i < potsRequests.size(); i++)
            {
                Request request = potsRequests[i];

                //if it is our thieve request
                if (request.pid == myPID)
                {
                    potsRequests.erase(potsRequests.begin() + i);   
                    requestFound = true;
                    break;
                }
            }
            if (!requestFound)
            {
                //for each request in toilRequests
                for (int i = 0; i < toilRequests.size(); i++)
                {
                    Request request = toilRequests[i];

                    //if it is our thieve request
                    if (request.pid == myPID)
                    {
                        toilRequests.erase(toilRequests.begin() + i);   
                        break;
                    }
                }
            }

            break;
        }

        case TAG_ACK:
        {
            
            if (debugMode)
            {
                printf("[Thieve %d] got ack from process%d \n", myPID, senderID);
            }

            //increase ack count by 1
            gottenACK++;

            break;
        }

        default:
        {
            break;
        }
        
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
//breakOrFix == 0 -> find item to break
//breakOrFix == 1 -> find item to fix
//return pair (all available items, choice)
//choice == 0 -> flowerpot
//choice == 1 -> toilet
std::pair<int, bool> flowerpotOrToilet(bool breakOrFix)
{
    //create flowerpots counter
    int potsToChangeCount = 0;
    //count
    //for each flowerpot in potStatus
    for (auto flowerpot : potStatus)
    {
        //find items to break
        if (breakOrFix == BREAK)
        {
            if (flowerpot->status == FIXED)
            {
                potsToChangeCount++;
            }
        }
        //find items to fix
        if (breakOrFix == FIX)
        {
            if (flowerpot->status == BROKEN)
            {
                potsToChangeCount++;
            }
        }
    }

    //create toilets counter
    int toilsToChangeCount = 0;
    //count
    //for each toilet in toilStatus
    for (auto toilet : toilStatus)
    {
        //find items to break
        if (breakOrFix == BREAK)
        {
            if (toilet->status == FIXED)
            {
                toilsToChangeCount++;
            }
        }
        //find items to fix
        if (breakOrFix == FIX)
        {
            if (toilet->status == BROKEN)
            {
                toilsToChangeCount++;
            }
        }
    }

    //requests counts
    int flowerpotRequestsCount = potsRequests.size();
    int toiletRequestsCount = toilRequests.size();

    //calculate requests to resources ratio
    double flowerpotsRatio = (potsToChangeCount == 0) ? 0.0 : flowerpotRequestsCount / potsToChangeCount;
    double toiletsRatio = (toilsToChangeCount == 0) ? 0.0 : toiletRequestsCount / toilsToChangeCount;

    //choose smaller ratio
    bool choice;
    if (flowerpotsRatio < toiletsRatio)
    {
        choice = FLOWERPOT;
        return std::make_pair(potsToChangeCount, choice);
    }
    else
    {
        choice = TOILET;
        return std::make_pair(toilsToChangeCount, choice);
    }
}

//choose an item to change
//breakOrFix == 0 -> find item to break
//breakOrFix == 1 -> find item to fix
//return pair (choice, itemID)
//choice == -1 -> nothing to change
//choice == 0 -> flowerpot
//choice == 1 -> toilet
std::pair<int, int> findItemToChange(bool breakOrFix)
{
    //find what to change
    std::pair<int, bool> choice = flowerpotOrToilet(breakOrFix);

    //sign that there is no broken toilets and flowerpots
    //so we have to wait some time for thieves to break something
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
            int smallestChangeStampValue = -1;
            int smallestChangeStampID = -1;

            //for each flowerpot in potStatus
            for (int flowerpotID = 0; flowerpotID < potStatus.size(); flowerpotID++)
            {
                //current flowerpot
                Flowerpot *flowerpot = potStatus[flowerpotID];

                if (flowerpot->status != breakOrFix)
                {
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
                    else if (smallestChangeStampValue == -1)
                    {
                        smallestChangeStampValue = flowerpot->changeStamp;
                        smallestChangeStampID = flowerpotID;
                    }
                    else if (flowerpot->changeStamp < smallestChangeStampValue)
                    {
                        smallestChangeStampValue = flowerpot->changeStamp;
                        smallestChangeStampID = flowerpotID;
                    }
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
            int smallestChangeStampValue = -1;
            int smallestChangeStampID = -1;

            //for each toilet in toilets
            for (int toiletID = 0; toiletID < toilStatus.size(); toiletID++)
            {
                //current toilet
                Toilet *toilet = toilStatus[toiletID];

                //if toilet status is what we are looking for
                if (toilet->status != breakOrFix)
                {
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
                    else if (smallestChangeStampValue == -1)
                    {
                        smallestChangeStampValue = toilet->changeStamp;
                        smallestChangeStampID = toiletID;
                    }
                    else if (toilet->changeStamp < smallestChangeStampValue)
                    {
                        smallestChangeStampValue = toilet->changeStamp;
                        smallestChangeStampID = toiletID;
                    }
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
void sendRequest(std::pair<int, int> item, int tag)
{
    switch (tag)
    {
        case TAG_TOILET_TO_BREAK:
        {
            if (myPID % 2 == THIEVE)
            {
                printf("[Thieve %d] Requesting to break toilet%d\n", myPID, item.second);
            }
            else if (myPID % 2 == BENEFACTOR)
            {
                printf("[Benefactor %d] Requesting to break toilet%d\n", myPID, item.second);
            }
            
            //find toilet by id
            int toiletID = item.second;
            Toilet *toilet = toilStatus[toiletID];

            //create request
            Request request(lamport_clock, myPID, toiletID, toilet->changeStamp);
            //store request in global memory
            toilRequests.push_back(request);

            //send req broadcast to other
            thievesBroadcast(lamport_clock, toiletID, toilet->changeStamp, TAG_TOILET_TO_BREAK, totalProcesses, myPID);
            break;
        }

        case TAG_TOILET_TO_REPAIR:
        {
            if (myPID % 2 == THIEVE)
            {
                printf("[Thieve %d] Requesting to fix toilet%d\n", myPID, item.second);
            }
            else if (myPID % 2 == BENEFACTOR)
            {
                printf("[Benefactor %d] Requesting to fix toilet%d\n", myPID, item.second);
            }

            //find toilet by id
            int toiletID = item.second;
            Toilet *toilet = toilStatus[toiletID];

            //create request
            Request request(lamport_clock, myPID, toiletID, toilet->changeStamp);
            //store request in global memory
            toilRequests.push_back(request);

            //send req broadcast to other
            benefactorsBroadcast(lamport_clock, toiletID, toilet->changeStamp, TAG_TOILET_TO_REPAIR, totalProcesses, myPID);
            break;
        }

        case TAG_POT_TO_BREAK:
        {
            if (myPID % 2 == THIEVE)
            {
                printf("[Thieve %d] Requesting to break flowerpot%d\n", myPID, item.second);
            }
            else if (myPID % 2 == BENEFACTOR)
            {
                printf("[Benefactor %d] Requesting to break flowerpot%d\n", myPID, item.second);
            }

            //find flowerpot by id
            int flowerpotID = item.second;
            Flowerpot *flowerpot = potStatus[flowerpotID];

            //create request
            Request request(lamport_clock, myPID, flowerpotID, flowerpot->changeStamp);
            //store request in global memory
            potsRequests.push_back(request);

            //send req broadcast to other
            thievesBroadcast(lamport_clock, flowerpotID, flowerpot->changeStamp, TAG_POT_TO_BREAK, totalProcesses, myPID);
            break;
        }

        case TAG_POT_TO_REPAIR:
        {
            if (myPID % 2 == THIEVE)
            {
                printf("[Thieve %d] Requesting to fix flowerpot%d\n", myPID, item.second);
            }
            else if (myPID % 2 == BENEFACTOR)
            {
                printf("[Benefactor %d] Requesting to fix flowerpot%d\n", myPID, item.second);
            }

            //find flowerpot by id
            int flowerpotID = item.second;
            Flowerpot *flowerpot = potStatus[flowerpotID];

            //create request
            Request request(lamport_clock, myPID, flowerpotID, flowerpot->changeStamp);
            //store request in global memory
            potsRequests.push_back(request);

            //send req broadcast to other
            benefactorsBroadcast(lamport_clock, flowerpotID, flowerpot->changeStamp, TAG_POT_TO_REPAIR, totalProcesses, myPID);
            break;
        }
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

    //our process is a thieve
    if (myPID % 2 == THIEVE)
    {
        //wait for access
        while (gottenACK < thievesCount - 1 && 
            stillWaiting && 
            processStatus == REQUESTING)
        {
            continue;
        }
    }
    //our process is a benefactor
    else if (myPID % 2 == BENEFACTOR)
    {
        //wait for access
        while (gottenACK < benefactorsCount - 1 && 
            stillWaiting && 
            processStatus == REQUESTING)
        {
            continue;
        }
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
        lamport_clock++;

        //find flowerpot by id
        int flowerpotID = item.second;
        Flowerpot *flowerpot = potStatus[flowerpotID];

        //update item status
        flowerpot->status = FIXED;

        //increment item changeStamp
        flowerpot->changeStamp++;

        //iterate throu potsRequests
        for (int i = 0; i < potsRequests.size(); i++)
        {
            //take next request
            Request request = potsRequests[i];

            //if it is same flowerpot
            if (request.rid == flowerpotID)
            {
                //remove request from potsRequests
                potsRequests.erase(potsRequests.begin() + i);
            }
        }

        //send fixed broadcast to others
        broadcast(lamport_clock, flowerpotID, flowerpot->changeStamp, TAG_POT_REPAIRED, totalProcesses, myPID);
    }
    //toilet choosen
    else if (item.first == TOILET)
    {
        //increment process clock
        lamport_clock++;

        //find toilet by id
        int toiletID = item.second;
        Toilet *toilet = toilStatus[toiletID];

        //update item status
        toilet->status = FIXED;

        //increment item changeStamp
        toilet->changeStamp++;

        //iterate throu toilRequests
        for (int i = 0; i < toilRequests.size(); i++)
        {
            //take next request
            Request request = toilRequests[i];

            //if it is same toilet
            if (request.rid == toiletID)
            {
                //remove request from toilRequests
                toilRequests.erase(toilRequests.begin() + i);
            }
        }

        //send fixed broadcast to others
        broadcast(lamport_clock, toiletID, toilet->changeStamp, TAG_TOILET_REPAIRED, totalProcesses, myPID);
    }
}

void runBenefactorLoop()
{
    while (run_program)
    {
        //1st = what object, 2nd = id of chosen obj.
        std::pair<int, int> choice = findItemToChange(true);

        if (debugMode)
        {
            if (choice.first == FLOWERPOT)
            {
                printf("[Benefactor %d] Flowerpot with id %d chosen \n", myPID, choice.second);
            }
            else if (choice.first == TOILET)
            {
                printf("[Benefactor %d] Toilet with id %d chosen \n", myPID, choice.second);
            }
            else
            {
                printf("[Benefactor %d] Nothing to fix \n", myPID);
                sleep(2);
                printf("[Benefactor %d] Starting new loop \n", myPID);
                continue;
            }
        }

        //no item choosen
        if (choice.first == -1)
        {
            //nothing to fix
            //start new loop
            sleep(2);

            //set ack count to 0
            gottenACK = 0;

            printf("[Benefactor %d] Starting new loop \n", myPID);

            continue;
        }

        //process requesting resource now
        processStatus = REQUESTING;
        //function which send request to others
        sendRequest(choice, (choice.first == TOILET) ? TAG_TOILET_TO_REPAIR : TAG_POT_TO_REPAIR);

        //wait untill you wont get all ACK needed (? check this in recieverLoop and increment variable ?) - return bool if we can or dont.
        stillWaiting = true;
        bool canIEnter = waitForACK(gottenACK, stillWaiting);

        //set ack count to 0
        gottenACK = 0;


        //enter critical section
        if (canIEnter)
        {
            //process in critical section
            processStatus = INCRITICALSECTION;

            if (choice.first == FLOWERPOT)
            {
                printf("[Benefactor %d] Fixing flowerpot with id %d \n", myPID, choice.second);
            }
            else if (choice.first == TOILET)
            {
                printf("[Benefactor %d] Fixing Toilet with id %d \n", myPID, choice.second);
            }

            fixItem(choice);

            //process in not requesting resource now
            processStatus = NOTREQUESTING;
        }
        else
        {
            //process is not requesting resource now
            processStatus = NOTREQUESTING;

            //we couldnt break, no clock incrementation, just sleep for some time to decide what do I do next.
            //you didnt enter critical section, you dont increment your clock.
            printf("[Benefactor %d] Couldn't enter critical section\n", myPID);
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
        lamport_clock++;

        //find flowerpot by id
        int flowerpotID = item.second;
        Flowerpot *flowerpot = potStatus[flowerpotID];

        //update item status
        flowerpot->status = BROKEN;

        //increment item changeStamp
        flowerpot->changeStamp++;

        //iterate throu potsRequests
        for (int i = 0; i < potsRequests.size(); i++)
        {
            //take next request
            Request request = potsRequests[i];

            //if it is same flowerpot
            if (request.rid == flowerpotID)
            {
                //remove request from potsRequests
                potsRequests.erase(potsRequests.begin() + i);
            }
        }

        //send broken broadcast to others
        broadcast(lamport_clock, flowerpotID, flowerpot->changeStamp, TAG_POT_BROKEN, totalProcesses, myPID);
    }
    //toilet choosen
    else if (item.first == TOILET)
    {
        //increment process clock
        lamport_clock++;

        //find toilet by id
        int toiletID = item.second;
        Toilet *toilet = toilStatus[toiletID];

        //update item status
        toilet->status = BROKEN;

        //increment item changeStamp
        toilet->changeStamp++;

        //iterate throu toilRequests
        for (int i = 0; i < toilRequests.size(); i++)
        {
            //take next request
            Request request = toilRequests[i];

            //if it is same toilet
            if (request.rid == toiletID)
            {
                //remove request from toilRequests
                toilRequests.erase(toilRequests.begin() + i);
            }
        }

        //send broken broadcast to others
        broadcast(lamport_clock, toiletID, toilet->changeStamp, TAG_TOILET_BROKEN, totalProcesses, myPID);
    }
}

void runThieveLoop()
{
    while (run_program)
    {
        //pair made of 1st int = what is it, 2nd int = id.
        std::pair<int, int> choice = findItemToChange(false);
        
        if (debugMode)
        {
            if (choice.first == FLOWERPOT)
            {
                printf("[Thieve %d] Flowerpot with id %d chosen \n", myPID, choice.second);
            }
            else if (choice.first == TOILET)
            {
                printf("[Thieve %d] Toilet with id %d chosen \n", myPID, choice.second);
            }
            else
            {
                printf("[Thieve %d] Nothing to break \n", myPID);
                sleep(2);
                printf("[Thieve %d] Starting new loop \n", myPID);
                continue;
            }
        }
        //no item choosen
        else if (choice.first == -1)
        {
            //nothing to break
            //start new loop
            sleep(2);

            //set ack count to 0
            gottenACK = 0;

            printf("[Thieve %d] Starting new loop \n", myPID);

            continue;
        }

        //process requesting resource now
        processStatus = REQUESTING;
        //function which send request to others (add parameters)
        sendRequest(choice, (choice.first == TOILET) ? TAG_TOILET_TO_BREAK : TAG_POT_TO_BREAK);

        //wait untill you wont get all ACK needed (? check this in recieverLoop and increment variable ?) - return bool if we can or dont.
        stillWaiting = true;
        bool canIEnter = waitForACK(gottenACK, stillWaiting);

        //set ack count to 0
        gottenACK = 0;

        if (canIEnter)
        {
            //process in critical section
            processStatus = INCRITICALSECTION;

            if (choice.first == FLOWERPOT)
            {
                printf("[Thieve %d] Breaking flowerpot with id %d \n", myPID, choice.second);
            }
            else if (choice.first == TOILET)
            {
                printf("[Thieve %d] Breaking Toilet with id %d \n", myPID, choice.second);
            }

            breakItem(choice);

            //process in not requesting resource now
            processStatus = NOTREQUESTING;
        }
        else
        {
            //process in not requesting resource now
            processStatus = NOTREQUESTING;

            //we couldnt break, no clock incrementation, just sleep for some time to decide what do I do next.
            printf("[Thieve %d] Couldn't enter critical section\n", myPID);
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

    //calculate benefactors and thieves count
    //if totalProcesses number is odd, there will be more thieves
    benefactorsCount = totalProcesses / 2;
    thievesCount = totalProcesses - benefactorsCount;

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

    printf("[Process %d] Waiting for others to complete working\n", myPID);
    sleep(2);

    MPI_Finalize();
    return 0;
}
