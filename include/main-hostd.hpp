#ifndef __MAIN_HOSTD_HPP_
#define __MAIN_HOSTD_HPP

#include <json11.hpp>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <qemu-hypervisor.hpp>
#include <qemu-link.hpp>
#include <qemu-manage.hpp>

void onReceiveMessage(redisAsyncContext *c, void *reply, void *privdata);
std::string hostd_generate_client_name();
void onReservationsMessage(json11::Json::object arguments);
void onLaunchMessage(json11::Json::object json);
std::string hostd_generate_client_name();
void broadcastMessage(std::string reply, std::string &message);
void onResetMessage(json11::Json::object arguments);
void *hi_malloc(unsigned long size);


#endif