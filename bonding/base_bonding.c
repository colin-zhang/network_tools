#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>

#include <netdb.h>
#include <ifaddrs.h>
#include <linux/if_link.h>


#define UWARE_DEFAULT_IP_DOMAIN "192.168.168.0"
#define BOND_SYSFS(d) "/sys/class/net/%s/bonding/"#d

#define DEBUG_PRINT(fmt, args...)  fprintf(stderr, "DBG:%s(%d)-%s: "fmt"\n", __FILE__, __LINE__, __FUNCTION__, ##args);

static char dev_bond_ip[16] = {0};

static int miimon = 0;

static int 
dev_bond_if_exist(const char *ifn)
{
    struct ifaddrs *ifaddr, *ifa;
    int  n, ret;

    if (getifaddrs(&ifaddr) == -1) {
        DEBUG_PRINT("%s", strerror(errno));
        return -1;
    }

    for (ifa = ifaddr, ret = 1; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_name == NULL)
            continue;
        if (strcmp(ifa->ifa_name, ifn) == 0) {
            ret = 0;
            break;
        }
    }

    freeifaddrs(ifaddr);
    return ret;
}


static int 
dev_bond_if_up_down(const char *ethNum, int up)  
{  
    struct ifreq ifr;  
    int sockfd;  

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {  
        DEBUG_PRINT("%s", strerror(errno));
        return -1;    
    }  

    strcpy(ifr.ifr_name, ethNum);  
    if (ioctl(sockfd, SIOCGIFFLAGS, &ifr) < 0) {  
        DEBUG_PRINT("%s", strerror(errno));  
        close(sockfd);  
        return -1;    
    }

    if (up) {
        ifr.ifr_flags |= IFF_UP;  
    } else {
        ifr.ifr_flags &= ~IFF_UP;  
    }
    
    if (ioctl(sockfd, SIOCSIFFLAGS, &ifr) < 0) {  
        DEBUG_PRINT("%s", strerror(errno)); 
        close(sockfd);  
        return -1;    
    }  

    close(sockfd);  
    return 1;  
}  


static int 
dev_bond_set_if_ip(const char *ifn, const char *ipaddr) 
{  
    int fd;  
    struct sockaddr_in addr;  
    struct ifreq ifr;  

    if (ipaddr == NULL || ifn == NULL) {
        return -1;  
    }

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0 )) == -1) {  
        DEBUG_PRINT("%s", strerror(errno)); 
        return -1;  
    }  

    bzero(&addr, sizeof(addr));  
    addr.sin_family = AF_INET;  
    inet_pton(AF_INET, ipaddr, &addr.sin_addr);

    bzero(&ifr, sizeof(ifr));  
    memcpy(&ifr.ifr_addr, &addr, sizeof(addr));  
    strncpy(ifr.ifr_name, ifn, sizeof(ifr.ifr_name));     

    if (ioctl(fd, SIOCSIFADDR, &ifr) < 0 ) {  
        DEBUG_PRINT("%s", strerror(errno));
        close(fd); 
        return -1;  
    }  

    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;  

    if (ioctl(fd, SIOCSIFFLAGS, &ifr) < 0 ) {  
        DEBUG_PRINT("%s", strerror(errno));
        close(fd); 
        return -1;  
    }  

    close(fd);  
    return 0;  
} 


static char*
dev_bond_get_bond_ip(int slot_id)
{
    int a, b, c, d;
    sscanf(UWARE_DEFAULT_IP_DOMAIN, "%d.%d.%d.%d", &a, &b, &c, &d);
    snprintf(dev_bond_ip, sizeof(dev_bond_ip), "%d.%d.%d.%d", a, b, c, d + slot_id);
    return dev_bond_ip;
}


static int
dev_bond_write_sysfs(const char* which, const char *ifn, const char *value)
{
    int fd = 0, ret;
    char path[256] = {0};

    snprintf(path, sizeof(path), which, ifn);
#if 0
    FILE *fp = NULL;
    fp = fopen(path, "w");
    if (fp != NULL) {
        fprintf(fp, "%s", value);\
        fflush(fp);
        fclose(fp);
        return 0;
    }

    return -1;
#else
    fd = open(path, O_WRONLY);
    if (fd < 0) {
        DEBUG_PRINT("path=%s, %s ", path, strerror(errno));
        return -1; 
    }

    ret = write(fd, value, strlen(value));

    if (ret != (int)strlen(value)) {
        DEBUG_PRINT("path=%s, value=%s, %s", path, value, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
#endif

}

static int 
dev_bond_bonding_slave(const char *ifn, const char *slava_ifn, int if_add)
{
    char value[128] = {0};

    if (dev_bond_if_exist(slava_ifn) == 0) {
        dev_bond_if_up_down(slava_ifn, 0);
        if (if_add) {
            snprintf(value, sizeof(value), "+%s", slava_ifn);
        } else {
            snprintf(value, sizeof(value), "-%s", slava_ifn);
        }
        
        dev_bond_write_sysfs(BOND_SYSFS(slaves), ifn, value);
    }
    return 0;
}

static int 
dev_bond_config_bond(const char *ifn, char **slave, const char *mode, int if_add)
{
    int i = 0;
    char tmp[16];

    dev_bond_write_sysfs(BOND_SYSFS(mode), ifn, mode);
    if (miimon != 0) {
        snprintf(tmp, sizeof(tmp), "%d", miimon);
        dev_bond_write_sysfs(BOND_SYSFS(miimon), ifn, tmp);
    }
    
    while (slave[i]) {
        dev_bond_bonding_slave(ifn, slave[i], if_add);
        i++;
    }

    return 0;
}


void 
help()
{
    fprintf(stdout, 
            "Usage:"
            "\t-s, salve name\n"
            "\t-a, add slave\n"
            "\t-d, delete slave\n"
            "\t-m, bonding mode\n"
            "\t-i, miimon\n"
            "\t\t-m 0, (balance-rr)Round-robin policy\n"
            "\t\t-m 1, (active-backup)Active-backup policy\n"
            "\t\t-m 2, (balance-xor)XOR policy\n"
            "\t\t-m 3, broadcast\n"
            "\t\t-m 4, (802.3ad)IEEE 802.3ad Dynamic link aggregation\n"
            "\t\t-m 5, (balance-tlb)Adaptive transmit load balancing\n"
            "\t\t-m 6, (balance-alb)Adaptive load balancing\n"
            );
    fflush(stdout);
}


int main(int argc, char *argv[])
{
    int ret = 0, opt = 0, i = 0;
    char *slotid;
    char* salves[32] = {0};
    char bonding_mode[32] = {0};
    int dev_bond_slot_id = 0;
    int add_flag = -1;

    while ((opt = getopt(argc, argv, "ads:m:")) != -1) 
    {
        switch (opt) 
        {
            case 'a':
                add_flag = 1;
                break;
            case 'd':
                add_flag = 0;
                break;
            case 's':
                salves[i] = strdup(optarg);
                i++;
                break;
            case 'm':
                strcpy(bonding_mode, optarg);
                break;
            case 'i':
                miimon = atoi(optarg);
                break;
            default: 
                help();
                exit(0);
        }
    }

    if ((bonding_mode[0] == 0 && add_flag == 1) || i == 0 || add_flag == -1) {
        help();
        exit(0);
    }
    

    slotid = getenv("slotid");
    if (slotid) {
        dev_bond_slot_id = atoi(slotid);
    }

    if (dev_bond_if_exist("bond0") == 0) {
        dev_bond_config_bond("bond0", salves, bonding_mode, add_flag);
        dev_bond_set_if_ip("bond0", dev_bond_get_bond_ip(dev_bond_slot_id));
    }

    while (i) {
        free(salves[i--]);
    }

    return 0;
}
