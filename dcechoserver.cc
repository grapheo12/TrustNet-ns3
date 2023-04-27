#include "main.h"

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("DCEchoServer");

    NS_OBJECT_ENSURE_REGISTERED(DCEchoServer);

    TypeId
    DCEchoServer::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::DCEchoServer")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<DCEchoServer>()
                .AddAttribute("Port",
                            "Port on which we listen for incoming packets.",
                            UintegerValue(DCSERVER_ECHO_PORT),
                            MakeUintegerAccessor(&DCEchoServer::m_port),
                            MakeUintegerChecker<uint16_t>())
                .AddAttribute("PacketWindowSize",
                            "The size of the window used to compute the packet loss. This value "
                            "should be a multiple of 8.",
                            UintegerValue(32),
                            MakeUintegerAccessor(&DCEchoServer::GetPacketWindowSize,
                                                &DCEchoServer::SetPacketWindowSize),
                            MakeUintegerChecker<uint16_t>(8, 256))
                .AddTraceSource("Rx",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&DCEchoServer::m_rxTrace),
                                "ns3::Packet::TracedCallback")
                .AddTraceSource("RxWithAddresses",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&DCEchoServer::m_rxTraceWithAddresses),
                                "ns3::Packet::TwoAddressTracedCallback");
        return tid;
    }

    DCEchoServer::DCEchoServer()
        : m_lossCounter(0)
    {
        NS_LOG_FUNCTION(this);
        m_received = 0;
        isSwitchSet = false;
    }

    DCEchoServer::~DCEchoServer()
    {
        NS_LOG_FUNCTION(this);
    }

    uint16_t
    DCEchoServer::GetPacketWindowSize() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetBitMapSize();
    }

    void
    DCEchoServer::SetPacketWindowSize(uint16_t size)
    {
        NS_LOG_FUNCTION(this << size);
        m_lossCounter.SetBitMapSize(size);
    }

    uint32_t
    DCEchoServer::GetLost() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetLost();
    }

    uint64_t
    DCEchoServer::GetReceived() const
    {
        NS_LOG_FUNCTION(this);
        return m_received;
    }

    void
    DCEchoServer::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void
    DCEchoServer::StartApplication()
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

        m_socket->SetRecvCallback(MakeCallback(&DCEchoServer::HandleRead, this));

        if (!m_socket6)
        {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket6 = Socket::CreateSocket(GetNode(), tid);
            Inet6SocketAddress local = Inet6SocketAddress(Ipv6Address::GetAny(), m_port);
            if (m_socket6->Bind(local) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
        }

        m_socket6->SetRecvCallback(MakeCallback(&DCEchoServer::HandleRead, this));

        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        switch_socket = Socket::CreateSocket(GetNode(), tid);
        if (switch_socket->Bind() == -1){
            NS_FATAL_ERROR("Failed to bind socket");
        }
        switch_socket->Connect(
            InetSocketAddress(Ipv4Address::ConvertFrom(my_rib), RIBADSTORE_PORT));
        
        switch_socket->SetRecvCallback(MakeCallback(&DCEchoServer::HandleSwitch, this));
        std::string s = "GIVESWITCHES";
        Ptr<Packet> p = Create<Packet>((const uint8_t *)s.c_str(), s.size());
        switch_socket->Send(p);
    }

    void
    DCEchoServer::HandleSwitch(Ptr<Socket> sock)
    {
        Address from;
        Ptr<Packet> p;

        while (p = sock->RecvFrom(from)){
            if (p->GetSize() > 0){
                std::stringstream ss;
                p->CopyData(&ss, p->GetSize());
                std::string temp = ss.str();

                // todo: change advertisement process into path response processing
                if (temp.find("path:") != std::string::npos) {
                    NS_LOG_INFO("DCEchoServer GIVEPATH response: " << temp);
                    
                    // * deserialize the advertisement packet
                    std::string body = temp.substr(5);


                    std::vector<std::string> path;
                    auto pos = body.find(",");
                    while (pos != std::string::npos) {
                        // Ipv4Address toAdd = Ipv4Address(body.substr(0, pos).c_str());
                        std::string toAdd = body.substr(0, pos);
                        path.push_back(toAdd);
                        body = body.substr(pos+1);
                        pos = body.find(",");
                    }


                    Ipv4Address chosen = *(switches_in_my_td.begin());      // TODO: Round robin choice
                    if (!switch_socket){
                        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
                        switch_socket = Socket::CreateSocket(GetNode(), tid);

                        switch_socket->Connect(
                            InetSocketAddress(chosen, OVERLAY_FWD));

                    }
                    
                    // * Send out the packet
                    std::string origin_server = path[path.size()-1];
                    NS_LOG_INFO("origin_server is: " << origin_server);
                    path.pop_back();
                 
                } else {
                    NS_LOG_INFO("DCEchoServer GIVESWITCHES response: " << temp);

                    std::istringstream iss(ss.str());

                    while (iss){
                        std::string addr_str = "0.0.0.0";
                        iss >> addr_str;
                        if (addr_str == "0.0.0.0"){
                            break;
                        }

                        Ipv4Address addr(addr_str.c_str());
                        switches_in_my_td.insert(addr);
                    }

                    for (auto &addr: switches_in_my_td){
                        NS_LOG_INFO("DCEchoServer Switches: " << addr);
                    }
                }

                Ipv4Address switch_addr_to_use = *(switches_in_my_td.begin());
                TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
                reply_socket = Socket::CreateSocket(GetNode(), tid);
                reply_socket->Bind();
                reply_socket->Connect(InetSocketAddress(switch_addr_to_use, OVERLAY_FWD));
                isSwitchSet = true;
            }
        }
    }

    void
    DCEchoServer::StopApplication()
    {
        NS_LOG_FUNCTION(this);

        if (m_socket)
        {
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }
    }

    void
    DCEchoServer::HandleRead(Ptr<Socket> socket)
    {
        // NS_LOG_FUNCTION(this << socket);
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
                SeqTsHeader seqTs;
                packet->RemoveHeader(seqTs);   
                uint32_t currentSequenceNumber = seqTs.GetSeq();
                size_t receivedSize = packet->GetSize();
                if (receivedSize < 16){
                    continue;
                }
                uint32_t *buff = new uint32_t[receivedSize / sizeof(uint32_t) + 2];
                uint32_t sz = packet->CopyData((uint8_t *)buff, receivedSize);
                if (sz < 16){
                    delete[] buff;
                    continue;
                }
                if (!isSwitchSet){
                    NS_LOG_INFO("Received message but no switch set, so dropping");
                    delete[] buff;
                    continue;
                }

                buff[0] = PACKET_MAGIC_DOWN;
                buff[2] = buff[1] - 1;

                Ptr<Packet> replyPacket = Create<Packet>((const uint8_t *)buff, sz);
                NS_LOG_INFO("Echoing packet");
                replyPacket->AddHeader(seqTs);
                reply_socket->Send(replyPacket);
                NS_LOG_INFO("Packet echo done");

                if (InetSocketAddress::IsMatchingType(from))
                {
                    // NS_LOG_INFO("TraceDelay: RX " << receivedSize << " bytes from "
                                                // << InetSocketAddress::ConvertFrom(from).GetIpv4()
                                                // << " Sequence Number: " << currentSequenceNumber
                                                // << " Uid: " << packet->GetUid() << " TXtime: "
                                                // << seqTs.GetTs() << " RXtime: " << Simulator::Now()
                                                // << " Delay: " << Simulator::Now() - seqTs.GetTs());
                }
                else if (Inet6SocketAddress::IsMatchingType(from))
                {
                    // NS_LOG_INFO("TraceDelay: RX " << receivedSize << " bytes from "
                                                // << Inet6SocketAddress::ConvertFrom(from).GetIpv6()
                                                // << " Sequence Number: " << currentSequenceNumber
                                                // << " Uid: " << packet->GetUid() << " TXtime: "
                                                // << seqTs.GetTs() << " RXtime: " << Simulator::Now()
                                                // << " Delay: " << Simulator::Now() - seqTs.GetTs());
                }

                m_lossCounter.NotifyReceived(currentSequenceNumber);
                m_received++;
            }
        }
    }


}