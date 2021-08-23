#include <iostream>
#include "qemu.hpp"

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage(): ./qemu-headless instance-profile{small,default=medium,large} macvtap" << std::endl;
        exit(-1);
    }

    std::string instanceargument = std::string(argv[1]);
    std::string tapname = std::string(argv[2]);
    std::cout << "Using instance-profile: " << instanceargument << std::endl;

    std::vector<std::string> args;
    QEMU_init(args, tapname);
    QEMU_instance(args, instanceargument);
    QEMU_display(args, QEMU_DISPLAY::VNC);
    QEMU_drives(args, "/mnt/faststorage/vms/test2.img");
    QEMU_Launch(args);

    return EXIT_SUCCESS;
}