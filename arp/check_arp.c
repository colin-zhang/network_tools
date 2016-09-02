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

int getarp(const char *ifn, const char *ip) 
{
    int s = 0, res = 0;
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
         res = -1;
         goto ret;
    }
    if (arpreq.arp_flags & ATF_COM) {
        res = 1;
        goto ret;
    }
ret:
    close(s);
    return res;
}

int
check_arp_by_ip(const char *ip, const char *ifn_cmp)
{
    struct ifaddrs *ifaddr, *ifa;
    int family, s, n;
    char host[NI_MAXHOST];
    int len;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return -1;
    }

    len = strlen(NULL == ifn_cmp ? "\0" : ifn_cmp);

    for (ifa = ifaddr, n = 0; ifa != NULL; ifa = ifa->ifa_next, n++) {
        if (ifa->ifa_name != NULL) {
            if (len > 0 && strlen(ifa->ifa_name) >= len) {
                if (memcmp(ifn_cmp, ifa->ifa_name, len) != 0) continue;
            }
            if (getarp(ifa->ifa_name, ip) > 0 ) {
                  freeifaddrs(ifaddr);
                  return 1;
            }
        } 
    }
    freeifaddrs(ifaddr);
    return 0;
}

int main(int argc, char *argv[])
{
    int ret = check_arp_by_ip(argv[1], "");
    printf("ret = %d\n", ret);
    return 0;
}