#include <omnetpp.h>
using namespace omnetpp;

//generated header files from .msg files
#include "echo_m.h"
#include "sync_m.h"
#include "variance_m.h"

class NODE : public cSimpleModule
{
    public:
        NODE();
        virtual ~NODE();

    protected:
        virtual void initialize();
        virtual void handleMessage(cMessage *msg);

    private:
        cModule *Observer;
        double clock;         // current node clock
        double clock_sum;     // clock sum variable for echo
        cMessage* timerMsg;   // the self clock timer for each node to add [0,1] to its clock
        bool isSink;
        int numNeighbors;
        bool reached;         // echo: has node been reached by the explorer wave
        int numResponses;     // how many of the neighbors have responded
        int predecessor;      // the gate id of the parent who sent the explorer message to this node
        int baseId;
        cMessage* exploreMsg; // explore wave message
        echo* echoMsg;        // echo type, backwards echo message
        variance* observerMsg;
        int numberofechoes;   // node variable, number of nodes it got echoes back from
        sync* syncMsg;        // sync type, sync clocks message with the average clock value from the sink..
        cMessage* sinkTimer;  // timer (default=7secs) for each clock sync cycle
        bool synced;          // accept only one sync per sync cycle
        int syncCount;        // how many sync requests we got from neighbors (used to reset synced status to false after a sync cycle is over).
        // ayylmao
        //double holdClocks[100]; // holder array in each node for the values of their children's nodes
                                // every time it receives an echo, it adds the value to the holdClocks[numberofechoes] position
}; Define_Module(NODE);


void NODE::initialize()
{
    isSink = par("sink").boolValue();
    numNeighbors = gateSize("port"); // Reads the size of the "port" gate vector which is the number of neighbors
    clock_sum = 0;
    numberofechoes = 0; // local echo number (there's also an echo number that gets transferred from child to parent with echoes)
    baseId = gateBaseId("port$o");
    // clocks
    clock = 0;
    WATCH(clock); // able to examine the variable under simulation
    timerMsg = new cMessage("clock_timer");
    scheduleAt(simTime() + 0, timerMsg); // first clock adding, run instantly

    synced = false;
    syncCount = 0;

    reached = false;
    // sink
    if(isSink){
        sinkTimer = new cMessage("timer for sync");
        scheduleAt(50, sinkTimer);
    }
    numResponses = 0; // all nodes start with 0, even the sink (since he didn't get the explorer from anyone.. he created it)

    // VARIANCE STUFF >>>>



    //DEBUG WATCHES
    WATCH(numResponses);
    WATCH(reached);
    WATCH(baseId); WATCH(numNeighbors); WATCH(predecessor); WATCH(clock_sum); WATCH(numberofechoes); WATCH(syncCount);

}

void NODE::handleMessage(cMessage *msg)
{
    // SINK explorer sync period timer T = 7s
    if( msg == sinkTimer && isSink){
        reached = true;
        exploreMsg = new cMessage("explorer wave", 14);
        for(int i = 0; i < numNeighbors; ++i){
            send(exploreMsg->dup(), "port$o", i); // in order to send the same message to multiple nodes, you need to duplicate it
        }
        delete exploreMsg;
        scheduleAt(simTime() + 50, sinkTimer);
    }

    // SELF CLOCK MESSAGE
    if ( msg == timerMsg ){
        // every 1 second, the self timerMsg arrives, and adds a random value to the clock from 0 to 1.
        clock += uniform(0, 1);
        EV << "my clock is" << clock << "\n";

        // <<= VARIANCE ==> sending clocks to observer
        variance* observerMsg = new variance("clocks", 44);
        observerMsg->setMyclock(clock);

        cModule *Observer = getParentModule()->getSubmodule("Observer_of_Variance"); //getting pointer to observer module
        sendDirect(observerMsg, Observer, "in"); // sending directly to the unconnected input gate of Observer
        // <<= end variance section ==>

        scheduleAt(simTime() + 1, timerMsg);
    }

    // ECHO "EXPLORER" WAVE (no need for sink checks anywhere since sink will never get an explorer wave back at him, only echo messages)
    if ( msg->getKind() == 14 ){
        // if its first time being reached..
        if (!reached){
            numResponses += 1;
            reached = true;
            EV << "i am reached..";
            predecessor = msg->getArrivalGate()->getIndex(); // since it wasn't reached before, the one who sent this exploreMsg is the predecessor/parent in the spanning tree

            for( int i = 0; i < numNeighbors ; i++){
                cGate * gatef = gate(baseId + i);

                /*
                // debug logging to console
                EV << gatef->getFullName() << ": ";
                EV << "id=" << gatef->getId() << ", ";
                if (!gatef->isVector()){
                    EV << "scalar gate, ";}
                else{
                    EV << "gate " << gatef->getIndex() << " in vector " << gatef->getName() << " of size " << gatef->getVectorSize() << ", ";
                    EV << "type:" << cGate::getTypeName(gatef->getType()) << "\n";}
                */
                if(gatef->getIndex() != predecessor){
                    send(msg->dup(), gatef); // propagate explorer wave to every neighbor except parent
                }
            }

            delete msg; // since we are sending duplicates of msg, we should delete our own copy at the end..
        }
        else{
            numResponses += 1;
            delete msg; // added recently, to combat non-destroyed explorer wave msg at end of simulation
            // if it was already reached..
            // we are getting an explorer message from a "non-child" node.. we just increment echoes from neighbors..
        }
        if ( numResponses == numNeighbors ){
            // in this case, we are done with our children echoes, and got the final message (a late explorer message) from a neighbor
            // OR we are a "leaf" node with only 1 neighbor.. our predecessor (parent)
            EV << " echoes == neighbors\n";
            numResponses = 0; // resetting for next run
            reached = false;

            echo * echoMsg = new echo("echo", 20);
            echoMsg->setNumberofnodes(0); //since we are a leaf (not 1 but 0, since we are counting our children in numberofnodes)
            echoMsg->setEchoClock(clock_sum + clock); // clock sum from children + current self clock
            send(echoMsg, "port$o" , predecessor); //msg, gatename, gateindex
        }
    }


    // receiving ECHO message (msg_id=20) from child node
    // add its clock variable to our clock_sum, and delete the message
    if (msg->getKind() == 20){
        echo *echom = check_and_cast<echo *>(msg); // casting msg to echo??

        EV << "received echo \n";
        numResponses += 1;      // increments both from explorer messages and from echoes.
        numberofechoes = numberofechoes + 1 + echom->getNumberofnodes(); // for each child that sends an echo, add one + the child's number in the echo message to be carried onwards - counting network nodes to check for connected graph

        clock_sum += echom->getEchoClock(); // adding child's clock to the sum

        if ( numResponses == numNeighbors ){
            reached = false;
            numResponses = 0;
            //delete msg;  //we do not need the children's echo messages since we created a new one to send..

            if (!isSink){
                // done: cleaning up node for next run, and sending clock echo to parent
                echo * echoMsg = new echo("echo", 20);
                echoMsg->setEchoClock(clock_sum + clock); // clock sum from children + current self clock
                echoMsg->setNumberofnodes(numberofechoes);
                send(echoMsg, "port$o" , predecessor); //msg, gatename, gateindex
                }
            else{
                // SYNC START
                // we have the "full" echo back at SINK, and we are finished with gathering the clock values
                // check for connected network
                bubble("Clocks read, starting sync");
                if(numberofechoes == int(getParentModule()->par("numNodes")) -1 ) // nodes-1 because we did not count sink(ourself) in our echoes counting
                    bubble("CONNECTED NETWORK!");
                else
                    bubble("DISCONNECTED NETWORK!");

                // we have to now resync every clock in the networks nodes
                for( int i = 0; i < numNeighbors ; i++){
                    sync * syncMsg = new sync("clock sync message", 99); // the sync clocks message
                    syncMsg->setSyncClock(clock_sum / numberofechoes);   // SyncClock is the average clock of all the nodes: (sum from all nodes in the network) / (number of all echoes (all nodes..))
                    cGate * gatef = gate(baseId + i);
                    send(syncMsg, gatef);
                }
                clock = (clock + (clock_sum/numberofechoes)) / 2; // === !!! NEW: updating the clock of sink too ??
            }
            clock_sum = 0;
            numberofechoes = 0; //reset for next run
        }
        delete msg; // added recently.. seems to eliminate non-destroyed echo msg at end of simulation
    }


    // CLOCK AVERAGE SYNC MESSAGE (msg_id=99)
    if(msg->getKind() == 99){
        // we got the sync command, lets sync to all neighbors except our predecessor..
        if(isSink){
            delete msg;
            return;//sync does not send out sync messages since he already did
        }
        if(!synced){
            synced = true;
            syncCount += 1;
            sync *syncm = check_and_cast<sync *>(msg); // casting msg to sync??
            clock = (clock + syncm->getSyncClock()) / 2; //current clock value + sync value (new) /2

            for( int i = 0; i < numNeighbors ; i++){
                cGate * gatef = gate(baseId + i);
                send(syncm->dup(), gatef);
                /*if(gatef->getIndex() != predecessor){
                    send(syncm->dup(), gatef); // push clock sync command to every neighbor except parent
                    }*/
            }
            if(syncCount == numNeighbors){
                // case of 1 neighbor only
                synced = false;
                syncCount = 0;
            }
            delete msg;
        }
        else{
            // if has already synced
            syncCount += 1;
            if(syncCount == numNeighbors){ // +1 because we will get one from our parent too
                synced = false;
                syncCount = 0;
            }
            delete msg;//recently added to combat non-destroyed sync msg at end of simulation run, works!

        }
    }
}


NODE::NODE(){
    // here if initialization never takes place, for destructor below to not crash and burn
    sinkTimer = nullptr;
    timerMsg = nullptr;
    exploreMsg = nullptr;
    echoMsg = nullptr;
    syncMsg = nullptr;
}

NODE::~NODE(){
    // destructor of self messages, every other message is destroyed during runtime
    cancelAndDelete(sinkTimer);
    cancelAndDelete(timerMsg);
}
