#include <qemu-hostd.hpp>

std::string redis = QEMU_DEFAULT_REDIS;
std::string username = "redis";
std::string password = "foobared";
std::string clientname = hostd_generate_client_name();

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

std::string hostd_generate_client_name()
{
    static std::random_device r;
    static std::default_random_engine e1(r());
    static std::uniform_int_distribution<int> dis(0, 15);

    std::stringstream ss;
    int i = 0, count = 0;
    ss << std::hex;
    for (i = 0; i < 8; i++)
    {
        ss << dis(e1);
    }

    return ss.str();
}

void onLaunchMessage(json11::Json::object arguments)
{
    // First, get the ARN, and then we setup the context.
    std::string instance = QEMU_DEFAULT_INSTANCE;
    std::string arn = arguments["arn"].string_value();
    if (arn.empty())
    {
        std::cerr << "ARN not supplied" << std::endl;
    }

    // Allow for instance.
    if (!arguments["instance"].string_value().empty())
    {
        instance = arguments["instance"].string_value();
    }

    QemuContext ctx;
    QEMU_allocate_drive(arn, 32);
    int result = QEMU_drive(ctx, m3_string_format("/mnt/faststorage/vms/%s.img", arn.c_str()));
    if (result == -1)
    {
        return;
    }

    //QEMU_ephimeral(ctx);
    QEMU_instance(ctx, instance);
    QEMU_display(ctx, QEMU_DISPLAY::VNC);
    QEMU_machine(ctx, QEMU_DEFAULT_MACHINE);
    std::string tapname = QEMU_allocate_macvtap(ctx, "enp2s0");
    QEMU_Notify_Started(ctx); // Notify Qemu started.

    std::cout << "Launching: " << arn << ", pid: " << getpid() << std::endl;

    int status_guest;
    pid_t guest = fork();
    if (guest == 0)
    {
        // Setup redis.
        std::string guestid = QEMU_Guest_ID(ctx); // Returns an UUIDv4.
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
        redisReply *redisr;
        redisr = (redisReply *)redisCommand(c, "AUTH %s", password.c_str());
        freeReplyObject(redisr);
        redisr = (redisReply *)redisCommand(c, "SUBSCRIBE qmp-%s ", guestid.c_str());
        freeReplyObject(redisr);

        std::cout << "redis-listener (" << getpid() << ") hi." << std::endl;

        // Start the back-and-forth loop
        while (redisGetReply(c, (void **)&redisr) == REDIS_OK)
        {
            if (redisr->type == REDIS_REPLY_ARRAY)
            {

                std::string str_error;
                json11::Json jsn = json11::Json::parse(redisr->element[2]->str, str_error);
                if (!str_error.empty())
                {
                    continue;
                }

                json11::Json jsn_object = jsn.object_items();
                std::string command = jsn_object["execute"].string_value();

                // TODO: intercept and parse json, in reply-str, find "exit" and stuff,
                std::cout << "Forward to QMP-socket " << redisr->element[2]->str << std::endl;
                char str[4096];
                int s = QEMU_OpenQMPSocket(ctx);
                int t = send(s, redisr->element[2]->str, strlen(redisr->element[2]->str) + 1, 0);
                sleep(1); // This is a variable point, that needs to be looked at. e-poll?
                t = recv(s, str, 4096, 0);

                if (command.compare("system_powerdown") == 0)
                {
                    freeReplyObject(redisr);
                    close(s);
                    break;
                }

                // consume message
                freeReplyObject(redisr);
                close(s);
            }
        }

        std::cout << "redis-listener (" << getpid() << ") bye." << std::endl;
        redisFree(c);
        exit(0);
    }

    std::cout << "parent-listener (" << getpid() << ") continuing parent." << std::endl;

    // fork and wait, return - then cleanup files.
    pid_t pid = fork();
    int status;
    if (pid == 0)
    {
        std::cout << "qemu-launcher (" << getpid() << ") hi." << std::endl;
        QEMU_launch(ctx, true); // where qemu-launch, BLOCKS.
        exit(0);
    }

    /**
     * This block, sends back the UUID to a topic, if somebody cares.
     */
    redisContext *c1 = redisConnect(redis.c_str(), 6379);
    if (c1 == NULL || c1->err)
    {
        if (c1)
        {
            std::cerr << "Error connecting to REDIS: " << c1->errstr << std::endl;
            exit(-1);
        }
        else
        {
            printf("Can't allocate redis context\n");
        }
    }
    
    redisReply *rconfirmation;
    std::string confirmation = m3_string_format("{ \"uuidv4\": \"%s\" }", QEMU_Guest_ID(ctx).c_str());
    rconfirmation = (redisReply *)redisCommand(c1, "AUTH %s", password.c_str());
    freeReplyObject(rconfirmation);
    rconfirmation = (redisReply *)redisCommand(c1, "PUBLISH reply-activation-%s %s", clientname.c_str(), confirmation.c_str());
    freeReplyObject(rconfirmation);
    redisFree(c1);

    
    // Finally, we wait until the pid have returned, and send notifications.
    std::cout << "parent-listener (" << getpid() << ") waiting for child: " << pid << std::endl;
    pid_t w = waitpid(pid, &status, WUNTRACED | WCONTINUED);
    if (WIFEXITED(status))
    {
        QEMU_Notify_Exited(ctx);
        QEMU_Delete_Link(ctx, tapname);

        // Hack, to avoid defunct processes.
        std::string powerdown = "{ \"execute\": \"system_powerdown\" }";
        std::string guestid = QEMU_Guest_ID(ctx); // Returns an UUIDv4.
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
        redisReply *redisr1;
        redisr1 = (redisReply *)redisCommand(c, "AUTH %s", password.c_str());
        freeReplyObject(redisr1);
        redisr1 = (redisReply *)redisCommand(c, "PUBLISH qmp-%s %s", guestid.c_str(), powerdown.c_str());
        freeReplyObject(redisr1);
        redisFree(c);

        std::cout << "parent-listener (" << getpid() << ") will wait for redis-listener: " << guest << std::endl;

        // We need the redis-kid, too:
        pid_t x = waitpid(guest, &status_guest, WUNTRACED | WCONTINUED);
    }
    else if (WIFSIGNALED(status))
    {
        printf("killed by signal %d\n", WTERMSIG(status));
    }
    else if (WIFSTOPPED(status))
    {
        printf("stopped by signal %d\n", WSTOPSIG(status));
    }
    else if (WIFCONTINUED(status))
    {
        printf("continued\n");
    }

    std::cout << "parent-listener (" << getpid() << ") bye." << std::endl;
}

void onActivationMessage(redisAsyncContext *c, void *reply, void *privdata)
{
    redisReply *r = (redisReply *)reply;
    if (reply == NULL)
        return;

    if (r->type == REDIS_REPLY_ARRAY)
    {
        for (int j = 0; j < r->elements; j++)
        {
            printf("%u) %s\n", j, r->element[j]->str);
        }

        if (r->element[2]->str != NULL)
        {
            std::string err;
            json11::Json jsn = json11::Json::parse(r->element[2]->str, err);
            if (!err.empty())
            {
                std::cerr << "Could not parse JSON" << std::endl;
            }

            // To find, the white rabbit, first look at the top-hat.
            json11::Json jsobj = jsn.object_items();
            std::string jsclass = jsobj["execute"].string_value();
            json11::Json::object arguments = jsobj["arguments"].object_items();
            if (arguments.empty())
            {
                return;
            }

            // Next, we launch it.
            if (jsclass == "launch")
            {
                std::thread t(&onLaunchMessage, arguments);
                t.detach();
            }

            // But we can also, migrate it
            if (jsclass == "migrate")
            {
                std::cout << "MIGRATE not-implemented" << std::endl;
            }

            // But we can also, migrate it
            if (jsclass == "register")
            {
                std::cout << "register not-implemented" << std::endl;
            }
        }
    }
}

int main(int argc, char *argv[])
{
    bool verbose = false;
    struct event_base *base = event_base_new();
    signal(SIGPIPE, SIG_IGN);

    for (int i = 1; i < argc; ++i)
    { // Remember argv[0] is the path to the program, we want from argv[1] onwards

        if (std::string(argv[i]).find("-h") != std::string::npos)
        {
            std::cout << "Usage(): " << argv[0] << " (-h) -redis {default=" << QEMU_DEFAULT_REDIS << "} -user {default=" << username << "} -password {default=" << password << "} -clientname {default=" << clientname << "}" << std::endl;
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
        if (std::string(argv[i]).find("-clientname") != std::string::npos && (i + 1 < argc))
        {
            clientname = argv[i + 1];
        }
    }

    redisAsyncContext *c = redisAsyncConnect(redis.c_str(), 6379);
    if (c->err)
    {
        std::cerr << "Error " << c->errstr << std::endl;
        exit(-1);
    }

    std::cout << " Connecting to redis " << redis << std::endl;
    redisAsyncCommand(c, NULL, NULL, m3_string_format("AUTH %s", password.c_str()).c_str());
    redisLibeventAttach(c, base);
    redisAsyncCommand(c, onActivationMessage, NULL, m3_string_format("SUBSCRIBE activation-%s", clientname.c_str()).c_str());
    std::cout << " Subscribed to activation-" << clientname << std::endl;
    event_base_dispatch(base);

    return EXIT_SUCCESS;
}
