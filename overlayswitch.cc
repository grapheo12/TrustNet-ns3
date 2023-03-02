#include "main.h"

OverlaySwitch::OverlaySwitch(Address myAddr, Address ribAddr)
{
    my_addr = myAddr;
    rib_addr = ribAddr;
}

OverlaySwitch::~OverlaySwitch()
{

}

ApplicationContainer
OverlaySwitch::Install(Ptr<Node> node)
{
    pingClientFactory.SetTypeId(OverlaySwitchPingClient::GetTypeId());
    pingClient = pingClientFactory.Create<OverlaySwitchPingClient>();
    pingClient->SetRemote(rib_addr, RIBLSM_PORT);
    pingClient->SetAttribute("MaxPackets", UintegerValue(100));
    pingClient->SetAttribute("Interval", TimeValue(Seconds(1.)));
    pingClient->SetAttribute("PacketSize", UintegerValue(1024));

    node->AddApplication(pingClient);
    
    ApplicationContainer app(pingClient);
    return app;
}