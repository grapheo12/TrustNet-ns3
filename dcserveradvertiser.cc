#include "main.h"


namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("DCServerAdvertiser");

    NS_OBJECT_ENSURE_REGISTERED(DCServerAdvertiser);

    TypeId
    DCServerAdvertiser::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::DCServerAdvertiser")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<DCServerAdvertiser>()
                .AddAttribute("MaxPackets",
                            "The maximum number of packets the application will send",
                            UintegerValue(100),
                            MakeUintegerAccessor(&DCServerAdvertiser::m_count),
                            MakeUintegerChecker<uint32_t>())
                .AddAttribute("Interval",
                            "The time to wait between packets",
                            TimeValue(Seconds(1.0)),
                            MakeTimeAccessor(&DCServerAdvertiser::m_interval),
                            MakeTimeChecker())
                .AddAttribute("RemoteAddress",
                            "The destination Address of the outbound packets",
                            AddressValue(),
                            MakeAddressAccessor(&DCServerAdvertiser::m_peerAddress),
                            MakeAddressChecker())
                .AddAttribute("RemotePort",
                            "The destination port of the outbound packets",
                            UintegerValue(RIBADSTORE_PORT),
                            MakeUintegerAccessor(&DCServerAdvertiser::m_peerPort),
                            MakeUintegerChecker<uint16_t>())
                .AddAttribute("PacketSize",
                            "Size of packets generated. The minimum packet size is 12 bytes which is "
                            "the size of the header carrying the sequence number and the time stamp.",
                            UintegerValue(1024),
                            MakeUintegerAccessor(&DCServerAdvertiser::m_size),
                            MakeUintegerChecker<uint32_t>(12, 65507));
        return tid;
    }

    DCServerAdvertiser::DCServerAdvertiser()
    {
        NS_LOG_FUNCTION(this);
        m_sent = 0;
        m_totalTx = 0;
        m_socket = nullptr;
        m_sendEvent = EventId();

        sentNum = 0;
        dcNameList.push_back("GIVEPEERS");
    }

    DCServerAdvertiser::~DCServerAdvertiser()
    {
        NS_LOG_FUNCTION(this);
    }

    void
    DCServerAdvertiser::SetRemote(Address ip, uint16_t port)
    {
        NS_LOG_FUNCTION(this << ip << port);
        m_peerAddress = ip;
        m_peerPort = port;
    }

    void
    DCServerAdvertiser::SetRemote(Address addr)
    {
        NS_LOG_FUNCTION(this << addr);
        m_peerAddress = addr;
    }

    void
    DCServerAdvertiser::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void
    DCServerAdvertiser::StartApplication()
    {
        NS_LOG_FUNCTION(this);

        if (!m_socket)
        {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
            if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
            {
                if (m_socket->Bind() == -1)
                {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                m_socket->Connect(
                    InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
            }
            else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
            {
                if (m_socket->Bind6() == -1)
                {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                m_socket->Connect(
                    Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
            }
            else if (InetSocketAddress::IsMatchingType(m_peerAddress) == true)
            {
                if (m_socket->Bind() == -1)
                {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                m_socket->Connect(m_peerAddress);
            }
            else if (Inet6SocketAddress::IsMatchingType(m_peerAddress) == true)
            {
                if (m_socket->Bind6() == -1)
                {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                m_socket->Connect(m_peerAddress);
            }
            else
            {
                NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
            }
        }

    #ifdef NS3_LOG_ENABLE
        std::stringstream peerAddressStringStream;
        if (Ipv4Address::IsMatchingType(m_peerAddress))
        {
            peerAddressStringStream << Ipv4Address::ConvertFrom(m_peerAddress);
        }
        else if (Ipv6Address::IsMatchingType(m_peerAddress))
        {
            peerAddressStringStream << Ipv6Address::ConvertFrom(m_peerAddress);
        }
        else if (InetSocketAddress::IsMatchingType(m_peerAddress))
        {
            peerAddressStringStream << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4();
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
        {
            peerAddressStringStream << Inet6SocketAddress::ConvertFrom(m_peerAddress).GetIpv6();
        }
        m_peerAddressString = peerAddressStringStream.str();
    #endif // NS3_LOG_ENABLE

        m_socket->SetRecvCallback(MakeCallback(&DCServerAdvertiser::HandlePeers, this));
        m_socket->SetAllowBroadcast(true);
        m_sendEvent = Simulator::Schedule(Seconds(1.0), &DCServerAdvertiser::Send, this);
    }

    void
    DCServerAdvertiser::HandlePeers(Ptr<Socket> socket)
    {
        NS_LOG_FUNCTION(this << socket);
        Ptr<Packet> packet;
        Address from;
        Address localAddress;
        while ((packet = socket->RecvFrom(from)))
        {
            socket->GetSockName(localAddress);
            if (packet->GetSize() > 0){
                std::stringstream ss;
                packet->CopyData(&ss, packet->GetSize());
                NS_LOG_INFO("Peer Info received: " << ss.str());
            }
        }
    }

    void
    DCServerAdvertiser::StopApplication()
    {
        NS_LOG_FUNCTION(this);
        Simulator::Cancel(m_sendEvent);
    }

    void
    DCServerAdvertiser::Send()
    {
        NS_LOG_FUNCTION(this);
        NS_ASSERT(m_sendEvent.IsExpired());

        if (dcNameList.size() == 0 || sentNum == dcNameList.size()){
            return;
        }
        SeqTsHeader seqTs;
        seqTs.SetSeq(m_sent);
        Ptr<Packet> p = Create<Packet>(0); // 8+4 : the size of the seqTs header
        p->AddHeader(seqTs);
        Ptr<Packet> __p = Create<Packet>((const uint8_t *)dcNameList[sentNum].c_str(), dcNameList[sentNum].size());
        p->AddAtEnd(__p);

        if ((m_socket->Send(p)) >= 0)
        {
            ++m_sent;
            sentNum++;
            m_totalTx += p->GetSize();
    #ifdef NS3_LOG_ENABLE
            NS_LOG_INFO("TraceDelay TX " << m_size << " bytes to " << m_peerAddressString << " Uid: "
                                        << p->GetUid() << " Time: " << (Simulator::Now()).As(Time::S));
    #endif // NS3_LOG_ENABLE
        }
    #ifdef NS3_LOG_ENABLE
        else
        {
            NS_LOG_INFO("Error while sending " << m_size << " bytes to " << m_peerAddressString);
        }
    #endif // NS3_LOG_ENABLE

        if (sentNum < dcNameList.size())
        {
            m_sendEvent = Simulator::Schedule(m_interval, &DCServerAdvertiser::Send, this);
        }
    }

    uint64_t
    DCServerAdvertiser::GetTotalTx() const
    {
        return m_totalTx;
    }
}
