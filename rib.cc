#include "main.h"
#include "ribtraceroute.h"
#include <fstream>
#include <sstream>


RIB::RIB(Address myAddr, std::map<std::string, int> *addr_map)
{
    my_addr = myAddr;
    addr_map_ = addr_map;
}

RIB::~RIB()
{
    // Simulator::Destory will destory the sub-objects
}

ApplicationContainer RIB::Install(Ptr<Node> node)
{
    my_node = node;
    adStoreFactory.SetTypeId(RIBAdStore::GetTypeId());
    adStore = adStoreFactory.Create<RIBAdStore>();
    adStore->SetAttribute("Port", UintegerValue(RIBADSTORE_PORT));
    node->AddApplication(adStore);
    this->ads = &adStore->db;

    linkManagerFactory.SetTypeId(RIBLinkStateManager::GetTypeId());
    linkManager = linkManagerFactory.Create<RIBLinkStateManager>();
    linkManager->SetAttribute("Port", UintegerValue(RIBLSM_PORT));
    node->AddApplication(linkManager);
    this->liveSwitches = &linkManager->liveSwitches;

    adStore->SetContext((void *)this);
    ApplicationContainer apps;
    apps.Add(adStore);
    apps.Add(linkManager);

    return apps;
}


bool RIB::AddPeers(std::vector<Address> &addresses)
{
    for (auto a = addresses.begin(); a != addresses.end(); a++) 
    {
        peers.insert(Address(*a)); // copy and create new address instance
    }

    return true;
}

ApplicationContainer RIB::InstallTraceRoute(const std::vector<Address>& all_ribs_, std::map<std::string, int> *addr_map)
{
    ApplicationContainer trApps;
    for (auto &x: all_ribs_){
        if (x == my_addr) continue;
        RIBTraceRouteHelper trHelper(Ipv4Address::ConvertFrom(x));
        trApps.Add(trHelper.Install(this, my_node));
        std::stringstream ss;
        ss << "traces/";
        Ipv4Address::ConvertFrom(my_addr).Print(ss);
        ss << "_";
        Ipv4Address::ConvertFrom(x).Print(ss);
        ss << ".trace";
        Ptr<OutputStreamWrapper> outw = Create<OutputStreamWrapper>(ss.str(), std::ofstream::out);
        trHelper.PrintTraceRouteAt(my_node, outw, addr_map);
    }

    return trApps;
}
