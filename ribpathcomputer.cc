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
    RIBPathComputer::SendPath(Ptr<Socket> socket, Address dest, std::string path) 
    {
        std::string str_repr = "path:" + path;
        NS_LOG_INFO("path to send: " << str_repr);
        Ptr<Packet> p = Create<Packet>((const uint8_t *)str_repr.c_str(), str_repr.size());
        NS_LOG_INFO("Send to client " << socket->SendTo(p, 0, dest));
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
                std::stringstream ss;
                packet->CopyData(&ss, packet->GetSize());
                std::string payload = ss.str();
                // continue;
                // SeqTsHeader seqTs;
                // packet->RemoveHeader(seqTs);
                if (payload.find("GIVEPATH") != std::string::npos) {
                    NS_LOG_INFO("RibPathComputer got packet: " << payload);
                    Json::Value root;
                    Json::Reader reader;
                    bool parsingSuccessful = reader.parse(payload.substr(9), root);
                    if (!parsingSuccessful) {
                        NS_LOG_WARN("GIVEPATH request cannot be parsed correctly");
                    }
                    std::string client_name = root["client_name"].asString();
                    std::string dc_name = root["dc_name"].asString();

                    
                    RIB* rib = (RIB *) (this->parent_ctx);
                    auto ptr = rib->trustRelations->find(dc_name);
                    if (ptr == rib->trustRelations->end()) {
                        NS_LOG_WARN("Unable to find the destination DC name in RIB");
                        continue;
                    }
                    std::string dc_server_ip = ptr->second.first;
                    std::vector<std::string> path_vec = GetPath(client_name, dc_server_ip);

                    
                    std::string path = "";
                    for (auto& ip : path_vec) {
                        if (ip == "me") {
                            int as_number = global_addr_to_AS.at(rib->my_addr);
                            ip = "AS" + std::to_string(as_number);
                        }
                        if (ip.find("AS") != std::string::npos) {
                            path.append(ip + ",");
                        }
                    }
                    // * add the destination ip into the path
                    path.append(path_vec[path_vec.size()-1]+",");
                    // path = path.substr(0, path.size()-1); // trim the last ","


                    // * Send the path to the client
                    Simulator::ScheduleNow(&RIBPathComputer::SendPath, this, socket, from, path);
                    
                }                

                // NS_LOG_INFO("Received link state packet: " << seqTs.GetSeq() << " " << seqTs.GetTs());
                

                // uint32_t currentSequenceNumber = seqTs.GetSeq();
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

                // m_lossCounter.NotifyReceived(currentSequenceNumber);
                m_received++;
            }
        }
    }

    void
    RIBPathComputer::SetContext(void *ctx)
    {
        parent_ctx = ctx;
    }

    bool
    RIBPathComputer::isItMe(std::string entity)
    {
        if (entity == "me") return true;
        
        RIB *rib = (RIB *)(this->parent_ctx);
        std::stringstream ss, ss2;
        ss << "AS" << rib->td_num;
        if (entity == ss.str()) return true;

        ss2 << Ipv4Address::ConvertFrom(rib->my_addr);
        if (entity == ss2.str()) return true;

        return false;

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
            // The entity "me", "AS#" and <my ip> should all be merged to "me"
            std::string s1 = isItMe(x.first) ? "me" : x.first;
            std::string s2 = isItMe(x.second.first) ? "me" : x.second.first;

            NS_LOG_INFO(s1 << " -> " << s2);

            if (trust_graph.nodes_to_id.find(s1) == trust_graph.nodes_to_id.end()){
                trust_graph.nodes_to_id[s1] = trust_graph.__node_cnt;
                trust_graph.id_to_nodes[trust_graph.__node_cnt] = s1;
                trust_graph.__node_cnt++;
            }

            if (trust_graph.nodes_to_id.find(s2) == trust_graph.nodes_to_id.end()){
                trust_graph.nodes_to_id[s2] = trust_graph.__node_cnt;
                trust_graph.id_to_nodes[trust_graph.__node_cnt] = s2;
                trust_graph.__node_cnt++;
            }

            int id1 = trust_graph.nodes_to_id[s1];
            int id2 = trust_graph.nodes_to_id[s2];
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
            // The entity "me", "AS#" and <my ip> should all be merged to "me"
            std::string s1 = isItMe(x.first) ? "me" : x.first;
            std::string s2 = isItMe(x.second) ? "me" : x.second;

            if (trust_graph.nodes_to_id.find(s1) == trust_graph.nodes_to_id.end()){
                trust_graph.nodes_to_id[s1] = trust_graph.__node_cnt;
                trust_graph.id_to_nodes[trust_graph.__node_cnt] = s1;
                trust_graph.__node_cnt++;
            }

            if (trust_graph.nodes_to_id.find(s2) == trust_graph.nodes_to_id.end()){
                trust_graph.nodes_to_id[s2] = trust_graph.__node_cnt;
                trust_graph.id_to_nodes[trust_graph.__node_cnt] = s2;
                trust_graph.__node_cnt++;
            }

            int id1 = trust_graph.nodes_to_id[s1];
            int id2 = trust_graph.nodes_to_id[s2];

            auto it2 = trust_graph.distrust_edges.find({id1, id2});
            if (it2 == trust_graph.distrust_edges.end()){
                trust_graph.distrust_edges.insert({id1, id2});
            }
        }

        for (auto &x: trust_graph.nodes_to_id){
            NS_LOG_INFO("Node Entry: " << x.first << "\t" << x.second);
        }
        for (auto &x: trust_graph.id_to_nodes){
            NS_LOG_INFO("Reverse Node Entry: " << x.first << " " << x.second);
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

        trust_graph.FloydWarshall();
        if (trust_graph.nodes_to_id.find("user:1") != trust_graph.nodes_to_id.end()){
            auto path = GetPath("user:1", "AS9");
            std::stringstream ss;
            for (std::string& x: path){
                ss << x << " -> ";
            }
            int dist = trust_graph.__distMatrix[{trust_graph.nodes_to_id["user:1"], trust_graph.nodes_to_id["AS9"]}].first;
            NS_LOG_INFO("Dummy Path: " << ss.str() << "Length: " << dist);
        }
    }

    std::vector<std::string>
    RIBPathComputer::GetPath(std::string startNode, std::string endNode)
    {
        std::vector<std::string> ans;
        if (trust_graph.nodes_to_id.find(startNode) == trust_graph.nodes_to_id.end()){
            NS_LOG_INFO("oqwebnobdfbxcvb");
            return ans;
        }
        if (trust_graph.nodes_to_id.find(endNode) == trust_graph.nodes_to_id.end()){
            NS_LOG_INFO("eruigbcvxjkxuirme");
            return ans;
        }

        int startId = trust_graph.nodes_to_id[startNode];
        int endId = trust_graph.nodes_to_id[endNode];

        if (trust_graph.__distMatrix[{startId, endId}].first == INT_MAX){
            return ans;
        }

        int pathLength = trust_graph.__distMatrix[{startId, endId}].first;
        for (int i = 0; i <= pathLength; i++){
            ans.push_back("");
        }

        int curr = endId;
        for (int i = pathLength; i >= 0; i--){
            // NS_LOG_INFO("Curr: " << curr << trust_graph.id_to_nodes[curr] << i);
            ans[i] += trust_graph.id_to_nodes[curr];
            // std::stringstream ss;
            // ss << "ljgkrjgkg " << trust_graph.__distMatrix[{startId, curr}].first << " " << trust_graph.__distMatrix[{startId, curr}].second;
            // NS_LOG_INFO(ss.str());
            curr = trust_graph.__distMatrix[{startId, curr}].second;
        }

        return ans;
    }
}

void
Graph::FloydWarshall()
{
    for (int i = 0; i < __node_cnt; i++){
        for (int j = 0; j < __node_cnt; j++){
            if (i == j){
                __distMatrix[{i, j}] = {0, i};
            }else{
                __distMatrix[{i, j}] = {INT_MAX, -1};
            }
        }

        auto it = trust_edges.equal_range(i);
        for (auto __it = it.first; __it != it.second; __it++){
            int j = __it->second;
            __distMatrix[{i, j}] = {1, i};
        }
    }


    for (int k = 0; k < __node_cnt; k++){
        for (int i = 0; i < __node_cnt; i++){
            for (int j = 0; j < __node_cnt; j++){
                long dist_ij = __distMatrix[{i, j}].first;
                long dist_ik = __distMatrix[{i, k}].first;
                long dist_kj = __distMatrix[{k, j}].first;

                if (dist_ij > dist_ik + dist_kj){
                    __distMatrix[{i, j}] = {dist_ik + dist_kj, __distMatrix[{k, j}].second};
                }
                
            }
        }

        // If (i, j) is a distrust edge, reset the distance to Infinite
        for (int i = 0; i < __node_cnt; i++){
            for (int j = 0; j < __node_cnt; j++){
                if (distrust_edges.find({i, j}) != distrust_edges.end()){
                    __distMatrix[{i, j}] = {INT_MAX, -1};
                }
            }
        }
    }
    

    NS_LOG_INFO("Floyd Warshall over");
}
