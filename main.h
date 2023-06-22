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
#define DCSERVER_ECHO_PORT 3007
#define CLIENT_REPLY_PORT 3008
#define OVERLAY_PROBER_PORT 3009
#define CLIENT_PROBER_PORT 3010
#define PACKET_MAGIC_UP 0xdeadface
#define PACKET_MAGIC_DOWN 0xcafebabe

using namespace ns3;

/* Declaring the utility class to pass compilation */
class NameDBEntry; 

/* Global map for mapping AS with their addresses*/
extern std::map<int, Address> global_AS_to_addr;
extern std::map<Address, int> global_addr_to_AS;

struct Graph {
    std::map<std::string, int> nodes_to_id;
    std::map<int, std::string> id_to_nodes;
    std::multimap<int, int> trust_edges;                                    // adjacency list representation
    std::set<std::pair<int, int>> distrust_edges;
    std::map<std::pair<int, int>, int> transitivity;                        // edge -> r_transitivity
    std::map<std::pair<int, int>, std::pair<int, int>> __distMatrix;        // (u, v) ---> (min dist from u to v, last node on path)
    int __node_cnt;

    void FloydWarshall();
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

    class DCEchoServer: public Application
    {
    public:
        static TypeId GetTypeId();
        DCEchoServer();
        ~DCEchoServer() override;
        uint32_t GetLost() const;
        uint64_t GetReceived() const;
        uint16_t GetPacketWindowSize() const;
        void SetPacketWindowSize(uint16_t size);
        Address my_rib;

    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void HandleRead(Ptr<Socket> socket);
        void HandleSwitch(Ptr<Socket> socket);
        bool isSwitchSet;
        Ptr<Socket> switch_socket;
        Ptr<Socket> reply_socket;
        std::set<Ipv4Address> switches_in_my_td;

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


    class OverlaySwitchNeighborProber: public Application
    {
    public:
        static TypeId GetTypeId();
        OverlaySwitchNeighborProber();
        ~OverlaySwitchNeighborProber() override;
        uint64_t GetTotalTx() const;

        void SimpliEchoBack(Ptr<Socket> socket, Address from, std::string& packet);
        void SimpliEchoBackClient(Address from, std::string& packet);
        void SimpliEchoRequest(Ptr<Socket> socket, Address to);
        std::unordered_map<int, std::pair<Address, int64_t>>& GetNearestPeerOSwitchMap();
        std::optional<Address> GetNearestOverlaySwitchInTD(int tdNumber);
        std::optional<Address> GetRROverlaySwitchInTD(int tdNumber);
        void* parent_ctx;

    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void Probe();
        void HandleRead(Ptr<Socket> socket);

        Time m_interval;  //!< Packet inter-send time
        uint32_t m_size;  //!< Size of the sent packet (including the SeqTsHeader)
        uint32_t max_packets;

        uint32_t m_sent;       //!< Counter for sent packets
        uint64_t m_totalTx;    //!< Total bytes sent
        Ptr<Socket> m_socket;  //!< Socket
        uint16_t m_port;       //!< Port on which we listen for incoming packets.
        EventId m_sendEvent;   //!< Event to send the next packet

        uint32_t rr_cnt;
        /// Callbacks for tracing the packet Rx events
        TracedCallback<Ptr<const Packet>> m_rxTrace;

        /// Callbacks for tracing the packet Rx events, includes source and destination addresses
        TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_rxTraceWithAddresses;
        
        std::unordered_map<int, std::pair<Address, int64_t>> m_nearestOverlaySwitchInPeerTDs;
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
        bool isItMe(std::string entity);
        std::vector<std::string> GetPath(std::string startNode, std::string endNode);
        void SendPath(Ptr<Socket> socket, Address dest, std::string path);

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
        const std::map<int, Ipv4Address>& GetPeerRibAddressMap() const;
        const std::map<int, std::set<Address>>& GetOverlaySwitchInOtherTDMap() const;
        uint32_t td_num;
        Time peer_calc_delay;
        Address rib_addr;
        void* parent_ctx; // Point to the parent OverlaySwitch class

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
        void SimpliEchoRequest(const Ipv4Address& dest);
        std::set<Ipv4Address> switches_in_my_td;
        std::set<std::string> dcnames_to_route;
        std::map<Address, int> peers_to_ASNum;
        Ipv4Address my_ip;

    protected:
        void DoDispose() override;

    private:
        void StartApplication() override;
        void StopApplication() override;
        void Send();
        void SendUsingPath(std::vector<std::string>& path, std::string& destination_ip);
        void GetSwitch();
        void GetPath();
        void HandleSwitch(Ptr<Socket> sock);
        void PledgeAllegiance();
        void HandleDCResponse(Ptr<Socket> sock);
        void HandleProberResponse(Ptr<Socket> sock);

        uint32_t m_count; //!< Maximum number of packets the application will send
        Time m_interval;  //!< Packet inter-send time
        uint32_t m_size;  //!< Size of the sent packet (including the SeqTsHeader)
        std::string m_name;

        uint32_t m_sent;       //!< Counter for sent packets
        uint64_t m_totalTx;    //!< Total bytes sent
        Ptr<Socket> m_socket;  //!< Socket
        Ptr<Socket> path_computer_socket;
        Ptr<Socket> switch_socket;
        Ptr<Socket> reply_socket;
        Ptr<Socket> switch_prober_server_socket; //!< server socket for accepting pushed packets from oswitch within domain
        Address m_peerAddress; //!< Remote peer address
        uint16_t m_peerPort;   //!< Remote peer port
        EventId m_sendEvent;   //!< Event to send the next packet
        
        std::optional<std::pair<Ipv4Address, int64_t>> m_nearestOverlaySwitchInMyDomain;
        
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

        std::map<Address, int> rib_addr_map_;
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
        Ptr<OverlaySwitchNeighborProber> neighborProber;
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
        ns3::ObjectFactory neighborProberFactory;
};

class DCServer
{
    public:
        Ptr<DCServerAdvertiser> advertiser;
        Ptr<DCEchoServer> echoServer;
        Address my_addr;
        Address rib_addr;
        Address switch_addr;

        DCServer(Address myAddr, Address ribAddr);
        ~DCServer();
        ApplicationContainer Install(Ptr<Node> node);

    private:
        ns3::ObjectFactory advertiserFactory;
        ns3::ObjectFactory echoServerFactory;
};



/* Util class reprenting a row in the advertisement store db */
class NameDBEntry
{
public:
    struct TrustCert {
        std::string type;
        std::string entity;
        std::string issuer;
        int r_transitivity;
    };

    struct DistrustCert {
        std::string type;
        std::string entity;
        std::string issuer;
    };

    NameDBEntry(std::string& _dc_name, Ipv4Address& _origin_AS_addr, std::string& _td_path, Ipv4Address&  _origin_server, TrustCert _trust_cert, std::vector<DistrustCert> _distrust_cert);

    ~NameDBEntry();

    static NameDBEntry* FromAdvertisementStr(std::string& serialized);
    std::string ToAdvertisementStr();

    std::string dc_name;
    Ipv4Address origin_AS_addr;
    std::vector<Ipv4Address> td_path;
    Ipv4Address origin_server;
    TrustCert trust_cert;
    std::vector<DistrustCert> distrust_certs;

    // possibly also expire time...


};