//
// Created by blazej on 12.06.2020.
//

#ifndef BENEFACTORS_AND_THIEVES_2020_COMMUNICATION_H
#define BENEFACTORS_AND_THIEVES_2020_COMMUNICATION_H

#include <mpi/mpi.h>
#include "utils.h"

extern bool debugMode;

void send(int &clock, int message, int changeStamp, int tag, int reciever, int sender);

void recieve(int &clock, int data[], MPI_Status &status, int tag, int reviever, int sender);

void broadcast(int &clock, int message, int extra_message, int tag, int world_size, int sender);

#endif //BENEFACTORS_AND_THIEVES_2020_COMMUNICATION_H
