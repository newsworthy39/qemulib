
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
    bool all = true;

    std::string usage = m3_string_format("usage(): %s (-help) (-force) (reservation://%s)",
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
            all = false;
        }
    }

    std::vector<std::tuple<std::string, std::string>> reservations = QEMU_get_reservations();
    std::for_each(reservations.begin(), reservations.end(), [force, all, &reservation](std::tuple<std::string, std::string> &res)
                  {
        if (std::get<0>(res) == reservation || all == true)
            if (force)
            {
                std::cout << "Sending kill to " << std::get<1>( res ) << std::endl;
                QEMU_kill(std::get<0>(res));
            }
            else
            {
                std::cout << "Sending powerdown to " << std::get<1>(res) << std::endl;
                QEMU_powerdown(std::get<0>(res));
            } });

    return EXIT_SUCCESS;
}
