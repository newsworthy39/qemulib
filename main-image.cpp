#include <string>
#include <fstream>
#include <streambuf>
#include <iostream>
#include <qemu-hypervisor.hpp>
#include <qemu-context.hpp>
#include <qemu-images.hpp>

int main(int argc, char *argv[])
{

  std::string database = QEMU_DEFAULT_IMAGEDB;
  bool verbose = false;

  for (int i = 1; i < argc; ++i)
  { // Remember argv[0] is the path to the program, we want from argv[1] onwards

    if (std::string(argv[i]).find("-h") != std::string::npos)
    {
      std::cout << "Usage(): " << argv[0] << " (-h) (-v) -d {default=" << QEMU_DEFAULT_IMAGEDB << "} " << std::endl;
      exit(-1);
    }

    if (std::string(argv[i]).find("-v") != std::string::npos)
    {
      verbose = true;
    }

    if (std::string(argv[i]).find("-d") != std::string::npos && (i + 1 < argc))
    {
      database = argv[i + 1];
    }
  }

  std::map<std::string, std::string> images = qemuListImages(database);
  for (std::map<std::string, std::string>::iterator it = images.begin(); it != images.end(); it++)
  {
    std::cout << "Image " << it->first << ", " << it->second << std::endl;
  }
}
