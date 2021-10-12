#include <qemu-images.hpp>

std::vector<std::tuple<std::string, std::string>> drives{
        {"dhcp-", "guest-cloud-dhcp-test"},
        {"webserver-", "ubuntu2004backingfile"}
};