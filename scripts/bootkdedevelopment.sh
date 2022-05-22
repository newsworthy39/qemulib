#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

sudo build/qemu -headless -model c1-smaller -network default -profile dhcp -vol slow:16g instance://dhcp-1234
sudo build/qemu -headless -model t1-large -network default -profile ami instance://testing-1234