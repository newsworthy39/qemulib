#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <string.h>
#include <unistd.h>

int main()
{
        int s = socket(AF_VSOCK, SOCK_STREAM, 0);

        struct sockaddr_vm addr;
        memset(&addr, 0, sizeof(struct sockaddr_vm));
        addr.svm_family = AF_VSOCK;
        addr.svm_port = 9999;
        addr.svm_cid = VMADDR_CID_HOST;

        connect(s,(struct sockaddr*) &addr, sizeof(struct sockaddr));

        send(s, "Hello, world!", 13, 0);

        close(s);

        return 0;
}