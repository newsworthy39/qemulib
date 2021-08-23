#include <iostream>
#include "qemu.hpp"

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        std::cout << "Usage(): ./qemu-gtk instance-profile{small,default=medium,large} macvtap{default=macvatap0} hdd-drive-full-path" << std::endl;
        exit(-1);
    }

    std::string instanceargument = std::string(argv[1]);
    std::string tapname          = std::string(argv[2]);
    std::string hdd              = std::string(argv[3]);

    std::vector<std::string> args;
    QEMU_init(args, instanceargument);
    QEMU_display(args, QEMU_DISPLAY::GTK);
    QEMU_drives(args, hdd);
    QEMU_Launch(args, tapname);

    return EXIT_SUCCESS;
}
