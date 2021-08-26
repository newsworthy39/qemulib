#include "qemu.hpp"

/**
 * This defines our instances
 */
std::map<std::string, std::tuple<int, int>> instancemodels = {
    {"small", {1024, 1}},
    {"medium", {2048, 2}},
    {"large", {4096, 4}},
    {"xlarge", {8196, 8}}};

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

bool fileExists(const std::string &filename)
{
    struct stat buffer;
    return (stat(filename.c_str(), &buffer) == 0);
}

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
void QEMU_machine(std::vector<std::string> &args, const std::string &instanceargument)
{
    std::string guestname = generate_uuid_v4();
    int memory = 2048, cpu = 2;

    PushArguments(args, "-name", guestname);
    PushArguments(args, "-M", "ubuntu");              // machine-type
    PushArguments(args, "-device", "virtio-rng-pci"); // random
    PushArguments(args, "-monitor", string_format("unix:/tmp/%s.monitor,server,nowait", guestname.c_str()));
    PushArguments(args, "-pidfile", string_format("/tmp/%s.pid", guestname.c_str()));
    PushArguments(args, "-runas", "gandalf");
    PushArguments(args, "-watchdog", "i6300esb");
    PushArguments(args, "-watchdog-action", "reset");
    PushArguments(args, "-k", QEMU_LANG);
    // PushArguments(args, "-smbios","type=1,serial=ds=nocloud-net;s=http://10.0.92.38:9000/");
    PushArguments(args, "-smbios", "type=1,serial=ds=None");

    auto it = instancemodels.find(instanceargument);
    while (it != instancemodels.end())
    {
        std::tuple<int, int> value = it->second;
        memory = std::get<0>(value);
        cpu = std::get<1>(value);
        it = instancemodels.end();
    }

    PushArguments(args, "-m", string_format("%d", memory));                                       // memory
    PushArguments(args, "-smp", string_format("cpus=%d,cores=%d,maxcpus=%d", cpu, cpu, cpu * 2)); // cpu setup.
    PushArguments(args, "-cpu", "host");

    std::cout << "Using instance-profile: " << instanceargument << ", memory: " << memory << ", cpu: " << cpu << std::endl;
}

void QEMU_drive(std::vector<std::string> &args, std::string drive)
{
    if (fileExists(drive))
    {
        PushArguments(args, "-drive", string_format("file=%s,if=virtio,index=%d,media=disk,format=qcow2,cache=off", drive.c_str(), getNumberOfDrives(args)));

        std::cout << "Using drive: " << drive << std::endl;
    }
    else
    {
        std::cerr << "The drive " << drive << " does not exists: " << strerror(errno) << std::endl;
        exit(-1);
    }
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

void QEMU_Launch(std::vector<std::string> &args, std::string tapname)
{
    // Finally, open the tap, before turning into a qemu binary, launching
    // the hypervisor.
    auto tappath = string_format("/dev/tap%d", if_nametoindex(tapname.c_str()));
    int fd = open(tappath.c_str(), O_RDWR);
    if (fd == -1)
    {
        std::cerr << "Error opening network-device " << tappath << ": " << strerror(errno) << std::endl;
        exit(-1);
    }

    std::cout << "Using network-device: " << tappath << std::endl;
    PushArguments(args, "-netdev", string_format("tap,fd=%d,id=guest0", fd));
    PushArguments(args, "-device", string_format("virtio-net,mac=%s,netdev=guest0,id=internet-dev", getMacSys(tapname).c_str()));
    // PushArguments(args, "-smbios", "type=41,designation='Onboard LAN',instance=1,kind=ehternet,pcidev=internet-dev")

    PushArguments(args, "-boot", "cd"); // Boot with ISO if disk is missing.
    PushArguments(args, "-drive", string_format("file=/home/gandalf/Downloads/Applications/ubuntu-20.04-live-server-amd64.iso,index=%d,media=cdrom", getNumberOfDrives(args)));
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

    execvp(left_argv[0], &left_argv[0]);
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