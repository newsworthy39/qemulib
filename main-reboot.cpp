
#include <qemu-reboot.hpp>

// NASTY.
void *hi_malloc(unsigned long size)
{
    return malloc(size);
}

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
    std::string uuidv4;
    std::string redis = QEMU_DEFAULT_REDIS;
    std::string username = "redis";
    std::string password = "foobared";
    std::string topic = "activation-14ddf77c";
    std::string arn = "";
    std::string usage = m3_string_format("usage(): %s (-h) -redis {default=%s} -user {default=%s} -password {default=********} "
                                         "reservation (e.q reservation://29fa6a16-4630-488b-a839-d0277e3de0e1) ",
                                         argv[0], redis.c_str(), username.c_str());

    for (int i = 1; i < argc; ++i)
    { // Remember argv[0] is the path to the program, we want from argv[1] onwards

        if (std::string(argv[i]).find("-h") != std::string::npos)
        {
            std::cout << usage << std::endl;
            exit(EXIT_FAILURE);
        }

        if (std::string(argv[i]).find("-v") != std::string::npos)
        {
            verbose = true;
        }

        if (std::string(argv[i]).find("-redis") != std::string::npos)
        {
            redis = argv[i + 1];
        }

        if (std::string(argv[i]).find("-user") != std::string::npos && (i + 1 < argc))
        {
            username = argv[i + 1];
        }

        if (std::string(argv[i]).find("-password") != std::string::npos && (i + 1 < argc))
        {
            password = argv[i + 1];
        }

        if (std::string(argv[i]).find("reservation://") != std::string::npos)
        {
            const std::string delimiter = "://";
            uuidv4 = std::string(argv[i]).substr(std::string(argv[i]).find(delimiter) + 3);
        }
    }

    if (uuidv4.empty())
    {
        std::cerr << "Error: Missing reservation." << std::endl;
        std::cout << usage << std::endl;
        exit(EXIT_FAILURE);
    }

    // Hack, to avoid defunct processes.
    struct timeval timeout = {1, 5000}; // 1.5 seconds timeout

    redisContext *c = redisConnectWithTimeout(redis.c_str(), 6379, timeout);
    if (c == NULL || c->err)
    {
        if (c)
        {
            std::cerr << "Error connecting to REDIS: " << c->errstr << std::endl;
            exit(EXIT_FAILURE);
        }
        else
        {
            printf("Can't allocate redis context\n");
        }
    }

    // Maybe a RAII aproach, would solve this tedious "freereplyobject"..
    std::string launch = "{ \"execute\": \"system_reset\" }";
    redisReply *redisr1;
    redisr1 = (redisReply *)redisCommand(c, "AUTH %s", password.c_str());
    freeReplyObject(redisr1);
    redisr1 = (redisReply *)redisCommand(c, "PUBLISH qmp-%s %s", uuidv4.c_str(), launch.c_str());
    freeReplyObject(redisr1);
    redisFree(c);

    return EXIT_SUCCESS;
}
