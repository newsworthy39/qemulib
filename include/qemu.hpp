#ifndef __QEMU_BRIDGE_HPP__
#define __QEMU_BRIDGE_HPP__

#include <string>
#include <xxd.hpp>

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


// the desktop-template
extern unsigned char resources_template_desktop[];
extern unsigned int resources_template_desktop_len;

#endif