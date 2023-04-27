#include "main.h"
#include <string>
#include <sstream>
// #define DATAGRAM_SIZE (8+path.size()+16 + 2)
#define DATAGRAM_SIZE 100


namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("DummyClient2");

    NS_OBJECT_ENSURE_REGISTERED(DummyClient2);

    TypeId
    DummyClient2::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::DummyClient2")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<DummyClient2>()
                .AddAttribute("Name",
                            "The name client advertises itself as, of the format user:<blah_blah>",
                            StringValue("user:1"),
                            MakeStringAccessor(&DummyClient2::m_name),
                            MakeStringChecker())
                .AddAttribute("MaxPackets",
                            "The maximum number of packets the application will send",
                            UintegerValue(100),
                            MakeUintegerAccessor(&DummyClient2::m_count),
                            MakeUintegerChecker<uint32_t>())
                .AddAttribute("Interval",
                            "The time to wait between packets",
                            TimeValue(Seconds(1.0)),
                            MakeTimeAccessor(&DummyClient2::m_interval),
                            MakeTimeChecker())
                .AddAttribute("RemoteAddress",
                            "The destination Address of the outbound packets",
                            AddressValue(),
                            MakeAddressAccessor(&DummyClient2::m_peerAddress),
                            MakeAddressChecker())
                .AddAttribute("RemotePort",
                            "The destination port of the outbound packets",
                            UintegerValue(RIBADSTORE_PORT),
                            MakeUintegerAccessor(&DummyClient2::m_peerPort),
                            MakeUintegerChecker<uint16_t>())
                .AddAttribute("PacketSize",
                            "Size of packets generated. The minimum packet size is 12 bytes which is "
                            "the size of the header carrying the sequence number and the time stamp.",
                            UintegerValue(1024),
                            MakeUintegerAccessor(&DummyClient2::m_size),
                            MakeUintegerChecker<uint32_t>(12, 65507));
        return tid;
    }

    DummyClient2::DummyClient2()
    {
        NS_LOG_FUNCTION(this);
        m_sent = 0;
        m_totalTx = 0;
        m_socket = nullptr;
        m_sendEvent = EventId();
    }

    DummyClient2::~DummyClient2()
    {
        NS_LOG_FUNCTION(this);
    }

    void
    DummyClient2::SetRemote(Address ip, uint16_t port)
    {
        NS_LOG_FUNCTION(this << ip << port);
        m_peerAddress = ip;
        m_peerPort = port;
    }

    void
    DummyClient2::SetRemote(Address addr)
    {
        NS_LOG_FUNCTION(this << addr);
        m_peerAddress = addr;
    }

    void
    DummyClient2::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }
    void
    DummyClient2::HandleDCResponse(Ptr<Socket> sock)
    {
        Ptr<Packet> packet;
        Address from;
        while ((packet = sock->RecvFrom(from))){
            if (packet->GetSize() > 0){
                uint32_t receivedSize = packet->GetSize();
                SeqTsHeader seqTs;
                packet->RemoveHeader(seqTs);
                // uint32_t currentSequenceNumber = seqTs.GetSeq();
                if (receivedSize < 16){
                    continue;
                }
                uint32_t *buff = new uint32_t[receivedSize / sizeof(uint32_t) + 2];
                uint32_t sz = packet->CopyData((uint8_t *)buff, receivedSize);
                if (sz < 16){
                    delete[] buff;
                    continue;
                }
                uint32_t content_sz = buff[3];
                uint32_t hop_cnt = buff[1];

                if (32 + 4 * hop_cnt + 64 + content_sz > sz){
                    delete[] buff;
                    continue;
                }

                if (content_sz > 0){
                    int64_t *content = (int64_t *)((char *)buff + 32 + 4 * hop_cnt + 64);
                    int64_t timeNow = Simulator::Now().GetMicroSeconds();

                    NS_LOG_INFO("Receive delay: " << timeNow - (*content) << " Hops: " << buff[1]);
                }else{
                    NS_LOG_INFO("Received Empty reply from DC");
                }
            }
        }
    }
    void
    DummyClient2::StartApplication()
    {
        NS_LOG_FUNCTION(this);

        if (!m_socket)
        {
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            m_socket = Socket::CreateSocket(GetNode(), tid);
            reply_socket = Socket::CreateSocket(GetNode(), tid);
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), CLIENT_REPLY_PORT);
            if (reply_socket->Bind(local) == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            reply_socket->SetRecvCallback(MakeCallback(&DummyClient2::HandleDCResponse, this));

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

        m_socket->SetRecvCallback(MakeCallback(&DummyClient2::HandleSwitch, this));
        m_socket->SetAllowBroadcast(true);
        m_sendEvent = Simulator::Schedule(Seconds(0.1), &DummyClient2::GetSwitch, this); // * first fetch local TD's live switches
        
        if (!path_computer_socket){
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            path_computer_socket = Socket::CreateSocket(GetNode(), tid);
            path_computer_socket->Bind();

            path_computer_socket->Connect(
                InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), RIBPATHCOMPUTER_PORT));

        }
        path_computer_socket->SetRecvCallback(MakeCallback(&DummyClient2::HandleSwitch, this));

        Simulator::Schedule(Seconds(0.2), &DummyClient2::PledgeAllegiance, this);       // * Include myself in RIB's trustgraph
        Simulator::Schedule(Seconds(3), &DummyClient2::GetPath, this); // * fetch advertised names

        NS_LOG_INFO(m_name << " started");
    }

    void
    DummyClient2::PledgeAllegiance()
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        Ptr<Socket> cert_socket = Socket::CreateSocket(GetNode(), tid);
        cert_socket->Connect(
            InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), RIBCERTSTORE_PORT)
        );

        SeqTsHeader seqTs;
        seqTs.SetSeq(0xffff);
        
        Json::Value root;
        std::stringstream ss;
        ss << Ipv4Address::ConvertFrom(m_peerAddress);

        root["issuer"] = m_name;
        root["type"] = "trust";
        root["entity"] = ss.str();

        Json::StyledWriter jsonWriter;
        std::string body = jsonWriter.write(root);

        Ptr<Packet> p = Create<Packet>((const uint8_t *)body.c_str(), body.size());
        p->AddHeader(seqTs);

        cert_socket->Send(p);
        NS_LOG_INFO("Pledged Allegiance to my RIB");

        // Dummy Distrust AS0

        root["type"] = "distrust";
        root["entity"] = "AS0";
        Json::StyledWriter jw2;
        body = jw2.write(root);
        Ptr<Packet> p2 = Create<Packet>((const uint8_t *)body.c_str(), body.size());
        p2->AddHeader(seqTs);
        cert_socket->Send(p2);
        NS_LOG_INFO("Distrust relation addded to AS0");
        


        cert_socket->Close();

    }
    void
    DummyClient2::GetSwitch()
    {
        NS_LOG_INFO("DummyClient2:GetSwitch");
        std::string s = "GIVESWITCHES";
        Ptr<Packet> p = Create<Packet>((const uint8_t *)s.c_str(), s.size());

        if (m_socket){
            m_socket->Send(p);   
        }
    }

    void
    DummyClient2::GetPath()
    {
        NS_LOG_INFO("DummyClient2:GetPath");
        // Client requesting a path
        // Packet format:
        // GIVEPATH {
            // client_name:xxxxxxx
            // dc_name:mmmmmmm 
        //}
        std::string s = "GIVEPATH";
        std::set<std::string>& namesToAsk = this->dcnames_to_route;
        Json::Value request;
        request["client_name"] = m_name;
        // request["dc_name"] = "";
        for (auto& n : namesToAsk) {
            // Packet Format is "GIVEADS [dc name]"
            request["dc_name"] = n;
            std::string res = s + " " + request.toStyledString();
            Ptr<Packet> p = Create<Packet>((const uint8_t *)res.c_str(), res.size());

            
            if (path_computer_socket){
                NS_LOG_INFO("sending through get path socket");
                path_computer_socket->Send(p);   
            }
        }

        
    }


    void
    DummyClient2::HandleSwitch(Ptr<Socket> sock)
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
                    NS_LOG_INFO("Dummy Client2 GIVEPATH response: " << temp);
                    
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
                    Simulator::ScheduleNow(&DummyClient2::SendUsingPath, this, path, origin_server);

                } else {
                    NS_LOG_INFO("Dummy Client2 GIVESWITCHES response: " << temp);

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
                        NS_LOG_INFO("Dummy Client2 Switches: " << addr);
                    }
                }

                
                
                
            }
        }
    }

    void
    DummyClient2::StopApplication()
    {
        NS_LOG_FUNCTION(this);
        Simulator::Cancel(m_sendEvent);
    }

    void
    DummyClient2::SendUsingPath(std::vector<std::string>& path, std::string& destination_ip)
    {
        NS_LOG_FUNCTION(this);
        // NS_ASSERT(m_sendEvent.IsExpired());
        for (auto& s : path) {
            NS_LOG_INFO("path components: " << s);
        }
        SeqTsHeader seqTs;
        seqTs.SetSeq(m_sent);
        /**********************************************
         * Packet format:
         * 
         * 0          4           8           12             16 
         * |  Magic   | #hops     | curr hop  | content size |
         * |-------------------------------------------------|
         * | src ip   | src port  | dest ip   | dest port    |
         * |-------------------------------------------------|
         * | hop 0    | hop 1     | hop 2     | ...          |
         * |-------------------------------------------------|
         * | Hop Signature    (64 bytes)                     |
         * |                                                 |
         * |                                                 |
         * |                                                 |
         * |-------------------------------------------------|
         * | Content .......                                 |
        */
        uint32_t buff[DATAGRAM_SIZE];
        buff[0] = PACKET_MAGIC_UP;
        buff[1] = (uint32_t) path.size();
        buff[2] = 0; 
        buff[3] = 4 * (DATAGRAM_SIZE - (8+path.size()+16));

        int64_t timeOfSending = Simulator::Now().GetMicroSeconds();
        
        buff[4] = my_ip.Get();
        buff[5] = CLIENT_REPLY_PORT;
        buff[6] = Ipv4Address(destination_ip.c_str()).Get();
        buff[7] = DCSERVER_ECHO_PORT;
        size_t offset = 8;
        
        for (int i = 0; i < (int)path.size(); ++i) {
            buff[offset++] = std::stoi(path[i].substr(path[i].find("AS")+2));
        }


        for (int i=0; i<16; ++i) {
            buff[offset++] = 0;
        }

        memcpy(buff + offset, &timeOfSending, sizeof(int64_t));


        Ptr<Packet> p = Create<Packet>((uint8_t *)buff, DATAGRAM_SIZE*4); // 8+4 : the size of the seqTs header
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

        // m_sendEvent = Simulator::Schedule(m_interval, &DummyClient::Send, this);
        Simulator::Schedule(Seconds(0.1), &DummyClient2::SendUsingPath, this, path, destination_ip);
        
    }

    void
    DummyClient2::Send()
    {
        NS_LOG_FUNCTION(this);
        NS_ASSERT(m_sendEvent.IsExpired());

        SeqTsHeader seqTs;
        seqTs.SetSeq(m_sent);
        
        

        uint32_t buff[27];
        buff[0] = PACKET_MAGIC_UP;
        buff[1] = 3;
        buff[2] = 0;
        buff[3] = 0;
        buff[4] = my_ip.Get();
        buff[5] = CLIENT_REPLY_PORT;
        Ipv4Address addr("11.0.0.1");
        buff[6] = addr.Get();
        buff[7] = DCSERVER_ECHO_PORT;
        buff[8] = 3;
        buff[9] = 1;
        buff[10] = 2;
        for (int i = 0; i < 16; i++){ // * signature is all 0 for dummy client
            buff[11 + i] = 0;
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


        m_sendEvent = Simulator::Schedule(m_interval, &DummyClient2::Send, this);
    }

    uint64_t
    DummyClient2::GetTotalTx() const
    {
        return m_totalTx;
    }
}