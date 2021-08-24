#include "qemu.hpp"

int main(int argc, char *argv[])
{

    std::string instanceargument = QEMU_DEFAULT_INSTANCE;
    std::string tapname = QEMU_DEFAULT_INTERFACE;
    std::vector<std::string> args;
    bool verbose = false;

    for (int i = 1; i < argc; ++i)
    { // Remember argv[0] is the path to the program, we want from argv[1] onwards

        if (std::string(argv[i]).find("-h") != std::string::npos)
        {
            std::cout << "Usage(): " << argv[0] << " (-h) (-v) -m {default=" << QEMU_DEFAULT_INSTANCE << "} -i {default=" << QEMU_DEFAULT_INTERFACE << "} -d hdd (n+1)" << std::endl;
            exit(-1);
        }

        if (std::string(argv[i]).find("-v") != std::string::npos)
        {
            verbose = true;
        }

        if (std::string(argv[i]).find("-m") != std::string::npos && (i + 1 < argc))
        {
            instanceargument = argv[i + 1];
        }

        if (std::string(argv[i]).find("-i") != std::string::npos && (i + 1 < argc))
        {
            tapname = argv[i + 1];
        }
        
        if (std::string(argv[i]).find("-d") != std::string::npos && (i + 1 < argc))
        {
            QEMU_drive(args, argv[i + 1]);
        }
    }

    QEMU_machine(args, instanceargument);
    QEMU_display(args, QEMU_DISPLAY::GTK);    
    QEMU_Launch(args, tapname);

    return EXIT_SUCCESS;
}
