#include <omnetpp.h>

using namespace omnetpp;

#include "variance_m.h"

class Observer : public cSimpleModule
{
    private:
        cOutVector varianceCountVector;
    protected:
        virtual void initialize();
        virtual void handleMessage(cMessage *msg);
        variance* observerMsg;
        double all_clocks[100];
        int num_clocks;     // number of clocks we gathered
        double mean;        // average of all clocks per cycle
        double mean_sum;
        double numerator_sum; // (x-mean)^2 sum..
        double f_variance;
        int numberofnodes;
};

Define_Module(Observer);

void Observer::initialize()
{
    for(int i = 0; i < 100; ++i){
        all_clocks[i] = 0;
    }
    mean = 0;
    num_clocks = 0; WATCH(num_clocks);
    numerator_sum = 0;
    mean_sum = 0;
    f_variance = 0;
    numberofnodes = int(getParentModule()->par("numNodes"));
}

void Observer::handleMessage(cMessage *msg)
{
    if(msg->getKind() == 44){
        variance * observerMsg = check_and_cast<variance *>(msg);
        all_clocks[num_clocks] = observerMsg->getMyclock();
        num_clocks += 1;
    }
    if(num_clocks == numberofnodes){
        for(int z = 0; z < numberofnodes; ++z)
            mean_sum += all_clocks[z];
        EV << "mean_sum: " << mean_sum;
        mean = double(mean_sum / numberofnodes);
        EV << "numberofnodes: " << numberofnodes;
        EV << "mean: " << mean;
        for(int x = 0; x < numberofnodes; ++x){
            numerator_sum += pow((all_clocks[x] - mean), 2); // summing all the expanded numerator terms of the variance equation
        }
        f_variance = numerator_sum / numberofnodes;
        EV << "Variance: " << f_variance;
        varianceCountVector.record(f_variance); // statistics recording
        // cleanup
        f_variance = 0;
        mean_sum = 0;
        numerator_sum = 0;
        num_clocks = 0;
        mean = 0;
        for(int i = 0; i < 100; ++i){
            all_clocks[i] = 0;
        }
    }
    delete msg;
}
