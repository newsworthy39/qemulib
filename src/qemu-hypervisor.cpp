#include <qemu-hypervisor.hpp>

/**
 * Helper functions 
 */
// https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
template <typename... Args>
std::string m2_string_format(const std::string &format, Args... args)
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

std::string m2_generate_uuid_v4()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++)
    {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++)
    {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++)
    {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++)
    {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++)
    {
        ss << dis(gen);
    };
    return ss.str();
}

bool fileExists(const std::string &filename)
{
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

/**
 * This defines our instances
 */
std::map<std::string, std::tuple<int, int>> instancemodels = {
    {"t1-small", {1024, 1}},
    {"t1-medium", {2048, 2}},
    {"t1-large", {4096, 4}},
    {"t1-xlarge", {8196, 8}}};



int getNumberOfDrives(std::vector<std::string> &args)
{
    int driveCount = 0;
    for (std::vector<std::string>::iterator it = args.begin(); it != args.end(); ++it)
    {
        if ((*it) == "-drive")
            driveCount++;
    }
    return driveCount;
}

/** Stack functions */
void PushSingleArgument(QemuContext &ctx, std::string value)
{
    ctx.push_back(value);
}

void PushArguments(QemuContext &ctx, std::string key, std::string value)
{
    PushSingleArgument(ctx, key);
    PushSingleArgument(ctx, value);
}

void QEMU_Generate_ID(QemuContext &ctx)
{
    std::string id = m2_generate_uuid_v4();
    PushArguments(ctx, "-name", id);
    std::cout << "Using name: " << id << std::endl;
}

std::string QEMU_Guest_ID(QemuContext &ctx)
{
    QemuContext::iterator p = std::find(ctx.begin(), ctx.end(), "-name");
    if (p != ctx.end())
    {
        return *++(p); // return the next field.
    }
    else
    {
        QEMU_Generate_ID(ctx);
        return QEMU_Guest_ID(ctx);
    }
}

/**
 * QEMU_Accept_Incoming (QemuContext, int port)
 */
void QEMU_Accept_Incoming(QemuContext &ctx, int port)
{
    PushArguments(ctx, "-incoming", m2_string_format("tcp:0:%d", port));
}

/*
 * QEMU_init (int memory, int numcpus)
 * Have a look at https://bugzilla.redhat.com/show_bug.cgi?id=1777210
 */
void QEMU_instance(QemuContext &ctx, const std::string &instanceargument)
{
    QEMU_Generate_ID(ctx);

    int memory = 2048, cpu = 2;
    std::string guestid = QEMU_Guest_ID(ctx);

    PushArguments(ctx, "-device", "virtio-rng-pci"); // random
    PushArguments(ctx, "-monitor", m2_string_format("unix:/tmp/%s.monitor,server,nowait", guestid.c_str()));
    PushArguments(ctx, "-pidfile", m2_string_format("/tmp/%s.pid", guestid.c_str()));
    PushArguments(ctx, "-qmp", m2_string_format("unix:/tmp/%s.socket,server,nowait", guestid.c_str()));
    PushArguments(ctx, "-runas", "gandalf");
    PushArguments(ctx, "-watchdog", "i6300esb");
    PushArguments(ctx, "-watchdog-action", "reset");
    PushArguments(ctx, "-k", QEMU_LANG);
    // PushArguments(args, "-smbios","type=1,serial=ds=nocloud-net;s=http://10.0.92.38:9000/");
    PushArguments(ctx, "-smbios", "type=1,serial=ds=None");
    PushArguments(ctx, "-boot", "menu=off,order=cdn,once=c,strict=off"); // Boot with ISO if disk is missing.
    PushArguments(ctx, "-rtc", "base=utc,clock=host,driftfix=slew");

    for (auto it = instancemodels.begin(); it != instancemodels.end(); it++)
    {
        std::string first = it->first;
        std::tuple<int, int> second = it->second;

        if (first.starts_with(instanceargument))
        {
            memory = std::get<0>(second);
            cpu = std::get<1>(second);
            break;
        }
    }

    PushArguments(ctx, "-m", m2_string_format("%d", memory));                                       // memory
    PushArguments(ctx, "-smp", m2_string_format("cpus=%d,cores=%d,maxcpus=%d,threads=1,dies=1", cpu, cpu, cpu * 2)); // cpu setup.
    PushArguments(ctx, "-cpu", "host");

    std::cout << "Using instance-profile: " << instanceargument << ", memory: " << memory << ", cpu: " << cpu << std::endl;
}

void QEMU_drive(std::vector<std::string> &args, const std::string &drive)
{
    if (fileExists(drive))
    {
        PushArguments(args, "-drive", m2_string_format("file=%s,if=virtio,index=%d,media=disk,format=qcow2,cache=writeback,id=blockdev", drive.c_str(), getNumberOfDrives(args)));
        std::cout << "Using drive: " << drive << std::endl;
    }
    else
    {
        std::cerr << "The drive " << drive << " does not exists: " << strerror(errno) << std::endl;
        exit(-1);
    }
}

/*
 QEMU_machine
 Sets the correct machine and ISO backend.
 */
void QEMU_machine(QemuContext &args, const std::string model)
{
    const std::string delimiter = "/";
    const std::string type = model.substr(0, model.find(delimiter));

    // Add a safe-list, here - match it against QEMU drivers.
    PushArguments(args, "-M", type);
    std::cout << "Using model: " << type << std::endl;
}

void QEMU_iso(QemuContext &args, const std::string &model, const std::string &database)
{
    const std::string delimiter = "/";
    const std::string type = model.substr(0, model.find(delimiter));
    std::map<std::string, std::string> images = qemuListImages(database);

    for (auto it = images.begin(); it != images.end(); it++)
    {
        std::string first = it->first;
        std::string isopath = it->second;

        if (first.starts_with(model) && fileExists(isopath))
        {
            PushArguments(args, "-drive", m2_string_format("file=%s,index=%d,media=cdrom", isopath.c_str(), getNumberOfDrives(args)));
            std::cout << "Using iso: " << isopath << std::endl;
            break;
        }
    }
}

void QEMU_display(std::vector<std::string> &ctx, const QEMU_DISPLAY &display)
{
    if (display == QEMU_DISPLAY::NONE)
    {
        PushArguments(ctx, "-display", "none"); // display
    }
    if (display == QEMU_DISPLAY::VNC)
    {
        PushArguments(ctx, "-vnc", "localhost:0,to=99"); // display
    }
    if (display == QEMU_DISPLAY::GTK)
    {
        PushArguments(ctx, "-display", "gtk"); // display
    }

//    PushArguments(ctx, "-vga", "virtio"); // set vga card.
}

void QEMU_ephimeral(QemuContext &ctx)
{
    PushSingleArgument(ctx, "-snapshot"); // snapshot
}

/**
 * This needs inside a child-process, who owns everything.
 */
void QEMU_launch(QemuContext &args, bool block)
{
    // check to daemonize
    if (block == false)
        PushSingleArgument(args, "-daemonize");

    // Finally, we copy it into a char-array, to make it compatible with execvp and run it.
    std::vector<char *> left_argv;
    left_argv.push_back(const_cast<char *>(QEMU_DEFAULT_SYSTEM));
    left_argv.push_back(const_cast<char *>("-enable-kvm"));
    for (int i = 0; i < args.size(); i++)
    {
        left_argv.push_back(const_cast<char *>(args[i].c_str()));
    }
    left_argv.push_back(NULL); // leave a null

    execvp(left_argv[0], &left_argv[0]); // we'll never return from this, unless an error occurs.

    std::cerr << "Error on exec of " << left_argv[0] << ": " << strerror(errno) << std::endl;
    _exit(errno == ENOENT ? 127 : 126);
}

std::vector<std::string> QEMU_List_VMImages(const std::filesystem::path filter, const std::filesystem::path path)
{
    std::vector<std::string> files;
    using std::filesystem::directory_iterator;
    std::string reg = std::string(path) + std::string(filter);

    for (const auto &file : directory_iterator(path))
    {
        std::string f = file.path();

        if (f.find(reg) == 0)
        {
            files.push_back(f);
        }
    }

    return files;
}

void QEMU_Notify_Started(QemuContext &ctx)
{
}

void QEMU_Notify_Exited(QemuContext &ctx)
{

    std::string guestid = QEMU_Guest_ID(ctx);
    std::string str_mon = m2_string_format("/tmp/%s.monitor", guestid.c_str());
    std::string str_pid = m2_string_format("/tmp/%s.pid", guestid.c_str());
    std::string str_socket = m2_string_format("/tmp/%s.socket", guestid.c_str());

    if (fileExists(str_mon))
    {
        unlink(str_mon.c_str());
    }

    if (fileExists(str_pid))
    {
        unlink(str_pid.c_str());
    }

    if (fileExists(str_socket))
    {
        unlink(str_socket.c_str());
    }
}