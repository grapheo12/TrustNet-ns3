#include "main.h"

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("RIBLinkStateManager");

    NS_OBJECT_ENSURE_REGISTERED(RIBLinkStateManager);

    TypeId
    RIBLinkStateManager::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::RIBLinkStateManger")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<RIBLinkStateManager>()
                .AddAttribute("Port",
                            "Port on which we listen for incoming packets.",
                            UintegerValue(RIBLSM_PORT),
                            MakeUintegerAccessor(&RIBLinkStateManager::m_port),
                            MakeUintegerChecker<uint16_t>())
                .AddAttribute("PacketWindowSize",
                            "The size of the window used to compute the packet loss. This value "
                            "should be a multiple of 8.",
                            UintegerValue(32),
                            MakeUintegerAccessor(&RIBLinkStateManager::GetPacketWindowSize,
                                                &RIBLinkStateManager::SetPacketWindowSize),
                            MakeUintegerChecker<uint16_t>(8, 256))
                .AddTraceSource("Rx",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&RIBLinkStateManager::m_rxTrace),
                                "ns3::Packet::TracedCallback")
                .AddTraceSource("RxWithAddresses",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&RIBLinkStateManager::m_rxTraceWithAddresses),
                                "ns3::Packet::TwoAddressTracedCallback");
        return tid;
    }

    RIBLinkStateManager::RIBLinkStateManager()
        : m_lossCounter(0)
    {
        NS_LOG_FUNCTION(this);
        m_received = 0;
    }

    RIBLinkStateManager::~RIBLinkStateManager()
    {
        NS_LOG_FUNCTION(this);
    }

    uint16_t
    RIBLinkStateManager::GetPacketWindowSize() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetBitMapSize();
    }

    void
    RIBLinkStateManager::SetPacketWindowSize(uint16_t size)
    {
        NS_LOG_FUNCTION(this << size);
        m_lossCounter.SetBitMapSize(size);
    }

    uint32_t
    RIBLinkStateManager::GetLost() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetLost();
    }

    uint64_t
    RIBLinkStateManager::GetReceived() const
    {
        NS_LOG_FUNCTION(this);
        return m_received;
    }

    void
    RIBLinkStateManager::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void
    RIBLinkStateManager::StartApplication()
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

        m_socket->SetRecvCallback(MakeCallback(&RIBLinkStateManager::HandleRead, this));

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

        m_socket6->SetRecvCallback(MakeCallback(&RIBLinkStateManager::HandleRead, this));
    }

    void
    RIBLinkStateManager::StopApplication()
    {
        NS_LOG_FUNCTION(this);

        if (m_socket)
        {
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }
    }

    void
    RIBLinkStateManager::HandleRead(Ptr<Socket> socket)
    {
        NS_LOG_FUNCTION(this << socket);
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
                uint32_t receivedSize = packet->GetSize();
                SeqTsHeader seqTs;
                packet->RemoveHeader(seqTs);
                
                NS_LOG_INFO("Received link state packet: " << seqTs.GetSeq() << " " << seqTs.GetTs());

                auto nameOfPeer = InetSocketAddress::ConvertFrom(from).GetIpv4(); // TODO: Change to 256 bit name
                liveSwitches.insert(nameOfPeer);
                // TODO: Handle timeout

                uint32_t currentSequenceNumber = seqTs.GetSeq();
                if (InetSocketAddress::IsMatchingType(from))
                {
                    NS_LOG_INFO("TraceDelay: RX " << receivedSize << " bytes from "
                                                << InetSocketAddress::ConvertFrom(from).GetIpv4()
                                                << " Sequence Number: " << currentSequenceNumber
                                                << " Uid: " << packet->GetUid() << " TXtime: "
                                                << seqTs.GetTs() << " RXtime: " << Simulator::Now()
                                                << " Delay: " << Simulator::Now() - seqTs.GetTs());
                }
                else if (Inet6SocketAddress::IsMatchingType(from))
                {
                    NS_LOG_INFO("TraceDelay: RX " << receivedSize << " bytes from "
                                                << Inet6SocketAddress::ConvertFrom(from).GetIpv6()
                                                << " Sequence Number: " << currentSequenceNumber
                                                << " Uid: " << packet->GetUid() << " TXtime: "
                                                << seqTs.GetTs() << " RXtime: " << Simulator::Now()
                                                << " Delay: " << Simulator::Now() - seqTs.GetTs());
                }

                m_lossCounter.NotifyReceived(currentSequenceNumber);
                m_received++;
            }
        }
    }

}