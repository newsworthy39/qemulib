
#include <qemu-launch.hpp>

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
    std::string instance = QEMU_DEFAULT_INSTANCE;
    std::string redis = QEMU_DEFAULT_REDIS;
    std::string username = "redis";
    std::string password = "foobared";
    std::string client = "14ddf77c";

    for (int i = 1; i < argc; ++i)
    { // Remember argv[0] is the path to the program, we want from argv[1] onwards

        if (std::string(argv[i]).find("-h") != std::string::npos)
        {
            std::cout << "Usage(): " << argv[0] << " (-h) -redis {default=" << QEMU_DEFAULT_REDIS << "} -user {default=" << username << "} -password {default=" << password << "} -instance {default=" << QEMU_DEFAULT_INSTANCE << "}"
                      << " -client " << std::endl;
            exit(-1);
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
        if (std::string(argv[i]).find("-instance") != std::string::npos && (i + 1 < argc))
        {
            instance = argv[i + 1];
        }
    }

    // Hack, to avoid defunct processes.
    redisContext *c = redisConnect(redis.c_str(), 6379);
    if (c == NULL || c->err)
    {
        if (c)
        {
            std::cerr << "Error connecting to REDIS: " << c->errstr << std::endl;
            exit(-1);
        }
        else
        {
            printf("Can't allocate redis context\n");
        }
    }

    // Maybe a RAII aproach, would solve this tedious "freereplyobject"..
    std::string launch = m3_string_format("{ \"execute\": \"launch\", \"arguments\": { \"arn\": \"mj-tester\", \"instance\": \"%s\" } }", instance.c_str());
    redisReply *redisr1;
    redisr1 = (redisReply *)redisCommand(c, "AUTH %s", password.c_str());
    freeReplyObject(redisr1);
    redisr1 = (redisReply *)redisCommand(c, "PUBLISH activation-%s %s", client.c_str(), launch.c_str());
    
    

    freeReplyObject(redisr1);
    redisFree(c);

    return EXIT_SUCCESS;
}
