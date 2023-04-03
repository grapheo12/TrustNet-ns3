/*
 * Copyright (c) 2019 Ritsumeikan University, Shiga, Japan
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Alberto Gallegos Ramonet <ramonet@fc.ritsumei.ac.jp>
 */

#ifndef RIBTRACEROUTE_H
#define RIBTRACEROUTE_H

#include "ns3/application.h"
#include "ns3/average.h"
#include "ns3/nstime.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/simulator.h"
#include "ns3/traced-callback.h"

#include "ns3/assert.h"
#include "ns3/boolean.h"
#include "ns3/icmpv4-l4-protocol.h"
#include "ns3/icmpv4.h"
#include "ns3/inet-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/uinteger.h"
#include "ns3/application-container.h"
#include "ns3/node-container.h"
#include "ns3/object-factory.h"
#include "ns3/output-stream-wrapper.h"
#include "main.h"

#include <map>

namespace ns3
{

class Socket;

/**
 * \ingroup internet-apps
 * \defgroup v4traceroute V4Traceroute
 */

/**
 * \ingroup v4traceroute
 * \brief Traceroute application sends one ICMP ECHO request with TTL=1,
 *        and after receiving an ICMP TIME EXCEED reply, it increases the
 *        TTL and repeat the process to reveal all the intermediate hops to
 *        the destination.
 *
 */
class RIBTraceRoute : public Application
{
  public:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    RIBTraceRoute();
    ~RIBTraceRoute() override;
    RIB *parent_ctx;
    /**
     * \brief Prints the application traced routes into a given OutputStream.
     * \param stream the output stream
     */
    void Print(Ptr<OutputStreamWrapper> stream, std::map<std::string, int> *addr_map);

  private:
    std::map<int, Ipv4Address> traceOutput;
    std::map<std::string, int> *addr_map;
    void StartApplication() override;
    void StopApplication() override;
    void DoDispose() override;
    /**
     * \brief Return the application ID in the node.
     * \returns the application id
     */
    uint32_t GetApplicationId() const;
    /**
     * \brief Receive an ICMP Echo
     * \param socket the receiving socket
     *
     * This function is called by lower layers through a callback.
     */
    void Receive(Ptr<Socket> socket);

    /** \brief Send one (ICMP ECHO) to the destination.*/
    void Send();

    /** \brief Starts a timer after sending an ICMP ECHO.*/
    void StartWaitReplyTimer();

    /** \brief Triggers an action if an ICMP TIME EXCEED have not being received
     *         in the time defined by StartWaitReplyTimer.
     */
    void HandleWaitReplyTimeout();

    /// Remote address
    Ipv4Address m_remote;

    /// Wait  interval  seconds between sending each packet
    Time m_interval;
    /**
     * Specifies  the number of data bytes to be sent.
     * The default is 56, which translates into 64 ICMP data bytes when
     * combined with the 8 bytes of ICMP header data.
     */
    uint32_t m_size;
    /// The socket we send packets from
    Ptr<Socket> m_socket;
    /// ICMP ECHO sequence number
    uint16_t m_seq;
    /// produce traceroute style output if true
    bool m_verbose;
    /// Start time to report total ping time
    Time m_started;
    /// Next packet will be sent
    EventId m_next;
    /// The Current probe value
    uint32_t m_probeCount;
    /// The maximum number of probe packets per hop
    uint16_t m_maxProbes;
    /// The current TTL value
    uint16_t m_ttl;
    /// The maximium Ttl (Max number of hops to trace)
    uint32_t m_maxTtl;
    /// The wait time until the response is considered lost.
    Time m_waitIcmpReplyTimeout;
    /// The timer used to wait for the probes ICMP replies
    EventId m_waitIcmpReplyTimer;
    /// All sent but not answered packets. Map icmp seqno -> when sent
    std::map<uint16_t, Time> m_sent;

    /// Stream of characters used for printing a single route
    std::ostringstream m_osRoute;
    /// The Ipv4 address of the latest hop found
    std::ostringstream m_routeIpv4;
    /// Stream of the traceroute used for the output file
    Ptr<OutputStreamWrapper> m_printStream;
};


class RIBTraceRouteHelper
{
  public:
    /**
     * Create a V4TraceRouteHelper which is used to make life easier for people wanting
     * to use TraceRoute
     *
     * \param remote The address which should be traced
     */
    RIBTraceRouteHelper(Ipv4Address remote);

    /**
     * Install a TraceRoute application on each Node in the provided NodeContainer.
     *
     * \param nodes The NodeContainer containing all of the nodes to get a V4TraceRoute
     *              application.
     *
     * \returns A list of TraceRoute applications, one for each input node
     */
    ApplicationContainer Install(RIB *rib, NodeContainer nodes) const;

    /**
     * Install a TraceRoute application on the provided Node.  The Node is specified
     * directly by a Ptr<Node>
     *
     * \param node The node to install the V4TraceRouteApplication on.
     *
     * \returns An ApplicationContainer holding the TraceRoute application created.
     */
    ApplicationContainer Install(RIB *rib, Ptr<Node> node) const;

    /**
     * Install a TraceRoute application on the provided Node.  The Node is specified
     * by a string that must have previously been associated with a Node using the
     * Object Name Service.
     *
     * \param nodeName The node to install the V4TraceRouteApplication on.
     *
     * \returns An ApplicationContainer holding the TraceRoute application created.
     */
    ApplicationContainer Install(RIB *rib, std::string nodeName) const;

    /**
     * \brief Configure traceRoute applications attribute
     * \param name   attribute's name
     * \param value  attribute's value
     */
    void SetAttribute(std::string name, const AttributeValue& value);
    /**
     * \brief Print the resulting trace routes from given node.
     * \param node The origin node where the traceroute is initiated.
     * \param stream The outputstream used to print the resulting traced routes.
     */
    static void PrintTraceRouteAt(Ptr<Node> node, Ptr<OutputStreamWrapper> stream, std::map<std::string, int> *addr_map);

  private:
    /**
     * \brief Do the actual application installation in the node
     * \param node the node
     * \returns a Smart pointer to the installed application
     */
    Ptr<Application> InstallPriv(RIB *rib, Ptr<Node> node) const;
    /// Object factory
    ObjectFactory m_factory;
};
}
#endif /*V4TRACEROUTE_H*/
