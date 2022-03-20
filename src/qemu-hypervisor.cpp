#include <qemu-hypervisor.hpp>

/**
 * @brief Overloads section
 *
 */

std::ostream &operator<<(std::ostream &os, const struct Model &model)
{
    os << model.name << ", " << model.cpus << " cpus and " << model.memory << " MB with host-flags: " << model.flags << ", architecture: " << model.arch;
    return os;
}

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

/** Stack functions */
void PushDeviceArgument(QemuContext &ctx, std::string value)
{
    ctx.devices.push_back(value);
}

void PushDriveArgument(QemuContext &ctx, std::string value)
{
    ctx.drives.push_back(value);
}

void PushArguments(QemuContext &ctx, std::string key, std::string value)
{
    PushDeviceArgument(ctx, key);
    PushDeviceArgument(ctx, value);
}

/**
 * @brief QEMU_reservation_id
 * returns the uuidv4 reservationid
 * @param ctx QemuContext
 * @return std::string
 */
std::string QEMU_reservation_id(QemuContext &ctx)
{
    if (ctx.reservationid.length() == 0)
        ctx.reservationid = m2_generate_uuid_v4();

    return ctx.reservationid;
}

/**
 * @brief QEMU_instanceID
 * returns the instance name of the guest (the friendly name)
 * @param ctx QEMUContext
 * @return std::string
 */
const std::string QEMU_instanceid(QemuContext &ctx)
{
    auto it = std::find_if(ctx.devices.begin(), ctx.devices.end(), [&ctx](const std::string &ct)
                           { return "-name" == ct; });

    if (it != ctx.devices.end())
    {
        return *++(it); // return the next field.
    }

    return "";
}

/**
 * @brief QEMU_getuser
 * returns the user, that the guest runs as.
 * @param ctx
 * @return std::string
 */
std::string QEMU_getuser(QemuContext &ctx)
{
    auto it = std::find_if(ctx.devices.begin(), ctx.devices.end(), [&ctx](const std::string &ct)
                           { return "-runas" == ct; });

    if (it != ctx.devices.end())
    {
        return *++(it); // return the next field.
    }

    return "root";
}

/**
 * QEMU_Accept_Incoming (QemuContext, int port)
 */
void QEMU_Accept_Incoming(QemuContext &ctx, int port)
{
    PushArguments(ctx, "-incoming", m2_string_format("tcp:0:%d", port));
}

/**
 * @brief QEMU_vsock
 * This creates a vmware-like vmci/vsock-virtio af_vsock interface.
 *
 * @param ctx
 * @param identifer
 */
void QEMU_vsock(QemuContext &ctx, const unsigned int cid)
{
    std::cout << "Using vsock" << std::endl;
    // This could be vhost-sock metadataservice.
    PushArguments(ctx, "-device", m2_string_format("vhost-vsock-pci,id=vhost-vsock-pci0,guest-cid=%u", cid));

    std::string guestid = QEMU_reservation_id(ctx);

    // lets save the cid
    std::string cidfile = m2_string_format("/tmp/%s.cid", guestid.c_str());

    std::ofstream file;
    file.open(cidfile.c_str());
    file << cid;
    file.close();
}

/**
 * @brief QEMU_User
 * sets the user, the instance is supposed to run under.
 *
 * @param ctx A valid QemuContext.
 * @param user a username matching /etc/passwd
 */
void QEMU_user(QemuContext &ctx, const std::string user)
{
    PushArguments(ctx, "-runas", user);
}

/*
 * QEMU_init (int memory, int numcpus)
 * Have a look at https://bugzilla.redhat.com/show_bug.cgi?id=1777210
 */
void QEMU_instance(QemuContext &ctx, const std::string instanceid, const std::string language)
{
    std::string guestid = QEMU_reservation_id(ctx);

    PushArguments(ctx, "-name", instanceid);

    PushArguments(ctx, "-watchdog", "i6300esb");
    PushArguments(ctx, "-watchdog-action", "reset");
    PushArguments(ctx, "-k", language);
    PushArguments(ctx, "-boot", "menu=off,order=cdn,strict=off"); // Boot with ISO if disk is missing.
    PushArguments(ctx, "-rtc", "base=utc,clock=host,driftfix=slew");
    PushArguments(ctx, "-monitor", m2_string_format("unix:/tmp/%s.monitor,server,nowait", guestid.c_str()));
    PushArguments(ctx, "-pidfile", m2_string_format("/tmp/%s.pid", guestid.c_str()));
    PushArguments(ctx, "-qmp", m2_string_format("unix:/tmp/%s.socket,server,nowait", guestid.c_str()));

    // This is the QEMU GUEST AGENT
    PushArguments(ctx, "-chardev", m2_string_format("socket,path=/tmp/qga-%s.socket,server,nowait,id=qga0", guestid.c_str()));
    PushArguments(ctx, "-device", "virtio-serial");
    PushArguments(ctx, "-device", "virtserialport,chardev=qga0,name=org.qemu.guest_agent.0");

    // This is the QEMU META-DATA SERVICE
    // PushArguments(ctx, "-device", "virtio-serial");
    // PushArguments(ctx, "-chardev", m2_string_format("socket,path=/tmp/qms-%s.socket,server=on,wait=off,telnet=off,id=qms0", guestid.c_str()));
    // PushArguments(ctx, "-device", "virtserialport,chardev=qms0,name=org.qemu.metadata_service.0");

    PushArguments(ctx, "-m", m2_string_format("%d", ctx.model.memory));                                                                               // memory
    PushArguments(ctx, "-smp", m2_string_format("cpus=%d,cores=%d,maxcpus=%d,threads=1,dies=1", ctx.model.cpus, ctx.model.cpus, ctx.model.cpus * 2)); // cpu setup.
    PushArguments(ctx, "-cpu", ctx.model.flags);

    // AuthenticIntel)
    // CPU="-cpu host,kvm=on,vendor=GenuineIntel,+hypervisor,+invtsc,+kvm_pv_eoi,+kvm_pv_unhalt";;
    // AuthenticAMD|*)
    //# Used in past versions: +movbe,+smep,+xgetbv1,+xsavec
    //# Warn on AMD:           +fma4,+pcid
    // CPU="-cpu Penryn,kvm=on,vendor=GenuineIntel,+aes,+avx,+avx2,+bmi1,+bmi2,+fma,+hypervisor,+invtsc,+kvm_pv_eoi,+kvm_pv_unhalt,+popcnt,+ssse3,+sse4.2,vmware-cpuid-freq=on,+xsave,+xsaveopt,check";;
}

/**
 * @brief QEMU_drive registers a drive, with a limited bandwidth 16mbps pr drive.
 *
 * @param args QemuContext
 * @param drivepath The path to the drive
 * @return int -1 on error. 0 on success.
 */
int QEMU_drive(QemuContext &args, const std::string &drivepath)
{
    int bootindex = args.drives.size(); // thats better.
    if (fileExists(drivepath))
    {
        std::string blockdevice = generateRandomPrefixedString("block", 4);
        PushDriveArgument(args, m2_string_format("if=virtio,index=%d,file=%s,media=disk,format=qcow2,cache=writeback,throttling.bps-total=16777216,id=%s", ++bootindex, drivepath.c_str(), blockdevice.c_str()));
        std::cout << "Using drive[" << bootindex << "]: " << drivepath << std::endl;
        return 0;
    }
    else
    {
        std::cerr << "The drive " << drivepath << " does not exists: " << strerror(errno) << std::endl;
        return -1;
    }
}

/**
 * @brief Qemu_bootdrive, registers a boot-drive.
 *
 * @param args QemuContext
 * @param drivepath the path to the drive.
 * @return int -1 on error. 0 on success.
 */
int QEMU_bootdrive(QemuContext &ctx, const std::string &drivepath, size_t bpstotal = 16777216)
{
    if (fileExists(drivepath))
    {
        std::string blockdevice = generateRandomPrefixedString("block", 4);
        PushDriveArgument(ctx, m2_string_format("if=virtio,index=0,file=%s,media=disk,format=qcow2,cache=writeback,throttling.bps-total=%d,id=%s", drivepath.c_str(), bpstotal, blockdevice.c_str()));
        std::cout << "Using boot-drive[0]: " << drivepath << "." << std::endl;
        ctx.rootdrive = drivepath;
        return 0;
    }
    else
    {
        std::cerr << "The drive " << drivepath << " does not exists: " << strerror(errno) << std::endl;
        return -1;
    }
}

/**
 * QEMU_allocate_drive(std::string id, ssize_t sz)
 * Will create a new image, but refuse to overwrite old ones.
 */
void QEMU_allocate_drive(std::string path, std::string sz)
{
    if (!fileExists(QEMU_DEFAULT_IMG))
    {
        std::cerr << QEMU_DEFAULT_IMG << " is missing" << std::endl;
        exit(-1);
    }

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
        left_argv.push_back(const_cast<char *>(sz.c_str()));
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
void QEMU_allocate_backed_drive(std::string path, std::string sz, std::string backingfile)
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
        left_argv.push_back(const_cast<char *>(sz.c_str()));
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
 * TODO: Make this follow full-path pattern.
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
    // PushDeviceArgument(ctx, "-snapshot"); // Deprecated in favour of the disk-backed-method
    ctx.mEphimeral = true;
    std::cout << "Using snapshot, the bootdrive will removed on exit." << std::endl;
}

/**
 * QEMU_launch()
 * This launches a hypervisor.
 */
void QEMU_launch(QemuContext &ctx, bool block, const std::string hypervisor)
{
    if (!fileExists(hypervisor))
    {
        std::cerr << hypervisor << " is missing." << std::endl;
        exit(-1);
    }

    // check to daemonize
    if (block == false)
        PushDeviceArgument(ctx, "-daemonize");

    // Allways, add these last
    PushArguments(ctx, "-device", "virtio-rng-pci"); // random
    PushArguments(ctx, "-device", "virtio-balloon");

    // Finally, we copy it into a char-array, to make it compatible with execvp and run it.
    std::vector<char *> left_argv;
    left_argv.push_back(const_cast<char *>(hypervisor.c_str()));
    left_argv.push_back(const_cast<char *>("-enable-kvm"));

    // First, we take the devices.
    std::for_each(ctx.devices.begin(), ctx.devices.end(), [&left_argv](const std::string &device)
                  { left_argv.push_back(const_cast<char *>(device.c_str())); });

    // Then we take, the drives in a orderly fasion.
    std::for_each(ctx.drives.rbegin(), ctx.drives.rend(), [&left_argv](const std::string &drive)
                  { 
                      
                      left_argv.push_back(const_cast<char *>("-drive")); 
                      left_argv.push_back(const_cast<char *>(drive.c_str())); });

    // We leave a null, to indicate EOF.
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
    std::string instanceid = QEMU_instanceid(ctx);
    std::string guestid = QEMU_reservation_id(ctx);

    // Leave a message in /tmp/reservationid.instanceid (this allows us to use QEMU_isntanceid from the reservationid)
    std::string str_instance = m2_string_format("/tmp/%s.instance", guestid.c_str());
    std::ofstream instancefile;
    instancefile.open(str_instance.c_str());
    instancefile << instanceid;
    instancefile.close();
}

/**
 * QEMU_Notify_started
 * Callback, that can be used when a hypervisor stops
 */
void QEMU_notified_exited(QemuContext &ctx)
{
    std::string guestid = QEMU_reservation_id(ctx);
    std::vector<std::string> paths;
    paths.push_back(m2_string_format("/tmp/%s.monitor", guestid.c_str()));
    paths.push_back(m2_string_format("/tmp/%s.pid", guestid.c_str()));
    paths.push_back(m2_string_format("/tmp/%s.socket", guestid.c_str()));
    paths.push_back(m2_string_format("/tmp/qga-%s.socket", guestid.c_str()));
    paths.push_back(m2_string_format("/tmp/qms-%s.socket", guestid.c_str()));
    paths.push_back(m2_string_format("/tmp/%s.cid", guestid.c_str()));
    paths.push_back(m2_string_format("/tmp/%s.instance", guestid.c_str()));

    // Should we remove the drives?
    if (ctx.mEphimeral) {
        paths.push_back(ctx.rootdrive);
    }

    std::for_each(paths.begin(), paths.end(), [](const std::string &ref)
                  {
        if (fileExists(ref)) {
            unlink(ref.c_str());
        } });
}

/**
 * QEMU_Cluod_init_arguments
 * Sets the cloud-init arguments-source
 * // serial=ds=nocloud-net;s=http://10.10.0.1:8000/
 */
void QEMU_cloud_init_network(QemuContext &ctx, const std::string instanceid, const std::string cloud_settings_src)
{
    std::string reversed = instanceid;
    reversed.reserve(); // Make it more unique.
    std::size_t str_hash = std::hash<std::string>{}(reversed);
    std::string hostname = generatePrefixedUniqueString("i", str_hash, 8);
    std::string instance = generatePrefixedUniqueString("i", str_hash, 32);

    PushArguments(ctx, "-smbios", m2_string_format("type=1,serial=ds=nocloud-net;h=%s;i=%s;s=%s", hostname.c_str(), instance.c_str(), cloud_settings_src.c_str()));
    PushArguments(ctx, "-smbios", m2_string_format("type=11,value=cloud-init:ds=nocloud-net;h=%s;i=%s;s=%s", hostname.c_str(), instance.c_str(), cloud_settings_src.c_str()));

    std::cout << "Using NoCloud-Net: hostname: " << hostname << ", instance-id: " << instance << ", cloud-init: " << cloud_settings_src << std::endl;
}

/**
 * @brief QEMU_cloud_init_default
 * sets a default cloud-init with a instance-id and a hostname.
 * value=cloud-init:ds=nocloud;
 *
 * @param ctx QemuContext
 * @param instanceid the instanceid.
 */
void QEMU_cloud_init_default(QemuContext &ctx, std::string instanceid)
{
    std::string reversed = instanceid;
    reversed.reserve(); // Make it more unique.
    std::size_t str_hash = std::hash<std::string>{}(reversed);
    std::string hostname = generatePrefixedUniqueString("i", str_hash, 8);
    std::string instance = generatePrefixedUniqueString("i", str_hash, 32);

    // Support both
    PushArguments(ctx, "-smbios", m2_string_format("type=1,serial=ds=nocloud;h=%s;i=%s", hostname.c_str(), instance.c_str()));
    PushArguments(ctx, "-smbios", m2_string_format("type=11,value=cloud-init:ds=nocloud;h=%s;i=%s", hostname.c_str(), instance.c_str()));

    std::cout << "Using NoCloud: hostname: " << hostname << ", instance-id: " << instance << std::endl;
}

/**
 * @brief QEMU_oemstring
 * Sets additional oemstrings for a vm.
 * @param ctx QemuContext
 * @param oemstrings std::vector<std::string>
 */
void QEMU_oemstring(QemuContext &ctx, std::vector<std::string> oemstrings)
{
    std::for_each(oemstrings.begin(), oemstrings.end(), [&ctx](const std::string &oemstring)
                  { PushArguments(ctx, "-smbios", m2_string_format("type=11,value=%s", oemstring.c_str())); });
}

/**
 * QEMU_Cluod_init_arguments
 * Removes any cloud-init arguments
 * serial=ds=None
 */
void QEMU_cloud_init_remove(QemuContext &ctx)
{
    PushArguments(ctx, "-smbios", "type=11,value=cloud-init:ds=None");
}

/**
 * QEMU_get_pid
 * get pid of running hypervisor.
 */
pid_t QEMU_get_pid(const std::string &reservationid)
{
    std::string str_pid = m2_string_format("/tmp/%s.pid", reservationid.c_str());
    if (!fileExists(str_pid))
    {
        return -1;
    }
    pid_t pid;
    std::ifstream pidfile;
    pidfile.open(str_pid.c_str());
    pidfile >> pid;
    pidfile.close();
    return pid;
}

/**
 * @brief QEMU_getcid.
 * Returns a CID belonging to a reservation.
 *
 * @param reservationid
 * @return unsigned int
 */
const unsigned int QEMU_getcid(const std::string &reservationid)
{
    std::string str_cid = m2_string_format("/tmp/%s.cid", reservationid.c_str());
    if (!fileExists(str_cid))
    {
        return -1;
    }
    std::ifstream cidfile;
    unsigned int cid;
    cidfile.open(str_cid.c_str());
    cidfile >> cid;
    cidfile.close();
    return cid;
}

/**
 * @brief QEMU_getcid.
 * Returns a CID belonging to a reservation.
 *
 * @param reservationid
 * @return unsigned int
 */
const std::string QEMU_instanceid(const std::string &reservationid)
{
    std::string str_instance = m2_string_format("/tmp/%s.instance", reservationid.c_str());
    std::ifstream instancefile;

    std::string instanceid;
    instancefile.open(str_instance.c_str());
    instancefile >> instanceid;
    instancefile.close();
    return instanceid;
}

std::vector<std::tuple<std::string, std::string>> QEMU_get_reservations()
{
    std::vector<std::tuple<std::string, std::string>> _vec;
    std::string path = "/tmp";
    for (const auto &entry : std::filesystem::directory_iterator(path))
    {
        std::string path = entry.path();
        if (path.find(".instance") != std::string::npos)
        {
            std::string reservation = path.substr(5, path.find(".instance") - 5);
            std::string instance = QEMU_instanceid(reservation);

            _vec.push_back(std::make_tuple(reservation, instance));
        }
    }

    return _vec;
}

/**
 * @brief QEMU_isrunning
 * Is used to determine if the instanceid is currently active.
 * 
 * @param instanceid 
 * @return true 
 * @return false 
 */
bool QEMU_isrunning(const std::string &instanceid)
{
    std::vector<std::tuple<std::string, std::string>> reservations = QEMU_get_reservations();
    auto it = std::find_if(reservations.begin(), reservations.end(), [&instanceid](std::tuple<std::string, std::string> &res) {
        if (std::get<1>(res) == instanceid ) {
           return true;
        } 
        return false;
    });

    if (it != reservations.end())
    {
        return true;
    }
    return false;
}