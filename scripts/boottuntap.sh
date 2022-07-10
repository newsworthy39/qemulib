#!/bin/bash
if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

sudo build/qemu -model t1-medium -ephimeral -network tun -profile ami instance://ami-123456
