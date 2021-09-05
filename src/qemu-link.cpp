#include <qemu-link.hpp>

std::string allocateLink(std::string masterinterface)
{
    struct rtnl_link *link;
    struct nl_cache *link_cache;
    struct nl_sock *sk;
    struct nl_addr *addr;
    int err, master_index;

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

    addr = nl_addr_build(AF_LLC, ether_aton("00:11:22:33:44:55"), ETH_ALEN);
    rtnl_link_set_addr(link, addr);

    nl_addr_put(addr);

    rtnl_link_macvtap_set_mode(link, rtnl_link_macvtap_str2mode("bridge"));
    rtnl_link_set_name(link, "test00");

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

void deleteLink(std::string interface)
{
}