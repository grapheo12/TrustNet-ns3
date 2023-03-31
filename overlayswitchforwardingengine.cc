#include "main.h"
#include <istream>
#include <string>

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("OverlaySwitchForwardingEngine");
    NS_OBJECT_ENSURE_REGISTERED(OverlaySwitchForwardingEngine);

    TypeId
    OverlaySwitchForwardingEngine::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::OverlaySwitchForwardingEngine")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<OverlaySwitchForwardingEngine>()
                .AddAttribute("Port",
                            "Port on which we listen for incoming packets.",
                            UintegerValue(OVERLAY_FWD),
                            MakeUintegerAccessor(&OverlaySwitchForwardingEngine::m_port),
                            MakeUintegerChecker<uint16_t>())
                .AddAttribute("PacketWindowSize",
                            "The size of the window used to compute the packet loss. This value "
                            "should be a multiple of 8.",
                            UintegerValue(32),
                            MakeUintegerAccessor(&OverlaySwitchForwardingEngine::GetPacketWindowSize,
                                                &OverlaySwitchForwardingEngine::SetPacketWindowSize),
                            MakeUintegerChecker<uint16_t>(8, 256))
                .AddTraceSource("Rx",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&OverlaySwitchForwardingEngine::m_rxTrace),
                                "ns3::Packet::TracedCallback")
                .AddTraceSource("RxWithAddresses",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&OverlaySwitchForwardingEngine::m_rxTraceWithAddresses),
                                "ns3::Packet::TwoAddressTracedCallback");
        return tid;
    }

    OverlaySwitchForwardingEngine::OverlaySwitchForwardingEngine()
        : m_lossCounter(0)
    {
        NS_LOG_FUNCTION(this);
        m_received = 0;
    }

    OverlaySwitchForwardingEngine::~OverlaySwitchForwardingEngine()
    {
        NS_LOG_FUNCTION(this);
    }

    uint16_t
    OverlaySwitchForwardingEngine::GetPacketWindowSize() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetBitMapSize();
    }

    void
    OverlaySwitchForwardingEngine::SetPacketWindowSize(uint16_t size)
    {
        NS_LOG_FUNCTION(this << size);
        m_lossCounter.SetBitMapSize(size);
    }

    uint32_t
    OverlaySwitchForwardingEngine::GetLost() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetLost();
    }

    uint64_t
    OverlaySwitchForwardingEngine::GetReceived() const
    {
        NS_LOG_FUNCTION(this);
        return m_received;
    }

    void
    OverlaySwitchForwardingEngine::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void
    OverlaySwitchForwardingEngine::StartApplication()
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

        m_socket->SetRecvCallback(MakeCallback(&OverlaySwitchForwardingEngine::HandleRead, this));

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

        m_socket6->SetRecvCallback(MakeCallback(&OverlaySwitchForwardingEngine::HandleRead, this));
        Simulator::Schedule(peer_calc_delay, &OverlaySwitchForwardingEngine::PopulatePeers, this);
    }

    void
    OverlaySwitchForwardingEngine::StopApplication()
    {
        NS_LOG_FUNCTION(this);

        if (m_socket)
        {
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }
    }


    /*************************************
     * Packet format:
     * 
     * 0        4       8           12             16 
     * |  Magic | #hops | curr hop  | content size |
     * |-------------------------------------------|
     * | hop 0  | hop 1 | hop 2     | ...          |
     * |-------------------------------------------|
     * | Hop Signature    (64 bytes)               |
     * |                                           |
     * |                                           |
     * |                                           |
     * |-------------------------------------------|
     * | Content .......                           |
    */
    void
    OverlaySwitchForwardingEngine::HandleRead(Ptr<Socket> socket)
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
                uint32_t receivedSize = packet->GetSize();
                SeqTsHeader seqTs;
                packet->RemoveHeader(seqTs);
                uint32_t currentSequenceNumber = seqTs.GetSeq();

                if (receivedSize < 16){
                    continue;
                }
                uint32_t *buff = new uint32_t[receivedSize / sizeof(uint32_t) + 2];
                uint32_t sz = packet->CopyData((uint8_t *)buff, receivedSize / sizeof(uint32_t) + 2);
                if (sz < 16){
                    delete[] buff;
                    continue;
                }

                if (buff[0] != PACKET_MAGIC){
                    delete[] buff;
                    continue;
                }

                uint32_t hop_cnt = buff[1];
                uint32_t curr_hop = buff[2];
                uint32_t content_sz = buff[3];
                if (sz < 16 + 4 * hop_cnt + 64 + content_sz){
                    delete[] buff;
                    continue;
                }
                if (curr_hop >= hop_cnt || buff[curr_hop] != td_num){
                    delete[] buff;
                    continue;
                }
                buff[2]++;
                auto it = oswitch_in_other_td.find(buff[2]);
                if (it == oswitch_in_other_td.end() || it->second.size() == 0){
                    delete[] buff;
                    continue;
                }
                Address next_hop_addr = *(it->second.begin());      // TODO: Do round robin here.

                Ptr<Packet> fwdPacket = Create<Packet>((const uint8_t *)buff, sz);


                Simulator::ScheduleNow(&OverlaySwitchForwardingEngine::ForwardPacket, this, next_hop_addr, fwdPacket);
                delete[] buff;
                

                m_lossCounter.NotifyReceived(currentSequenceNumber);
                m_received++;
            }
        }
    }


    void
    OverlaySwitchForwardingEngine::ForwardPacket(Address who, Ptr<Packet> what)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

        Ptr<Socket> sock = Socket::CreateSocket(GetNode(), tid);
        sock->Connect(
            InetSocketAddress(Ipv4Address::ConvertFrom(who), OVERLAY_FWD));

        sock->Send(what);
    }

    void
    OverlaySwitchForwardingEngine::PopulatePeers()
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

        givepeers_socket = Socket::CreateSocket(GetNode(), tid);
        givepeers_socket->Connect(
            InetSocketAddress(Ipv4Address::ConvertFrom(rib_addr), RIBLSM_PORT));
        
        givepeers_socket->SetRecvCallback(MakeCallback(&OverlaySwitchForwardingEngine::HandlePeersCallback, this));

        NS_LOG_INFO("Attempting to access GIVEPEERS API");
        std::string cmd = "GIVEPEERS";
        Ptr<Packet> pkt = Create<Packet>((uint8_t *)cmd.c_str(), cmd.size());
        givepeers_socket->Send(pkt);
    }

    void
    OverlaySwitchForwardingEngine::HandlePeersCallback(Ptr<Socket> sock)
    {
        Address from;
        Ptr<Packet> p;
        while (p = sock->RecvFrom(from)){
            NS_LOG_INFO("Received Response from: " << from << " Size: " << p->GetSize());
            if (p->GetSize() > 0){
                std::stringstream ss;
                p->CopyData(&ss, p->GetSize());
                std::string resp = ss.str();
                std::istringstream iss(resp);
                while (iss){
                    int __td_num = -2;
                    std::string __addr_string = "0.0.0.0";
                    iss >> __td_num >> __addr_string;
                    if (__addr_string == "0.0.0.0"){
                        break;
                    }
                    temp_peering_rib_addrs[__td_num] = Ipv4Address(__addr_string.c_str());
                }
            }

            for (auto &x: temp_peering_rib_addrs){
                NS_LOG_INFO("GIVEPEERS Response Entry: " << x.first << " Addr: " << x.second);
                Simulator::ScheduleNow(&OverlaySwitchForwardingEngine::PopulateSwitches, this, x.first, x.second);
            }

            
        }
    }

    // Slowly entering trailing Callback hell. Save our souls.
    void
    OverlaySwitchForwardingEngine::PopulateSwitches(int __td_num, Ipv4Address addr)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

        Ptr<Socket> sock = Socket::CreateSocket(GetNode(), tid);
        sock->Connect(
            InetSocketAddress(Ipv4Address::ConvertFrom(addr), RIBADSTORE_PORT));
        
        sock->SetRecvCallback(MakeCallback(&OverlaySwitchForwardingEngine::HandleSwitchesCallback, this));

        giveswitches_sockets.push_back(sock);

        std::string cmd = "GIVESWITCHES";
        Ptr<Packet> p = Create<Packet>((uint8_t *)cmd.c_str(), cmd.size());
        sock->Send(p);
    }

    void
    OverlaySwitchForwardingEngine::HandleSwitchesCallback(Ptr<Socket> sock)
    {
        Address from;
        Ptr<Packet> p;
        while (p = sock->RecvFrom(from)){
            Ipv4Address __from = InetSocketAddress::ConvertFrom(from).GetIpv4();
            NS_LOG_INFO("Received GIVESWITCHES Response from: " << __from << " Size: " << p->GetSize());
            int __td_num = -1;
            for (auto &x: temp_peering_rib_addrs){
                if (x.second == __from){
                    __td_num = x.first;
                    break;
                }
            }
            if (__td_num == -1){
                NS_LOG_INFO("Unknown peer, continuing: " << __from);
                continue;
            }

            if (p->GetSize() > 0){
                std::stringstream ss;
                p->CopyData(&ss, p->GetSize());
                std::string resp = ss.str();

                NS_LOG_INFO("GIVESWITCHES Response: " << resp);
                
                std::istringstream iss(resp);
                std::set<Address> addr_set;

                while (iss){
                    std::string addr_str = "0.0.0.0";
                    iss >> addr_str;
                    if (addr_str == "0.0.0.0"){
                        break;
                    }

                    Ipv4Address addr(addr_str.c_str());
                    addr_set.insert(addr);
                }

                if (oswitch_in_other_td.find(__td_num) == oswitch_in_other_td.end()){
                    oswitch_in_other_td[__td_num] = addr_set;
                }else{
                    for (auto &a: addr_set){
                        oswitch_in_other_td[__td_num].insert(a);
                    }
                }

                NS_LOG_INFO("OSwitch in TD: " << td_num << " Other TDs included: " << oswitch_in_other_td.size());


            }

        }

    }

}