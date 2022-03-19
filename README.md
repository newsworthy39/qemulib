# qemulib
 Library to easily create/manage vms.

# Example directory
as QEMUlib grows, examples will include different cpp tools, that serve
both as a test-bed, and as example-documentation outside of the documentation
directory. Inside the Example-directory as additional documentation of the
individual tools/test-beds and showcases different usages of qemu-lib.


# Build
Requires Make, g++ and the ability, to build with c++20 flag. 

Run 'Make' in the root-dir.

_This builds a static-library, that used is used to link into the examples,
that serve as primitive test-bed._

# Libraries used to make examples.
* libraries/json11. See README.md and License.txt
* Netlink route 3
* Yaml CPP
* Pthread

