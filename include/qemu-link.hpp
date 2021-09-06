#ifndef __QEMU_LINK_HPP__
#define __QEMU_LINK_HPP__

#include <netinet/ether.h>

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/macvtap.h>
#include <linux/netlink.h>
#include <net/if.h>
#include <string>
#include <iostream>
#include <random>
#include <memory>
#include <qemu-hypervisor.hpp>

std::string QEMU_Allocate_Link(std::string masterinterface);
void QEMU_Delete_Link(QemuContext &ctx, std::string interface);
int QEMU_Open_Link(QemuContext &ctx, std::string interface);
void QEMU_Close_Link(QemuContext &ctx, int linkfd);
std::string QEMU_Generate_Link_Mac();
std::string QEMU_Generate_Link_Name();

#endif