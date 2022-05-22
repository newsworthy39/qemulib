#ifndef __QEMU_BRIDGE_HPP__
#define __QEMU_BRIDGE_HPP__

#include <string>
#include <qemu-link.hpp>



struct Image
{
    std::string name;
    std::string datastore;
    std::string backingimage;
    std::string filename;
    std::string sz;
    std::string media;
    size_t bpstotal;
    std::string cloudinit;
};


// the desktop-template
extern unsigned char resources_template_desktop[];
extern unsigned int resources_template_desktop_len;
unsigned int generate_random_cid();
#endif