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

std::string allocateLink(std::string masterinterface);

void deleteLink(std::string interface) ;

#endif