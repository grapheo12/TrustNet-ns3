#include "main.h"

OverlaySwitch::OverlaySwitch(int td_num_, Address myAddr, Address ribAddr, Time peer_calc_delay_)
{
    my_addr = myAddr;
    rib_addr = ribAddr;
    td_num = td_num_;
    peer_calc_delay = peer_calc_delay_;
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
    fwdEng->peer_calc_delay = peer_calc_delay;
    fwdEng->rib_addr = rib_addr;

    node->AddApplication(fwdEng);
    
    ApplicationContainer app;
    app.Add(pingClient);
    app.Add(fwdEng);
    return app;
}