#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

sudo build/qemu -headless -model c1-faster -ephimeral -network tuntap -profile ami instance://ami-1234