#include "main.h"
#include "ribtraceroute.h"
#include <fstream>
#include <sstream>


RIB::RIB(int td_num_, Address myAddr, std::map<std::string, int> *addr_map)
{
    my_addr = myAddr;
    addr_map_ = addr_map;
    td_num = td_num_;
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

    pathComputerFactory.SetTypeId(RIBPathComputer::GetTypeId());
    pathComputer = pathComputerFactory.Create<RIBPathComputer>();
    pathComputer->SetAttribute("Port", UintegerValue(RIBPATHCOMPUTER_PORT));
    node->AddApplication(pathComputer);

    certStoreFactory.SetTypeId(RIBCertStore::GetTypeId());
    certStore = certStoreFactory.Create<RIBCertStore>();
    certStore->SetAttribute("Port", UintegerValue(RIBCERTSTORE_PORT));
    node->AddApplication(certStore);
    this->trustRelations = &certStore->trustRelations;
    this->distrustRelations = &certStore->distrustRelations;

    linkManager->SetContext((void *)this);
    adStore->SetContext((void *)this);
    certStore->SetContext((void *)this);
    pathComputer->SetContext((void *)this);
    
    ApplicationContainer apps;
    apps.Add(adStore);
    apps.Add(linkManager);
    apps.Add(certStore);

    return apps;
}


bool RIB::AddPeers(std::vector<std::pair<int, Address>> &addresses)
{
    for (auto a = addresses.begin(); a != addresses.end(); a++) 
    {
        peers[a->first] = a->second; // copy and create new address instance
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
        ss << "results-1/traces/";
        Ipv4Address::ConvertFrom(my_addr).Print(ss);
        ss << "_";
        Ipv4Address::ConvertFrom(x).Print(ss);
        ss << ".trace";
        Ptr<OutputStreamWrapper> outw = Create<OutputStreamWrapper>(ss.str(), std::ofstream::out);
        trHelper.PrintTraceRouteAt(my_node, outw, addr_map);
    }

    for (size_t i = 0; i < all_ribs_.size(); i++){
        rib_addr_map_[all_ribs_[i]] = i;
    }

    return trApps;
}
