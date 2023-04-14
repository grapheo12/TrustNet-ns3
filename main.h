#pragma once

// TODO: Anycast Hysteresis;
// Failover at Network level so that switches can adapt for mobility
#include "ns3/applications-module.h"
#include "ns3/brite-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/nix-vector-helper.h"
#include "ns3/point-to-point-module.h"
#include "json/json.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <cstdlib>
#include <map>
#include <string>
#include <cassert>
#include <random>


#define RIBADSTORE_PORT 3001
#define RIBLSM_PORT 3002
#define OVERLAY_PING 3003
#define OVERLAY_FWD 3004
#define RIBCERTSTORE_PORT 3005
#define RIBPATHCOMPUTER_PORT 3006
#define PACKET_MAGIC 0xdeadface


using namespace ns3;

/* Declaring the utility class to pass compilation */
class NameDBEntry; 

/* Global map for mapping AS with their addresses*/
extern std::map<int, Address> global_AS_to_addr;
extern std::map<Address, int> global_addr_to_AS;

struct Graph {
    std::map<std::string, int> nodes_to_id;
    std::multimap<int, int> trust_edges;          // adjacency list representation
    std::multimap<int, int> distrust_edges;
    std::map<std::pair<int, int>, int> transitivity;  // edge -> r_transitivity
    int __node_cnt;
};


namespace ns3{

    class DCServerAdvertiser : public Application
    {
    public:
        static TypeId GetTypeId();
        std::vector<std::string> dcNameList;
        uint32_t sentNum;

        DCServerAdvertiser();
        ~DCServerAdvertiser() override;
        void SetRemote(Address ip, uint16_t port);
        void SetRemote(Address addr);
        uint64_t GetTotalTx() const;
        void HandlePeers(Ptr<Socket> socket);

    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void Send();

        uint32_t m_count; //!< Maximum number of packets the application will send
        Time m_interval;  //!< Packet inter-send time
        uint32_t m_size;  //!< Size of the sent packet (including the SeqTsHeader)

        uint32_t m_sent;       //!< Counter for sent packets
        uint64_t m_totalTx;    //!< Total bytes sent
        Ptr<Socket> m_socket;  //!< Socket
        Address m_peerAddress; //!< Remote peer address
        uint16_t m_peerPort;   //!< Remote peer port
        EventId m_sendEvent;   //!< Event to send the next packet
    #ifdef NS3_LOG_ENABLE
        std::string m_peerAddressString; //!< Remote peer address string
    #endif                               // NS3_LOG_ENABLE
    };


    // Sends all the certs then dies.
    class DCOwner: public Application
    {
    public:
        static TypeId GetTypeId();
        DCOwner();
        ~DCOwner() override;
        
        std::string my_name;            // is_khan? Some Bollywood pun

        struct CertInfo {
            Address rib_addr;
            std::string type;
            std::string entity;
            std::string issuer;
            int r_transitivity;
        };

        std::vector<CertInfo> certs_to_send;


    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void Send(Ptr<Socket> sock, CertInfo cinfo);
    };


    class RIBAdStore : public Application
    {
    public:
        static TypeId GetTypeId();
        RIBAdStore();
        ~RIBAdStore() override;
        uint32_t GetLost() const;
        uint64_t GetReceived() const;
        uint16_t GetPacketWindowSize() const;
        void SetPacketWindowSize(uint16_t size);
        void SetContext(void *ctx);
        void SendOverlaySwitches(Ptr<Socket> socket, Address dest);
        void SendClients(Ptr<Socket> socket, Address dest, std::string name);
        std::unordered_map<std::string, std::vector<NameDBEntry*>> db;

        void *parent_ctx;
    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void HandleRead(Ptr<Socket> socket);
        bool UpdateNameCache(NameDBEntry* entry);
        void ForwardAds(Ptr<Socket> socket, std::string& content, Address dest);

        uint16_t m_port;                 //!< Port on which we listen for incoming packets.
        Ptr<Socket> m_socket;            //!< IPv4 Socket
        Ptr<Socket> m_socket6;           //!< IPv6 Socket
        uint64_t m_received;             //!< Number of received packets
        PacketLossCounter m_lossCounter; //!< Lost packet counter
        /// Callbacks for tracing the packet Rx events
        TracedCallback<Ptr<const Packet>> m_rxTrace;

        /// Callbacks for tracing the packet Rx events, includes source and destination addresses
        TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_rxTraceWithAddresses;
    };


    // Switch needs to continuously ping RIB, to ensure liveness.
    // Switches don't need to ping each other.
    // Because if 2 switches can reach the RIB separately,
    // Underlying IP ensures there is atleast one path between them.
    // (Assuming the routers are not configured to explicitly avoid that)
    class OverlaySwitchPingClient: public Application
    {
    public:
        static TypeId GetTypeId();
        // TODO: list of peers
        OverlaySwitchPingClient();
        ~OverlaySwitchPingClient() override;
        void SetRemote(Address ip, uint16_t port);
        void SetRemote(Address addr);
        uint64_t GetTotalTx() const;

    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void Send();

        uint32_t m_count; //!< Maximum number of packets the application will send
        Time m_interval;  //!< Packet inter-send time
        uint32_t m_size;  //!< Size of the sent packet (including the SeqTsHeader)

        uint32_t m_sent;       //!< Counter for sent packets
        uint64_t m_totalTx;    //!< Total bytes sent
        Ptr<Socket> m_socket;  //!< Socket
        Address m_peerAddress; //!< Remote peer address
        uint16_t m_peerPort;   //!< Remote peer port
        EventId m_sendEvent;   //!< Event to send the next packet
    #ifdef NS3_LOG_ENABLE
        std::string m_peerAddressString; //!< Remote peer address string
    #endif  
    };

    class RIBLinkStateManager: public Application
    {
    public:
        static TypeId GetTypeId();
        RIBLinkStateManager();
        ~RIBLinkStateManager() override;
        uint32_t GetLost() const;
        uint64_t GetReceived() const;
        uint16_t GetPacketWindowSize() const;
        void SetContext(void *ctx);
        void SetPacketWindowSize(uint16_t size);
        std::set<Ipv4Address> liveSwitches;
        void *parent_ctx;

    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void HandleRead(Ptr<Socket> socket);
        void SendPeers(Ptr<Socket> socket, Address addr);

        uint16_t m_port;                 //!< Port on which we listen for incoming packets.
        Ptr<Socket> m_socket;            //!< IPv4 Socket
        Ptr<Socket> m_socket6;           //!< IPv6 Socket
        uint64_t m_received;             //!< Number of received packets
        PacketLossCounter m_lossCounter; //!< Lost packet counter

        /// Callbacks for tracing the packet Rx events
        TracedCallback<Ptr<const Packet>> m_rxTrace;

        /// Callbacks for tracing the packet Rx events, includes source and destination addresses
        TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_rxTraceWithAddresses;

    };

    class RIBPathComputer: public Application
    {
    public:
        static TypeId GetTypeId();
        RIBPathComputer();
        ~RIBPathComputer() override;
        uint32_t GetLost() const;
        uint64_t GetReceived() const;
        uint16_t GetPacketWindowSize() const;
        void SetContext(void *ctx);
        void SetPacketWindowSize(uint16_t size);
        void *parent_ctx;

        Graph trust_graph;

    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void HandleRead(Ptr<Socket> socket);
        void ComputeGraph();



        uint16_t m_port;                 //!< Port on which we listen for incoming packets.
        Ptr<Socket> m_socket;            //!< IPv4 Socket
        Ptr<Socket> m_socket6;           //!< IPv6 Socket
        uint64_t m_received;             //!< Number of received packets
        PacketLossCounter m_lossCounter; //!< Lost packet counter

        /// Callbacks for tracing the packet Rx events
        TracedCallback<Ptr<const Packet>> m_rxTrace;

        /// Callbacks for tracing the packet Rx events, includes source and destination addresses
        TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_rxTraceWithAddresses;

    };

    class RIBCertStore: public Application
    {
    public:
        static TypeId GetTypeId();
        RIBCertStore();
        ~RIBCertStore() override;
        uint32_t GetLost() const;
        uint64_t GetReceived() const;
        uint16_t GetPacketWindowSize() const;
        void SetContext(void *ctx);
        void SetPacketWindowSize(uint16_t size);
        std::set<Ipv4Address> liveSwitches;
        void *parent_ctx;

        std::multimap<std::string, std::pair<std::string, int>> trustRelations;
        std::multimap<std::string, std::string> distrustRelations;


    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void HandleRead(Ptr<Socket> socket);

        uint16_t m_port;                 //!< Port on which we listen for incoming packets.
        Ptr<Socket> m_socket;            //!< IPv4 Socket
        Ptr<Socket> m_socket6;           //!< IPv6 Socket
        uint64_t m_received;             //!< Number of received packets
        PacketLossCounter m_lossCounter; //!< Lost packet counter


        /// Callbacks for tracing the packet Rx events
        TracedCallback<Ptr<const Packet>> m_rxTrace;

        /// Callbacks for tracing the packet Rx events, includes source and destination addresses
        TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_rxTraceWithAddresses;
    };


    class OverlaySwitchForwardingEngine: public Application
    {
    public:
        static TypeId GetTypeId();
        OverlaySwitchForwardingEngine();
        ~OverlaySwitchForwardingEngine() override;
        uint32_t GetLost() const;
        uint64_t GetReceived() const;
        uint16_t GetPacketWindowSize() const;
        void SetPacketWindowSize(uint16_t size);
        uint32_t td_num;
        Time peer_calc_delay;
        Address rib_addr;

    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void HandleRead(Ptr<Socket> socket);

        uint16_t m_port;                 //!< Port on which we listen for incoming packets.
        Ptr<Socket> m_socket;            //!< IPv4 Socket
        Ptr<Socket> m_socket6;           //!< IPv6 Socket
        uint64_t m_received;             //!< Number of received packets
        PacketLossCounter m_lossCounter; //!< Lost packet counter

        /// Callbacks for tracing the packet Rx events
        TracedCallback<Ptr<const Packet>> m_rxTrace;

        /// Callbacks for tracing the packet Rx events, includes source and destination addresses
        TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_rxTraceWithAddresses;

        std::map<int, std::set<Address>> oswitch_in_other_td;

        void ForwardPacket(Address who, uint32_t port, Ptr<Packet> what);
        void PopulatePeers();
        Ptr<Socket> givepeers_socket;
        void HandlePeersCallback(Ptr<Socket> sock);
        std::map<int, Ipv4Address> temp_peering_rib_addrs;
        void PopulateSwitches(int __td_num, Ipv4Address addr);
        void HandleSwitchesCallback(Ptr<Socket> sock);
        std::vector<Ptr<Socket>> giveswitches_sockets;
        std::map<Address, Ptr<Socket>> sock_cache;

    };

    class DummyClient: public Application
    {
    public:
        static TypeId GetTypeId();
        DummyClient();
        ~DummyClient() override;
        void SetRemote(Address ip, uint16_t port);
        void SetRemote(Address addr);
        uint64_t GetTotalTx() const;
        std::set<Ipv4Address> switches_in_my_td;

    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void Send();
        void GetSwitch();
        void HandleSwitch(Ptr<Socket> sock);
        uint32_t m_count; //!< Maximum number of packets the application will send
        Time m_interval;  //!< Packet inter-send time
        uint32_t m_size;  //!< Size of the sent packet (including the SeqTsHeader)

        uint32_t m_sent;       //!< Counter for sent packets
        uint64_t m_totalTx;    //!< Total bytes sent
        Ptr<Socket> m_socket;  //!< Socket
        Ptr<Socket> switch_socket;
        Address m_peerAddress; //!< Remote peer address
        uint16_t m_peerPort;   //!< Remote peer port
        EventId m_sendEvent;   //!< Event to send the next packet
    #ifdef NS3_LOG_ENABLE
        std::string m_peerAddressString; //!< Remote peer address string
    #endif
    };


    class DummyClient2: public Application
    {
    public:
        static TypeId GetTypeId();
        DummyClient2();
        ~DummyClient2() override;
        void SetRemote(Address ip, uint16_t port);
        void SetRemote(Address addr);
        uint64_t GetTotalTx() const;
        std::set<Ipv4Address> switches_in_my_td;
        std::set<std::string> dcnames_to_route;
        std::map<Address, int> peers_to_ASNum;

    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void Send();
        void SendUsingPath(std::vector<Ipv4Address>& path, Ipv4Address& destination_ip);
        void GetSwitch();
        void GetAds();
        void HandleSwitch(Ptr<Socket> sock);

        uint32_t m_count; //!< Maximum number of packets the application will send
        Time m_interval;  //!< Packet inter-send time
        uint32_t m_size;  //!< Size of the sent packet (including the SeqTsHeader)

        uint32_t m_sent;       //!< Counter for sent packets
        uint64_t m_totalTx;    //!< Total bytes sent
        Ptr<Socket> m_socket;  //!< Socket
        Ptr<Socket> switch_socket;
        Address m_peerAddress; //!< Remote peer address
        uint16_t m_peerPort;   //!< Remote peer port
        EventId m_sendEvent;   //!< Event to send the next packet
        // EventId m_sendEvent2;
        
    #ifdef NS3_LOG_ENABLE
        std::string m_peerAddressString; //!< Remote peer address string
    #endif
    };
}

class RIB
{
    public:
        Ptr<RIBAdStore> adStore;
        Ptr<RIBLinkStateManager> linkManager;
        Ptr<RIBCertStore> certStore;
        Ptr<RIBPathComputer> pathComputer;
        Address my_addr;
        
        std::unordered_map<std::string, std::vector<NameDBEntry*>> *ads;
        std::set<Ipv4Address> *liveSwitches;
        std::multimap<std::string, std::pair<std::string, int>> *trustRelations;
        std::multimap<std::string, std::string> *distrustRelations;
        std::map<int, Address> peers;
        std::map<Address, int> peers_to_ASNum;
        int td_num;

        RIB(int td_num, Address myAddr, std::map<std::string, int> *addr_map);
        ~RIB();
        ApplicationContainer Install(Ptr<Node> node);
        bool AddPeers(std::vector<std::pair<int, Address>> &addresses);

        ApplicationContainer InstallTraceRoute(const std::vector<Address>& all_ribs_, std::map<std::string, int> *addr_map);

    private:
        std::map<std::string, int> *addr_map_;
        Ptr<Node> my_node;
        ns3::ObjectFactory adStoreFactory;
        ns3::ObjectFactory linkManagerFactory;
        ns3::ObjectFactory certStoreFactory;
        ns3::ObjectFactory pathComputerFactory;
};



class OverlaySwitch
{
    public:
        Ptr<OverlaySwitchPingClient> pingClient;
        Ptr<OverlaySwitchForwardingEngine> fwdEng;
        Address my_addr;
        Address rib_addr;
        int td_num;
        Time peer_calc_delay;

        OverlaySwitch(int td_num, Address myAddr, Address ribAddr, Time peer_calc_delay_);
        ~OverlaySwitch();
        ApplicationContainer Install(Ptr<Node> node);

    private:
        ns3::ObjectFactory pingClientFactory;
        ns3::ObjectFactory pingServerFactory;
        ns3::ObjectFactory fwdEngFactory;
};

class DCServer
{
    public:
        Ptr<DCServerAdvertiser> advertiser;
        Address my_addr;
        Address rib_addr;
        Address switch_addr;

        DCServer(Address myAddr, Address ribAddr);
        ~DCServer();
        ApplicationContainer Install(Ptr<Node> node);

    private:
        ns3::ObjectFactory advertiserFactory;
};



/* Util class reprenting a row in the advertisement store db */
class NameDBEntry
{
public:
    NameDBEntry(std::string& _dc_name, Ipv4Address& _origin_AS_addr, std::string& _td_path, Ipv4Address&  _origin_server);

    ~NameDBEntry();

    static NameDBEntry* FromAdvertisementStr(std::string& serialized);
    std::string ToAdvertisementStr();

    std::string dc_name;
    Ipv4Address origin_AS_addr;
    std::vector<Ipv4Address> td_path;
    Ipv4Address origin_server;

    // possibly also expire time...


};