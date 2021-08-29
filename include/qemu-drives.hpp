#ifndef __QEMU_IMAGES_HPP__
#define __QEMU_IMAGES_HPP__

#include <json11.hpp>
#include <iostream>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <memory>
#include <string>
#include <vector>
#include <fcntl.h>
#include <map>
#include <cstring>
#include <random>
#include <filesystem>
#include <stdexcept>
#include <sys/wait.h>
#include <fstream>

std::map<std::string, std::string> qemuListImages();

void qemuRegisterImage(std::string model, std::string type);

bool m1_fileExists(const std::string &filename);

json11::Json qemuGetFileContents(const std::string& databasefilepath, std::string& err);

void qemuWriteFileContents(const std::string &filename, const json11::Json& jsonobj);

std::map<std::string, std::string> qemuListImages(const std::string& databasepath);

#endif