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
    std::string topic = "";
    std::string arn = "";
    std::string usage = m3_string_format("usage(): %s (-h) -redis {default=%s} -user {default=%s} -password {default=********} -instance {default=%s} " 
                      "activation-test-br0://test-server-instance", argv[0], redis.c_str(), username.c_str(), instance.c_str());

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
        if (std::string(argv[i]).find("-instance") != std::string::npos && (i + 1 < argc))
        {
            instance = argv[i + 1];
        }

        if (std::string(argv[i]).find("://") != std::string::npos)
        {
            const std::string delimiter = "://";
            topic = std::string(argv[i]).substr(0, std::string(argv[i]).find(delimiter));
            arn = std::string(argv[i]).substr(std::string(argv[i]).find(delimiter) + 3);
        }
    }

    if (arn.empty())
    {
        std::cerr << "Error: ARN not supplied" << std::endl;
        std::cout << usage << std::endl;        
        exit(EXIT_FAILURE);
    }

      if (topic.empty())
    {
        std::cerr << "Error: TOPIC not supplied" << std::endl;
        std::cout << usage << std::endl;        
        exit(EXIT_FAILURE);
    }

    std::cout << "Will launch " << arn << " on " << topic << "." << std::endl;

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

    // Make sure, the activation-topic exists, 

    // Maybe a RAII aproach, would solve this tedious "freereplyobject"..
    std::string launch = m3_string_format("{ \"execute\": \"launch\", \"arguments\": { \"arn\": \"%s\", \"instance\": \"%s\" } }", arn.c_str(), instance.c_str());
    redisReply *redisr1;
    redisr1 = (redisReply *)redisCommand(c, "AUTH %s", password.c_str());
    freeReplyObject(redisr1);
    redisr1 = (redisReply *)redisCommand(c, "PUBLISH %s %s", topic.c_str(), launch.c_str());
    freeReplyObject(redisr1);
    redisr1 = (redisReply *)redisCommand(c, "SUBSCRIBE reply-%s", topic.c_str());
    while (redisGetReply(c, (void **)&redisr1) == REDIS_OK)
    {
        if (redisr1 == NULL)
            break;

        if (redisr1->type == REDIS_REPLY_ARRAY)
        {
#ifdef __DEBUG__
            for (int j = 0; j < redisr1->elements; j++)
            {
                printf("%u) %s\n", j, redisr1->element[j]->str);
            }
#endif
            std::string str_error;
            json11::Json jsn = json11::Json::parse(redisr1->element[2]->str, str_error);
            if (!str_error.empty())
            {
                std::cerr << "Could not parse JSON in reply." << std::endl;
                continue;
            }

            json11::Json jsn_object = jsn.object_items();
            std::string uuidv4 = jsn_object["uuidv4"].string_value();

            std::cout << "Reservation: " << uuidv4 << std::endl;

            // consume message
            freeReplyObject(redisr1);
            break;

            // consume message
        }
    }

    redisFree(c);

    return EXIT_SUCCESS;
}
