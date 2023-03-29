#include "main.h"


NameDBEntry::NameDBEntry(std::string& _dc_name, int _r_transitivity, Ipv4Address& _origin_AS_addr) {
    dc_name = _dc_name;
    r_transitivity = _r_transitivity;
    origin_AS_addr = _origin_AS_addr;
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
    int r_transitivity = deserializeRoot.get("r_transitivity", 1).asInt();
    Ipv4Address origin_AS_addr = Ipv4Address(deserializeRoot.get("origin_AS", "123.123.123.123").asString().c_str());
    return new NameDBEntry(dc_name, r_transitivity, origin_AS_addr);
}


std::string NameDBEntry::ToAdvertisementStr() {
    Json::Value serializeRoot;
    serializeRoot["dc_name"] = dc_name;
    serializeRoot["r_transitivity"] = r_transitivity;
    std::stringstream ss;
    origin_AS_addr.Print(ss);
    serializeRoot["origin_AS"] = ss.str();
    // serialize the packet
    Json::StyledWriter writer;
    return writer.write(serializeRoot);;
}




