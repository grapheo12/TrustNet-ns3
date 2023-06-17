#include "main.h"

namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("OverlaySwitchNeighborProber");

    NS_OBJECT_ENSURE_REGISTERED(OverlaySwitchNeighborProber);

    TypeId
    OverlaySwitchNeighborProber::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::OverlaySwitchNeighborProber")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<OverlaySwitchNeighborProber>()
                .AddAttribute("Port",
                            "Port on which we listen for incoming packets.",
                            UintegerValue(OVERLAY_PROBER_PORT),
                            MakeUintegerAccessor(&OverlaySwitchNeighborProber::m_port),
                            MakeUintegerChecker<uint16_t>())
                .AddAttribute("Interval",
                            "The time to wait between packets",
                            TimeValue(Seconds(100.0)),
                            MakeTimeAccessor(&OverlaySwitchNeighborProber::m_interval),
                            MakeTimeChecker())
                .AddAttribute("PacketSize",
                            "Size of packets generated. The minimum packet size is 12 bytes which is "
                            "the size of the header carrying the sequence number and the time stamp.",
                            UintegerValue(1024),
                            MakeUintegerAccessor(&OverlaySwitchNeighborProber::m_size),
                            MakeUintegerChecker<uint32_t>(12, 65507))
                .AddTraceSource("Rx",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&OverlaySwitchNeighborProber::m_rxTrace),
                                "ns3::Packet::TracedCallback")
                .AddTraceSource("RxWithAddresses",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&OverlaySwitchNeighborProber::m_rxTraceWithAddresses),
                                "ns3::Packet::TwoAddressTracedCallback");
        return tid;
    }

    OverlaySwitchNeighborProber::OverlaySwitchNeighborProber()
    {
        NS_LOG_FUNCTION(this);
        m_sent = 0;
        m_totalTx = 0;
        m_socket = nullptr;
        m_sendEvent = EventId();
    }

    OverlaySwitchNeighborProber::~OverlaySwitchNeighborProber()
    {
        NS_LOG_FUNCTION(this);
    }

    void
    OverlaySwitchNeighborProber::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    std::unordered_map<int, std::pair<Address, int64_t>>& 
    OverlaySwitchNeighborProber::GetNearestPeerOSwitchMap() {
        return m_nearestOverlaySwitchInPeerTDs;
    }

    std::optional<Address> 
    OverlaySwitchNeighborProber::GetNearestOverlaySwitchInTD(int tdNumber) {
        if (m_nearestOverlaySwitchInPeerTDs.find(tdNumber) == m_nearestOverlaySwitchInPeerTDs.end()) {
            return std::nullopt;
        }
        auto [addr, rtt] = m_nearestOverlaySwitchInPeerTDs[tdNumber];
        return addr;
    }

    void
    OverlaySwitchNeighborProber::StartApplication()
    {
        NS_LOG_FUNCTION(this);

        if (!m_socket)
        {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            if (m_socket->Bind(local) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
        }

        m_socket->SetRecvCallback(MakeCallback(&OverlaySwitchNeighborProber::HandleRead, this));

        // m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        // m_socket->SetAllowBroadcast(true);
        m_sendEvent = Simulator::Schedule(Seconds(50), &OverlaySwitchNeighborProber::Probe, this);
    }

    void
    OverlaySwitchNeighborProber::StopApplication()
    {
        NS_LOG_FUNCTION(this);
        Simulator::Cancel(m_sendEvent);
    }






    void 
    OverlaySwitchNeighborProber::HandleRead(Ptr<Socket> socket) {
        Ptr<Packet> packet;
        Address from;
        Address localAddress;
        while ((packet = socket->RecvFrom(from)))
        {
            socket->GetSockName(localAddress);
            m_rxTrace(packet);
            m_rxTraceWithAddresses(packet, from, localAddress);
            if (packet->GetSize() > 0)
            {
                std::stringstream ss;
                packet->CopyData(&ss, packet->GetSize());
                std::string payload = ss.str();


                // Process the echo request packet by echoing it back to where it originates
                if (payload.size() > 11 && payload.substr(0, 11) == "ECHOREQUEST") {
                    Simulator::ScheduleNow(&OverlaySwitchNeighborProber::SimpliEchoBack, this, socket, from, payload);
                } 
                
                // Update nearest overlay switch in each peer trust domain with the response packet
                else if (payload.size() > 12 && payload.substr(0, 12) == "ECHORESPONSE") {
                    // * Format: 
                    // *     "ECHOBACK [SendTime] [TDNum]"
                    
                    const char* payloadBuf = payload.c_str();
                    int64_t sendTime = *(int64_t*)(payloadBuf+13);
                    int64_t timeNow = Simulator::Now().GetMicroSeconds();
                    int64_t timeDiff = timeNow - sendTime;

                    int from_TD = *(int*)(payloadBuf+22);
                    
                    NS_LOG_INFO("OverlaySwitchNeighborProber receives ECHORESPONSE: " << payload.substr(0, 12) << ". SendTime=" << sendTime << ", timeNow=" << timeNow << " from TD: " << from_TD);
                    from = InetSocketAddress::ConvertFrom(from).GetIpv4(); // Remove port information, only ip address is left in its representation
                    bool updated = false;
                    if (m_nearestOverlaySwitchInPeerTDs.find(from_TD) != m_nearestOverlaySwitchInPeerTDs.end()) {
                        auto& [oswitchAddress, rtt] = m_nearestOverlaySwitchInPeerTDs[from_TD];
                        if (timeDiff < rtt) {
                            updated = true;
                            m_nearestOverlaySwitchInPeerTDs[from_TD] = std::make_pair(from, timeDiff);
                        }
                    } else {
                        updated = true;
                        m_nearestOverlaySwitchInPeerTDs[from_TD] = std::make_pair(from, timeDiff);
                    }

                    NS_LOG_INFO("Nearest Overlay Switch In Peer TD Map is updated! Nearest OSwitch for TD " << from_TD << " is " << from << " with RTT = " << timeDiff);

                } else {
                    NS_LOG_INFO("OSwitchProber received unidentified packet: " << payload);
                }  
            }
        }
    }


    void OverlaySwitchNeighborProber::SimpliEchoBack(Ptr<Socket> socket, Address from, std::string& packetContent) 
    {
        OverlaySwitch* rib = (OverlaySwitch*) parent_ctx;
        int myTDNumber = global_addr_to_AS.at(rib->rib_addr);
        
        // * Create data buffer to send
        // * Format: 
        // *     "ECHORESPONSE [SendTime] [TDNum]"
        size_t bufSize = 12+1+8+1+4;
        char buf[bufSize];
        
        memcpy(buf, "ECHORESPONSE", 12);
        buf[12] = ' ';
        memcpy(buf+13, packetContent.data()+12, 8);
        buf[21] = ' ';
        *(int *)(buf+22) = myTDNumber;

        // Create socket
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

        Ptr<Socket> sock = Socket::CreateSocket(GetNode(), tid);
        // InetSocketAddress(Ipv4Address::ConvertFrom(from), OVERLAY_PROBER_PORT)
        Ipv4Address target = InetSocketAddress::ConvertFrom(from).GetIpv4();
        uint16_t port = InetSocketAddress::ConvertFrom(from).GetPort();

        NS_LOG_INFO("1234567890123489035712 from: " << target << ", port: " << port);
        sock->Connect(InetSocketAddress(target, OVERLAY_PROBER_PORT));

        
        NS_LOG_INFO("Received SimpliSendEcho request, sending back...");
        Ptr<Packet> p = Create<Packet>((const uint8_t *)buf, bufSize);
        NS_LOG_INFO("Send status: " << sock->Send(p));
    }


    void OverlaySwitchNeighborProber::SimpliEchoRequest(Ptr<Socket> socket, Address dest) 
    {
        // OverlaySwitch* rib = (OverlaySwitch*) parent_ctx;
        // int myTDNumber = global_addr_to_AS.at(rib->rib_addr);
        int64_t timeNow = Simulator::Now().GetMicroSeconds();

        // * Create data buffer to send
        // * Format: 
        // *     "ECHOREQUEST [SendTime]"
        size_t bufSize = 11 + 1 + 8;
        char buf[bufSize];

        memcpy(buf, "ECHOREQUEST", 11);
        buf[11] = ' ';
        *(int64_t *)(buf+12) = timeNow;

        // Create socket
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

        Ptr<Socket> sock = Socket::CreateSocket(GetNode(), tid);
        sock->Connect(InetSocketAddress(Ipv4Address::ConvertFrom(dest), OVERLAY_PROBER_PORT));

        
        NS_LOG_INFO("Sending SimpliEcho request");
        Ptr<Packet> p = Create<Packet>((const uint8_t *)buf, bufSize);
        NS_LOG_INFO("Send status: " << sock->Send(p));
    }

    

    void
    OverlaySwitchNeighborProber::Probe()
    {
        
        NS_LOG_FUNCTION(this);
        

        // Get addresses of all overlay switches in peering Trust Domains
        OverlaySwitch* parent_ctx = (OverlaySwitch*) (this->parent_ctx);
        const std::map<int, ns3::Ipv4Address>& peerRibAddressMap = parent_ctx->fwdEng->GetPeerRibAddressMap();
        const std::map<int, std::set<Address>>& overlaySwitchInOtherTDMap = parent_ctx->fwdEng->GetOverlaySwitchInOtherTDMap();
        for (auto it=peerRibAddressMap.begin(); it!=peerRibAddressMap.end(); it++) {
            int peerTDNum = it->first;
            if (overlaySwitchInOtherTDMap.find(peerTDNum) != overlaySwitchInOtherTDMap.end()) {
                const std::set<Address>& targets = overlaySwitchInOtherTDMap.at(peerTDNum);
                for (auto& target_oswitch_addr : targets) {
                    // Send echo request to target_oswitch_addr
                    Simulator::ScheduleNow(&OverlaySwitchNeighborProber::SimpliEchoRequest, this, m_socket, target_oswitch_addr);
                }

            }
        }

        // Ptr<Packet> p = Create<Packet>(0); // 8+4 : the size of the seqTs header

        // if ((m_socket->Send(p)) >= 0)
        // {
        //     ++m_sent;
        //     m_totalTx += p->GetSize();
        // }
        m_sendEvent = Simulator::Schedule(m_interval, &OverlaySwitchNeighborProber::Probe, this);
    }

}
