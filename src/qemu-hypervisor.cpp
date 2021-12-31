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

std::string generateRandomPrefixedString(std::string prefix, int length = 8)
{
    static std::random_device r;
    static std::default_random_engine e1(r());
    static std::uniform_int_distribution<int> dis(0, 15);

    std::stringstream ss;
    int i = 0, count = 0;
    ss << std::hex;
    for (i = 0; i < length; i++)
    {
        ss << dis(e1);
    }

    return m2_string_format("%s-%s", prefix.c_str(), ss.str().c_str());
}

/**
 * generatePrefixedUniqueString(std::string prefix, int length)
 */
std::string generatePrefixedUniqueString(std::string prefix, std::size_t hash, unsigned int length = 8)
{
    std::stringstream ss;
    int i = 0, count = 0;
    ss << std::hex;
    ss << hash;

    return m2_string_format("%s-%s", prefix.c_str(), ss.str().substr(0, length).c_str());
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

/** Stack functions */
void PushSingleArgument(QemuContext &ctx, std::string value)
{
    ctx.devices.push_back(value);
}

void PushArguments(QemuContext &ctx, std::string key, std::string value)
{
    PushSingleArgument(ctx, key);
    PushSingleArgument(ctx, value);
}

void PushSingleDriveArgument(QemuContext &ctx, std::string value)
{
    ctx.drives.push_back(value);
}

void PushDriveArgument(QemuContext &ctx, std::string key, std::string value)
{
    PushSingleDriveArgument(ctx, key);
    PushSingleDriveArgument(ctx, value);
}

std::string QEMU_reservation_id(QemuContext &ctx)
{
    auto it = std::find_if(ctx.devices.begin(), ctx.devices.end(), [&ctx](const std::string &ct)
                           { return "-name" == ct; });

    if (it != ctx.devices.end())
    {
        return *++(it); // return the next field.
    }
    else
    {
        std::string id = m2_generate_uuid_v4();
        PushArguments(ctx, "-name", id);
        std::cout << "Using reservation-id: " << id << std::endl;
        return id;
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
    int memory = 2048, cpu = 2;

    std::string guestid = QEMU_reservation_id(ctx);

    PushArguments(ctx, "-device", "virtio-rng-pci"); // random
    PushArguments(ctx, "-device", "virtio-balloon");
    PushArguments(ctx, "-runas", "gandalf");
    PushArguments(ctx, "-watchdog", "i6300esb");
    PushArguments(ctx, "-watchdog-action", "reset");
    PushArguments(ctx, "-k", QEMU_LANG);
    PushArguments(ctx, "-boot", "menu=off,order=cdn,strict=off"); // Boot with ISO if disk is missing.
    PushArguments(ctx, "-rtc", "base=utc,clock=host,driftfix=slew");
    PushArguments(ctx, "-monitor", m2_string_format("unix:/tmp/%s.monitor,server,nowait", guestid.c_str()));
    PushArguments(ctx, "-pidfile", m2_string_format("/tmp/%s.pid", guestid.c_str()));
    PushArguments(ctx, "-qmp", m2_string_format("unix:/tmp/%s.socket,server,nowait", guestid.c_str()));

    // This is the QEMU GUEST AGENT
    PushArguments(ctx, "-chardev", m2_string_format("socket,path=/tmp/qga-%s.socket,server,nowait,id=qga0", guestid.c_str()));
    PushArguments(ctx, "-device", "virtio-serial");
    PushArguments(ctx, "-device", "virtserialport,chardev=qga0,name=org.qemu.guest_agent.0");

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

    PushArguments(ctx, "-m", m2_string_format("%d", memory));                                                        // memory
    PushArguments(ctx, "-smp", m2_string_format("cpus=%d,cores=%d,maxcpus=%d,threads=1,dies=1", cpu, cpu, cpu * 2)); // cpu setup.
    PushArguments(ctx, "-cpu", "host");

    //AuthenticIntel)
    //CPU="-cpu host,kvm=on,vendor=GenuineIntel,+hypervisor,+invtsc,+kvm_pv_eoi,+kvm_pv_unhalt";;
    //AuthenticAMD|*)
    //# Used in past versions: +movbe,+smep,+xgetbv1,+xsavec
    //# Warn on AMD:           +fma4,+pcid
    //CPU="-cpu Penryn,kvm=on,vendor=GenuineIntel,+aes,+avx,+avx2,+bmi1,+bmi2,+fma,+hypervisor,+invtsc,+kvm_pv_eoi,+kvm_pv_unhalt,+popcnt,+ssse3,+sse4.2,vmware-cpuid-freq=on,+xsave,+xsaveopt,check";;

    std::cout << "Using instance-profile: " << instanceargument << ", memory: " << memory << ", cpu: " << cpu << std::endl;
}

/**
 * QEMU_drive()
 */
int QEMU_drive(QemuContext &args, const std::string &drive, unsigned int bootindex = 0)
{
    if (fileExists(drive))
    {
        std::string blockdevice = generateRandomPrefixedString("block", 4);
        PushDriveArgument(args, "-drive", m2_string_format("if=virtio,index=%d,file=%s,media=disk,format=qcow2,cache=writeback,id=%s", bootindex, drive.c_str(), blockdevice.c_str()));
        std::cout << "Using drive: " << drive << std::endl;
        return 0;
    }
    else
    {
        std::cerr << "The drive " << drive << " does not exists: " << strerror(errno) << std::endl;
        return -1;
    }
}

/**
 * QEMU_allocate_drive(std::string id, ssize_t sz)
 * Will create a new image, but refuse to overwrite old ones.
 */
void QEMU_allocate_drive(std::string path, ssize_t sz)
{
    if (fileExists(path))
    {
        return;
    }

    int status = 0;
    pid_t child = fork();
    if (child == 0)
    {

        // Finally, we copy it into a char-array, to make it compatible with execvp and run it.
        std::vector<char *> left_argv;
        left_argv.push_back(const_cast<char *>(QEMU_DEFAULT_IMG));
        left_argv.push_back(const_cast<char *>("create"));
        left_argv.push_back(const_cast<char *>("-f"));
        left_argv.push_back(const_cast<char *>("qcow2"));
        left_argv.push_back(const_cast<char *>(path.c_str()));
        left_argv.push_back(const_cast<char *>(m2_string_format("%dG", sz).c_str()));
        left_argv.push_back(NULL); // leave a null

        execvp(left_argv[0], &left_argv[0]); // we'll never return from this, unless an error occurs.
    }
    else
    {
        // Finally, we wait until the pid have returned, and send notifications.
        pid_t w = waitpid(child, &status, WUNTRACED | WCONTINUED);
    }
}

/**
 * QEMU_allocate_drive(std::string id, std::string backingid, ssize_t sz)
 * Will create a new image, but refuse to overwrite old ones.
 */
void QEMU_allocate_backed_drive(std::string path, ssize_t sz, std::string backingfile)
{
    if (fileExists(path))
    {
        return;
    }

    int status = 0;
    pid_t child = fork();
    if (child == 0)
    {

        // Finally, we copy it into a char-array, to make it compatible with execvp and run it.
        std::vector<char *> left_argv;
        left_argv.push_back(const_cast<char *>(QEMU_DEFAULT_IMG));
        left_argv.push_back(const_cast<char *>("create"));
        left_argv.push_back(const_cast<char *>("-f"));
        left_argv.push_back(const_cast<char *>("qcow2"));
        left_argv.push_back(const_cast<char *>("-b"));
        left_argv.push_back(const_cast<char *>(backingfile.c_str()));
        left_argv.push_back(const_cast<char *>(path.c_str()));
        left_argv.push_back(const_cast<char *>(m2_string_format("%dG", sz).c_str()));
        left_argv.push_back(NULL); // leave a null

        execvp(left_argv[0], &left_argv[0]); // we'll never return from this, unless an error occurs.
    }
    else
    {
        // Finally, we wait until the pid have returned, and send notifications.
        pid_t w = waitpid(child, &status, WUNTRACED | WCONTINUED);
    }
}

/**
 * QEMU_rebase_backed_drive(std::string id, std::string backingpath)
 * Will rebase image onto a backing-image, but only while offline.
 */
void QEMU_rebase_backed_drive(std::string id, std::string backingfilepath)
{
    std::string drive = m2_string_format("/home/gandalf/vms/%s.img", id.c_str());
    if (!(fileExists(drive) || fileExists(backingfilepath))) // demorgan.
    {
        std::cerr << "One of the arguments file, isn't present." << std::endl;
        return;
    }

    int status = 0;
    pid_t child = fork();
    if (child == 0)
    {
        // Finally, we copy it into a char-array, to make it compatible with execvp and run it.
        std::vector<char *> left_argv;
        left_argv.push_back(const_cast<char *>(QEMU_DEFAULT_IMG));
        left_argv.push_back(const_cast<char *>("rebase"));
        left_argv.push_back(const_cast<char *>("-f"));
        left_argv.push_back(const_cast<char *>("qcow2"));
        left_argv.push_back(const_cast<char *>("-b"));
        left_argv.push_back(const_cast<char *>(backingfilepath.c_str()));
        left_argv.push_back(const_cast<char *>(drive.c_str()));
        left_argv.push_back(NULL); // leave a null

        execvp(left_argv[0], &left_argv[0]); // we'll never return from this, unless an error occurs.
    }
    else
    {
        // Finally, we wait until the pid have returned, and send notifications.
        pid_t w = waitpid(child, &status, WUNTRACED | WCONTINUED);
    }
}

/*
 QEMU_machine
 Sets the correct machine-type.
 */
void QEMU_machine(QemuContext &args, const std::string model)
{
    const std::string delimiter = "/";
    const std::string type = model.substr(0, model.find(delimiter));

    // Add a safe-list, here - match it against QEMU drivers.
    PushArguments(args, "-M", type);
    std::cout << "Using model: " << type << std::endl;
}

/**
 * QEMU_iso(QemuContext )
 * Add an ISO to the hypervisor.
 * std::string isopath
 */
void QEMU_iso(QemuContext &ctx, const std::string iso)
{
    PushArguments(ctx, "-cdrom", m2_string_format("%s", iso.c_str()));
    std::cout << "Using iso: " << iso << std::endl;
}

/**
 * QEMU_display
 * Sets the display, for the monitor
 */
void QEMU_display(QemuContext &ctx, const QEMU_DISPLAY &display)
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

/**
 * QEMU_ephimeral
 * Makes a instance, into a readonly version
 */
void QEMU_ephimeral(QemuContext &ctx)
{
    PushSingleArgument(ctx, "-snapshot"); // snapshot
}

/**
 * QEMU_launch()
 * This launches a hypervisor.
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

    // First, we take the devices.
    std::for_each(args.devices.begin(), args.devices.end(), [&left_argv](const std::string &device)
                  { left_argv.push_back(const_cast<char *>(device.c_str())); });

    // Then we take, the drives in a orderly fasion.
    std::for_each(args.drives.begin(), args.drives.end(), [&left_argv](const std::string &drive)
                  { left_argv.push_back(const_cast<char *>(drive.c_str())); });

    // Next, we copy the darn drives
    left_argv.push_back(NULL); // leave a null

    execvp(left_argv[0], &left_argv[0]); // we'll never return from this, unless an error occurs.

    std::cerr << "Error on exec of " << left_argv[0] << ": " << strerror(errno) << std::endl;
    _exit(errno == ENOENT ? 127 : 126);
}

/**
 * QEMU_Notify_started
 * Callback, that can be used when a hypervisor starts
 */
void QEMU_notified_started(QemuContext &ctx)
{
}

/**
 * QEMU_Notify_started
 * Callback, that can be used when a hypervisor stops
 */
void QEMU_notified_exited(QemuContext &ctx)
{

    std::string guestid = QEMU_reservation_id(ctx);
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

/**
 * QEMU_Cluod_init_arguments
 * Sets the cloud-init arguments-source
 * // serial=ds=nocloud-net;s=http://10.10.0.1:8000/
 */
void QEMU_cloud_init_arguments(QemuContext &ctx, std::string cloud_settings_src)
{
    PushArguments(ctx, "-smbios", m2_string_format("type=1,serial=ds=nocloud-net;s=%s", cloud_settings_src.c_str()));
}

/**
 * QEMU_Cluod_init_arguments
 * Sets the cloud-init arguments-source
 * // serial=ds=nocloud-net;s=http://10.10.0.1:8000/
 */
void QEMU_cloud_init_file(QemuContext &ctx, std::string hostname, std::string instance_id)
{
    PushArguments(ctx, "-smbios", m2_string_format("type=1,serial=ds=nocloud;h=%s;i=%s;s=file:///tmp/seed", hostname.c_str(), instance_id.c_str()));
}

/**
 * QEMU_Cluod_init_arguments
 * Removes any cloud-init arguments
 * serial=ds=None
 */
void QEMU_cloud_init_remove(QemuContext &ctx)
{
    PushArguments(ctx, "-smbios", "type=1,serial=ds=None");
}

/**
 * QEMU_cloud_init_default
 * Removes any cloud-init arguments
 * serial=ds=None
 */
void QEMU_cloud_init_default(QemuContext &ctx, std::string hostname, std::string instanceid)
{
    PushArguments(ctx, "-smbios", m2_string_format("type=1,serial=ds=nocloud;h=%s;i=%s", hostname.c_str(), instanceid.c_str()));
}

/**
 * QEMU_get_pid
 * get pid of running hypervisor.
 */
pid_t QEMU_get_pid(QemuContext &ctx)
{
    std::string guestid = QEMU_reservation_id(ctx);
    std::string str_pid = m2_string_format("/tmp/%s.pid", guestid.c_str());
    pid_t pid;
    std::ifstream pidfile;
    pidfile.open(str_pid.c_str());
    pidfile >> pid;
    pidfile.close();
    return pid;
}