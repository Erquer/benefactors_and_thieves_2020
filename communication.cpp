//
// Created by blazej on 12.06.2020.
//

#include "communication.h"

extern pthread_mutex_t clock_mutex;

void send(int &clock, int message, int changeStamp, int tag, int reciever, int sender)
{
    pthread_mutex_lock(&clock_mutex);

    //message data to send -> id to change, clock, changestamp;
    int messData[3];

    messData[0] = clock;
    messData[1] = message;
    messData[2] = changeStamp;

    MPI_Send(&messData, 3, MPI_INT, reciever, tag, MPI_COMM_WORLD);

    if (debugMode)
    {
        printf("[%05d][%02d][TAG: %03d] Send '%d' and '%d' to process %d.\n",
               messData[0], sender, tag, messData[1], messData[2], reciever);
    }

    pthread_mutex_unlock(&clock_mutex);
}

void recieve(int &clock, int data[], MPI_Status &status, int tag, int reviever, int sender)
{
    if (tag != -1)
    {
        MPI_Recv(data, 3, MPI_INT, sender, tag, MPI_COMM_WORLD, &status);
    }
    else
    {
        MPI_Recv(data, 3, MPI_INT, sender, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
    }
    pthread_mutex_lock(&clock_mutex);
    pthread_mutex_unlock(&clock_mutex);
}

void broadcast(int &clock, int message, int changeStamp, int tag, int world_size, int sender)
{
    if (debugMode)
    {
        printf("[Process %d][TAG: %d] Send broadcast with %d and %d\n",
               sender, tag, message, changeStamp);
    }

    //send message to all processes
    for (int processID = 0; processID < world_size; processID++)
    {
        if (processID != sender)
        {
            send(clock, message, changeStamp, tag, processID, sender);
        }
    }
    for (int i = 0; i < world_size; i++)
    {
        //send to everyone except me
        if (i != sender)
        {
            send(clock, message, extra_message, tag, i, sender);
        }
    }
}
