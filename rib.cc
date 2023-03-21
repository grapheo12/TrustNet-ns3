#include "main.h"

RIB::RIB(Address myAddr)
{
    my_addr = myAddr;
}

RIB::~RIB()
{
    // Simulator::Destory will destory the sub-objects
}

ApplicationContainer RIB::Install(Ptr<Node> node)
{
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