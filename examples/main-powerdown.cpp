
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
    bool bVerbose = false, bForce = false, bAll = true;
    std::string filter;
    std::string usage = m3_string_format("usage(): %s (-help) (-force) (-filter)",
                                         argv[0]);

    for (int i = 1; i < argc; ++i)
    { // Remember argv[0] is the path to the program, we want from argv[1] onwards

        if (std::string(argv[i]).find("-help") != std::string::npos)
        {
            std::cout << usage << std::endl;
            exit(EXIT_FAILURE);
        }

        if (std::string(argv[i]).find("-v") != std::string::npos)
        {
            bVerbose = true;
        }

        if (std::string(argv[i]).find("-force") != std::string::npos)
        {
            bForce = true;
        }

        if (std::string(argv[i]).find("-filter") != std::string::npos && (i + 1 < argc))
        {
            filter = argv[i + 1];
            bAll = false;
        }
    }

    std::vector<std::tuple<std::string, std::string>> reservations = QEMU_get_reservations();
    std::for_each(reservations.begin(), reservations.end(), [bForce, bAll, &filter](std::tuple<std::string, std::string> &res)
                  {
        if (std::get<1>(res).starts_with(filter) || bAll == true)
            if (bForce)
            {
                std::cout << "Sending kill to " << std::get<0>( res ) << std::endl;
                QEMU_kill(std::get<0>(res));
            }
            else
            {
                std::cout << "Sending powerdown to " << std::get<0>(res) << std::endl;
                QEMU_powerdown(std::get<0>(res));
            } });

    return EXIT_SUCCESS;
}
