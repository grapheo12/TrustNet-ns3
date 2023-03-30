#include "main.h"
#include <vector>
#include <random>

#define TOPOLOGY_SUBNET "10.0.0.0"
#define SERVER_SUBNET   "11.0.0.0"
#define CLIENT_SUBNET   "9.0.0.0"
#define SWITCH_SUBNET   "8.0.0.0"
#define COMMON_MASK     "255.255.255.0"

NS_LOG_COMPONENT_DEFINE("TrustNet_Main");

std::vector<std::pair<NodeContainer, Ipv4InterfaceContainer>>   // AS => NodeContainer, Interface of servers
randomNodeAssignment(
    BriteTopologyHelper& bth,
    InternetStackHelper& stack,
    Ipv4AddressHelper& address,                                 // SetBase before calling this function
    double load_ratio)                                          // load_ratio = #new nodes / #leaf nodes in AS; load_ratio = 0 => 1 node per AS
{
    NS_LOG_INFO("Random P2P node Init");
    std::vector<std::pair<NodeContainer, Ipv4InterfaceContainer>> assgn;
    uint32_t nas = bth.GetNAs();
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    for (uint32_t i = 0; i < nas; i++){
        uint32_t nleaf = bth.GetNLeafNodesForAs(i);
        uint32_t nserver = (uint32_t)(load_ratio * nleaf);
        if (nserver == 0) nserver++;

        NS_LOG_INFO("AS: " << i << " Leaf Nodes: " << nleaf << " Nodes to create: " << nserver);
        std::default_random_engine eng;
        std::uniform_int_distribution<uint32_t> dist(0, nserver - 1);

        NodeContainer servers;
        Ipv4InterfaceContainer interfaces;
        servers.Create(nserver);
        stack.Install(servers);

        for (uint32_t j = 0; j < nserver; j++){
            NodeContainer __nodes;
            __nodes.Add(servers.Get(j));
            __nodes.Add(bth.GetLeafNodeForAs(i, dist(eng)));
            auto __p2pdevs = p2p.Install(__nodes);
            Ipv4InterfaceContainer __interfaces = address.Assign(__p2pdevs);
            interfaces.Add(__interfaces.Get(0));
        }

        assgn.push_back(std::make_pair(servers, interfaces));
    }

    return assgn;
}

void assignRandomASPeers(const std::vector<RIB *>& ribs) 
{
    auto rng = std::default_random_engine {};
    std::vector<RIB *> queue = ribs; // copies the ribs vector

    for (auto rib = ribs.begin(); rib != ribs.end(); rib++)
    {
        // * seeding is turned off so that each run of the program has the same series of random numbers
        // srand((unsigned) time(NULL)); 
        int num_peers = rand() % (ribs.size() / 2);
        std::shuffle(queue.begin(), queue.end(), rng);

        std::vector<Address> addresses;
        for (int i = 0; i < num_peers; i++)
        {
            RIB* picked = queue[queue.size()-1-i];
            Address picked_addr = Address(picked->my_addr);
            addresses.push_back(picked_addr);
        }
        // * add peers for current rib
        (*rib)->AddPeers(addresses);

        NS_LOG_INFO("number of peers for rib " << (*rib)->my_addr << " is " << (*rib)->peers.size());
    }
}

std::pair<std::vector<RIB *>, ApplicationContainer>
installRIBs(
    std::vector<std::pair<NodeContainer, Ipv4InterfaceContainer>>& serverAssgn,
    Time start, Time stop, std::map<std::string, int> *addr_map)
{
    std::vector<RIB *> ribs;
    ApplicationContainer apps;

    for (auto& x: serverAssgn){
        RIB *rib = new RIB(x.second.GetAddress(0), addr_map);
        apps.Add(rib->Install(x.first.Get(0)));
        ribs.push_back(rib);
    }

    std::vector<Address> rib_addrs;
    for (auto &y: ribs){
        rib_addrs.push_back(y->my_addr);
    }

    for (auto &y: ribs){
        Time __gap = Seconds(0);
        Time __duration = Seconds(1);
        ApplicationContainer __app = y->InstallTraceRoute(rib_addrs, addr_map);
        for (int i = 0; i < __app.GetN(); i++){
            ApplicationContainer a;
            a.Add(__app.Get(i));
            a.Start(start + __gap);
            a.Stop(start + __gap + __duration);
            __gap += __duration;
        }
        // apps.Add(__app);
    }

    apps.Start(start);
    apps.Stop(stop);

    auto ret = std::make_pair(ribs, apps);

    return ret;
}

std::pair<std::vector<OverlaySwitch *>, ApplicationContainer>
installSwitches(
    std::vector<std::pair<NodeContainer, Ipv4InterfaceContainer>>& switchAssgn,
    std::vector<std::pair<NodeContainer, Ipv4InterfaceContainer>>& serverAssgn,
    Time start, Time stop)
{
    std::vector<OverlaySwitch *> oswitches;
    ApplicationContainer apps;

    uint32_t nas = serverAssgn.size();

    for (uint32_t i = 0; i < nas; i++){
        for (uint32_t j = 0; j < switchAssgn[i].second.GetN(); j++){
            OverlaySwitch *oswitch = new OverlaySwitch(switchAssgn[i].second.GetAddress(j), serverAssgn[i].second.GetAddress(0));
            ApplicationContainer oswitchApps(oswitch->Install(switchAssgn[i].first.Get(j)));

            oswitches.push_back(oswitch);
            apps.Add(oswitchApps);
        }
    }

    apps.Start(start);
    apps.Stop(stop);

    return std::make_pair(oswitches, apps);
}


int
main(int argc, char* argv[])
{
    LogComponentEnable("RIBAdStore", LOG_LEVEL_ALL);
    LogComponentEnable("RIBLinkStateManager", LOG_LEVEL_ALL);
    LogComponentEnable("DCServerAdvertiser", LOG_LEVEL_ALL);
    LogComponentEnable("OverlaySwitchPingClient", LOG_LEVEL_ALL);


    LogComponentEnable("TrustNet_Main", LOG_LEVEL_ALL);

    // BRITE needs a configuration file to build its graph. By default, this
    // example will use the TD_ASBarabasi_RTWaxman.conf file. There are many others
    // which can be found in the BRITE/conf_files directory
    std::string confFile = "scratch/trustnet/brite-conf.conf";
    bool tracing = false;
    bool nix = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("confFile", "BRITE conf file", confFile);
    cmd.AddValue("tracing", "Enable or disable ascii tracing", tracing);
    cmd.AddValue("nix", "Enable or disable nix-vector routing", nix);

    cmd.Parse(argc, argv);

    // Invoke the BriteTopologyHelper and pass in a BRITE
    // configuration file and a seed file. This will use
    // BRITE to build a graph from which we can build the ns-3 topology
    BriteTopologyHelper bth(confFile);
    bth.AssignStreams(3);

    PointToPointHelper p2p;

    InternetStackHelper stack;

    if (nix)
    {
        Ipv4NixVectorHelper nixRouting;
        stack.SetRoutingHelper(nixRouting);
    }

    Ipv4AddressHelper address;
    address.SetBase(TOPOLOGY_SUBNET, COMMON_MASK);

    bth.BuildBriteTopology(stack);
    bth.AssignIpv4Addresses(address);

    NS_LOG_INFO("Number of AS created " << bth.GetNAs());
    std::map<std::string, int> addr_map;

    NodeContainer client;

    client.Create(1);
    stack.Install(client);

    int numLeafNodesInAsNine = bth.GetNLeafNodesForAs(9);
    client.Add(bth.GetLeafNodeForAs(9, numLeafNodesInAsNine - 1)); // * attach the client to the final leaf of the AS-9 

    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer p2pClientDevices;
    p2pClientDevices = p2p.Install(client); // * establish a p2p link between the client and the leaf router

    address.SetBase("11.1.0.0", "255.255.0.0"); // ? Why this range? Why does the switch also have a client IP?
    Ipv4InterfaceContainer clientInterfaces;
    clientInterfaces = address.Assign(p2pClientDevices); // * assign ipv4 addresses for both endpoints of the p2p link

    address.SetBase(SERVER_SUBNET, COMMON_MASK); // * 1 server per AS
    auto serverAssgn = randomNodeAssignment(
        bth, stack, address, 0
    );

    address.SetBase(SWITCH_SUBNET, COMMON_MASK); // * 1 overlay switch out of every 10 underlay switches 
    auto switchAssgn = randomNodeAssignment(
        bth, stack, address, 0.1
    );

    auto ribs = installRIBs(serverAssgn, Seconds(0.5), Seconds(120.0), &addr_map);

    // * add peers to each RIB
    // assignRandomASPeers(ribs.first);

    for (int i = 0; i < bth.GetNAs(); i++){
        for (int j = 0; j < bth.GetNLeafNodesForAs(i); j++){
            Ptr<Node> node = bth.GetLeafNodeForAs(i, j);
            for (int k = 0; k < node->GetNDevices(); k++){
                Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
                Ipv4Address addr = ipv4->GetAddress(k, 0).GetAddress();
                // Address addr = node->GetDevice(k)->GetAddress();
                assert(addr_map.find(addr) == addr_map.end());
                std::stringstream ss;
                ss << addr;
                addr_map[ss.str()] = i;
            }
        }
        for (int j = 0; j < bth.GetNNodesForAs(i); j++){
            Ptr<Node> node = bth.GetNodeForAs(i, j);
            for (int k = 0; k < node->GetNDevices(); k++){
                Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
                Ipv4Address addr = ipv4->GetAddress(k, 0).GetAddress();
                // Address addr = node->GetDevice(k)->GetAddress();
                assert(addr_map.find(addr) == addr_map.end());
                std::stringstream ss;
                ss << addr;
                addr_map[ss.str()] = i;
            }
            
        }
    }


    // ns3::ObjectFactory fac;
    // fac.SetTypeId(DCServerAdvertiser::GetTypeId());

    // Ptr<DCServerAdvertiser> echoClient = fac.Create<DCServerAdvertiser>();
    // echoClient->SetRemote(ribs.first[9].my_addr, RIBADSTORE_PORT);
    // echoClient->SetAttribute("MaxPackets", UintegerValue(100));
    // echoClient->SetAttribute("Interval", TimeValue(Seconds(1.)));
    // echoClient->SetAttribute("PacketSize", UintegerValue(1024));

    DCServer dcs(clientInterfaces.GetAddress(0), ribs.first[3]->my_addr); // ? Why index 3
    ApplicationContainer clientApps(dcs.Install(client.Get(0)));

    for (int i = 0; i < 10; i++){
        dcs.advertiser->dcNameList.push_back("Shubham Mishra");
    }

    clientApps.Start(Seconds(4.0));
    clientApps.Stop(Seconds(15.0));

    auto switches = installSwitches(switchAssgn, serverAssgn, Seconds(0.9), Seconds(15.0));

    if (!nix)
    {
        Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    }

    if (tracing)
    {
        AsciiTraceHelper ascii;
        p2p.EnableAsciiAll(ascii.CreateFileStream("briteLeaves.tr"));
    }
    // Run the simulator
    Simulator::Stop(Seconds(600.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
