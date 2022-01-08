
#include <qemu-powerdown.hpp>

template <typename... Args>
std::string m3_string_format(const std::string &format, Args... args)
{
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    if (size_s <= 0)
    {
        throw std::runtime_error("Error during formatting.");
    }
    auto size = static_cast<size_t>(size_s);
    auto buf = std::make_unique<char[]>(size);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}

int main(int argc, char *argv[])
{
    bool verbose = false;
    std::string topic = "";
    std::string reservation = "";
    bool force = false;

    std::string usage = m3_string_format("usage(): %s (-help) (-force) reservation://%s",
                                           argv[0], reservation.c_str());

    for (int i = 1; i < argc; ++i)
    { // Remember argv[0] is the path to the program, we want from argv[1] onwards

        if (std::string(argv[i]).find("-help") != std::string::npos)
        {
            std::cout << usage << std::endl;
            exit(EXIT_FAILURE);
        }

        if (std::string(argv[i]).find("-v") != std::string::npos)
        {
            verbose = true;
        }

         if (std::string(argv[i]).find("-force") != std::string::npos)
        {
            force = true;
        }

        if (std::string(argv[i]).find("://") != std::string::npos)
        {
            const std::string delimiter = "://";
            reservation = std::string(argv[i]).substr(std::string(argv[i]).find(delimiter) + 3);
        }
    }
    
    if (reservation.empty())
    {
        std::cerr << "Error: reservation not supplied" << std::endl;
        std::cout << usage << std::endl;
        exit(EXIT_FAILURE);
    }

    std::cout << "Using reservation " << reservation ;

    if (!force) {
        std::cout << " without force " << std::endl;
        QEMU_powerdown(reservation);
    } else {
        std::cout << " with force " << std::endl;
        QEMU_kill(reservation);
    }

    return EXIT_SUCCESS;
}
