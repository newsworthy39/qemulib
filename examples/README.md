# Examples

The example directory, contains different modern cpp-idioms, to use qemu-lib. 
As QEMUlib aspires, to be used in a modern cpp-context, and allow integration
with different libraries through message-passing, these examples serve as a
test-bed for qemulib as well. 

Additional test-harnesses will be added, in the future as complexity and compatibility
issues aries as a result of added possibilites.

## Main.cpp
An test-bed and tool to start VMs of a commandline. Uses JSON and Yaml to create vm-discs, 
with backing-files allowing for hierarchial storage. Single Host-only. Although a '-accept'
flag, has been added, no teleporting of VMS through a single-host and this cli-tool is available.
This tool allows for a standard built-pc, with urandom pci-devices, and zero-network configuration-tools
but can be modified to different architectures. This focuses on a debian variant and amd64-architectures.


* Hierarchical storage
* Network creation (linux-bridges, and macvtap) controlled through a registry.yaml-file.
* Uses standard models as accustomed from the cloud.
* Headless-mode, exposes and unencrypted/no-password vnc-link through loopback only.
* Machine Image registry to allow for easy vm-creation and snapshot-mode.
* Guest-tools interaction through qemu-guest-tools through most distribution packages (windows and linux)
* QMP-socket, for advanced interaction (see qemu-manage-cpp)
* "Reservations" for vm identification and management.
* vhost-networking, for near-line speed.
* Cloud-init switches for cloud-init recipes booting additional management frameworks using NoCloud-net.

## Main-metadataservice.cpp
An example of how to use VMware(r) contributed AF_VSOCK to communicate through a pci-device in a packet-manner 
ackin to berkeley sockets. Examples of AF_Vsock binaries that can be installed into a guest, and communicate through
a metadataservice allows for zero-network configuration of a guest at boot-time (assuming a binary is installed)

## Main-interfaces.cpp
An example of how to obtain a guest-ip after dhcp-allocation/pre-backed network ip using QMP-sockets and Qemus prebuilt QMP-library.
This examples uses the notion of reservations to identify and target single vm qmp-sockets for communication, and is another
example of zero-network host-to-guest communication.

## Main-powerdown.cpp
An example of how to use modern c++ idioms, from STL to iterate a vector of reservations and issue QMP-commands, controlling a vm.
In this case, to powerdown (acpi-power).

## Main-instances.cpp
An example of how to use modern c++ idioms, from STL to iterate a vector of reservations and describes, which guests are currently running.

# Build
Requires Make, g++ and the ability, to build with c++20 flag. 

Run 'Make' in the root-directory

_This builds a static-library, that used is used to link into the examples,
that serve as primitive test-bed._