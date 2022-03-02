#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

build/qemu -headless -model c1-smaller -network default instance://dhcp-1234
# build/qemu -headless -model c1-faster -network default instance://testing-1234
