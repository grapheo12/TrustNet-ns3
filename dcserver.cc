#include "main.h"

DCServer::DCServer(Address myAddr, Address ribAddr)
{
    my_addr = myAddr;
    rib_addr = ribAddr;
}

DCServer::~DCServer()
{

}

ApplicationContainer
DCServer::Install(Ptr<Node> node)
{
    advertiserFactory.SetTypeId(DCServerAdvertiser::GetTypeId());
    advertiser = advertiserFactory.Create<DCServerAdvertiser>();
    advertiser->SetRemote(rib_addr, RIBADSTORE_PORT);
    advertiser->SetAttribute("MaxPackets", UintegerValue(100));
    advertiser->SetAttribute("Interval", TimeValue(Seconds(1.)));
    advertiser->SetAttribute("PacketSize", UintegerValue(1024));

    node->AddApplication(advertiser);
    
    ApplicationContainer app(advertiser);
    return app;
}