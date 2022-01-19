#include <qemu-manage.hpp>

/**
 * QEMU_powerdown(QemuContext)
 * Powers down the QEMU instance, by sending a message via the QMP-socket
 */
void QEMU_powerdown(std::string &reservationid)
{
    char buffer[4096]{0};

    int s = QEMU_OpenQMPSocketFromPath(reservationid);

    std::string powerdown = "{ \"execute\": \"system_powerdown\" }\0";

    int t = send(s, powerdown.c_str(), powerdown.size(), 0);

    sleep(1);

    if (-1 == recv(s, buffer, 4096, 0))
    {
        std::cerr << "Error, when sending powerdown to " << reservationid << std::endl;
    }

    close(s);
}

void QEMU_kill(std::string &reservationid)
{
    pid_t pid = QEMU_get_pid(reservationid);
    kill(pid, SIGTERM);
}

void QEMU_reset(std::string &reservationid)
{
    char buffer[4096]{0};

    int s = QEMU_OpenQMPSocketFromPath(reservationid);

    std::string reset = "{ \"execute\": \"system_reset\" }";

    int t = send(s, reset.c_str(), reset.size() + 1, 0);
    sleep(1);
    if (-1 == recv(s, buffer, 4096, 0))
    {
        std::cerr << "Error, when sending system_reset to " << reservationid << std::endl;
    }

    close(s);
}

/**
 * @brief fetches the network configuration from a guest, using
 * its guestid.
 *
 * @param guestid
 */
std::string QEMU_qga_qinterfaces(std::string &reservationid)
{
    char buffer[4096]{0};

    int s = QEMU_OpenQGASocketFromPath(reservationid);

    std::string interfaces = "{ \"execute\": \"guest-network-get-interfaces\" }";

    int t = send(s, interfaces.c_str(), interfaces.size(), 0);
    sleep(1);
    if (-1 == recv(s, buffer, 4096, 0))
    {
        std::cerr << "Error, when querying interfaces from " << reservationid << std::endl;
    }
    close(s);

    return buffer;
}
