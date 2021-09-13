#include <qemu-hostd.hpp>

std::string redis = QEMU_DEFAULT_REDIS;
std::string username = "redis";
std::string password = "foobared";

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

void onLaunchMessage(json11::Json::object arguments)
{
    // First, get the ARN, and then we setup the context.
    std::string arn = arguments["arn"].string_value();
    std::cout << "Launching: " << arn << std::endl;

    QemuContext ctx;
    int result = QEMU_drive(ctx, m3_string_format("/mnt/faststorage/vms/%s.img", arn.c_str()));
    if (result == -1) {
        return;
    }
    QEMU_ephimeral(ctx);
    QEMU_instance(ctx, QEMU_DEFAULT_INSTANCE);
    QEMU_display(ctx, QEMU_DISPLAY::VNC);
    QEMU_machine(ctx, QEMU_DEFAULT_MACHINE);
    std::string tapname = QEMU_allocate_macvtap(ctx, "enp2s0");
    QEMU_Notify_Started(ctx); // Notify Qemu started.

    // fork and wait, return - then cleanup files.
    pid_t pid = fork();
    int status;
    if (pid == 0)
    {
        QEMU_launch(ctx, true); // where qemu-launch, BLOCKS.
    }
    else
    {
        do
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
            redisr = (redisReply *)redisCommand(c, m3_string_format("AUTH %s", password.c_str()).c_str());
            freeReplyObject(redisr);
            redisr = (redisReply *)redisCommand(c, m3_string_format("SUBSCRIBE qmp-%s", guestid.c_str()).c_str());
            freeReplyObject(redisr);

            // Start the back-and-forth loop
            while (redisGetReply(c, (void **)&redisr) == REDIS_OK)
            {
                if (redisr->type == REDIS_REPLY_ARRAY)
                {

                    std::string str_error;
                    json11::Json jsn = json11::Json::parse(redisr->element[2]->str, str_error);
                    if (!str_error.empty()) {
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

            redisFree(c);
            std::cout << "Waiting for sub-process, to exit." << std::endl;

            // Finally, we wait until the pid have returned, and send notifications.
            pid_t w = waitpid(pid, &status, WUNTRACED | WCONTINUED);
            if (WIFEXITED(status))
            {
                QEMU_Notify_Exited(ctx);
                QEMU_Delete_Link(ctx, tapname);
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
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));

        std::cout << "Bye." << std::endl;
    }
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

            // To find, the white rabbit, first look at the top-hat.
            json11::Json jsobj = jsn.object_items();
            std::string jsclass = jsobj["execute"].string_value();
            json11::Json::object arguments = jsobj["arguments"].object_items();
            if (!arguments.empty()) {
                return;
            }

            // Next, we launch it.
            if (jsclass == "launch")
            {
                std::thread t(&onLaunchMessage, arguments);
                t.detach();
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
            std::cout << "Usage(): " << argv[0] << " (-h) -redis {default=" << QEMU_DEFAULT_REDIS << "} -user {default=" << username << "} -password {default=" << password << "} " << std::endl;
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
    redisAsyncCommand(c, onActivationMessage, NULL, "SUBSCRIBE activation");
    std::cout << " Subscribed to activation " << std::endl;
    event_base_dispatch(base);

    return EXIT_SUCCESS;
}
