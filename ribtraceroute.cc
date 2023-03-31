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
 * Author: Alberto Gallegos Ramonet
 *
 * Traceroute uses ICMPV4 echo messages to trace all the middle hops to a given destination.
 * It also shows the delay time it takes for a round trip to complete for each
 * set probe (default 3).
 *
 */


#include "ribtraceroute.h"
#include "ns3/names.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("RIBTraceRoute");
NS_OBJECT_ENSURE_REGISTERED(RIBTraceRoute);

TypeId
RIBTraceRoute::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RIBTraceRoute")
                            .SetParent<Application>()
                            .SetGroupName("Internet-Apps")
                            .AddConstructor<RIBTraceRoute>()
                            .AddAttribute("Remote",
                                          "The address of the machine we want to trace.",
                                          Ipv4AddressValue(),
                                          MakeIpv4AddressAccessor(&RIBTraceRoute::m_remote),
                                          MakeIpv4AddressChecker())
                            .AddAttribute("Verbose",
                                          "Produce usual output.",
                                          BooleanValue(true),
                                          MakeBooleanAccessor(&RIBTraceRoute::m_verbose),
                                          MakeBooleanChecker())
                            .AddAttribute("Interval",
                                          "Wait interval between sent packets.",
                                          TimeValue(Seconds(0)),
                                          MakeTimeAccessor(&RIBTraceRoute::m_interval),
                                          MakeTimeChecker())
                            .AddAttribute("Size",
                                          "The number of data bytes to be sent, real packet will "
                                          "be 8 (ICMP) + 20 (IP) bytes longer.",
                                          UintegerValue(56),
                                          MakeUintegerAccessor(&RIBTraceRoute::m_size),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("MaxHop",
                                          "The maximum number of hops to trace.",
                                          UintegerValue(30),
                                          MakeUintegerAccessor(&RIBTraceRoute::m_maxTtl),
                                          MakeUintegerChecker<uint32_t>())
                            .AddAttribute("ProbeNum",
                                          "The number of packets send to each hop.",
                                          UintegerValue(3),
                                          MakeUintegerAccessor(&RIBTraceRoute::m_maxProbes),
                                          MakeUintegerChecker<uint16_t>())
                            .AddAttribute("Timeout",
                                          "The waiting time for a route response before a timeout.",
                                          TimeValue(Seconds(5)),
                                          MakeTimeAccessor(&RIBTraceRoute::m_waitIcmpReplyTimeout),
                                          MakeTimeChecker());
    return tid;
}

RIBTraceRoute::RIBTraceRoute()
    : m_interval(Seconds(0)),
      m_size(56),
      m_socket(nullptr),
      m_seq(0),
      m_verbose(true),
      m_probeCount(0),
      m_maxProbes(3),
      m_ttl(1),
      m_maxTtl(30),
      m_waitIcmpReplyTimeout(Seconds(5))
{
    m_osRoute.clear();
    m_routeIpv4.clear();
    parent_ctx = NULL;
}

RIBTraceRoute::~RIBTraceRoute()
{
}

void
RIBTraceRoute::Print(Ptr<OutputStreamWrapper> stream, std::map<std::string, int> *addr_map_)
{
    addr_map = addr_map_;
    m_printStream = stream;
}

void
RIBTraceRoute::StartApplication()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_LOGIC("Application started");
    m_started = Simulator::Now();

    if (m_verbose)
    {
        NS_LOG_UNCOND("Traceroute to " << m_remote << ", " << m_maxTtl << " hops Max, " << m_size
                                       << " bytes of data.");
    }

    if (m_printStream)
    {
        *m_printStream->GetStream() << "Traceroute to " << m_remote << ", " << m_maxTtl
                                    << " hops Max, " << m_size << " bytes of data.\n";
    }

    m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::Ipv4RawSocketFactory"));
    m_socket->SetAttribute("Protocol", UintegerValue(Icmpv4L4Protocol::PROT_NUMBER));

    NS_ASSERT(m_socket);
    m_socket->SetRecvCallback(MakeCallback(&RIBTraceRoute::Receive, this));

    InetSocketAddress src = InetSocketAddress(Ipv4Address::GetAny(), 0);
    int status;
    status = m_socket->Bind(src);
    NS_ASSERT(status != -1);

    m_next = Simulator::ScheduleNow(&RIBTraceRoute::StartWaitReplyTimer, this);
}

void
RIBTraceRoute::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_next.IsRunning())
    {
        m_next.Cancel();
    }

    if (m_waitIcmpReplyTimer.IsRunning())
    {
        m_waitIcmpReplyTimer.Cancel();
    }

    if (m_socket)
    {
        m_socket->Close();
    }

    if (m_verbose)
    {
        NS_LOG_UNCOND("\nTrace Complete");
    }

    if (m_printStream)
    {
        *m_printStream->GetStream() << "Trace Complete\n" << std::endl;
        *m_printStream->GetStream() << "Map output:" << std::endl;
        std::set<int> all_as;
        for (auto &x: traceOutput){
            int as = -1;
            std::stringstream ss;
            ss << x.second;
            auto it = addr_map->find(ss.str());
            if (it != addr_map->end()){
                as = it->second;
            }
            *m_printStream->GetStream() << x.first << " " << x.second << " AS: " << as << std::endl;
            if (as != -1){
                all_as.insert(as);
            }
        }

    
        if (parent_ctx && all_as.size() == 2){
            for (auto &as: all_as){
                if (as != parent_ctx->td_num){
                    parent_ctx->peers[as] = m_remote;
                }
            }
        }
    }
}

void
RIBTraceRoute::DoDispose()
{
    NS_LOG_FUNCTION(this);

    if (m_next.IsRunning() || m_waitIcmpReplyTimer.IsRunning())
    {
        StopApplication();
    }

    m_socket = nullptr;
    Application::DoDispose();
}

uint32_t
RIBTraceRoute::GetApplicationId() const
{
    NS_LOG_FUNCTION(this);
    Ptr<Node> node = GetNode();
    for (uint32_t i = 0; i < node->GetNApplications(); ++i)
    {
        if (node->GetApplication(i) == this)
        {
            return i;
        }
    }
    NS_ASSERT_MSG(false, "forgot to add application to node");
    return 0;
}

void
RIBTraceRoute::Receive(Ptr<Socket> socket)
{
    NS_LOG_FUNCTION(this << socket);

    while (m_socket->GetRxAvailable() > 0)
    {
        Address from;
        Ptr<Packet> p = m_socket->RecvFrom(0xffffffff, 0, from);
        NS_LOG_DEBUG("recv " << p->GetSize() << " bytes");
        NS_ASSERT(InetSocketAddress::IsMatchingType(from));
        InetSocketAddress realFrom = InetSocketAddress::ConvertFrom(from);
        NS_ASSERT(realFrom.GetPort() == 1);
        Ipv4Header ipv4;
        p->RemoveHeader(ipv4);
        NS_ASSERT(ipv4.GetProtocol() == Icmpv4L4Protocol::PROT_NUMBER);
        Icmpv4Header icmp;
        p->RemoveHeader(icmp);

        if (icmp.GetType() == Icmpv4Header::ICMPV4_TIME_EXCEEDED)
        {
            Icmpv4TimeExceeded timeoutResp;
            p->RemoveHeader(timeoutResp);

            // GetData () gets 64 bits of data, but the received packet
            // only contains 32 bits of data.
            uint8_t data[8];
            timeoutResp.GetData(data);

            // Get the 7th and 8th Octect to obtain the Sequence number from
            // the original packet.
            uint16_t recvSeq;
            recvSeq = (uint16_t)data[7] << 0;
            recvSeq |= (uint16_t)data[6] << 8;

            std::map<uint16_t, Time>::iterator i = m_sent.find(recvSeq);
            if (i != m_sent.end())
            {
                Time sendTime = i->second;
                NS_ASSERT(Simulator::Now() >= sendTime);
                Time delta = Simulator::Now() - sendTime;

                m_routeIpv4.str("");
                m_routeIpv4.clear();
                m_routeIpv4 << realFrom.GetIpv4();
                m_osRoute << delta.As(Time::MS);
                if (m_probeCount == m_maxProbes)
                {
                    if (m_verbose)
                    {
                        NS_LOG_UNCOND(m_ttl << " " << m_routeIpv4.str() << " " << m_osRoute.str());
                    }

                    if (m_printStream)
                    {
                        traceOutput[m_ttl] = realFrom.GetIpv4();
                        *m_printStream->GetStream()
                            << m_ttl << " " << m_routeIpv4.str() << " " << m_osRoute.str() << "\n";
                    }
                    m_osRoute.str("");
                    m_osRoute.clear();
                    m_routeIpv4.str("");
                    m_routeIpv4.clear();
                }
                else
                {
                    m_osRoute << " ";
                }

                m_waitIcmpReplyTimer.Cancel();

                if (m_ttl < m_maxTtl + 1)
                {
                    m_next =
                        Simulator::Schedule(m_interval, &RIBTraceRoute::StartWaitReplyTimer, this);
                }
            }
        }
        else if (icmp.GetType() == Icmpv4Header::ICMPV4_ECHO_REPLY &&
                 m_remote == realFrom.GetIpv4())
        {
            // When UDP is used, TraceRoute should stop until ICMPV4_DEST_UNREACH
            // (with code (3) PORT_UNREACH) is received, however, the current
            // ns-3 implementation does not include the UDP version of traceroute.
            // The traceroute ICMP version (the current version) stops until max_ttl is reached
            // or until an ICMP ECHO REPLY is received m_maxProbes times.

            Icmpv4Echo echo;
            p->RemoveHeader(echo);
            std::map<uint16_t, Time>::iterator i = m_sent.find(echo.GetSequenceNumber());

            if (i != m_sent.end() && echo.GetIdentifier() == 0)
            {
                uint32_t dataSize = echo.GetDataSize();

                if (dataSize == m_size)
                {
                    Time sendTime = i->second;
                    NS_ASSERT(Simulator::Now() >= sendTime);
                    Time delta = Simulator::Now() - sendTime;

                    m_sent.erase(i);

                    if (m_verbose)
                    {
                        m_routeIpv4.str("");
                        m_routeIpv4.clear();
                        m_routeIpv4 << realFrom.GetIpv4();
                        m_osRoute << delta.As(Time::MS);

                        if (m_probeCount == m_maxProbes)
                        {
                            NS_LOG_UNCOND(m_ttl << " " << m_routeIpv4.str() << " "
                                                << m_osRoute.str());
                            if (m_printStream)
                            {
                                traceOutput[m_ttl] = realFrom.GetIpv4();
                                *m_printStream->GetStream() << m_ttl << " " << m_routeIpv4.str()
                                                            << " " << m_osRoute.str() << "\n";
                            }

                            m_osRoute.clear();
                            m_routeIpv4.clear();
                        }
                        else
                        {
                            m_osRoute << " ";
                        }
                    }
                }
            }

            m_waitIcmpReplyTimer.Cancel();
            if (m_probeCount == m_maxProbes)
            {
                if (m_verbose)
                {
                    NS_LOG_UNCOND("\nTrace Complete");
                }

                if (m_printStream)
                {
                    *m_printStream->GetStream() << "Trace Complete\n" << std::endl;
                }
                Simulator::ScheduleNow(&RIBTraceRoute::StopApplication, this);
            }
            else if (m_ttl < m_maxTtl + 1)
            {
                m_next = Simulator::Schedule(m_interval, &RIBTraceRoute::StartWaitReplyTimer, this);
            }
        }
    }
}

void
RIBTraceRoute::Send()
{
    NS_LOG_INFO("m_seq=" << m_seq);
    Ptr<Packet> p = Create<Packet>();
    Icmpv4Echo echo;
    echo.SetSequenceNumber(m_seq);
    m_seq++;
    echo.SetIdentifier(0);

    //
    // We must write quantities out in some form of network order.  Since there
    // isn't an htonl to work with we just follow the convention in pcap traces
    // (where any difference would show up anyway) and borrow that code.  Don't
    // be too surprised when you see that this is a little endian convention.
    //
    NS_ASSERT(m_size >= 16);

    Ptr<Packet> dataPacket = Create<Packet>(m_size);
    echo.SetData(dataPacket);
    p->AddHeader(echo);
    Icmpv4Header header;
    header.SetType(Icmpv4Header::ICMPV4_ECHO);
    header.SetCode(0);
    if (Node::ChecksumEnabled())
    {
        header.EnableChecksum();
    }

    p->AddHeader(header);

    if (m_probeCount < m_maxProbes)
    {
        m_probeCount++;
    }
    else
    {
        m_probeCount = 1;
        m_ttl++;
    }

    m_sent.insert(std::make_pair(m_seq - 1, Simulator::Now()));
    m_socket->SetIpTtl(m_ttl);

    InetSocketAddress dst = InetSocketAddress(m_remote, 0);
    m_socket->SendTo(p, 0, dst);
}

void
RIBTraceRoute::StartWaitReplyTimer()
{
    NS_LOG_FUNCTION(this);
    if (!m_waitIcmpReplyTimer.IsRunning())
    {
        NS_LOG_LOGIC("Starting WaitIcmpReplyTimer at " << Simulator::Now() << " for "
                                                       << m_waitIcmpReplyTimeout);

        m_waitIcmpReplyTimer = Simulator::Schedule(m_waitIcmpReplyTimeout,
                                                   &RIBTraceRoute::HandleWaitReplyTimeout,
                                                   this);
        Send();
    }
}

void
RIBTraceRoute::HandleWaitReplyTimeout()
{
    if (m_ttl < m_maxTtl + 1)
    {
        m_next = Simulator::Schedule(m_interval, &RIBTraceRoute::StartWaitReplyTimer, this);
    }

    m_osRoute << "*  ";
    if (m_probeCount == m_maxProbes)
    {
        if (m_verbose)
        {
            NS_LOG_UNCOND(m_ttl << " " << m_routeIpv4.str() << " " << m_osRoute.str());
        }

        if (m_printStream)
        {
            traceOutput[m_ttl] = Ipv4Address(m_routeIpv4.str().c_str());
            *m_printStream->GetStream()
                << m_ttl << " " << m_routeIpv4.str() << " " << m_osRoute.str() << "\n";
        }
        m_osRoute.str("");
        m_osRoute.clear();
        m_routeIpv4.str("");
        m_routeIpv4.clear();
    }
}

RIBTraceRouteHelper::RIBTraceRouteHelper(Ipv4Address remote)
{
    m_factory.SetTypeId("ns3::RIBTraceRoute");
    m_factory.Set("Remote", Ipv4AddressValue(remote));
}

void
RIBTraceRouteHelper::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
RIBTraceRouteHelper::Install(RIB *rib, Ptr<Node> node) const
{
    return ApplicationContainer(InstallPriv(rib, node));
}

ApplicationContainer
RIBTraceRouteHelper::Install(RIB *rib, std::string nodeName) const
{
    Ptr<Node> node = Names::Find<Node>(nodeName);
    return ApplicationContainer(InstallPriv(rib, node));
}

ApplicationContainer
RIBTraceRouteHelper::Install(RIB *rib, NodeContainer c) const
{
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        apps.Add(InstallPriv(rib, *i));
    }

    return apps;
}

Ptr<Application>
RIBTraceRouteHelper::InstallPriv(RIB *rib, Ptr<Node> node) const
{
    Ptr<RIBTraceRoute> app = m_factory.Create<RIBTraceRoute>();
    app->parent_ctx = rib;
    node->AddApplication(app);

    return app;
}

void
RIBTraceRouteHelper::PrintTraceRouteAt(Ptr<Node> node, Ptr<OutputStreamWrapper> stream, std::map<std::string, int> *addr_map)
{
    Ptr<RIBTraceRoute> trace;

    for (uint32_t i = node->GetNApplications() - 1; i >= 0; i--)
    {
        trace = node->GetApplication(i)->GetObject<RIBTraceRoute>();
        if (trace)
        {
            *stream->GetStream() << "Tracing Route from Node " << node->GetId() << "\n";
            trace->Print(stream, addr_map);
            return;
        }
    }
    NS_ASSERT_MSG(false, "No RIBTraceRoute application found in node " << node->GetId());
}


} // namespace ns3
