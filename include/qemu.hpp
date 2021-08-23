#ifndef __QEMU_HPP__
#define __QEMU_HPP__

#include <iostream>
#include <sys/types.h>
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

enum QEMU_DISPLAY {
    GTK, VNC, NONE
};

/**
 * Helper functions
 */
// https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
template <typename... Args>
std::string string_format(const std::string &format, Args... args);
std::string displayArgumentAsString(const QEMU_DISPLAY& display);
std::string generate_uuid_v4();
std::string getMacSys(std::string tapname);
int getTapIndex(std::string tapname);


/** Stack functions */
void PushSingleArgument(std::vector<std::string> &args, std::string value);
void PushArguments(std::vector<std::string> &args, std::string key, std::string value);

/*
 * QEMU_init (std::vector<std::string>, int memory, int numcpus)
*/
void QEMU_init(std::vector<std::string> &args, std::string tapname);

/*
 * QEMU_drives (std::vector<std::string>, int memory, int numcpus)
*/
void QEMU_drives(std::vector<std::string> &args, std::string drive);

/**
 * QEMU_launch
 */
void QEMU_Launch(std::vector<std::string> &args);

/**
 * QEMU_Display(std::vector<std::string> &args, const QEMU_DISPLAY& display);
 */
void QEMU_display(std::vector<std::string> &args, const QEMU_DISPLAY& display);


/**
 * QEMU_instance(std::vector<std::string> &args, std::string argument)
 */
void QEMU_instance(std::vector<std::string> &args, std::string argument) ;

#endif