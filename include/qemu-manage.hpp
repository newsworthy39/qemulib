#ifndef __QEMU_MANAGE_HPP__
#define __QEMU_MANAGE_HPP__


#include <qemu-hypervisor.hpp>
#include <qemu-link.hpp>

void QEMU_powerdown(QemuContext &ctx);
void QEMU_kill(QemuContext &ctx);
void QEMU_reset(QemuContext &ctx);

#endif