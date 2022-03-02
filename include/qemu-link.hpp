#ifndef __QEMU_LINK_HPP__
#define __QEMU_LINK_HPP__

#include <netinet/ether.h>
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/macvtap.h>

#include <linux/if_tun.h> // for tun-devices.
#include <linux/netlink.h>
#include <net/if.h>
#include <string>
#include <iostream>
#include <random>
#include <memory>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <qemu-hypervisor.hpp>

std::string QEMU_allocate_tun(QemuContext &ctx);
std::string QEMU_allocate_macvtap(QemuContext &ctx, std::string masterinterface);
std::string generateRandomMACAddress();
std::string QEMU_Generate_Link_Name(std::string prefix, int length);

void QEMU_delete_link(QemuContext &ctx, std::string interface);

int QEMU_OpenQMPSocket(QemuContext &ctx);
int QEMU_OpenQMPSocketFromPath(std::string &guestid);

int QEMU_OpenQGASocketFromPath(std::string &guestid);
int QEMU_allocate_bridge(std::string bridge);
void QEMU_enslave_interface(std::string bridge, std::string slave);
int QEMU_tun_allocate(const std::string device);
int QEMU_link_up(const std::string ifname);
void QEMU_set_namespace(std::string namespace_path);
void QEMU_set_default_namespace();
void QEMU_set_interface_address(const std::string device, const std::string address, const std::string cidr);
std::string QEMU_get_interface_cidr(const std::string device);
void QEMU_iptables_set_masquerade(std::string cidr);
void QEMU_set_router(bool on);

#endif