#include "main.h"

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("RIBPathComputer");

    NS_OBJECT_ENSURE_REGISTERED(RIBPathComputer);

    TypeId
    RIBPathComputer::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::RIBPathComputer")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<RIBPathComputer>()
                .AddAttribute("Port",
                            "Port on which we listen for incoming packets.",
                            UintegerValue(RIBLSM_PORT),
                            MakeUintegerAccessor(&RIBPathComputer::m_port),
                            MakeUintegerChecker<uint16_t>())
                .AddAttribute("PacketWindowSize",
                            "The size of the window used to compute the packet loss. This value "
                            "should be a multiple of 8.",
                            UintegerValue(32),
                            MakeUintegerAccessor(&RIBPathComputer::GetPacketWindowSize,
                                                &RIBPathComputer::SetPacketWindowSize),
                            MakeUintegerChecker<uint16_t>(8, 256))
                .AddTraceSource("Rx",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&RIBPathComputer::m_rxTrace),
                                "ns3::Packet::TracedCallback")
                .AddTraceSource("RxWithAddresses",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&RIBPathComputer::m_rxTraceWithAddresses),
                                "ns3::Packet::TwoAddressTracedCallback");
        return tid;
    }

    RIBPathComputer::RIBPathComputer()
        : m_lossCounter(0)
    {
        NS_LOG_FUNCTION(this);
        m_received = 0;
        parent_ctx = NULL;
        trust_graph.__node_cnt = 0;
    }

    RIBPathComputer::~RIBPathComputer()
    {
        NS_LOG_FUNCTION(this);
    }

    uint16_t
    RIBPathComputer::GetPacketWindowSize() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetBitMapSize();
    }

    void
    RIBPathComputer::SetPacketWindowSize(uint16_t size)
    {
        NS_LOG_FUNCTION(this << size);
        m_lossCounter.SetBitMapSize(size);
    }

    uint32_t
    RIBPathComputer::GetLost() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetLost();
    }

    uint64_t
    RIBPathComputer::GetReceived() const
    {
        NS_LOG_FUNCTION(this);
        return m_received;
    }

    void
    RIBPathComputer::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void
    RIBPathComputer::StartApplication()
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

        m_socket->SetRecvCallback(MakeCallback(&RIBPathComputer::HandleRead, this));

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

        m_socket6->SetRecvCallback(MakeCallback(&RIBPathComputer::HandleRead, this));

        Simulator::Schedule(Seconds(0.5), &RIBPathComputer::ComputeGraph, this);
    }

    void
    RIBPathComputer::StopApplication()
    {
        NS_LOG_FUNCTION(this);

        if (m_socket)
        {
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }
    }

    void
    RIBPathComputer::HandleRead(Ptr<Socket> socket)
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
                continue;
                SeqTsHeader seqTs;
                packet->RemoveHeader(seqTs);
                
                NS_LOG_INFO("Received link state packet: " << seqTs.GetSeq() << " " << seqTs.GetTs());
                

                uint32_t currentSequenceNumber = seqTs.GetSeq();
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

    void
    RIBPathComputer::SetContext(void *ctx)
    {
        parent_ctx = ctx;
    }

    void
    RIBPathComputer::ComputeGraph()
    {
        Simulator::Schedule(Seconds(1.0), &RIBPathComputer::ComputeGraph, this);

        if (!parent_ctx){
            NS_LOG_INFO("No RIB");
            return;
        }

        RIB *rib = (RIB *)parent_ctx;
        if (!(rib->trustRelations && rib->distrustRelations)){
            NS_LOG_INFO("No Trust/Distrust Relations");
            return;
        }

        NS_LOG_INFO("From RIB: AS" << rib->rib_addr_map_[rib->my_addr] << " Recalculating Trust Relation Graph...");

        for (auto &x: *(rib->trustRelations)){
            NS_LOG_INFO(x.first << " -> " << x.second.first);
            if (trust_graph.nodes_to_id.find(x.first) == trust_graph.nodes_to_id.end()){
                trust_graph.nodes_to_id[x.first] = trust_graph.__node_cnt++;
            }

            if (trust_graph.nodes_to_id.find(x.second.first) == trust_graph.nodes_to_id.end()){
                trust_graph.nodes_to_id[x.second.first] = trust_graph.__node_cnt++;
            }

            int id1 = trust_graph.nodes_to_id[x.first];
            int id2 = trust_graph.nodes_to_id[x.second.first];
            auto it2 = trust_graph.trust_edges.equal_range(id1);
            bool found = false;
            for (auto __it = it2.first; __it != it2.second; __it++){
                if (__it->second == id2){
                    found = true;
                    break;
                }
            }
            if (!found){
                trust_graph.trust_edges.insert({id1, id2});
            }

            if (x.second.second != INT_MAX){
                // Only add an entry if the r_transitivity is not infinite.
                // Typically only DCOwners and Users can specify r_transitivity
                trust_graph.transitivity[{id1, id2}] = x.second.second;
            }
        }

        for (auto &x: *(rib->distrustRelations)){
            if (trust_graph.nodes_to_id.find(x.first) == trust_graph.nodes_to_id.end()){
                trust_graph.nodes_to_id[x.first] = trust_graph.__node_cnt++;
            }

            if (trust_graph.nodes_to_id.find(x.second) == trust_graph.nodes_to_id.end()){
                trust_graph.nodes_to_id[x.second] = trust_graph.__node_cnt++;
            }

            int id1 = trust_graph.nodes_to_id[x.first];
            int id2 = trust_graph.nodes_to_id[x.second];

            auto it2 = trust_graph.distrust_edges.equal_range(id1);
            bool found = false;
            for (auto __it = it2.first; __it != it2.second; __it++){
                if (__it->second == id2){
                    found = true;
                    break;
                }
            }
            if (!found){
                trust_graph.distrust_edges.insert({id1, id2});
            }
        }

        for (auto &x: trust_graph.nodes_to_id){
            NS_LOG_INFO("Node Entry: " << x.first << "\t" << x.second);
        }
        for (auto &x: trust_graph.trust_edges){
            auto it = trust_graph.transitivity.find({x.first, x.second});
            if (it != trust_graph.transitivity.end()){
                NS_LOG_INFO("Trust Edge: " << x.first << " -> " << x.second << " (" << it->second << ")");
            }else{
                NS_LOG_INFO("Trust Edge: " << x.first << " -> " << x.second);
            }
        }
        for (auto &x: trust_graph.distrust_edges){
            NS_LOG_INFO("Distrust Edge: " << x.first << " -> " << x.second);
        }
    }
}