#ifndef __QEMU_MANAGE_HPP__
#define __QEMU_MANAGE_HPP__

#include <qemu-link.hpp>

void QEMU_powerdown(std::string &reservationid);
void QEMU_kill(std::string &reservationid);
void QEMU_reset(std::string &reservationid);
std::string QEMU_interfaces(std::string &guestid);

#endif