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

std::string getMacSys(std::string tapname)
{
    std::string mac;
    ;
    auto tapfile = m3_string_format("/sys/class/net/%s/address", tapname.c_str());
    std::ifstream myfile(tapfile.c_str());
    if (myfile.is_open())
    {                  // always check whether the file is open
        myfile >> mac; // pipe file's content into stream
    }

    return mac;
}

std::string QEMU_Generate_Link_Mac()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    int i = 0, count = 0;
    ss << std::hex;
    ss << "02:";
    for (i = 0; i < 10; i++)
    {
        ss << dis(gen);
        if (++count == 2 && i < 8)
        {
            ss << ":";
            count = 0;
        }
    }

    return ss.str();
}

std::string QEMU_Generate_Link_Name()
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

    return m3_string_format("tap-%s", ss.str().c_str());
}

std::string QEMU_allocate_macvtap(QemuContext &ctx, std::string masterinterface)
{
    struct rtnl_link *link;
    struct nl_cache *link_cache;
    struct nl_sock *sk;
    struct nl_addr *addr;
    int err, master_index;
    std::string mac = QEMU_Generate_Link_Mac();
    std::string link_name = QEMU_Generate_Link_Name();

    sk = nl_socket_alloc();
    if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0)
    {
        nl_perror(err, "Unable to connect socket");
    }

    if ((err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache)) < 0)
    {
        nl_perror(err, "Unable to allocate cache");
    }

    if (!(master_index = rtnl_link_name2i(link_cache, masterinterface.c_str())))
    {
        fprintf(stderr, "Unable to lookup enp2s0");
    }

    link = rtnl_link_macvtap_alloc();
    rtnl_link_set_link(link, master_index);
    rtnl_link_set_flags(link, IFF_UP);

    addr = nl_addr_build(AF_LLC, ether_aton(mac.c_str()), ETH_ALEN);
    rtnl_link_set_addr(link, addr);

    nl_addr_put(addr);

    rtnl_link_macvtap_set_mode(link, rtnl_link_macvtap_str2mode("bridge"));
    rtnl_link_set_name(link, link_name.c_str());

    if ((err = rtnl_link_add(sk, link, NLM_F_CREATE)) < 0)
    {
        nl_perror(err, "Unable to add link");
    }

    rtnl_link_put(link);
    nl_close(sk);

    char *pchLinkName = rtnl_link_get_name(link);
    if (NULL == pchLinkName)
    {
        /* error */
        printf("rtnl_link_get_name failed\n");
    }

    // Finally, open the tap, before turning into a qemu binary, launching
    // the hypervisor.
    auto tappath = m3_string_format("/dev/tap%d", if_nametoindex(pchLinkName));
    int fd = open(tappath.c_str(), O_RDWR);
    if (fd == -1)
    {
        std::cerr << "Error opening network-device " << tappath << ": " << strerror(errno) << std::endl;
        exit(-1);
    }

    std::cout << "Using network-device: " << link_name << ", mac: " << getMacSys(link_name) << std::endl;
    PushArguments(ctx, "-netdev", m3_string_format("tap,id=guest0,fd=%d,vhost=on", fd));
    PushArguments(ctx, "-device", m3_string_format("virtio-net,mac=%s,netdev=guest0,id=internet-dev", getMacSys(link_name).c_str()));

    return link_name;
}

void QEMU_allocate_tun(QemuContext &ctx, std::string bridge)
{
    std::cout << "Using network-device: /dev/net/tun" << std::endl;
    PushArguments(ctx, "-netdev", m3_string_format("tap,id=guest0,script=no,downscript=no,br=%s,vhost=on", bridge));
    PushArguments(ctx, "-device", "virtio-net,netdev=guest0,id=internet-dev"); //, getMacSys(dest).c_str()));
}

void QEMU_Close_Link(QemuContext &ctx, int linkfd)
{
    close(linkfd);
}

void QEMU_Delete_Link(QemuContext &ctx, std::string interface)
{
    struct rtnl_link *link;
    struct nl_sock *sk;
    int err;

    sk = nl_socket_alloc();
    if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0)
    {
        nl_perror(err, "Unable to connect socket");
    }

    link = rtnl_link_alloc();
    rtnl_link_set_name(link, interface.c_str());

    if ((err = rtnl_link_delete(sk, link)) < 0)
    {
        nl_perror(err, "Unable to delete link");
    }

    rtnl_link_put(link);
    nl_close(sk);
}

int QEMU_OpenQMPSocket(QemuContext &ctx)
{
    // Connect to unix-qmp-socket
    int s, t, len;
    struct sockaddr_un remote;
    char str[4096];

    std::string guestid = QEMU_Guest_ID(ctx); // Returns an UUIDv4.
    std::string str_qmp = m3_string_format("/tmp/%s.socket", guestid.c_str());

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(1); // TODO: Gracefull handling, of error.
    }

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, str_qmp.c_str());
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(s, (struct sockaddr *)&remote, len) == -1)
    {
        std::cerr << "Could not connect to QMP-socket: " << str_qmp << std::endl;
        exit(-1);
    }
    t = send(s, "{ \"execute\": \"qmp_capabilities\"} ", strlen("{ \"execute\": \"qmp_capabilities\"} "), 0);
    t = recv(s, str, 4096, 0);
    return s;
}