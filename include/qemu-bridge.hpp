#ifndef __QEMU_BRIDGE_HPP__
#define __QEMU_BRIDGE_HPP__

#include <string>

std::ostream& operator<<(std::ostream &os, const struct Network &model);

enum NetworkTopology {
    Bridge,
    Macvtap
};

enum NetworkMacvtapMode {
    Private,
    VEPA,
    Bridged
};

struct Network {
    NetworkTopology topology;
    NetworkMacvtapMode macvtapmode;
    std::string interface;
    std::string name;
    std::string net_namespace;
    std::string cidr;
};

#endif