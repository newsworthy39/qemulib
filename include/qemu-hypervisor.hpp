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
#include <qemu-context.hpp>
#include <qemu-images.hpp>

#define QEMU_LANG "da"
#define QEMU_DEFAULT_SYSTEM "/usr/bin/qemu-system-x86_64"
#define QEMU_DEFAULT_INSTANCE "t1-medium"
#define QEMU_DEFAULT_MACHINE  "ubuntu/2004"
#define QEMU_DEFAULT_INTERFACE "macvtap0"
#define QEMU_DEFAULT_IMAGEDB "./mydb.json"

enum QEMU_DISPLAY
{
    GTK,
    VNC,
    NONE
};


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
 * QEMU_machine (QemuContext &args, const std::string &model, const std::string &dabase)
*/
void QEMU_machine(QemuContext &args, const std::string &model, const std::string &database);

/**
 * QEMU_launch
 */
void QEMU_Launch(QemuContext &args, std::string tapname);

/**
 * QEMU_Display(std::vector<std::string> &args, const QEMU_DISPLAY& display);
 */
void QEMU_display(QemuContext &args, const QEMU_DISPLAY &display);

/**
 * QEMU_List_Drives(std::filesystem::path filter, std::filesystem::path path);
 */
std::vector<std::string> QEMU_List_VMImages(const std::filesystem::path filter, const std::filesystem::path path);

#endif
