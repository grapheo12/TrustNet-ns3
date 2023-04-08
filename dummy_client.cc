#include "main.h"
#include <string>
#include <sstream>

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("DummyClient");

    NS_OBJECT_ENSURE_REGISTERED(DummyClient);

    TypeId
    DummyClient::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::DummyClient")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<DummyClient>()
                .AddAttribute("MaxPackets",
                            "The maximum number of packets the application will send",
                            UintegerValue(100),
                            MakeUintegerAccessor(&DummyClient::m_count),
                            MakeUintegerChecker<uint32_t>())
                .AddAttribute("Interval",
                            "The time to wait between packets",
                            TimeValue(Seconds(1.0)),
                            MakeTimeAccessor(&DummyClient::m_interval),
                            MakeTimeChecker())
                .AddAttribute("RemoteAddress",
                            "The destination Address of the outbound packets",
                            AddressValue(),
                            MakeAddressAccessor(&DummyClient::m_peerAddress),
                            MakeAddressChecker())
                .AddAttribute("RemotePort",
                            "The destination port of the outbound packets",
                            UintegerValue(RIBADSTORE_PORT),
                            MakeUintegerAccessor(&DummyClient::m_peerPort),
                            MakeUintegerChecker<uint16_t>())
                .AddAttribute("PacketSize",
                            "Size of packets generated. The minimum packet size is 12 bytes which is "
                            "the size of the header carrying the sequence number and the time stamp.",
                            UintegerValue(1024),
                            MakeUintegerAccessor(&DummyClient::m_size),
                            MakeUintegerChecker<uint32_t>(12, 65507));
        return tid;
    }

    DummyClient::DummyClient()
    {
        NS_LOG_FUNCTION(this);
        m_sent = 0;
        m_totalTx = 0;
        m_socket = nullptr;
        m_sendEvent = EventId();
    }

    DummyClient::~DummyClient()
    {
        NS_LOG_FUNCTION(this);
    }

    void
    DummyClient::SetRemote(Address ip, uint16_t port)
    {
        NS_LOG_FUNCTION(this << ip << port);
        m_peerAddress = ip;
        m_peerPort = port;
    }

    void
    DummyClient::SetRemote(Address addr)
    {
        NS_LOG_FUNCTION(this << addr);
        m_peerAddress = addr;
    }

    void
    DummyClient::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void
    DummyClient::StartApplication()
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

        m_socket->SetRecvCallback(MakeCallback(&DummyClient::HandleSwitch, this));
        m_socket->SetAllowBroadcast(true);
        m_sendEvent = Simulator::Schedule(Seconds(0.1), &DummyClient::GetSwitch, this); // * first fetch local TD's live switches

        NS_LOG_INFO("STartkagnljdkgs");
    }

    void
    DummyClient::GetSwitch()
    {
        NS_LOG_INFO("DummyClient:GetSwitch");
        std::string s = "GIVESWITCHES";
        Ptr<Packet> p = Create<Packet>((const uint8_t *)s.c_str(), s.size());

        if (m_socket){
            m_socket->Send(p);   
        }
    }

    void
    DummyClient::HandleSwitch(Ptr<Socket> sock)
    {
        Address from;
        Ptr<Packet> p;

        while (p = sock->RecvFrom(from)){
            if (p->GetSize() > 0){
                std::stringstream ss;
                p->CopyData(&ss, p->GetSize());
                NS_LOG_INFO("Dummy Client GIVESWITCHES response: " << ss.str());

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
                    NS_LOG_INFO("Dummy Client Switches: " << addr);
                }

                if (switches_in_my_td.size() == 0){
                    continue;
                }

                Ipv4Address chosen = *(switches_in_my_td.begin());      // TODO: Round robin choice
                if (!switch_socket){
                    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
                    switch_socket = Socket::CreateSocket(GetNode(), tid);

                    switch_socket->Connect(
                        InetSocketAddress(chosen, OVERLAY_FWD));

                    Simulator::ScheduleNow(&DummyClient::Send, this);
                }
            }
        }
    }

    void
    DummyClient::StopApplication()
    {
        NS_LOG_FUNCTION(this);
        Simulator::Cancel(m_sendEvent);
    }

    void
    DummyClient::Send()
    {
        NS_LOG_FUNCTION(this);
        NS_ASSERT(m_sendEvent.IsExpired());

        SeqTsHeader seqTs;
        seqTs.SetSeq(m_sent);
        uint32_t buff[25];
        buff[0] = PACKET_MAGIC;
        buff[1] = 3;
        buff[2] = 0;
        buff[3] = 0;
        Ipv4Address addr("11.0.0.1");
        buff[4] = addr.Get();
        buff[5] = 4002;
        buff[6] = 3;
        buff[7] = 1;
        buff[8] = 2;
        for (int i = 0; i < 16; i++){ // * signature is all 0 for dummy client
            buff[9 + i] = 0;
        }
        Ptr<Packet> p = Create<Packet>((uint8_t *)buff, 100); // 8+4 : the size of the seqTs header
        p->AddHeader(seqTs);

        if ((switch_socket->Send(p)) >= 0)
        {
            ++m_sent;
            m_totalTx += p->GetSize();
    #ifdef NS3_LOG_ENABLE
            // NS_LOG_INFO("TraceDelay TX " << m_size << " bytes to " << m_peerAddressString << " Uid: "
                                        // << p->GetUid() << " Time: " << (Simulator::Now()).As(Time::S));
    #endif // NS3_LOG_ENABLE
        }
    #ifdef NS3_LOG_ENABLE
        else
        {
            // NS_LOG_INFO("Error while sending " << m_size << " bytes to " << m_peerAddressString);
        }
    #endif // NS3_LOG_ENABLE


        m_sendEvent = Simulator::Schedule(m_interval, &DummyClient::Send, this);
    }

    uint64_t
    DummyClient::GetTotalTx() const
    {
        return m_totalTx;
    }
}