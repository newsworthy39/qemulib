#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

sudo build/qemu -headless -model c1-smaller -network default -profile dhcp instance://dhcp-1234
sudo build/qemu -headless -model c1-faster -snapshot -network default -profile ami  instance://ami-1234
sudo build/qemu -headless -model c1-faster -snapshot -network default -profile ami  instance://ami-1235
sudo build/qemu -headless -model c1-faster -snapshot -network default -profile ami  instance://ami-1236