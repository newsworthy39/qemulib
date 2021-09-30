#include <qemu-hostd.hpp>

std::string redis = QEMU_DEFAULT_REDIS;
std::string username = "redis";
std::string password = "foobared";
std::string topic = hostd_generate_client_name();
std::string bridge = "br0";
std::string nspace = "/var/run/netns/default";
std::string cloud_init = "None";

std::vector<QemuContext> reservations;

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

    return std::string(m3_string_format("activation-%s", ss.str().c_str()));
}

static void broadcastMessage(std::string reply, std::string &message)
{
    // Redis-stuff.
    /**
     * This block, sends back the UUID to a topic, if somebody cares.
     */
    redisContext *c1 = redisConnect(redis.c_str(), 6379);
    if (c1 == NULL || c1->err)
    {
        if (c1)
        {
            std::cerr << "(reply) Error connecting to REDIS: " << c1->errstr << std::endl;
            return;
        }
        else
        {
            printf("Can't allocate redis context\n");
            return;
        }
    }
    redisReply *rconfirmation;

    rconfirmation = (redisReply *)redisCommand(c1, "AUTH %s", password.c_str());
    freeReplyObject(rconfirmation);
    rconfirmation = (redisReply *)redisCommand(c1, "PUBLISH %s %s", reply.c_str(), message.c_str());
    freeReplyObject(rconfirmation);
    redisFree(c1);
}

void onPowerdownMessage(json11::Json::object arguments)
{
    // First, get the ARN, and then we setup the context.
    std::string arn = arguments["arn"].string_value();
    if (arn.empty())
    {
        std::cerr << "arn not supplied" << std::endl;
        return;
    }

        std::string reply = arguments["reply"].string_value();
    if (reply.empty())
    {
        std::cerr << "Reply not supplied" << std::endl;
        return;
    }

    for (auto it = reservations.begin(); it != reservations.end();)
    {
        QemuContext ctx = (*it);

        if (QEMU_Guest_ID(ctx) == arn)
        {
            QEMU_powerdown(ctx);

            std::string confirmation = m3_string_format("{ \"uuidv4\": \"%s\" }", QEMU_Guest_ID(ctx).c_str());

            broadcastMessage(reply, confirmation);

            break;
        }
        else
        {
            ++it;
        }
    }
}

void onReservationsMessage(json11::Json::object arguments)
{
    // First, get the ARN, and then we setup the context.

    std::string reply_topic = arguments["reply"].string_value();
    if (reply_topic.empty())
    {
        std::cerr << "reply_topic not supplied" << std::endl;
        return;
    }

    /**
     * This block, sends back the UUIDs to a topic, if somebody cares.
     */
    redisContext *c1 = redisConnect(redis.c_str(), 6379);
    if (c1 == NULL || c1->err)
    {
        if (c1)
        {
            std::cerr << "(reply) Error connecting to REDIS: " << c1->errstr << std::endl;
            exit(-1);
        }
        else
        {
            std::cerr << "Can't allocate redis context." << std::endl;
        }
    }

    struct reservations_json
    {
        std::string uuid;
        std::string arn;
        reservations_json(std::string uuid, std::string arn) : uuid(uuid), arn(arn){};
        reservations_json(const reservations_json &reserve) : uuid(reserve.uuid), arn(reserve.arn){};
        virtual ~reservations_json(){};
        json11::Json to_json() const { return json11::Json::array{arn, uuid}; };
    };

    std::vector<reservations_json> res;
    for (auto it = reservations.begin(); it != reservations.end();)
    {
        QemuContext ctx = (*it);
        reservations_json js{"server00", QEMU_Guest_ID(ctx)};
        res.push_back(js);
        ++it;
    }

    redisReply *rconfirmation;
    std::string serialized_reservations_1 = json11::Json(res).dump();
    std::string json_reply = m3_string_format("{ \"reservations\": %s }", serialized_reservations_1.c_str());
    std::cout << "Sending " << json_reply << std::endl;
    rconfirmation = (redisReply *)redisCommand(c1, "AUTH %s", password.c_str());
    freeReplyObject(rconfirmation);
    rconfirmation = (redisReply *)redisCommand(c1, "PUBLISH %s %s", reply_topic.c_str(), json_reply.c_str());
    freeReplyObject(rconfirmation);
    redisFree(c1);
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
    QEMU_allocate_backed_drive(arn, 32, "/mnt/faststorage/vms/ubuntu2004backingfile.img");
    int result = QEMU_drive(ctx, m3_string_format("/mnt/faststorage/vms/%s.img", arn.c_str()));
    if (result == -1)
    {
        return;
    }

    // QEMU_ephimeral(ctx);
    QEMU_instance(ctx, instance);
    QEMU_display(ctx, QEMU_DISPLAY::VNC);
    QEMU_machine(ctx, QEMU_DEFAULT_MACHINE);

    if (!cloud_init.compare("None"))
    {
        QEMU_cloud_init_arguments(ctx, cloud_init);
    }
    else
    {
        QEMU_cloud_init_remove(ctx);
    }

    //std::string tapname = QEMU_allocate_macvtap(ctx, "enp2s0");
    QEMU_set_namespace(nspace);
    int bridge_result = QEMU_allocate_bridge(bridge);
    if (bridge_result != 0)
    {
        std::cerr << "Bridge allocation error: " << bridge_result << std::endl;
        return;
    }

    QEMU_link_up(bridge, 1);
    std::string tapdevice = QEMU_allocate_tun(ctx);
    QEMU_enslave_interface(bridge, tapdevice);
    QEMU_set_default_namespace();
    QEMU_Notify_Started(ctx); // Notify Qemu started.

    // fork and wait, return - then cleanup files.
    pid_t pid = fork();
    if (pid == 0)
    {
        std::cout << "qemu-launcher (" << getpid() << ") hi." << std::endl;
        QEMU_launch(ctx, true); // where qemu-launch, BLOCKS.
        exit(0);                // never reach this point.
    }

    /**
     * We store, the uuid, for the possibility, to query the process (threaded)
     */
    reservations.push_back(ctx);
    std::string confirmation = m3_string_format("{ \"uuidv4\": \"%s\" }", QEMU_Guest_ID(ctx).c_str());
    broadcastMessage("reply-" + topic, confirmation);

    // Finally, we wait until the pid have returned, and send notifications.
    int status = 0;
    pid_t w = waitpid(pid, &status, WUNTRACED | WCONTINUED);
    if (WIFEXITED(status))
    {
        QEMU_Notify_Exited(ctx);

        // We have to be in the right namespace.
        QEMU_set_namespace(nspace);
        QEMU_Delete_Link(ctx, tapdevice);
        QEMU_set_default_namespace();

        /**
         * Finally we clean up the reservation 
         */

        for (auto it = reservations.begin(); it != reservations.end();)
        {
            QemuContext ct = (*it);

            if (QEMU_Guest_ID(ct) == QEMU_Guest_ID(ctx))
            {
                it = reservations.erase(it);
            }
            else
            {
                ++it;
            }
        }
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

void onReceiveMessage(redisAsyncContext *c, void *reply, void *privdata)
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
                std::cerr << "JSON Arguments object, must not be empty" << std::endl;
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
                std::cerr << "MIGRATE not-implemented" << std::endl;
            }

            // But we can also, migrate it
            if (jsclass == "reservations")
            {
                onReservationsMessage(arguments);
            }

            if (jsclass == "powerdown")
            {
                onPowerdownMessage(arguments);
            }
        }
    }
}

int main(int argc, char *argv[])
{

    bool verbose = false;
    struct event_base *base = event_base_new();
    signal(SIGPIPE, SIG_IGN);
    std::string usage = m3_string_format("Usage(): %s (-h) -redis {default=%s} -user {default=%s} -password {default=%s} -topic {default=%s} "
                                         "-bridge {default=%s} -cloudinit {default=%s} -namespace {default=%s}",
                                         argv[0], redis.c_str(),
                                         username.c_str(), password.c_str(), topic.c_str(), bridge.c_str(), cloud_init.c_str(), nspace.c_str());

    for (int i = 1; i < argc; ++i)
    { // Remember argv[0] is the path to the program, we want from argv[1] onwards

        if (std::string(argv[i]).find("-h") != std::string::npos)
        {
            std::cout << usage << std::endl;
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
        if (std::string(argv[i]).find("-topic") != std::string::npos && (i + 1 < argc))
        {
            topic = argv[i + 1];
        }

        if (std::string(argv[i]).find("-bridge") != std::string::npos && (i + 1 < argc))
        {
            bridge = argv[i + 1];
        }
        if (std::string(argv[i]).find("-namespace") != std::string::npos && (i + 1 < argc))
        {
            nspace = argv[i + 1];

            // Check, that namespace eixsts
            if (!m1_fileExists(nspace.c_str()))
            {
                std::cerr << "Error: Namespace " << nspace << " does, not exist." << std::endl;
                std::cout << usage << std::endl;
                exit(-1);
            }
        }

        if (std::string(argv[i]).find("-cloudinit") != std::string::npos && (i + 1 < argc))
        {
            cloud_init = argv[i + 1];
        }
    }
    redisAsyncContext *c = redisAsyncConnect(redis.c_str(), 6379);
    if (c->err)
    {
        std::cerr << "Error " << c->errstr << std::endl;
        exit(-1);
    }

    redisAsyncCommand(c, NULL, NULL, m3_string_format("AUTH %s", password.c_str()).c_str());
    redisLibeventAttach(c, base);
    redisAsyncCommand(c, onReceiveMessage, NULL, m3_string_format("SUBSCRIBE %s", topic.c_str()).c_str());

    std::cout << " Using redis: " << redis << std::endl;
    std::cout << " Using topic: " << topic << std::endl;
    std::cout << " Using cloud_init: " << cloud_init << std::endl;
    std::cout << " Using namespace: " << nspace << std::endl;
    std::cout << " Using bridge: " << bridge << std::endl;

    event_base_dispatch(base);

    return EXIT_SUCCESS;
}
