#include <iostream>
#include <qemu-hypervisor.hpp>
#include <qemu-link.hpp>

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
    QemuContext ctx;
    bool verbose = false;
    int port = 4444, snapshot = 0;
    std::string command(argv[0]);
    std::string instance = QEMU_DEFAULT_INSTANCE;
    std::string masterinterface = "enp2s0";
    std::string machine = QEMU_DEFAULT_MACHINE;
    QEMU_DISPLAY display = QEMU_DISPLAY::GTK;

    std::string usage = m3_string_format("usage(): %s (-help) (-verbose) (-headless) (-snapshot) -a {default=4444} "
                                         "-instance {default=%s} -masterinterface {default=%s} -machine {default=%s} -iso {default=none} -drive hdd (n+1)",
                                         argv[0], instance.c_str(), masterinterface.c_str(), machine.c_str());

    for (int i = 1; i < argc; ++i)
    { // Remember argv[0] is the path to the program, we want from argv[1] onwards

        if (std::string(argv[i]).find("-help") != std::string::npos)
        {
            std::cout << usage << std::endl;
            exit(-1);
        }

        if (std::string(argv[i]).find("-v") != std::string::npos)
        {
            verbose = true;
        }

        if (std::string(argv[i]).find("-headless") != std::string::npos)
        {
            display = QEMU_DISPLAY::VNC;
        }

        if (std::string(argv[i]).find("-instance") != std::string::npos && (i + 1 < argc))
        {
            instance = argv[i + 1];
        }

        if (std::string(argv[i]).find("-masterinterface") != std::string::npos && (i + 1 < argc))
        {
            masterinterface = argv[i + 1];
        }

        if (std::string(argv[i]).find("-machine") != std::string::npos && (i + 1 < argc))
        {
            machine = argv[i + 1];
        }

        if (std::string(argv[i]).find("-drive") != std::string::npos && (i + 1 < argc))
        {
            QEMU_drive(ctx, argv[i + 1]);
        }

        if (std::string(argv[i]).find("-a") != std::string::npos && (i + 1 < argc))
        {
            QEMU_Accept_Incoming(ctx, std::atoi(argv[i + 1]));
        }

        if (std::string(argv[i]).find("-snapshot") != std::string::npos)
        {
            QEMU_ephimeral(ctx);
        }
    }

    int status = 0;
    pid_t daemon = fork();
    if (daemon == 0)
    {

        QEMU_instance(ctx, instance);
        QEMU_display(ctx, display);
        QEMU_machine(ctx, machine);
        QEMU_Notify_Started(ctx);

        std::string tapdevice = QEMU_allocate_tun(ctx, "br0");
        std::string tapname = QEMU_allocate_macvtap(ctx, masterinterface);

        pid_t child = fork();
        if (child == 0)
        {
            QEMU_launch(ctx, true); // where qemu-launch, doesnt block..
        }
        else
        {

            pid_t w = waitpid(child, &status, WUNTRACED | WCONTINUED);
            if (WIFEXITED(status))
            {
                QEMU_Delete_Link(ctx, tapdevice);
                QEMU_Delete_Link(ctx, tapname);
            }

            return EXIT_SUCCESS;
        }
    }
}
