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
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);

    std::stringstream ss;
    int i = 0, count = 0;
    //ss << std::hex;
    for (i = 0; i < 4; i++)
    {
        ss << dis(gen);
    }

    return m3_string_format("tap-%04x", ss.str());
}

std::string QEMU_Allocate_Link(std::string masterinterface)
{
    struct rtnl_link *link;
    struct nl_cache *link_cache;
    struct nl_sock *sk;
    struct nl_addr *addr;
    int err, master_index;
    const std::string mac = QEMU_Generate_Link_Mac();
    const std::string link_name =  QEMU_Generate_Link_Name();

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

    std::string tap(pchLinkName);

    return tap;
}

int QEMU_Open_Link(QemuContext &ctx, std::string tapname)
{
    // Finally, open the tap, before turning into a qemu binary, launching
    // the hypervisor.
    auto tappath = m3_string_format("/dev/tap%d", if_nametoindex(tapname.c_str()));
    int fd = open(tappath.c_str(), O_RDWR);
    if (fd == -1)
    {
        std::cerr << "Error opening network-device " << tappath << ": " << strerror(errno) << std::endl;
        exit(-1);
    }

    std::cout << "Using network-device: " << tappath << ", mac: " << getMacSys(tapname) << std::endl;
    PushArguments(ctx, "-netdev", m3_string_format("tap,id=guest0,fd=%d,vhost=on", fd));
    PushArguments(ctx, "-device", m3_string_format("virtio-net,mac=%s,netdev=guest0,id=internet-dev", getMacSys(tapname).c_str()));

    return fd;
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