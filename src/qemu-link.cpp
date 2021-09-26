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

std::string QEMU_Generate_Link_Name(std::string prefix, int length = 8)
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

    return m3_string_format("%s-%s", prefix.c_str(), ss.str().c_str());
}

std::string QEMU_allocate_macvtap(QemuContext &ctx, std::string masterinterface)
{
    struct rtnl_link *link;
    struct nl_cache *link_cache;
    struct nl_sock *sk;
    struct nl_addr *addr;
    int err, master_index;
    std::string mac = QEMU_Generate_Link_Mac();
    std::string link_name = QEMU_Generate_Link_Name("macvtap", 8);

    sk = nl_socket_alloc();
    if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0)
    {
        nl_perror(err, "Unable to connect socket");
        exit(-1);
    }

    if ((err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache)) < 0)
    {
        nl_perror(err, "Unable to allocate cache");
        exit(-1);
    }

    if (!(master_index = rtnl_link_name2i(link_cache, masterinterface.c_str())))
    {
        fprintf(stderr, "Unable to lookup enp2s0");
        exit(-1);
    }

    link = rtnl_link_macvtap_alloc();
    rtnl_link_set_link(link, master_index);
    rtnl_link_set_flags(link, IFF_UP);

    addr = nl_addr_build(AF_LLC, ether_aton(mac.c_str()), ETH_ALEN);
    rtnl_link_set_addr(link, addr);

    nl_addr_put(addr);

    rtnl_link_macvtap_set_mode(link, rtnl_link_macvtap_str2mode("vepa"));
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

    std::cout << "Using network-device: " << link_name << ", mac: " << mac << std::endl;
    PushArguments(ctx, "-netdev", m3_string_format("tap,id=guest0,fd=%d,vhost=on", fd));
    PushArguments(ctx, "-device", m3_string_format("virtio-net,mac=%s,netdev=guest0,id=internet-dev", mac.c_str()));

    return link_name;
}

static bool has_vnet_hdr(int fd)
{
    unsigned int features = 0;

    if (ioctl(fd, TUNGETFEATURES, &features) == -1)
    {
        return false;
    }

    if (!(features & IFF_VNET_HDR))
    {
        return false;
    }

    return true;
}

int tun_alloc(char *dev)
{
    int ctlfd = socket(AF_INET, SOCK_STREAM, 0);

    struct ifreq ifr;
    int fd, err;

    if ((fd = open("/dev/net/tun", O_RDWR)) < 0)
        return 0;

    memset(&ifr, 0, sizeof(ifr));

    /* request a tap device, disable PI, and add vnet header support if
     * requested and it's available. 
    /* Flags: IFF_TUN   - TUN device (no Ethernet headers)
     *        IFF_TAP   - TAP device
     *
     *        IFF_NO_PI - Do not provide packet information
     */

    ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
    if (has_vnet_hdr(fd))
    {
        ifr.ifr_flags |= IFF_VNET_HDR;
    }

    if (*dev)
        strncpy(ifr.ifr_name, dev, IFNAMSIZ);

    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0)
    {
        close(fd);
        return err;
    }

    strcpy(dev, ifr.ifr_name);

    /* Linux uses the lowest enslaved MAC address as the MAC address of
     * the bridge.  Set MAC address to a high value so that it doesn't
     * affect the MAC address of the bridge.
     */
    if (ioctl(ctlfd, SIOCGIFHWADDR, &ifr) < 0)
    {
        fprintf(stderr, "failed to get MAC address of device `%s': %s\n",
                dev, strerror(errno));
        close(ctlfd);
        exit(-1);
    }
    ifr.ifr_hwaddr.sa_data[0] = 0xFE;
    ifr.ifr_hwaddr.sa_data[1] = 0x52;

    if (ioctl(ctlfd, SIOCSIFHWADDR, &ifr) < 0)
    {
        fprintf(stderr, "failed to set MAC address of device `%s': %s\n",
                dev, strerror(errno));
        close(ctlfd);
        exit(-1);
    }

    close(ctlfd);

    return fd;
}

void QEMU_enslave_interface(std::string bridge, std::string interface)
{
    struct nl_sock *sock;
    struct nl_cache *link_cache;
    struct rtnl_link *master, *slave;
    int err;

    sock = nl_socket_alloc();
    if ((err = nl_connect(sock, NETLINK_ROUTE)) < 0)
    {
        nl_perror(err, "Unable to connect socket");
        exit(-1);
    }

    if ((err = rtnl_link_alloc_cache(sock, AF_UNSPEC, &link_cache)) < 0)
    {
        nl_perror(err, "Unable to allocate cache");
        exit(-1);
    }

    if (!(master = rtnl_link_get_by_name(link_cache, bridge.c_str())))
    {
        fprintf(stderr, "Unknown bridge-link: %s\n", bridge.c_str());
        exit(-1);
    }

    if (!(slave = rtnl_link_get_by_name(link_cache, interface.c_str())))
    {
        fprintf(stderr, "Unknown device-link: %s\n", interface.c_str());
        exit(-1);
    }

    if ((err = rtnl_link_enslave(sock, master, slave)) < 0)
    {
        fprintf(stderr, "Unable to if_enslave %s to %s: %s\n",
                interface.c_str(), bridge.c_str(), nl_geterror(err));
        exit(-1);
    }

    nl_close(sock);
}

int skfd = -1; /* AF_INET socket for ioctl() calls.*/
int set_if_flags(char *ifname, short flags)
{
    struct ifreq ifr;
    int res = 0;

    ifr.ifr_flags = flags;
    strncpy(ifr.ifr_name, ifname, IFNAMSIZ);

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("socket error %s\n", strerror(errno));
        res = 1;
        goto out;
    }

    res = ioctl(skfd, SIOCSIFFLAGS, &ifr);
    if (res < 0)
    {
        printf("Interface '%s': Error: SIOCSIFFLAGS failed: %s\n",
               ifname, strerror(errno));
    }
    else
    {
        printf("Interface '%s': flags set to %04X.\n", ifname, flags);
    }
out:
    return res;
}
int QEMU_link_up(char *ifname, short flags)
{
    return set_if_flags(ifname, flags | IFF_UP);
}

void QEMU_set_namespace(std::string namespace_path)
{
    int nfd = open(namespace_path.c_str(), O_RDONLY);
    setns(nfd, CLONE_NEWNET);
    close(nfd);
}

void QEMU_set_default_namespace()
{
    int ofd = open("/var/run/netns/default", O_RDONLY);
    setns(ofd, CLONE_NEWNET);
    close(ofd);
}

std::string QEMU_allocate_tun(QemuContext &ctx)
{

    char dev[64] = {};
    int fd = tun_alloc(&dev[0]);
    if (fd == 0)
    {
        std::cerr << "Could not allocate tun" << std::endl;
        exit(-1);
    }

    QEMU_link_up(&dev[0], 1);
    std::string mac = QEMU_Generate_Link_Mac();

    // Thanks to, https://www.linuxquestions.org/questions/programming-9/linux-determining-mac-address-from-c-38217/:
    // int s;
    // struct ifreq buffer;
    // s = socket(PF_INET, SOCK_DGRAM, 0);
    // memset(&buffer, 0x00, sizeof(buffer));
    // strcpy(buffer.ifr_name, dev);
    // ioctl(s, SIOCGIFHWADDR, &buffer);
    // close(s);

    // char mac[20];
    // sprintf(&mac[0], "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
    //         (unsigned char)buffer.ifr_hwaddr.sa_data[0],
    //         (unsigned char)buffer.ifr_hwaddr.sa_data[1],
    //         (unsigned char)buffer.ifr_hwaddr.sa_data[2],
    //         (unsigned char)buffer.ifr_hwaddr.sa_data[3],
    //         (unsigned char)buffer.ifr_hwaddr.sa_data[4],
    //         (unsigned char)buffer.ifr_hwaddr.sa_data[5]);

    std::cout << "Using network-device: /dev/net/tun" << std::endl;
    PushArguments(ctx, "-netdev", m3_string_format("tap,id=guest1,fd=%d,vhost=on", fd));
    PushArguments(ctx, "-device", m3_string_format("virtio-net-pci,netdev=guest1,mac=%s", mac.c_str()));
    return std::string(dev);
}

std::string QEMU_allocate_tun1(QemuContext &ctx)
{

    // Thanks to, https://www.linuxquestions.org/questions/programming-9/linux-determining-mac-address-from-c-38217/:
    std::string tapdevice = QEMU_Generate_Link_Name("tap", 4);
    std::string mac = QEMU_Generate_Link_Mac();

    std::cout << "Using network-device: /dev/net/tun" << std::endl;
    PushArguments(ctx, "-netdev", m3_string_format("tap,id=guest1,ifname=%s,vhost=on,downscript=no,script=no", tapdevice.c_str()));
    PushArguments(ctx, "-device", m3_string_format("virtio-net-pci,netdev=guest1,mac=%s", mac.c_str()));
    return tapdevice;
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
