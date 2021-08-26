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
#include <tuple>
#include <cstring>
#include <fstream>
#include <sstream>
#include <random>
#include <filesystem>

#define QEMU_LANG "da"
#define QEMU_DEFAULT_SYSTEM "/usr/bin/qemu-system-x86_64"
#define QEMU_DEFAULT_GUEST_PATH "/mnt/faststorage/vms/"
#define QEMU_DEFAULT_FILTER "guest-cloud"
#define QEMU_DEFAULT_INSTANCE "medium"
#define QEMU_DEFAULT_INTERFACE "macvtap0"


// https://github.com/moises-silva/libnetlink-examples/blob/master/addvlan.c

enum QEMU_DISPLAY
{
    GTK,
    VNC,
    NONE
};

/**
 * Helper functions
 */
// https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
template <typename... Args>
std::string string_format(const std::string &format, Args... args);
std::string displayArgumentAsString(const QEMU_DISPLAY &display);
std::string generate_uuid_v4();
std::string getMacSys(std::string tapname);
int getTapIndex(std::string tapname);
bool fileExists(const std::string &filename);
int getNumberOfDrives(std::vector<std::string> &args);

/** Stack functions */
void PushSingleArgument(std::vector<std::string> &args, std::string value);
void PushArguments(std::vector<std::string> &args, std::string key, std::string value);

/*
 * QEMU_init (std::vector<std::string>, int memory, int numcpus)
*/
void QEMU_machine(std::vector<std::string> &args, const std::string &instanceargument);

/*
 * QEMU_drive (std::vector<std::string>, int memory, int numcpus)
*/
void QEMU_drive(std::vector<std::string> &args, std::string drive);

/**
 * QEMU_launch
 */
void QEMU_Launch(std::vector<std::string> &args, std::string tapname);

/**
 * QEMU_Display(std::vector<std::string> &args, const QEMU_DISPLAY& display);
 */
void QEMU_display(std::vector<std::string> &args, const QEMU_DISPLAY &display);

/**
 * QEMU_List_Drives(std::filesystem::path filter, std::filesystem::path path);
 */
std::vector<std::string> QEMU_List_VMImages(const std::filesystem::path filter, const std::filesystem::path path);

#endif
