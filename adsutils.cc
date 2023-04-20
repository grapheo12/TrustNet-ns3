#include "main.h"

NameDBEntry::NameDBEntry(std::string& _dc_name,
                         Ipv4Address& _origin_AS_addr,
                         std::string& _td_path,
                         Ipv4Address& _origin_server,
                         TrustCert _trust_cert,
                         std::vector<DistrustCert> _distrust_certs)
{
    dc_name = _dc_name;
    origin_AS_addr = _origin_AS_addr;
    origin_server = _origin_server;
    trust_cert = _trust_cert;
    distrust_certs = _distrust_certs;

    // deserialize the td_path to vector
    // td_path is in the form: A->B->C->D
    // we want a vector containing {A, B, C, D}
    // std::vector<Ipv4Address> td_path;
    std::string segment = _td_path;
    std::string delimiter = "->";
    std::string td_to_add = segment.substr(0, segment.find(delimiter));
    while (td_to_add.size() != 0)
    {
        td_path.push_back(Ipv4Address(td_to_add.c_str()));
        if (td_to_add.size() + 2 < segment.size())
        {
            segment = segment.substr(td_to_add.size() + 2);
        }
        else
        {
            segment = segment.substr(segment.size());
        }
        td_to_add = segment.substr(0, segment.find(delimiter));
    }
    // std::cout << "size of deserialized td_path is: " << td_path.size() << std::endl;
}

NameDBEntry::~NameDBEntry()
{
    // std::cout << "NameDBEntry destructor called" << std::endl;
}

NameDBEntry*
NameDBEntry::FromAdvertisementStr(std::string& serialized)
{
    // * deserialize the advertisement packet
    Json::Value deserializeRoot;
    Json::Reader reader;
    if (!reader.parse(serialized, deserializeRoot))
    {
        // std::cout << "Cannot parse ads: " << serialized << std::endl;
        return nullptr;
    }
    std::string dc_name = deserializeRoot.get("dc_name", "empty").asString();
    std::string td_path = deserializeRoot.get("td_path", "").asString();
    Ipv4Address origin_AS_addr =
        Ipv4Address(deserializeRoot.get("origin_AS", "123.123.123.123").asString().c_str());
    Ipv4Address origin_server =
        Ipv4Address(deserializeRoot.get("origin_server", "123.123.123.123").asString().c_str());
    
    // Parsing trust relation
    Json::Value trust_cert_root = deserializeRoot.get("trust_cert", "");
    TrustCert trust_cert;
    if (trust_cert_root != "")
    {
        trust_cert.type = trust_cert_root.get("type", "").asString();
        trust_cert.entity = trust_cert_root.get("entity", "").asString();
        trust_cert.issuer = trust_cert_root.get("issuer", "").asString();
        trust_cert.r_transitivity = trust_cert_root.get("r_transitivity", 0).asInt();
    }
    else
    {
        // set default values for trust_cert
        trust_cert.entity = "";
        trust_cert.type = "";
        trust_cert.issuer = "";
        trust_cert.r_transitivity = 0;
    }

    //  Parsing distrust relation
    Json::Value distrust_cert_root = deserializeRoot.get("distrust_certs", Json::Value(Json::arrayValue));
    std::vector<DistrustCert> distrust_certs;
    if (distrust_cert_root != Json::Value(Json::arrayValue)) {
        for (unsigned int i = 0; i < distrust_cert_root.size(); ++i) {
            DistrustCert distrust;
            distrust.type = distrust_cert_root[i].get("type", "").asString();
            distrust.entity = distrust_cert_root[i].get("entity", "").asString();
            distrust.issuer = distrust_cert_root[i].get("issuer", "").asString();

            distrust_certs.push_back(distrust);
        }
    }

    auto* entry = new NameDBEntry(dc_name, origin_AS_addr, td_path, origin_server, trust_cert, distrust_certs);
    return entry;
}

std::string
NameDBEntry::ToAdvertisementStr()
{
    Json::Value serializeRoot;
    serializeRoot["dc_name"] = dc_name;

    std::stringstream td_path_str;
    for (Ipv4Address addr : td_path)
    {
        std::stringstream ss;
        addr.Print(ss);
        td_path_str << ss.str() << "->";
    }
    std::string result = td_path_str.str();
    serializeRoot["td_path"] = result.substr(0, result.size() - 2);

    std::stringstream ss;
    origin_AS_addr.Print(ss);
    serializeRoot["origin_AS"] = ss.str();

    ss.str(std::string());
    origin_server.Print(ss);
    serializeRoot["origin_server"] = ss.str();

    // Add trust relation information
    Json::Value cert;
    cert["type"] = trust_cert.type;
    cert["entity"] = trust_cert.entity;
    cert["issuer"] = trust_cert.issuer;
    cert["r_transitivity"] = trust_cert.r_transitivity;

    serializeRoot["trust_cert"] = cert;

    // Add distrust relation information
    Json::Value d_certs = Json::Value(Json::arrayValue);
    for (unsigned int i = 0; i < distrust_certs.size(); ++i) {
        Json::Value d_cert;
        d_cert["type"] = distrust_certs[i].type;
        d_cert["entity"] = distrust_certs[i].entity;
        d_cert["issuer"] = distrust_certs[i].issuer;
        d_certs.append(d_cert);
    }

    serializeRoot["distrust_certs"] = d_certs;
    // serialize the packet
    Json::StyledWriter writer;
    return writer.write(serializeRoot);
    ;
}
