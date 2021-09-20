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
#include <sys/un.h>
#include <qemu-hypervisor.hpp>

std::string QEMU_allocate_tun(QemuContext &ctx, std::string bridge);
std::string QEMU_allocate_macvtap(QemuContext &ctx, std::string masterinterface);
std::string QEMU_Generate_Link_Mac();
std::string QEMU_Generate_Link_Name(std::string prefix, int length);
void QEMU_Delete_Link(QemuContext &ctx, std::string interface);
int QEMU_OpenQMPSocket(QemuContext &ctx);
void if_enslave(const char *masterdev, const char *slavedev);
int tun_alloc(char *dev);
int if_up(char *ifname, short flags);
#endif