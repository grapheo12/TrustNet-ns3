#include "main.h"


NameDBEntry::NameDBEntry(std::string& _dc_name, Ipv4Address& _origin_AS_addr, std::string& _td_path, Ipv4Address& _origin_server, CertInfo _cert_info) {
    dc_name = _dc_name;
    origin_AS_addr = _origin_AS_addr;
    origin_server = _origin_server;
    cert_info = _cert_info;
    
    // deserialize the td_path to vector
    // td_path is in the form: A->B->C->D
    // we want a vector containing {A, B, C, D}
    // std::vector<Ipv4Address> td_path;
    std::string segment = _td_path;
    std::string delimiter = "->";
    std::string td_to_add = segment.substr(0, segment.find(delimiter));
    while (td_to_add.size() != 0) {
        td_path.push_back(Ipv4Address(td_to_add.c_str()));
        if (td_to_add.size()+2 < segment.size()) {
            segment = segment.substr(td_to_add.size()+2);
        } else {
            segment = segment.substr(segment.size());
        }
        td_to_add = segment.substr(0, segment.find(delimiter));
    }
    // std::cout << "size of deserialized td_path is: " << td_path.size() << std::endl;

    
}


NameDBEntry::~NameDBEntry() {
    // std::cout << "NameDBEntry destructor called" << std::endl;
}


NameDBEntry* NameDBEntry::FromAdvertisementStr(std::string& serialized) {
    // * deserialize the advertisement packet
    Json::Value deserializeRoot;
    Json::Reader reader;
    if (!reader.parse(serialized, deserializeRoot)) {
        // std::cout << "Cannot parse ads: " << serialized << std::endl;
        return nullptr;
    }
    std::string dc_name = deserializeRoot.get("dc_name", "empty").asString();
    std::string td_path = deserializeRoot.get("td_path", "").asString();
    Ipv4Address origin_AS_addr = Ipv4Address(deserializeRoot.get("origin_AS", "123.123.123.123").asString().c_str());
    Ipv4Address origin_server = Ipv4Address(deserializeRoot.get("origin_server", "123.123.123.123").asString().c_str());
    Json::Value CertInfoRoot = deserializeRoot.get("cert_info", "");
    CertInfo cert_info;
    if (CertInfoRoot != "") {
        cert_info.type = CertInfoRoot.get("type", "").asString();
        cert_info.entity = CertInfoRoot.get("entity", "").asString();
        cert_info.issuer = CertInfoRoot.get("issuer", "").asString();
        cert_info.r_transitivity = CertInfoRoot.get("r_transitivity", 0).asInt();
    } else {
        // set default values for cert_info
        cert_info.entity = "";
        cert_info.type = "";
        cert_info.issuer = "";
        cert_info.r_transitivity = 0;
    }
    auto* entry = new NameDBEntry(dc_name, origin_AS_addr, td_path, origin_server, cert_info);

    return entry;
}


std::string NameDBEntry::ToAdvertisementStr() {
    Json::Value serializeRoot;
    serializeRoot["dc_name"] = dc_name;

    std::stringstream td_path_str;
    for (Ipv4Address addr : td_path) {
        std::stringstream ss;
        addr.Print(ss);
        td_path_str << ss.str() << "->";
    }
    std::string result = td_path_str.str();
    serializeRoot["td_path"] = result.substr(0, result.size()-2);
    
    std::stringstream ss;
    origin_AS_addr.Print(ss);
    serializeRoot["origin_AS"] = ss.str();
    
    ss.str(std::string());
    origin_server.Print(ss);
    serializeRoot["origin_server"] = ss.str();

    Json::Value cert;
    cert["type"] = cert_info.type;
    cert["entity"] = cert_info.entity;
    cert["issuer"] = cert_info.issuer;
    cert["r_transitivity"] = cert_info.r_transitivity;

    serializeRoot["cert_info"] = cert;
    // serialize the packet
    Json::StyledWriter writer;
    return writer.write(serializeRoot);;
}




