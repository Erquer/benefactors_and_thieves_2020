#include <vector>
#include <tuple>
#include <stdio.h>
#include <iostream>
#include "utils.h"

class Benefactor {
public:
    //constructor
    //save info about flowerpots and toilets
    //in Benefactor attributes
    Benefactor(std::vector <Flowerpot*> flowerpots, 
                std::vector <Toilet*> toilets, int PID);
    void runYourStuff();
private:
    //0 -> benefactor not requesting resources
    //1 -> benefactor requesting a resource
    bool status;

    int myPID;
    //lamport clock
    int time;

    //vector of all flowerpot requests
    std::vector <Request> flowerpotRequests;
    //vector of all toilet requests
    std::vector <Request> toiletRequests;
    //vector of all flowerpots 
    std::vector <Flowerpot*> flowerpots;
    //vector of all toilets
    std::vector <Toilet*> toilets;

    /////////////////////////////////////////////////////////

    //choose to fix flowerpot or toilet
    //0 -> flowerpot
    //1 -> toilet
    bool flowerpotOrToilet();

    //find an item to fix 
    //return item index from flowerpots/toilets vector
    std::pair<int,int> findItemToFix();

    
};