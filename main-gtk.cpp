#include <iostream>
#include "qemu.hpp"

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cout << "Usage(): ./qemu-gtk instance-profile{small,default=medium,large} macvtap{default=macvatap0} hdd" << std::endl;
        exit(-1);
    }

    std::string instanceargument = std::string(argv[1]);
    std::string tapname = std::string(argv[2]);
    std::cout << "Using instance-profile: " << instanceargument << std::endl;

    std::vector<std::string> args;
    QEMU_init(args, tapname);
    QEMU_instance(args, instanceargument);
    QEMU_display(args, QEMU_DISPLAY::GTK);
    QEMU_drives(args, "/mnt/faststorage/vms/test2.img");
    QEMU_Launch(args);

    return EXIT_SUCCESS;
}
