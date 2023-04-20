#include "main.h"


namespace ns3
{
    NS_LOG_COMPONENT_DEFINE("DCOwner");

    NS_OBJECT_ENSURE_REGISTERED(DCOwner);

    TypeId
    DCOwner::GetTypeId()
    {
        static TypeId tid =
            TypeId("ns3::DCOwner")
                .SetParent<Application>()
                .SetGroupName("Applications")
                .AddConstructor<DCOwner>();
        return tid;
    }

    DCOwner::DCOwner()
    {
        NS_LOG_FUNCTION(this);
    }

    DCOwner::~DCOwner()
    {
        NS_LOG_FUNCTION(this);
    }

    void
    DCOwner::DoDispose()
    {
        NS_LOG_FUNCTION(this);
        Application::DoDispose();
    }

    void
    DCOwner::StartApplication()
    {
        NS_LOG_FUNCTION(this);

        for (auto it = certs_to_send.begin(); it != certs_to_send.end(); it++){
            TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
            Ptr<Socket> m_socket = Socket::CreateSocket(GetNode(), tid);
            Address m_peerAddress = it->rib_addr;
            if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
            {
                if (m_socket->Bind() == -1)
                {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                m_socket->Connect(
                    InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), RIBCERTSTORE_PORT));
            }
            else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
            {
                if (m_socket->Bind6() == -1)
                {
                    NS_FATAL_ERROR("Failed to bind socket");
                }
                m_socket->Connect(
                    Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peerAddress), RIBCERTSTORE_PORT));
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
            m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
            m_socket->SetAllowBroadcast(true);

            Simulator::Schedule(Seconds(0.5), &DCOwner::Send, this, m_socket, *it);
        }


    }

    void
    DCOwner::StopApplication()
    {
        NS_LOG_FUNCTION(this);
    }

    void
    DCOwner::Send(Ptr<Socket> sock, CertInfo cinfo)
    {
        NS_LOG_FUNCTION(this);

        SeqTsHeader seqTs;
        seqTs.SetSeq(0xffff);

        NS_LOG_INFO("Sending cert to: " << cinfo.rib_addr);
        Json::Value root;

        std::stringstream ss;
        ss << my_name << ":" << cinfo.issuer;
        root["issuer"] = ss.str();
        root["type"] = cinfo.type;
        root["entity"] = cinfo.entity;

        Json::StyledWriter jsonWriter;
        std::string body = jsonWriter.write(root);

        Ptr<Packet> p = Create<Packet>((const uint8_t *)body.c_str(), body.size());
        p->AddHeader(seqTs);


        if ((sock->Send(p)) >= 0)
        {
            sock->Close();
        }
    }

}
