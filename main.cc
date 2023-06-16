#include "main.h"
#include <vector>
#include <random>

#define TOPOLOGY_SUBNET "10.0.0.0"
#define SERVER_SUBNET   "11.0.0.0"
#define CLIENT_SUBNET(x)   "9." x ".0.0"
#define CLIENT_NAME(x)  client##x
#define SWITCH_SUBNET   "8.0.0.0"
#define COMMON_MASK     "255.255.255.0"
#define DCSERVER_AS     1
#define GLOBAL_STOP_TIME 600.0

#define BUILD_P2P(name, as, addr)    NodeContainer name;\
name.Create(1);\
stack.Install(name);\
int numLeafNodesInAs##as##name = bth.GetNLeafNodesForAs((as));\
name.Add(bth.GetLeafNodeForAs((as), numLeafNodesInAs##as##name - 1));\
p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));\
p2p.SetChannelAttribute("Delay", StringValue("2ms"));\
NetDeviceContainer p2p##name##Devices;\
p2p##name##Devices = p2p.Install(name);\
address.SetBase(addr, "255.255.0.0");\
Ipv4InterfaceContainer name##Interfaces;\
name##Interfaces = address.Assign(p2p##name##Devices);


#define BUILD_CLIENT(id, name, factory, appContainer)    {\
Ptr<DummyClient2> dummyClient2 = factory.Create<DummyClient2>();\
dummyClient2->SetRemote(ribs.first[(id)]->my_addr, RIBADSTORE_PORT);\
dummyClient2->SetAttribute("MaxPackets", UintegerValue(100));\
dummyClient2->SetAttribute("Interval", TimeValue(Seconds(1.)));\
dummyClient2->SetAttribute("PacketSize", UintegerValue(1024));\
dummyClient2->SetAttribute("Name", StringValue(name));\
for (auto str : generated_names) {\
    dummyClient2->dcnames_to_route.insert("fogrobotics:" + str);\
}\
for (auto& ref : dummyClient2->dcnames_to_route) {\
    NS_LOG_INFO("DC name dummy client 2 will try to send message: " << ref);\
}\
dummyClient2->my_ip = CLIENT_NAME(id).Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetAddress();\
CLIENT_NAME(id).Get(0)->AddApplication(dummyClient2);\
appContainer.Add(dummyClient2);\
}

NS_LOG_COMPONENT_DEFINE("TrustNet_Main");

/* Global map for mapping AS with their addresses*/
std::map<int, Address> global_AS_to_addr = {};
std::map<Address, int> global_addr_to_AS = {};

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
        uint32_t nleaf = bth.GetNNodesForAs(i);
        uint32_t nserver = (uint32_t)(load_ratio * nleaf);
        if (nserver == 0) nserver++;

        NS_LOG_INFO("AS: " << i << " Nodes: " << nleaf << " Nodes to create: " << nserver);
        std::default_random_engine eng;
        std::uniform_int_distribution<uint32_t> dist(0, nserver - 1);

        NodeContainer servers;
        Ipv4InterfaceContainer interfaces;
        servers.Create(nserver);
        stack.Install(servers);

        for (uint32_t j = 0; j < nserver; j++){
            NodeContainer __nodes;
            __nodes.Add(servers.Get(j));
            __nodes.Add(bth.GetNodeForAs(i, dist(eng)));
            auto __p2pdevs = p2p.Install(__nodes);
            Ipv4InterfaceContainer __interfaces = address.Assign(__p2pdevs);
            interfaces.Add(__interfaces.Get(0));
        }

        assgn.push_back(std::make_pair(servers, interfaces));
    }

    return assgn;
}

// void assignRandomASPeers(const std::vector<RIB *>& ribs) 
// {
//     auto rng = std::default_random_engine {};
//     std::vector<RIB *> queue = ribs; // copies the ribs vector

//     for (auto rib = ribs.begin(); rib != ribs.end(); rib++)
//     {
//         // * seeding is turned off so that each run of the program has the same series of random numbers
//         // srand((unsigned) time(NULL)); 
//         int num_peers = rand() % (ribs.size() / 2);
//         std::shuffle(queue.begin(), queue.end(), rng);

//         std::vector<Address> addresses;
//         for (int i = 0; i < num_peers; i++)
//         {
//             RIB* picked = queue[queue.size()-1-i];
//             Address picked_addr = Address(picked->my_addr);
//             addresses.push_back(picked_addr);
//         }
//         // * add peers for current rib
//         (*rib)->AddPeers(addresses);

//         NS_LOG_INFO("number of peers for rib " << (*rib)->my_addr << " is " << (*rib)->peers.size());
//     }
// }

std::string gen_random(const int len) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    std::string tmp_s;
    tmp_s.reserve(len);

    for (int i = 0; i < len; ++i) {
        tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    
    return tmp_s;
}

std::pair<std::vector<RIB *>, ApplicationContainer>
installRIBs(
    std::vector<std::pair<NodeContainer, Ipv4InterfaceContainer>>& serverAssgn,
    Time start, Time stop, std::map<std::string, int> *addr_map)
{
    std::vector<RIB *> ribs;
    ApplicationContainer apps;

    for (size_t i = 0; i < serverAssgn.size(); i++){
        auto &x = serverAssgn[i];
        RIB *rib = new RIB(i, x.second.GetAddress(0), addr_map);
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
        for (uint32_t i = 0; i < __app.GetN(); i++){
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
            OverlaySwitch *oswitch = new OverlaySwitch(
                i, switchAssgn[i].second.GetAddress(j),
                serverAssgn[i].second.GetAddress(0), Seconds(15.0));
            ApplicationContainer oswitchApps(oswitch->Install(switchAssgn[i].first.Get(j)));

            oswitches.push_back(oswitch);
            apps.Add(oswitchApps);
        }
    }

    apps.Start(start);
    apps.Stop(stop);

    return std::make_pair(oswitches, apps);
}

void CreateAndEnqueueAds(const DCServer& dc_server, const std::string& dc_name) {
    // creat advertisement packet
    Json::Value serializeRoot;
    serializeRoot["dc_name"] = dc_name;
    
    Ipv4Address origin_AS_addr = Ipv4Address::ConvertFrom(dc_server.rib_addr);
    std::stringstream ss;
    origin_AS_addr.Print(ss);
    serializeRoot["origin_AS"] = ss.str();

    //  add dc server's IP into the serialization because client needs it when sending packets
    Ipv4Address origin_server_addr = Ipv4Address::ConvertFrom(dc_server.my_addr);
    std::stringstream ss2;
    origin_server_addr.Print(ss2);
    serializeRoot["origin_server"] = ss2.str();
    
    // serialize the packet
    Json::StyledWriter writer;
    std::string advertisement = writer.write(serializeRoot);
    NS_LOG_INFO("advertisement to advertise is: " << advertisement);
    // add name to the name list to be advertised
    dc_server.advertiser->dcNameList.push_back(advertisement);

    
}

void CreateAndEnqueueCert(const DCServer& dc_server, const std::string& dc_name, Ptr<DCOwner> dc_owner) {
    Ipv4Address origin_server_addr = Ipv4Address::ConvertFrom(dc_server.my_addr);
    std::stringstream ss;
    origin_server_addr.Print(ss);

    DCOwner::CertInfo cinfo;
    cinfo.entity = ss.str();
    cinfo.type = "trust";
    cinfo.r_transitivity = 100;
    cinfo.rib_addr = dc_server.rib_addr;
    cinfo.issuer = dc_name;
    dc_owner->certs_to_send.push_back(cinfo);
}


int
main(int argc, char* argv[])
{
    LogComponentEnable("RIBAdStore", LOG_LEVEL_ALL);
    LogComponentEnable("RIBCertStore", LOG_LEVEL_ALL);
    LogComponentEnable("RIBLinkStateManager", LOG_LEVEL_ALL);
    LogComponentEnable("RIBPathComputer", LOG_LEVEL_ALL);
    LogComponentEnable("DCServerAdvertiser", LOG_LEVEL_ALL);
    LogComponentEnable("OverlaySwitchPingClient", LOG_LEVEL_ALL);
    LogComponentEnable("OverlaySwitchForwardingEngine", LOG_LEVEL_ALL);
    LogComponentEnable("DummyClient", LOG_LEVEL_ALL);
    LogComponentEnable("DummyClient2", LOG_LEVEL_ALL);
    LogComponentEnable("DCOwner", LOG_LEVEL_ALL);
    LogComponentEnable("DCEchoServer", LOG_LEVEL_ALL);
    LogComponentEnable("TrustNet_Main", LOG_LEVEL_ALL);
    LogComponentEnable("OverlaySwitchNeighborProber", LOG_LEVEL_ALL);

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

    // ! Testing: Multiple DC servers in different domains
    BUILD_P2P(dcStore1, 1, "11.1.0.0")
    BUILD_P2P(dcStore2, 2, "11.2.0.0")
    BUILD_P2P(dcStore3, 3, "11.4.0.0") // ! 11.3.0.0 is taken, so this is 11.4.0.0 for now. Need justification...

    BUILD_P2P(client2, 2, CLIENT_SUBNET("2"));
    BUILD_P2P(client3, 3, CLIENT_SUBNET("3"));
    BUILD_P2P(client4, 4, CLIENT_SUBNET("4"));
    BUILD_P2P(client5, 5, CLIENT_SUBNET("5"));
    BUILD_P2P(client6, 6, CLIENT_SUBNET("6"));
    BUILD_P2P(client7, 7, CLIENT_SUBNET("7"));
    BUILD_P2P(client8, 8, CLIENT_SUBNET("8"));
    BUILD_P2P(client9, 9, CLIENT_SUBNET("9"));

    // Same AS as the DCServerAdvertiser below.
    BUILD_P2P(dcOwner, DCSERVER_AS, "11.3.0.0")


    address.SetBase(SERVER_SUBNET, COMMON_MASK); // * 1 server per AS
    auto serverAssgn = randomNodeAssignment(
        bth, stack, address, 0
    );

    address.SetBase(SWITCH_SUBNET, COMMON_MASK); // * 1 overlay switch out of every 10 underlay switches 
    auto switchAssgn = randomNodeAssignment(
        bth, stack, address, 0.1
    );

    auto ribs = installRIBs(serverAssgn, Seconds(0.5), Seconds(GLOBAL_STOP_TIME), &addr_map);

    // * add peers to each RIB
    // assignRandomASPeers(ribs.first);

    for (uint32_t i = 0; i < bth.GetNAs(); i++){
        for (uint32_t j = 0; j < bth.GetNLeafNodesForAs(i); j++){
            Ptr<Node> node = bth.GetLeafNodeForAs(i, j);
            for (uint32_t k = 0; k < node->GetNDevices(); k++){
                Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
                Ipv4Address addr = ipv4->GetAddress(k, 0).GetAddress();
                // Address addr = node->GetDevice(k)->GetAddress();
                // assert(addr_map.find(addr) == addr_map.end());
                std::stringstream ss;
                ss << addr;
                addr_map[ss.str()] = i;
            }
        }
        for (uint32_t j = 0; j < bth.GetNNodesForAs(i); j++){
            Ptr<Node> node = bth.GetNodeForAs(i, j);
            for (uint32_t k = 0; k < node->GetNDevices(); k++){
                Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
                Ipv4Address addr = ipv4->GetAddress(k, 0).GetAddress();
                // Address addr = node->GetDevice(k)->GetAddress();
                // assert(addr_map.find(addr) == addr_map.end());
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


    ns3::ObjectFactory dcOwnerFactory; 
    dcOwnerFactory.SetTypeId(DCOwner::GetTypeId());
    Ptr<DCOwner> dco = dcOwnerFactory.Create<DCOwner>();
    dco->my_name = "fogrobotics";

    dcOwner.Get(0)->AddApplication(dco);
    ApplicationContainer dcoApp(dco);

    // ! Installing multiple DC servers
    DCServer dcs1(dcStore1Interfaces.GetAddress(0), ribs.first[1]->my_addr);
    ApplicationContainer dcApps1(dcs1.Install(dcStore1.Get(0)));

    DCServer dcs2(dcStore2Interfaces.GetAddress(0), ribs.first[2]->my_addr);
    ApplicationContainer dcApps2(dcs2.Install(dcStore2.Get(0)));

    DCServer dcs3(dcStore3Interfaces.GetAddress(0), ribs.first[3]->my_addr);
    ApplicationContainer dcApps3(dcs3.Install(dcStore3.Get(0)));

    // SECTION - Preparing advertisement and certificates to all DataCapsule servers
    std::set<std::string> generated_names;
    for (int i = 0; i < 10; i++){
        // generate new dc names
        std::string random_dc_name = gen_random(256);
        generated_names.insert(random_dc_name);

        CreateAndEnqueueAds(dcs1, random_dc_name);
        CreateAndEnqueueCert(dcs1, random_dc_name, dco);

        CreateAndEnqueueAds(dcs2, random_dc_name);
        CreateAndEnqueueCert(dcs2, random_dc_name, dco);

        CreateAndEnqueueAds(dcs3, random_dc_name);
        CreateAndEnqueueCert(dcs3, random_dc_name, dco);
    }



    dcoApp.Start(Seconds(13.0));
    dcoApp.Stop(Seconds(GLOBAL_STOP_TIME));

    dcApps1.Start(Seconds(30.0));
    dcApps1.Stop(Seconds(GLOBAL_STOP_TIME));

    dcApps2.Start(Seconds(30.0));
    dcApps2.Stop(Seconds(GLOBAL_STOP_TIME));

    dcApps3.Start(Seconds(30.0));
    dcApps3.Stop(Seconds(GLOBAL_STOP_TIME));

    auto switches = installSwitches(switchAssgn, serverAssgn, Seconds(0.9), Seconds(GLOBAL_STOP_TIME));


    ns3::ObjectFactory clientFactory; 
    clientFactory.SetTypeId(DummyClient2::GetTypeId());
    ApplicationContainer dummyClientApps;
    BUILD_CLIENT(2, "user:2", clientFactory, dummyClientApps)
    // BUILD_CLIENT(3, "user:3", clientFactory, dummyClientApps)
    // BUILD_CLIENT(4, "user:4", clientFactory, dummyClientApps)
    // BUILD_CLIENT(5, "user:5", clientFactory, dummyClientApps)
    // BUILD_CLIENT(6, "user:6", clientFactory, dummyClientApps)
    // BUILD_CLIENT(7, "user:7", clientFactory, dummyClientApps)
    // BUILD_CLIENT(8, "user:8", clientFactory, dummyClientApps)
    // BUILD_CLIENT(9, "user:9", clientFactory, dummyClientApps)
    dummyClientApps.Start(Seconds(300.0));
    dummyClientApps.Stop(Seconds(GLOBAL_STOP_TIME));

    MobilityHelper mh;
    mh.InstallAll();
    std::default_random_engine rGen;
    std::normal_distribution<double> rDist(0.0, 10.0);

    for (unsigned int i = 0; i < bth.GetNAs(); i++){
        double asX = 200 * cos(2 * 3.14 * (double)i / bth.GetNAs());
        double asY = 200 * sin(2 * 3.14 * (double)i / bth.GetNAs());
        for (unsigned int j = 0; j < bth.GetNNodesForAs(i); j++){
            Ptr<Node> n = bth.GetNodeForAs(i, j);
            double r = rDist(rGen);
            double x = asX + r * cos(2 * 3.14 * (double)j / bth.GetNNodesForAs(i));
            double y = asY + r * sin(2 * 3.14 * (double)j / bth.GetNNodesForAs(i));
            n->GetObject<MobilityModel>()->SetPosition({x, y, 0});
        }

        for (unsigned int j = 0; j < bth.GetNLeafNodesForAs(i); j++){
            Ptr<Node> n = bth.GetLeafNodeForAs(i, j);
            double r = 5 + rDist(rGen);
            double x = asX + r * cos(2 * 3.14 * (double)j / bth.GetNLeafNodesForAs(i));
            double y = asY + r * sin(2 * 3.14 * (double)j / bth.GetNLeafNodesForAs(i));
            n->GetObject<MobilityModel>()->SetPosition({x, y, 0});
        }

        for (unsigned int j = 0; j < switchAssgn[i].first.GetN(); j++){
            Ptr<Node> n = switchAssgn[i].first.Get(j);
            double r = 10 + rDist(rGen);
            double x = asX + r * cos(2 * 3.14 * (double)j / switchAssgn[i].first.GetN());
            double y = asY + r * sin(2 * 3.14 * (double)j / switchAssgn[i].first.GetN());
            n->GetObject<MobilityModel>()->SetPosition({x, y, 0});
        }

        for (unsigned int j = 0; j < serverAssgn[i].first.GetN(); j++){
            Ptr<Node> n = serverAssgn[i].first.Get(j);
            double r = 10 + rDist(rGen);
            double x = asX + r * cos(2 * 3.14 * (double)j / serverAssgn[i].first.GetN());
            double y = asY + r * sin(2 * 3.14 * (double)j / serverAssgn[i].first.GetN());
            n->GetObject<MobilityModel>()->SetPosition({x, y, 0});
        }
    }



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
    Simulator::Stop(Seconds(GLOBAL_STOP_TIME));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
