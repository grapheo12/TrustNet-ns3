#include "main.h"

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("RIBCertStore");

    NS_OBJECT_ENSURE_REGISTERED(RIBCertStore);

    TypeId
    RIBCertStore::GetTypeId()
    {
        // void *x;
        static TypeId tid =
            TypeId("ns3::RIBCertStore")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<RIBCertStore>()
                .AddAttribute("Port",
                            "Port on which we listen for incoming packets.",
                            UintegerValue(RIBCERTSTORE_PORT),
                            MakeUintegerAccessor(&RIBCertStore::m_port),
                            MakeUintegerChecker<uint16_t>())
                .AddAttribute("PacketWindowSize",
                            "The size of the window used to compute the packet loss. This value "
                            "should be a multiple of 8.",
                            UintegerValue(32),
                            MakeUintegerAccessor(&RIBCertStore::GetPacketWindowSize,
                                                &RIBCertStore::SetPacketWindowSize),
                            MakeUintegerChecker<uint16_t>(8, 256))
                .AddTraceSource("Rx",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&RIBCertStore::m_rxTrace),
                                "ns3::Packet::TracedCallback")
                .AddTraceSource("RxWithAddresses",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&RIBCertStore::m_rxTraceWithAddresses),
                                "ns3::Packet::TwoAddressTracedCallback");
        return tid;
    }

    RIBCertStore::RIBCertStore()
        : m_lossCounter(0)
    {
        NS_LOG_FUNCTION(this);
        m_received = 0;
        // parent_ctx = ctx;
    }

    void RIBCertStore::SetContext(void *ctx)
    {
        parent_ctx = ctx;
    }

    RIBCertStore::~RIBCertStore()
    {
        NS_LOG_FUNCTION(this);
    }

    uint16_t
    RIBCertStore::GetPacketWindowSize() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetBitMapSize();
    }

    void
    RIBCertStore::SetPacketWindowSize(uint16_t size)
    {
        NS_LOG_FUNCTION(this << size);
        m_lossCounter.SetBitMapSize(size);
    }

    uint32_t
    RIBCertStore::GetLost() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetLost();
    }

    uint64_t
    RIBCertStore::GetReceived() const
    {
        NS_LOG_FUNCTION(this);
        return m_received;
    }

    void
    RIBCertStore::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void
    RIBCertStore::StartApplication()
    {
        NS_LOG_FUNCTION(this);
        NS_LOG_INFO("RIBCertStore started");

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

        m_socket->SetRecvCallback(MakeCallback(&RIBCertStore::HandleRead, this));

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

        m_socket6->SetRecvCallback(MakeCallback(&RIBCertStore::HandleRead, this));

    }

    void
    RIBCertStore::StopApplication()
    {
        NS_LOG_FUNCTION(this);

        if (m_socket)
        {
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }
    }

    void
    RIBCertStore::HandleRead(Ptr<Socket> socket)
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
            if (packet->GetSize() > 0){
                SeqTsHeader seqTs;
                packet->RemoveHeader(seqTs);
                uint32_t currentSequenceNumber = seqTs.GetSeq();
                uint32_t receivedSize = packet->GetSize();

                /* Packet contents:
                 * {
                 *       "issuer": "DCOwnerName:DCName | ClientName",
                 *       "type": "trust | distrust",
                 *       "entity": "entity to trust/distrust",
                 *       "r_transitivity": value
                 * } 
                 */

                std::stringstream ss;
                packet->CopyData(&ss, packet->GetSize());
                std::string data = ss.str();
                Json::Reader jsonReader;
                Json::Value jsonData;
                if (!jsonReader.parse(data, jsonData)){
                    NS_LOG_INFO("Malformed JSON");
                    continue;
                }

                if (!(
                    jsonData.isMember("issuer") &&
                    jsonData.isMember("type") &&
                    jsonData.isMember("entity")
                )){
                    NS_LOG_INFO("Malformed JSON");
                    continue;
                }

                if (jsonData["type"].asString() == "trust"){
                    if (!jsonData.isMember("r_transitivity")){
                        jsonData["r_transitivity"] = INT_MAX;
                    }
                }
                
                NS_LOG_INFO("JSON Parsed Successfully");
                if (jsonData["type"].asString() == "trust"){
                    std::pair<std::string, int> __val = std::make_pair(jsonData["entity"].asString(), jsonData["r_transitivity"].asInt());
                    trustRelations.insert(std::make_pair(
                        jsonData["issuer"].asString(), __val));
                    
                    if (jsonData["issuer"].asString().find(":") != std::string::npos){
                        std::pair<std::string, int> __val2 = std::make_pair(jsonData["issuer"].asString(), INT_MAX);
                        trustRelations.insert(std::make_pair(jsonData["entity"].asString(), __val2));
                    }
                }else if (jsonData["type"].asString() == "distrust"){
                    distrustRelations.insert(std::make_pair(
                        jsonData["issuer"].asString(), jsonData["entity"].asString()));
                }

                for (auto &x: trustRelations){
                    NS_LOG_INFO("AS" << ((RIB *)parent_ctx)->td_num << ": Trust Relation: " << x.first << " "
                        << x.second.first << " " << x.second.second);
                }

                for (auto &x: distrustRelations){
                    NS_LOG_INFO("Distrust Relation: " << x.first << " " << x.second);
                }


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