#include "qemu.hpp"

/**
 * Helper functions 
 */
// https://stackoverflow.com/questions/2342162/stdstring-formatting-like-sprintf
template <typename... Args>
std::string string_format(const std::string &format, Args... args)
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

std::string getMacSys(std::string tapname)
{

    std::string mac;
    ;
    auto tapfile = string_format("/sys/class/net/%s/address", tapname.c_str());
    std::ifstream myfile(tapfile.c_str());
    if (myfile.is_open())
    {                  // always check whether the file is open
        myfile >> mac; // pipe file's content into stream
    }

    return mac;
}

int getTapIndex(std::string tapname)
{

    int tapIndex = 0;
    auto tapfile = string_format("/sys/class/net/%s/ifindex", tapname.c_str());
    std::ifstream myfile(tapfile.c_str());
    if (myfile.is_open())
    {                       // always check whether the file is open
        myfile >> tapIndex; // pipe file's content into stream
    }

    return tapIndex;
}

std::string generate_uuid_v4()
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

/** Stack functions */
void PushSingleArgument(std::vector<std::string> &args, std::string value)
{
    args.push_back(value);
}

void PushArguments(std::vector<std::string> &args, std::string key, std::string value)
{
    PushSingleArgument(args, key);
    PushSingleArgument(args, value);
}

/*
 * QEMU_init (int memory, int numcpus)
*/
void QEMU_init(std::vector<std::string> &args, std::string tapname)
{
    auto tappath = string_format("/dev/tap%d", getTapIndex(tapname));
    // Finally, open the tap.
    int fd = open(tappath.c_str(), O_RDWR);
    if (fd == -1)
    {
        std::cerr << "error opening tap " << tappath << ", " << strerror(errno) << std::endl;
        exit(-1);
    }

    std::cout << "Tap-device: /dev/tap" << fd << std::endl;
    std::string guestname = generate_uuid_v4();

    PushSingleArgument(args, "/usr/bin/qemu-system-x86_64");
    PushSingleArgument(args, "-enable-kvm");

    PushArguments(args, "-name", guestname);
    PushArguments(args, "-M", "ubuntu"); // machine-type
    PushArguments(args, "-netdev", string_format("tap,fd=%d,id=guest0", fd));
    PushArguments(args, "-device", string_format("virtio-net,mac=%s,netdev=guest0", getMacSys(tapname).c_str()));
    PushArguments(args, "-device", "virtio-rng-pci"); // random
    PushArguments(args, "-monitor", string_format("unix:/tmp/%s.monitor,server,nowait", guestname.c_str()));
    PushArguments(args, "-pidfile", string_format("/tmp/%s.pid", guestname.c_str()));
    PushArguments(args, "-runas", "gandalf");
    PushArguments(args, "-watchdog", "i6300esb");
    PushArguments(args, "-watchdog-action", "reset");
    PushArguments(args, "-k", "da");
    PushSingleArgument(args, "-daemonize");
}

void QEMU_drives(std::vector<std::string> &args, std::string drive)
{
    PushArguments(args, "-boot", "cd");
    PushArguments(args, "-drive", string_format("file=%s,if=virtio,index=0,media=disk,format=qcow2,cache=off", drive.c_str()));
    PushArguments(args, "-drive", "file=/home/gandalf/Downloads/Applications/ubuntu-20.04-live-server-amd64.iso,index=1,media=cdrom");
}

void QEMU_display(std::vector<std::string> &args, const QEMU_DISPLAY &display)
{
    std::string displayAsString = "gtk";
    if (display == QEMU_DISPLAY::NONE)
    {
        displayAsString = "none";
    }
    if (display == QEMU_DISPLAY::VNC)
    {
        displayAsString = "vnc=:1";
    }

    PushArguments(args, "-display", displayAsString); // display
}

void QEMU_instance(std::vector<std::string> &args, std::string instanceargument)
{
    int memory = 2048;
    int cpu = 2;

    std::map<std::string, std::tuple<int, int>> instances = {
        {"small", {1024, 1}},
        {"medium", {2048, 2}},
        {"large", {4096, 4}}};

    auto it = instances.find(instanceargument);
    while (it != instances.end())
    {
        std::tuple<int, int> value = it->second;
        memory = std::get<0>(value);
        cpu = std::get<1>(value);
        it = instances.end();
    }

    PushArguments(args, "-m", string_format("%d", memory)); // memory
    PushArguments(args, "-smp", string_format("cpus=%d,cores=%d,maxcpus=%d", cpu, cpu, cpu * 2)); // cpu setup.
    PushArguments(args, "-cpu", "host");
}

void QEMU_Launch(std::vector<std::string> &args)
{
    std::vector<char *> left_argv;
    for (int i = 0; i < args.size(); i++)
    {
        left_argv.push_back(const_cast<char *>(args[i].c_str()));
    }

    left_argv.push_back(NULL); // leave a null

    const char *arg0 = args[0].c_str();
    execvp(arg0, &left_argv[0]);
    std::cerr << "Error on exec of " << arg0 << ": " << strerror(errno) << std::endl;
    _exit(errno == ENOENT ? 127 : 126);
}

