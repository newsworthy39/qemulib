#include <qemu-images.hpp>

std::vector<std::tuple<std::string, std::string>> drives{
        {"meta-", "ubuntu2004backingfile"},
        {"vip-", "ubuntu2004backingfile"},
        {"test-", "ubuntu2004backingfile"},
        {"webserver-", "ubuntu2004backingfile"},
        {"ubuntu2004-master", "ubuntu2004backingfile"},
        {"ubuntu2004-redis-", "ubuntu2004-redis"},
        {"ubuntu2004-redis", "ubuntu2004backingfile"},
        {"ubuntu2004-", "ubuntu2004-master"},        
};
