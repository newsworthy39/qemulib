#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

sudo build/qemu -headless -model c1-faster -network macvtap -profile wgclient instance://wgclient-123456
