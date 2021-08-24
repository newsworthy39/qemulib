#include "qemu.hpp"

int main(int argc, char *argv[])
{
    std::string path = QEMU_DEFAULT_GUEST_PATH;
    std::string filter = QEMU_DEFAULT_FILTER;
    bool verbose = false;

    for (int i = 1; i < argc; ++i)
    { // Remember argv[0] is the path to the program, we want from argv[1] onwards

        if (std::string(argv[i]).find("-h") != std::string::npos)
        {
            std::cout << "Usage(): " << argv[0] << " (-h) (-v) -f {default=" << QEMU_DEFAULT_FILTER << "} -p {default=" << QEMU_DEFAULT_GUEST_PATH << "} " << std::endl;
            exit(-1);
        }

        if (std::string(argv[i]).find("-v") != std::string::npos)
        {
            verbose = true;
        }

        if (std::string(argv[i]).find("-f") != std::string::npos && (i + 1 < argc))
        {
            filter = argv[i + 1];
        }

        if (std::string(argv[i]).find("-p") != std::string::npos && (i + 1 < argc))
        {
            path = argv[i + 1];
        }
    }

    if (fileExists(path))
    {
        if (verbose)
        {
            std::cout << "Using " << path << ", " << filter << std::endl;
        }
        QEMU_List_VMImages(filter, path);
    }
    else
    {
        std::cerr << "Path " << path << " does not exists." << std::endl;
        exit(-1);
    }

    exit(0);
}
