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

#define RIBADSTORE_PORT 3001
#define RIBLSM_PORT 3002
#define OVERLAY_PING 3003


using namespace ns3;

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
        void SendPeers(Ptr<Socket> socket, Address dest);
        std::unordered_map<std::string, int> db;

        void *parent_ctx;
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
        void SetPacketWindowSize(uint16_t size);
        std::set<Address> liveSwitches;

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
}

class RIB
{
    public:
        Ptr<RIBAdStore> adStore;
        Ptr<RIBLinkStateManager> linkManager;
        Address my_addr;
        std::unordered_map<std::string, int> *ads;
        std::set<Address> *liveSwitches;
        std::set<Address> peers;

        RIB(Address myAddr);
        ~RIB();
        ApplicationContainer Install(Ptr<Node> node);
        bool AddPeers(std::vector<Address> &addresses);

    private:
        ns3::ObjectFactory adStoreFactory;
        ns3::ObjectFactory linkManagerFactory;
};

class OverlaySwitch
{
    public:
        Ptr<OverlaySwitchPingClient> pingClient;
        Address my_addr;
        Address rib_addr;

        OverlaySwitch(Address myAddr, Address ribAddr);
        ~OverlaySwitch();
        ApplicationContainer Install(Ptr<Node> node);

    private:
        ns3::ObjectFactory pingClientFactory;
        ns3::ObjectFactory pingServerFactory;
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
