#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi


sudo build/qemu -headless -model c1-smaller -network default -profile dhcp instance://dhcp-1234
# build/qemu -headless -model c1-faster -network default instance://testing-1234
