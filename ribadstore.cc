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
            std::map<int, Address>& addr = ((RIB *)this->parent_ctx)->peers;
            for (auto& [AS_num, a] : addr) {
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
    RIBAdStore::SendOverlaySwitches(Ptr<Socket> socket, Address dest)
    {
        RIB *rib = (RIB *)(this->parent_ctx); // * parent context is the RIB helper class
        std::cout << "Live switches: " << rib->liveSwitches->size() << std::endl;
        std::stringstream ss;
        for (auto x = rib->liveSwitches->begin(); x != rib->liveSwitches->end(); x++){
            ss << *x << " ";
        }
        std::string resp = ss.str();

        NS_LOG_INFO("Sending GIVESWITCHES response: " << resp);
        Ptr<Packet> p = Create<Packet>((const uint8_t *)resp.c_str(), resp.size());
        NS_LOG_INFO("Send status: " << socket->SendTo(p, 0, dest));
    }

    void
    RIBAdStore::SendClients(Ptr<Socket> socket, Address dest, std::string name) 
    {
        NS_LOG_INFO("sent to client the advertisement of " << name); 
        auto it = db.find(name);
        
        if (it == db.end() || it->second.size() == 0) {
            NS_LOG_ERROR("Cannot find local advertisement of the name: " << name);
            return;
        }
        // TODO: to distinguish which advertisement to send. They could have different origin AS. Currently using the 1st one found
        std::string str_repr = "ad:" + (it->second[0]->ToAdvertisementStr());
        Ptr<Packet> p = Create<Packet>((const uint8_t *)str_repr.c_str(), str_repr.size());
        NS_LOG_INFO("Send to client " << socket->SendTo(p, 0, dest));
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
            // check if advertisement from this origin AS already exist
            std::vector<NameDBEntry*>& all_ads = db[dc_name];
            bool already_exist = false;
            for (auto ad = all_ads.begin(); ad != all_ads.end(); ++ad) {
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
                // uint32_t receivedSize = packet->GetSize();
                // SeqTsHeader seqTs;
                // packet->RemoveHeader(seqTs);

                std::stringstream ss;
                packet->CopyData(&ss, packet->GetSize());
                std::string ad(ss.str());

                
                if (ad == "GIVESWITCHES"){
                    SendOverlaySwitches(socket, from);
                    continue;
                }

                if (ad.substr(0, 7) == "GIVEADS") {
                    SendClients(socket, from, ad.substr(8)); // Packet Format is "GIVEADS [dc name]", thus start from index 8
                    continue;
                }

                // * Remove header
                uint32_t receivedSize = packet->GetSize();
                SeqTsHeader seqTs;
                packet->RemoveHeader(seqTs);

                ss.str("");
                ss.clear(); // clear state flags
                packet->CopyData(&ss, packet->GetSize());
                ad = ss.str();
                // * printout the received packet body
                RIB *rib = (RIB *)(this->parent_ctx);
                // NS_LOG_INFO("I am ribadstore at " << Ipv4Address::ConvertFrom(rib->my_addr));
                NS_LOG_INFO("" << Ipv4Address::ConvertFrom(rib->my_addr) <<  " received: " << ad);

            NS_LOG_INFO("Entering FromAdvertisementStr");
                // * deserialize the advertisement packet
                NameDBEntry* advertised_entry = NameDBEntry::FromAdvertisementStr(ad);
            NS_LOG_INFO("Leaved FromAdvertisementStr");

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

            NS_LOG_INFO("Entering UpdateNameCache");
                bool updated = UpdateNameCache(advertised_entry);
            NS_LOG_INFO("Leaved UpdateNameCache");

                NS_LOG_INFO("Number of ads: " << db.size());

                NS_LOG_INFO("7123870127491273901275649604917284912074891274912749812");

                if (updated) {
                    // add itself to the td_path of the advertisement
                    advertised_entry->td_path.push_back(my_addr);
                    std::string serialized = advertised_entry->ToAdvertisementStr();

                    // (trust related) if current AS is the origin AS, don't forward if no certificate about 
                    // the origin server from the data capsule owner
                    auto& trust_relation_map = rib->certStore->trustRelations;

                    bool trust_curr_AS = false;
                    bool is_origin_AS_for_curr_ad = advertised_entry->origin_AS_addr == my_addr;
                    
                    // If I am not the origin AS and the ad is forwarded from someone not in my peer,
                    // Drop it
                    
                    if (!is_origin_AS_for_curr_ad){
                        if (advertised_entry->td_path.size() <= 1) continue;
                        bool found = false;
                        Address sender_addr = advertised_entry->td_path[advertised_entry->td_path.size() - 2];
                        for (auto &x: rib->peers){
                            // Costly search. TODO: Define a reverse map to make it constant / log n time
                            if (x.second == sender_addr){
                                found = true;
                                break;
                            }
                        }
                        if (!found){
                            NS_LOG_INFO("Potentially malicious advertisement. No changes made to trust relations. Dropping...");
                            continue;
                        }
                    }


                    // for (auto it = trust_relation_map.begin(); it != trust_relation_map.end(); it ++) {
                    //     NS_LOG_INFO("issuer: " << it->first << ", entity: " << it->second.first << ", type: " << it->second.second);
                    // }
                    if (is_origin_AS_for_curr_ad) {
                        auto [range_begin, range_end] = trust_relation_map.equal_range("fogrobotics:" + advertised_entry->dc_name);
                        for (auto it = range_begin; it != range_end; it++) {
                            // check if "entity" is the data capsule server name
                            if (Ipv4Address((it->second.first).c_str()) == advertised_entry->origin_server) {
                                trust_curr_AS = true;
                                // * Attach trust from DC owner to current name to the advertisement
                                advertised_entry->trust_cert.issuer = it->first;
                                advertised_entry->trust_cert.entity = it->second.first;
                                advertised_entry->trust_cert.r_transitivity = it->second.second;
                                advertised_entry->trust_cert.type = "trust";
                                // * Attach distrust relations of the DC owner
                                auto& distrust_relation_map = rib->certStore->distrustRelations;
                                auto [range_start, range_stop] = distrust_relation_map.equal_range("fogrobotics:" + advertised_entry->dc_name);
                                for (auto it = range_start; it != range_stop; ++it) {
                                    advertised_entry->distrust_certs.push_back(
                                        NameDBEntry::DistrustCert {"distrust", it->second, it->first}
                                    );
                                }
                                serialized = advertised_entry->ToAdvertisementStr();
                                break;
                            }
                        }
                    }
                    // if is_origin_AS_for_curr_ad, only forward if trust relation exist between DC owner and the DC server in my domain
                    // otherwise, flooding as usual
                    if (trust_curr_AS&&is_origin_AS_for_curr_ad) {
                        NS_LOG_INFO("I am origin AS, trust relationship exist, so forwarding ads to my peer.....");
                    }
                    if (!trust_curr_AS&&is_origin_AS_for_curr_ad) {
                        NS_LOG_INFO("I am origin AS, trust relationship does not exist, thus dropping this advertisement from the DC server advertiser.....");
                    }
                    
                    // save the received trust and distrust relations in local ribcertstore cache
                    if (!is_origin_AS_for_curr_ad) {
                        // * if not empty trust relation, add to cache
                        if ( !(advertised_entry->trust_cert.issuer.size() == 0
                            && advertised_entry->trust_cert.entity.size() == 0
                            && advertised_entry->trust_cert.r_transitivity == 0) ) {
                                std::pair<std::string, int> __val = std::make_pair(advertised_entry->trust_cert.entity, advertised_entry->trust_cert.r_transitivity);
                                rib->trustRelations->insert(std::make_pair(advertised_entry->trust_cert.issuer, __val));
                                std::pair<std::string, int> __val2 = std::make_pair(advertised_entry->trust_cert.issuer, INT_MAX);
                                rib->trustRelations->insert(std::make_pair(advertised_entry->trust_cert.entity, __val2));
                        }

                        if (advertised_entry->distrust_certs.size() != 0) {
                            for (auto& item : advertised_entry->distrust_certs) {
                                rib->distrustRelations->insert(std::make_pair(item.issuer, item.entity));
                            }
                        }

                        // Include the td_path in trust relations, Otherwise the graph is not complete
                        for (size_t i = 0; i < advertised_entry->td_path.size() - 1; i++){
                            Address u = advertised_entry->td_path[i + 1];
                            Address v = advertised_entry->td_path[i];
                            std::stringstream asu, asv;

                            auto itu = rib->rib_addr_map_.find(u);
                            if (itu == rib->rib_addr_map_.end()){
                                continue;
                            }
                            asu << "AS" << itu->second;

                            auto itv = rib->rib_addr_map_.find(v);
                            if (itv == rib->rib_addr_map_.end()){
                                continue;
                            }
                            asv << "AS" << itv->second;
                            rib->trustRelations->insert({asu.str(), {asv.str(), INT_MAX}});
                            NS_LOG_INFO("From RIB: AS" << rib->rib_addr_map_[rib->my_addr] << " Inserted extra: " << asu.str() << " -> " << asv.str());
                        }

                        // Include DCServer -> TD map
                        std::stringstream originStr, dcServerStr;
                        originStr << "AS" << rib->rib_addr_map_[advertised_entry->origin_AS_addr];
                        dcServerStr << advertised_entry->origin_server;
                        rib->trustRelations->insert({originStr.str(), {dcServerStr.str(), INT_MAX}});
                    }

                    if ((trust_curr_AS&&is_origin_AS_for_curr_ad) || !is_origin_AS_for_curr_ad) {
                        for (auto& [AS_num, addr] : rib->peers) {
                            //   cases not to forward:
                            //     1. the destination is what this ads came from
                            //     2. the destination is the origin AS
                            //     3. ...
                            Address dest_socket = InetSocketAddress(Ipv4Address::ConvertFrom(addr), RIBADSTORE_PORT);   
                            if (dest_socket != from && addr != advertised_entry->origin_AS_addr) {
                                
                                Ipv4Address temp = Ipv4Address::ConvertFrom(addr);
                                std::stringstream ss;
                                my_addr.Print(ss);
                                NS_LOG_INFO("RIB:" << ss.str() << ". Forward Ads to " << temp);
                                // introduce arbitrary delay to do the advertisement
                                // source: https://stackoverflow.com/questions/288739/generate-random-numbers-uniformly-over-an-entire-range
                                std::random_device rand_dev;
                                std::mt19937 generator(rand_dev());
                                std::uniform_int_distribution<int> distr(0, 10);

                                Simulator::Schedule(Seconds(distr(generator)), &RIBAdStore::ForwardAds, this, socket, serialized, dest_socket);
                                // Simulator::ScheduleNow(&RIBAdStore::ForwardAds, this, socket, serialized, dest_socket);
                            }
                        }
                    }
                    
                } else {
                    delete advertised_entry;
                }

                NS_LOG_INFO("ajsdfoijaofweifn,dasnf,masdnfm,asnfd,mnasd,fnwefiowe");

                

                
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