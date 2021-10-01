#include <qemu-manage.hpp>

/**
 * QEMU_powerdown(QemuContext)
 * Powers down the QEMU instance, by sending a message via the QMP-socket
 */
void QEMU_powerdown(QemuContext &ctx)
{
    char buffer[4096]{0};

    int s = QEMU_OpenQMPSocket(ctx);

    std::string powerdown = "{ \"execute\": \"system_powerdown\" }";

    int t = send(s, powerdown.c_str(), powerdown.size() + 1, 0);
    sleep(1);
    int k = recv(s, buffer, 4096, 0);
    if (k != -1)
    {
        std::cout << "Received " << buffer << std::endl;
    }

    close(s);
}

void QEMU_kill(QemuContext &ctx)
{
    pid_t pid = QEMU_get_pid(ctx);
    kill(pid, SIGTERM);
}

void QEMU_reset(QemuContext &ctx)
{

    char buffer[4096]{0};

    int s = QEMU_OpenQMPSocket(ctx);

    std::string reset = "{ \"execute\": \"system_reset\" }";

    int t = send(s, reset.c_str(), reset.size() + 1, 0);
    sleep(1);
    int k = recv(s, buffer, 4096, 0);
    if (k != -1)
    {
        std::cout << "Received " << buffer << std::endl;
    }

    close(s);
}