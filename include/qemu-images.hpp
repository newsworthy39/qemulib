#ifndef __QEMU_IMAGES_HPP__
#define __QEMU_IMAGES_HPP__

#include <vector>
#include <string>
#include <tuple>

extern std::vector<std::tuple<std::string,std::string>> drives;
extern std::vector<std::tuple<std::string,std::string>> datastores;

#endif