#include "main.h"

namespace ns3
{

    NS_LOG_COMPONENT_DEFINE("RIBAdStore");

    NS_OBJECT_ENSURE_REGISTERED(RIBAdStore);

    TypeId
    RIBAdStore::GetTypeId()
    {
        // void *x;
        static TypeId tid =
            TypeId("ns3::RIBAdStore")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<RIBAdStore>()
                .AddAttribute("Port",
                            "Port on which we listen for incoming packets.",
                            UintegerValue(RIBADSTORE_PORT),
                            MakeUintegerAccessor(&RIBAdStore::m_port),
                            MakeUintegerChecker<uint16_t>())
                .AddAttribute("PacketWindowSize",
                            "The size of the window used to compute the packet loss. This value "
                            "should be a multiple of 8.",
                            UintegerValue(32),
                            MakeUintegerAccessor(&RIBAdStore::GetPacketWindowSize,
                                                &RIBAdStore::SetPacketWindowSize),
                            MakeUintegerChecker<uint16_t>(8, 256))
                .AddTraceSource("Rx",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&RIBAdStore::m_rxTrace),
                                "ns3::Packet::TracedCallback")
                .AddTraceSource("RxWithAddresses",
                                "A packet has been received",
                                MakeTraceSourceAccessor(&RIBAdStore::m_rxTraceWithAddresses),
                                "ns3::Packet::TwoAddressTracedCallback");
        return tid;
    }

    RIBAdStore::RIBAdStore()
        : m_lossCounter(0)
    {
        NS_LOG_FUNCTION(this);
        m_received = 0;
        // parent_ctx = ctx;
    }

    void RIBAdStore::SetContext(void *ctx)
    {
        parent_ctx = ctx;
    }

    RIBAdStore::~RIBAdStore()
    {
        NS_LOG_FUNCTION(this);
    }

    uint16_t
    RIBAdStore::GetPacketWindowSize() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetBitMapSize();
    }

    void
    RIBAdStore::SetPacketWindowSize(uint16_t size)
    {
        NS_LOG_FUNCTION(this << size);
        m_lossCounter.SetBitMapSize(size);
    }

    uint32_t
    RIBAdStore::GetLost() const
    {
        NS_LOG_FUNCTION(this);
        return m_lossCounter.GetLost();
    }

    uint64_t
    RIBAdStore::GetReceived() const
    {
        NS_LOG_FUNCTION(this);
        return m_received;
    }

    void
    RIBAdStore::DoDispose()
    {
        // printout my ads store
        RIB *rib = (RIB *)(this->parent_ctx);
        Ipv4Address my_addr = Ipv4Address::ConvertFrom(rib->my_addr);
        std::stringstream ss;
        my_addr.Print(ss);
        std::string my_addr_str = ss.str();

        for (auto pair : this->db) {
            for (auto entry_ptr : pair.second) {
                std::stringstream td_path_str;
                for (Ipv4Address addr : entry_ptr->td_path) {
                    std::stringstream ss;
                    addr.Print(ss);
                    td_path_str << ss.str() << "->";
                }
                std::string result = td_path_str.str();
                std::string result_trim = result.substr(0, result.size()-2);
                NS_LOG_INFO("RibStore "<<my_addr_str<<"'s ads store has"<< "\n" <<" Name:" << pair.first << "\n" << "Entry: " << result_trim << "\n");
            }
        }

        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void
    RIBAdStore::StartApplication()
    {
        NS_LOG_FUNCTION(this);
        NS_LOG_INFO("RIBAdStore started");

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

        m_socket->SetRecvCallback(MakeCallback(&RIBAdStore::HandleRead, this));

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

        m_socket6->SetRecvCallback(MakeCallback(&RIBAdStore::HandleRead, this));

        Simulator::Schedule(Seconds(13.0), [this]{
            std::set<Address>& addr = ((RIB *)this->parent_ctx)->peers;
            for (auto a : addr) {
                NS_LOG_INFO(">> I am " << Ipv4Address::ConvertFrom(((RIB *)this->parent_ctx)->my_addr) << "peer: " << Ipv4Address::ConvertFrom(a));
            }
            });

    }

    void
    RIBAdStore::StopApplication()
    {
        NS_LOG_FUNCTION(this);

        if (m_socket)
        {
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        }
    }

    void
    RIBAdStore::SendPeers(Ptr<Socket> socket, Address dest)
    {
        RIB *rib = (RIB *)(this->parent_ctx); // * parent context is the RIB helper class
        std::cout << "Live switches: " << rib->liveSwitches->size() << std::endl;
        std::stringstream ss;
        for (auto x = rib->liveSwitches->begin(); x != rib->liveSwitches->end(); x++){
            ss << *x << " ";
        }
        std::string resp = ss.str();

        NS_LOG_INFO("Sending GIVEPEERS response: " << resp);
        Ptr<Packet> p = Create<Packet>((const uint8_t *)resp.c_str(), resp.size());
        NS_LOG_INFO("Send status: " << socket->SendTo(p, 0, dest));
    }


    /* Return true if the provided entry is in db, false if table not updated */
    bool
    RIBAdStore::UpdateNameCache(NameDBEntry* advertised)
    {
        bool updated = false;

        std::string& dc_name = advertised->dc_name;
        
        Ipv4Address origin_AS_addr = advertised->origin_AS_addr;

        RIB *rib = (RIB *)(this->parent_ctx); // * parent context is the RIB class
        // store ads if
        //   1. origin address is current rib's address
        //   2. no existing entry in db
        if (origin_AS_addr == rib->my_addr || db.find(dc_name) == db.end()) {
            db[dc_name] = std::vector<NameDBEntry*>{ advertised };
            updated = true;
        } else {
            auto iter = db.find(dc_name);

            // check if advertisement from this origin AS already exist
            std::vector<NameDBEntry*>& all_ads = db[dc_name];
            bool already_exist = false;
            for (auto ad = all_ads.begin(); ad != all_ads.end(); ++iter) {
                // if it does, update to have shorter path if possible
                if ((*ad)->origin_AS_addr == advertised->origin_AS_addr) {
                    already_exist = true;

                    if ((*ad)->td_path.size() > advertised->td_path.size()) {
                        delete all_ads[ad-all_ads.begin()];
                        all_ads[ad-all_ads.begin()] = advertised;
                        updated = true;
                    }

                    break;
                }
            }

            // add to the advertisement vector for this dc name if not seen before
            if (!already_exist) {
                all_ads.push_back(advertised);
                updated = true;
            }
                
        }

        return updated;
    }

    void
    RIBAdStore::ForwardAds(Ptr<Socket> socket, std::string& content, Address dest) {
        // Need to add header because the same type of header is alwasy striped down on the HandleRead() side
        SeqTsHeader seqTS;  //! not initialized, do we care sequence number and timestamp?
        Ptr<Packet> p = Create<Packet>(0); // 8+4 : the size of the seqTs header
        p->AddHeader(seqTS);
        Ptr<Packet> __p = Create<Packet>((const uint8_t *)content.c_str(), content.size());
        p->AddAtEnd(__p);
        NS_LOG_INFO("> Sent " << socket->SendTo(p, 0, dest));
    }

    void
    RIBAdStore::HandleRead(Ptr<Socket> socket)
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
                
                NS_LOG_INFO("Received packet: " << seqTs.GetSeq() << " " << seqTs.GetTs() << " " << packet->GetSize());
                std::stringstream ss;
                packet->CopyData(&ss, packet->GetSize());
                std::string ad(ss.str());

                // * printout the received packet body
                RIB *rib = (RIB *)(this->parent_ctx);
                // NS_LOG_INFO("I am ribadstore at " << Ipv4Address::ConvertFrom(rib->my_addr));
                // NS_LOG_INFO("" << Ipv4Address::ConvertFrom(rib->my_addr) <<  " received: " << ad);

                if (ad == "GIVEPEERS"){
                    SendPeers(socket, from);
                }else{
                    // * deserialize the advertisement packet
                    NameDBEntry* advertised_entry = NameDBEntry::FromAdvertisementStr(ad);
                    
                    if (advertised_entry == nullptr) {
                        NS_LOG_ERROR("Cannot parse ads: " << ad);
                        // std::abort();
                        continue;
                    }

                    // check if self is already in advertisement path to avoid loop
                    bool is_loop = false;
                    Ipv4Address my_addr = Ipv4Address::ConvertFrom(rib->my_addr);
                    for (auto addr : advertised_entry->td_path) {
                        if (addr == my_addr) {
                            is_loop = true;
                            break;
                        }
                    }
                    if (is_loop) {
                        NS_LOG_INFO("Detected advertising loop, ignoring current ads...");
                        continue;
                    }
                    bool updated = UpdateNameCache(advertised_entry);
                    NS_LOG_INFO("Number of ads: " << db.size());

                    if (updated) {
                        // add itself to the td_path of the advertisement
                        advertised_entry->td_path.push_back(my_addr);
                        std::string serialized = advertised_entry->ToAdvertisementStr();
                        for (auto addr : rib->peers) {
                            //   cases not to forware:
                            //     1. the destination is what this ads came from
                            //     2. the destination is the origin AS
                            //     3. .. 
                            Address dest_socket = InetSocketAddress(Ipv4Address::ConvertFrom(addr), RIBADSTORE_PORT);
                            if (dest_socket != from && addr != advertised_entry->origin_AS_addr) {
                                Ipv4Address temp = Ipv4Address::ConvertFrom(addr);
                                std::stringstream ss;
                                my_addr.Print(ss);
                                NS_LOG_INFO("RIB:" << ss.str() << ". Forward Ads to " << temp);
                                Simulator::ScheduleNow(&RIBAdStore::ForwardAds, this, socket, serialized, dest_socket);
                            }
                        }
                        
                    } else {
                        delete advertised_entry;
                    }

                }

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