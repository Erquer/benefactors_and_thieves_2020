#include "benefactor.h"

#include <stdio.h>
#include <iostream>
#include <unistd.h>
#include <tuple>


#define FLOWERPOT 0
#define TOILET 1
#define BROKEN 0
#define REPAIRED 1

//constructor
//save info about flowerpots and toilets
//in Benefactor attributes
Benefactor::Benefactor(std::vector <Flowerpot*> flowerpots, 
                        std::vector <Toilet*> toilets, int PID) {
    //set status and lamport clock to 0
    status = 0;
    time = 0;
    myPID = PID;
    this->flowerpots = flowerpots;                        
    this->toilets = toilets;
}

//choose to fix flowerpot or toilet
//0 -> flowerpot
//1 -> toilet
bool Benefactor::flowerpotOrToilet() {
    //create broken flowerpots counter
    int brokenFlowerpotsCount = 0;
    //count
    //for each flowerpot in flowerpots
    for (auto flowerpot : this->flowerpots){
        if (flowerpot->status == BROKEN){
            brokenFlowerpotsCount++;
        }
    }

    //create broken toilets counter
    int brokenToiletsCount = 0;
    //count
    //for each toilet in toilets
    for (auto toilet : this->toilets){
        if (toilet->status == BROKEN){
            brokenToiletsCount++;
        }
    }

    //requests counts
    int flowerpotRequestsCount = this->flowerpotRequests.size();
    int toiletRequestsCount = this->toiletRequests.size();

    //calculate requests to resources ratio
    double flowerpotsRatio = (brokenFlowerpotsCount == 0) ? 0.0 : flowerpotRequestsCount / brokenFlowerpotsCount;
    double toiletsRatio = (brokenToiletsCount == 0) ? 0.0 : toiletRequestsCount / brokenToiletsCount;

    //choose smaller ratio
    bool choice;
    if(flowerpotsRatio < toiletsRatio){
        choice = FLOWERPOT;
    }
    else{
        choice = TOILET;
    }

    return choice;
}

//find an item to fix 
//return item index from flowerpots/toilets vector
std::pair<int,int> Benefactor::findItemToFix() {
    //choose to fix flowerpot or toilet
    //0 -> flowerpot
    //1 -> toilet
    bool choice = this->flowerpotOrToilet();

    //flowerpot choosen
    if (choice == FLOWERPOT){
        //hold smallest flowerpot changeStamp
        //set this to the first flowerpot changeStamp value
        Flowerpot* firstFlowerpot = this->flowerpots[0];
        int smallestChangeStampValue = firstFlowerpot->changeStamp;
        int smallestChangeStampID = 0;

        //for each flowerpot in flowerpots
        for(int flowerpotID = 0; flowerpotID < this->flowerpots.size(); flowerpotID++){
            //current flowerpot
            Flowerpot* flowerpot = this->flowerpots[flowerpotID];

            //check if this flowerpot has no requests
            bool noRequests = true;
            //for each request in flowerpotRequests
            for(Request request : this->flowerpotRequests){
                //if request to this flowerpot exists break
                if(request.rid == flowerpotID){
                    noRequests = false;
                    break;
                }
            }

            //if flowerpot has no requests choose it as an item to fix
            if(noRequests){
                return std::make_pair(choice, flowerpotID);
            }
            //else keep looking for the smallest flowerpot changeStamp
            else if(flowerpot->changeStamp < smallestChangeStampValue) {
                smallestChangeStampValue = flowerpot->changeStamp;
                smallestChangeStampID = flowerpotID;
            }
        } //for

        //if every flowerpot have some requests
        //return the one with the smallest changeStamp
        return std::make_pair(choice, smallestChangeStampID);
    } //if

    //toilet choosen
    else if (choice == TOILET){
        //hold smallest toilet changeStamp
        //set this to the first toilet changeStamp value
        Toilet* firstToilet = this->toilets[0];
        int smallestChangeStampValue = firstToilet->changeStamp;
        int smallestChangeStampID = 0;

        //for each toilet in toilets
        for(int toiletID = 0; toiletID < this->toilets.size(); toiletID++){
            //current toilet
            Toilet* toilet = this->toilets[toiletID];

            //check if this toilet has no requests
            bool noRequests = true;
            //for each request in toiletRequests
            for(Request request : this->toiletRequests){
                //if request to this toilet exists break
                if(request.rid == toiletID){
                    noRequests = false;
                    break;
                }
            }

            //if toilet has no requests choose it as an item to fix
            if(noRequests){
                return std::make_pair(choice, toiletID);
            }
            //else keep looking for the smallest toilet changeStamp
            else if(toilet->changeStamp < smallestChangeStampValue) {
                smallestChangeStampValue = toilet->changeStamp;
                smallestChangeStampID = toiletID;
            }
        } //for

        //if every toilet have some requests
        //return the one with the smallest changeStamp
        return std::make_pair(choice, smallestChangeStampID);
    } //else if
    return std::make_pair(0,0);
} //int Benefactor::findItemToFix()

void Benefactor::runYourStuff(){
    
    std::pair<int,int> choice = this->findItemToFix();

    printf("[Benefactor %d] Chosen %d item with id %d \n",this->myPID,choice.first,choice.second);

    sleep(4);



}