#include "main.h"

OverlaySwitch::OverlaySwitch(int td_num_, Address myAddr, Address ribAddr)
{
    my_addr = myAddr;
    rib_addr = ribAddr;
    td_num = td_num_;
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


    fwdEngFactory.SetTypeId(OverlaySwitchForwardingEngine::GetTypeId());
    fwdEng = fwdEngFactory.Create<OverlaySwitchForwardingEngine>();
    fwdEng->td_num = td_num;

    node->AddApplication(fwdEng);
    
    ApplicationContainer app;
    app.Add(pingClient);
    app.Add(fwdEng);
    return app;
}