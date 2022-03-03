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
    bool nat;
    std::string router;
};

struct Image {
    std::string name;
    std::string datastore;
    std::string backingimage;
    std::string filename;
    std::string sz;
};


std::ostream &operator<<(std::ostream &os, const struct Network &net)
{
    switch (net.topology)
    {
    case NetworkTopology::Bridge:
    {
        os << "Bridge network association: " << net.name << " network " << net.cidr;
    }
    break;
    case NetworkTopology::Macvtap:
    {
        os << "Macvtap network association: " << net.name << ", master interface " << net.interface << ", mode: " << net.macvtapmode;
    }
    break;
    }

    os << ", namespace " << net.net_namespace;

    return os;
}

// the desktop-template
extern unsigned char resources_template_desktop[];
extern unsigned int resources_template_desktop_len;

#endif