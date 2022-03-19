
#include <qemu-metadataservice.hpp>
#include <linux/vm_sockets.h>
#include <sys/socket.h>
#include <iostream>
#include <chrono>
#include <thread>

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

int main(int argc, char *argv[])
{
    bool verbose = false;

    std::string usage = m3_string_format("usage(): %s (-help) ", argv[0]);

    for (int i = 1; i < argc; ++i)
    { // Remember argv[0] is the path to the program, we want from argv[1] onwards

        if (std::string(argv[i]).find("-help") != std::string::npos)
        {
            std::cout << usage << std::endl;
            exit(EXIT_FAILURE);
        }

        if (std::string(argv[i]).find("-v") != std::string::npos)
        {
            verbose = true;
        }
    }

    char buffer[4096];
    int s = socket(AF_VSOCK, SOCK_STREAM, 0);

    struct sockaddr_vm addr;
    memset(&addr, 0, sizeof(struct sockaddr_vm));
    addr.svm_family = AF_VSOCK;
    addr.svm_port = 9999;
    addr.svm_cid = VMADDR_CID_ANY;

    bind(s, (struct sockaddr *)&addr, sizeof(struct sockaddr_vm));
    listen(s, 0);

    while (true)
    {
        char buf[4096] = {0};
        size_t msg_len;
        struct sockaddr_vm peer_addr;
        socklen_t peer_addr_size = sizeof(struct sockaddr_vm);
        int peer_fd = accept(s, (struct sockaddr *)&peer_addr, &peer_addr_size);

        while (0 < (msg_len = recv(peer_fd, &buf, 4096, 0)))
        {
            // First we find the reservation,.
            std::vector<std::tuple<std::string,std::string>> reservations = QEMU_get_reservations();
            std::vector<std::tuple<std::string,std::string>>::iterator found = std::find_if(reservations.begin(), reservations.end(), [&peer_addr](const std::tuple<std::string, std::string> &reservation)
                                                                    { return QEMU_getcid(std::get<0>(reservation)) == peer_addr.svm_cid; });

            std::string output = m3_string_format("{ \"reservation\": \"%s\" }", std::get<0>(*found).c_str());

            std::cout << "Received " << msg_len << ", " << buf << " from reservation " << std::get<0>(*found) << std::endl;
            ;

            size_t bytes = send(peer_fd, output.c_str(), output.size(), 0);
        }
        using namespace std::chrono_literals;

        std::this_thread::sleep_for(10ms);
    }
    return EXIT_SUCCESS;
}
