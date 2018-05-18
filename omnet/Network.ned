network Network
{
    // this gets "built" during the network building phase before initialization of nodes 
    parameters:
        int init;
        int numNodes;
        double connectivity_radius;
        
    types:
        channel Channel extends ned.DelayChannel { delay = 100ms; }
        
    submodules:
        NODE[numNodes]: NODE;
        Observer_of_Variance: Observer;
        
    connections:
        for i = 0..numNodes - 1, for j = i + 1..numNodes - 1 {
            NODE[i].port++ <--> Channel <--> NODE[j].port++ if sqrt((NODE[i].xCor - NODE[j].xCor)^2 + (NODE[i].yCor - NODE[j].yCor)^2) <= connectivity_radius;
        }
}

simple NODE
{
    parameters:
        bool sink;
        double xCor;
        double yCor;
		int numNodes;
    gates:
        inout port[];
}

simple Observer
{
    gates:
        input in @directIn; // This will cause OMNeT++ not to complain that the gate is not connected in the network or compound module where the module is used. 
}