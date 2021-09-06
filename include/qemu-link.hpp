#ifndef __QEMU_LINK_HPP__
#define __QEMU_LINK_HPP__

#include <netinet/ether.h>

#include <netlink/netlink.h>
#include <netlink/route/link.h>
#include <netlink/route/link/macvtap.h>
#include <linux/netlink.h>
#include <linux/if.h>
#include <string>
#include <iostream>
#include <random>
#include <memory>


std::string QEMU_Allocate_Link(std::string masterinterface);

void QEMU_Delete_Link(std::string interface) ;

#endif