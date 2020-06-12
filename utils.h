//
// Created by blazej on 12.06.2020.
//

#ifndef BENEFACTORS_AND_THIEVES_2020_UTILS_H
#define BENEFACTORS_AND_THIEVES_2020_UTILS_H

#include <algorithm>
#include <vector>

    /*
     * Structure holding necessary data to ensure correct critical section access.
     */
    struct Request {
        /*
         * Lamport clock
         */
        int time;
        /*
         * Sender PID
         */
        int pid;

        /*
         * Constuctor
         * @param time - Lamport's clock
         * @param id - process ID
         */
        Request(int time, int id) {
            this->time = time;
            this->pid = id;
        }


        /*
         * Overriding operator <
         * @param str - Request who is comparing with this
         */
        bool operator < (const Request &str) const {
            if(time < str.time) {
                return true;
            }else if(time == str.time && pid < str.pid) {
                return true;
            }
            return false;
        }
    };

    /*
     * struct holding data about certain Flowerpot
     */
    struct Flowerpot{
        //number of times where this was modified
        int changeStamp;
        //boolean value if this is broken or repaired
        bool status;
        //new Flowerpot is repaired.
        Flowerpot(){
            changeStamp = 0;
            status = true;
        }

    };
    /*
     * struct holding data about certain Toilet
     */
    struct Toilet{
        int changeStamp;
        bool status;

        Toilet(){
            changeStamp = 0;
            status = true;
        }
    };




    void sort_Requests(std::vector<Request> &toSort);

    void sort_int_list(std::vector<int> &toSort);



#endif //BENEFACTORS_AND_THIEVES_2020_UTILS_H
