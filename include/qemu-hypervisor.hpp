#ifndef __QEMU_HPP__
#define __QEMU_HPP__

#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
#include <memory>
#include <string>
#include <stdexcept>
#include <vector>
#include <fcntl.h>
#include <map>
#include <cstring>
#include <random>
#include <filesystem>
#include <net/if.h>
#include <fstream>
#include <qemu-images.hpp>
#include <algorithm>

typedef std::vector<std::string> QemuContext;

enum QEMU_DISPLAY
{
    GTK,
    VNC,
    NONE
};

#define QEMU_LANG "da"
#define QEMU_DEFAULT_SYSTEM "/usr/bin/qemu-system-x86_64"
#define QEMU_DEFAULT_INSTANCE "t1-medium"
#define QEMU_DEFAULT_MACHINE  "ubuntu/2004"
#define QEMU_DEFAULT_INTERFACE "default/macvtap"
#define QEMU_DEFAULT_IMAGEDB "resources/mydb.json"

std::string getMacSys(std::string tapname);
std::string displayArgumentAsString(const QEMU_DISPLAY &display);
int getNumberOfDrives(std::vector<std::string> &args);

/** Stack functions */
void PushSingleArgument(QemuContext &args, std::string value);
void PushArguments(QemuContext &args, std::string key, std::string value);

/*
 * QEMU_init (std::vector<std::string>, int memory, int numcpus)
*/
void QEMU_instance(QemuContext &args, const std::string &instanceargument);

/*
 * QEMU_drive (std::vector<std::string>, int memory, int numcpus)
*/
void QEMU_drive(QemuContext &args, const std::string &drive);

/*
 * QEMU_machine (QemuContext &args, const std::string model)
*/
void QEMU_machine(QemuContext &args, const std::string model);

/*
 * QEMU_iso (QemuContext &args, const std::string &model, const std::string &dabase)
*/
void QEMU_iso(QemuContext &args, const std::string &model, const std::string &database);

/**
 * QEMU_launch(QemuContext& args, std::string tapname, bool daemonize)
 * Launches qemu process, blocking if block is set to true. Defaults is to non-block and return immediately.
 */
void QEMU_Launch(QemuContext &args, std::string tapname, bool block = false);

/**
 * QEMU_Display(std::vector<std::string> &args, const QEMU_DISPLAY& display);
 */
void QEMU_display(QemuContext &args, const QEMU_DISPLAY &display);

/**
 * QEMU_List_Drives(std::filesystem::path filter, std::filesystem::path path);
 */
std::vector<std::string> QEMU_List_VMImages(const std::filesystem::path filter, const std::filesystem::path path);

/**
 * QEMU_Notify_Exited
 */
void QEMU_Notify_Exited(QemuContext &ctx);

/*
 * QEMU_Notify_Started()
 */ 
void QEMU_Notify_Started(QemuContext &ctx);


// TODO: Label these helpers.
std::string QEMU_Guest_ID(QemuContext &ctx);
void QEMU_Generate_ID(QemuContext &ctx);

#endif
