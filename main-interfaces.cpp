
#include <qemu-interfaces.hpp>

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
    std::string instance = "";
    int force = 0;

    std::string usage = m3_string_format("usage(): %s (-help) reservation://%s",
                                          instance.c_str());

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

        if (std::string(argv[i]).find("://") != std::string::npos)
        {
            const std::string delimiter = "://";
            instance = std::string(argv[i]).substr(std::string(argv[i]).find(delimiter) + 3);
        }
    }

    if (instance.empty())
    {
        std::cerr << "Error: ARN not supplied" << std::endl;
        std::cout << usage << std::endl;
        exit(EXIT_FAILURE);
    }

    std::string error;
    json11::Json json = json11::Json::parse(QEMU_interfaces(instance), error);

    std::cout << json["return"].dump() << std::endl;

    return EXIT_SUCCESS;
}
