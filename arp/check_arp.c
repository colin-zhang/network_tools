#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>
#include <netinet/ether.h>
#include <netinet/if_ether.h> 

int getarp(const char *ifn, const char *ip) {

    int s;
    struct arpreq arpreq;
    struct sockaddr_in *sin;
    unsigned char *eap;

    memset(&arpreq, 0, sizeof(arpreq));
    strcpy(arpreq.arp_dev, ifn);

    sin = (struct sockaddr_in *) &arpreq.arp_pa;
    sin->sin_family = AF_INET;
    sin->sin_addr.s_addr = inet_addr(ip);

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("socket");
        return -1;
    }
    if (ioctl(s, SIOCGARP, &arpreq) < 0) {
         return -1;
    }
    if (arpreq.arp_flags & ATF_COM) {
        return 1;
    }
    close(s);
    return 0;
}

int
check_arp_by_ip(const char *ip)
{
    struct ifaddrs *ifaddr, *ifa;
    int family, s, n;
    char host[NI_MAXHOST];
    static char ifn[64];

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }
    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_name == NULL) continue;
        if (getarp(ifa->ifa_name, ip) > 0 ) {
              freeifaddrs(ifaddr);
              return 1;
        }
    } 
    freeifaddrs(ifaddr);
    return 0;
}

int main(int argc, char *argv[])
{
    int ret = check_arp_by_ip(argv[1]);
    printf("ret = %d\n", ret);
    return 0;
}