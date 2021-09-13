#ifndef __QEMU_LINK_HPP__
#define __QEMU_LINK_HPP__

#include <netinet/ether.h>
#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/macvtap.h>
//#include <linux/if.h>
#include <linux/if_tun.h>
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

void QEMU_allocate_tun(QemuContext &ctx, std::string bridge);
std::string QEMU_allocate_macvtap(QemuContext &ctx, std::string masterinterface);
std::string getMacSys(std::string tapname);
std::string QEMU_Generate_Link_Mac();
std::string QEMU_Generate_Link_Name();
void QEMU_Delete_Link(QemuContext &ctx, std::string interface);
void QEMU_Close_Link(QemuContext &ctx, int linkfd);
int QEMU_OpenQMPSocket(QemuContext &ctx);
#endif