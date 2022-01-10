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

std::string generateRandomMACAddress()
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

int QEMU_allocate_bridge(std::string bridge)
{
    struct rtnl_link *link;
    struct nl_cache *link_cache;
    struct nl_sock *sk;
    struct rtnl_link *ltap;
    int err;
    sk = nl_socket_alloc();
    if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0)
    {
        nl_perror(err, "Unable to connect socket");
        return err;
    }

    if ((err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache)) < 0)
    {
        nl_perror(err, "Unable to allocate cache");
        return err;
    }

    link = rtnl_link_alloc();
    if ((err = rtnl_link_set_type(link, "bridge")) < 0)
    {
        rtnl_link_put(link);
        return err;
    }
    rtnl_link_set_name(link, bridge.c_str());

    if ((err = rtnl_link_add(sk, link, NLM_F_CREATE)) < 0)
    {
        nl_perror(err, "Unable to allocate cache");
        return err;
    }
    rtnl_link_put(link);

    return 0;
}

std::string QEMU_allocate_macvtap(QemuContext &ctx, std::string masterinterface)
{
    struct rtnl_link *link;
    struct nl_cache *link_cache;
    struct nl_sock *sk;
    struct nl_addr *addr;
    int err, master_index;
    std::string guestId = generateRandomPrefixedString("guest", 4);
    std::string mac = generateRandomMACAddress();
    std::string link_name = generateRandomPrefixedString("macvtap", 8);

    sk = nl_socket_alloc();
    if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0)
    {
        nl_perror(err, "Unable to connect socket");
        exit(EXIT_FAILURE);
    }

    if ((err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache)) < 0)
    {
        nl_perror(err, "Unable to allocate cache");
        exit(EXIT_FAILURE);
    }

    if (!(master_index = rtnl_link_name2i(link_cache, masterinterface.c_str())))
    {
        fprintf(stderr, "Unable to lookup enp2s0");
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    std::cout << "Using network-device: " << link_name << ", mac: " << mac << std::endl;
    PushArguments(ctx, "-netdev", m3_string_format("tap,id=%s,fd=%d,vhost=on", guestId.c_str(), fd));
    PushArguments(ctx, "-device", m3_string_format("virtio-net,mac=%s,netdev=%s", mac.c_str(), guestId.c_str()));

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

int QEMU_tun_allocate(const std::string device)
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

    strncpy(ifr.ifr_name, device.c_str(), IFNAMSIZ);

    if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0)
    {
        close(fd);
        return err;
    }

    /* Linux uses the lowest enslaved MAC address as the MAC address of
     * the bridge.  Set MAC address to a high value so that it doesn't
     * affect the MAC address of the bridge.
     */
    if (ioctl(ctlfd, SIOCGIFHWADDR, &ifr) < 0)
    {
        std::cerr << "failed to get MAC address of device " << device.c_str() << strerror(errno);
        close(ctlfd);
        exit(EXIT_FAILURE);
    }

    ifr.ifr_hwaddr.sa_data[0] = 0xFE;
    ifr.ifr_hwaddr.sa_data[1] = 0x52;

    if (ioctl(ctlfd, SIOCSIFHWADDR, &ifr) < 0)
    {
        std::cerr << "failed to set MAC address of device " << device.c_str() << strerror(errno);
        close(ctlfd);
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    if ((err = rtnl_link_alloc_cache(sock, AF_UNSPEC, &link_cache)) < 0)
    {
        nl_perror(err, "Unable to allocate cache");
        exit(EXIT_FAILURE);
    }

    if (!(master = rtnl_link_get_by_name(link_cache, bridge.c_str())))
    {
        fprintf(stderr, "Unknown bridge-link: %s\n", bridge.c_str());
        exit(EXIT_FAILURE);
    }

    if (!(slave = rtnl_link_get_by_name(link_cache, interface.c_str())))
    {
        fprintf(stderr, "Unknown device-link: %s\n", interface.c_str());
        exit(EXIT_FAILURE);
    }

    if ((err = rtnl_link_enslave(sock, master, slave)) < 0)
    {
        fprintf(stderr, "Unable to if_enslave %s to %s: %s\n",
                interface.c_str(), bridge.c_str(), nl_geterror(err));
        exit(EXIT_FAILURE);
    }

    nl_close(sock);
}


int set_if_flags(const char *ifname, short flags)
{
    struct ifreq ifr;
    int res = 0;
    int skfd = -1; /* AF_INET socket for ioctl() calls.*/

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
        std::cout << "Interface " << ifname << ": flags set to " << flags << std::endl;
    }
out:
    return res;
}

std::string QEMU_get_interface_cidr(const std::string device)
{
    struct ifreq ifr;
    int res = 0;
    int skfd = -1; /* AF_INET socket for ioctl() calls.*/
    
    strncpy(ifr.ifr_name, device.c_str(), IFNAMSIZ);

    if ((skfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        printf("socket error %s\n", strerror(errno));
        res = 1;
    }

    /* I want to get an IPv4 IP address */
    ifr.ifr_addr.sa_family = AF_INET;

    /* I want IP address attached to "eth0" */
    strncpy(ifr.ifr_name, device.c_str(), IFNAMSIZ - 1);

    ioctl(skfd, SIOCGIFADDR, &ifr);

    close(skfd);

    /* display result */
    std::string ip = inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);

    return ip;
}

/**
 * @brief QEMU_set_interface_cidr
 * Sets the network cidr. Also. Its shit. Never mind.
 *
 * @param name the interface.
 * @param cidr the cidr.
 */
void QEMU_set_interface_cidr(const std::string device, const std::string cidr)
{

    std::string net;
    std::string address = cidr.substr(0, cidr.find("/"));
    std::string netmask = cidr.substr(cidr.find("/") + 1);
    if (netmask == "24")
    {
        net = "255.255.255.0";
    }
    if (netmask == "16")
    {
        net = "255.255.0.0";
    }
    if (netmask == "8")
    {
        net = "255.0.0.0";
    }

    struct ifreq ifr;
    int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);

    strncpy(ifr.ifr_name, device.c_str(), IFNAMSIZ);

    ifr.ifr_addr.sa_family = AF_INET;
    struct sockaddr_in *addr = (struct sockaddr_in *)&ifr.ifr_addr;
    inet_pton(AF_INET, address.c_str(), &addr->sin_addr);
    ioctl(fd, SIOCSIFADDR, &ifr);

    inet_pton(AF_INET, net.c_str(), &addr->sin_addr);
    ioctl(fd, SIOCSIFNETMASK, &ifr);

    ioctl(fd, SIOCGIFFLAGS, &ifr);
    strncpy(ifr.ifr_name, device.c_str(), IFNAMSIZ);
    ifr.ifr_flags |= (IFF_UP | IFF_RUNNING);

    ioctl(fd, SIOCSIFFLAGS, &ifr);
}

int QEMU_link_up(const std::string ifname)
{
    return set_if_flags(ifname.c_str(), 1 | IFF_UP);
}

void QEMU_set_namespace(std::string namespace_path)
{
    int nfd = open(namespace_path.c_str(), O_RDONLY);
    setns(nfd, CLONE_NEWNET);
    close(nfd);
}

/*
 * QEMU_set_Default_namespace()
 * Changes back to the root namespace.
 * DISCLAIMER: This only works, when PID namespaces is not in use!
 */
void QEMU_set_default_namespace()
{
    int ofd = open("/proc/1/ns/net", O_RDONLY);
    setns(ofd, CLONE_NEWNET);
    close(ofd);
}

std::string QEMU_allocate_tun(QemuContext &ctx)
{
    std::string guestId = generateRandomPrefixedString("guest", 4);
    std::string tapdevice = generateRandomPrefixedString("tap", 4);
    int fd = QEMU_tun_allocate(tapdevice.c_str());
    if (fd == 0)
    {
        std::cerr << "Could not allocate tun" << std::endl;
        exit(EXIT_FAILURE);
    }

    QEMU_link_up(tapdevice);

    // Inside mac, is best served, by NOT being, the outside mac.
    std::string mac = generateRandomMACAddress();

    std::cout << "Using network-device: /dev/net/tun (mac: " << mac << ")" << std::endl;
    PushArguments(ctx, "-netdev", m3_string_format("tap,id=%s,fd=%d,vhost=on", guestId.c_str(), fd));
    PushArguments(ctx, "-device", m3_string_format("virtio-net-pci,netdev=%s,mac=%s", guestId.c_str(), mac.c_str()));

    return tapdevice;
}

void QEMU_delete_link(QemuContext &ctx, std::string interface)
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

/**
 * @brief QEMU_OpenQMPSocket. Opens a socket to the qemu guest monitor
 *
 * @param ctx QemuContext.
 * @return int a open socket.
 */
int QEMU_OpenQMPSocket(QemuContext &ctx)
{
    std::string guestid = QEMU_reservation_id(ctx); // Returns an UUIDv4.
    return QEMU_OpenQMPSocketFromPath(guestid);
}

/**
 * @brief QEMU_OpenQMPSocket. Opens a socket to the qemu guest monitor
 *
 * @param std::string reservationid
 * @return int a open socket.
 */
int QEMU_OpenQMPSocketFromPath(std::string &reservationid)
{
    // Connect to unix-qmp-socket
    int s, t, len;
    struct sockaddr_un remote;
    char str[4096];

    std::string str_qmp = m3_string_format("/tmp/%s.socket", reservationid.c_str());

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, str_qmp.c_str());
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(s, (struct sockaddr *)&remote, len) == -1)
    {
        std::cerr << "Could not connect to QMP-socket: " << str_qmp << std::endl;
        exit(EXIT_FAILURE);
    }
    t = send(s, "{ \"execute\": \"qmp_capabilities\"} ", strlen("{ \"execute\": \"qmp_capabilities\"} "), 0);
    t = recv(s, str, 4096, 0);
    return s;
}

int QEMU_OpenQGASocketFromPath(std::string &guestid)
{
    // Connect to unix-qmp-socket
    int s, t, len;
    struct sockaddr_un remote;
    char str[4096];

    std::string str_qmp = m3_string_format("/tmp/qga-%s.socket", guestid.c_str());

    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, str_qmp.c_str());
    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(s, (struct sockaddr *)&remote, len) == -1)
    {
        std::cerr << "Could not connect to QMP-socket: " << str_qmp << std::endl;
        exit(EXIT_FAILURE);
    }
    // t = send(s, "{ \"execute\": \"qmp_capabilities\"} ", strlen("{ \"execute\": \"qmp_capabilities\"} "), 0);
    // t = recv(s, str, 4096, 0);
    return s;
}
