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

static int miimon = -1;
static int use_carrier = -1;
static int updelay = -1;
static int downdelay = -1;

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
    if (miimon != -1) {
        snprintf(tmp, sizeof(tmp), "%d", miimon);
        dev_bond_write_sysfs(BOND_SYSFS(miimon), ifn, tmp);
    }

    if (updelay != -1) {
        snprintf(tmp, sizeof(tmp), "%d", updelay);
        dev_bond_write_sysfs(BOND_SYSFS(updelay), ifn, tmp);
    }

    if (downdelay != -1) {
        snprintf(tmp, sizeof(tmp), "%d", downdelay);
        dev_bond_write_sysfs(BOND_SYSFS(downdelay), ifn, tmp);
    }

    if (use_carrier != -1) {
        snprintf(tmp, sizeof(tmp), "%d", use_carrier);
        dev_bond_write_sysfs(BOND_SYSFS(use_carrier), ifn, tmp);
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
            "\t-b xx, bond interface\n"
            "\t-a xx, add slave\n"
            "\t-r xx, remove slave\n"
            "\t-i xx, miimon\n"
            "\t-c xx, use_carrier\n"
            "\t-u xx, updelay\n"
            "\t-d xx, downdelay\n"
            "\t-m xx, bonding mode\n"
            "\t\t-m 0, (balance-rr)Round-robin policy\n"
            "\t\t-m 1, (active-backup)Active-backup policy\n"
            "\t\t-m 2, (balance-xor)XOR policy\n"
            "\t\t-m 3, broadcast\n"
            "\t\t-m 4, (802.3ad)IEEE 802.3ad Dynamic link aggregation\n"
            "\t\t-m 5, (balance-tlb)Adaptive transmit load balancing\n"
            "\t\t-m 6, (balance-alb)Adaptive load balancing\n"
            "Example 1: base_bonding -b bond0 -a eth0 -a eth1 -i 100 -m 1\n"
            "Example 2: base_bonding -b eth0  --no_bondig\n"

            );
    fflush(stdout);
}


int main(int argc, char *argv[])
{
    int ret = 0, opt = 0, i = 0;
    int opt_index, no_bondig = 0;
    char *slotid = NULL;
    char *base_if = "bond0";
    char* salves[32] = {0};
    char bonding_mode[32] = {0};
    int dev_bond_slot_id = 0;
    int add_flag = -1;

    struct option long_opts[] = {
        {"no_bonding", no_argument, 0, 0},
    };

    while ((opt = getopt_long(argc, argv, "b:a:r:m:i:u:d:c:", long_opts, &opt_index)) != -1) 
    {
        switch (opt) 
        {
            case 0:
                no_bondig += 2;
                break;
            case 'b':
                base_if = argv[optind - 1];
                no_bondig += 1;
                break;
            case 'a':
                add_flag = 1;
                salves[i++] = strdup(optarg);
                break;
            case 'r':
                add_flag = 0;
                salves[i++] = strdup(optarg);
                break;
            case 'm':
                strcpy(bonding_mode, optarg);
                break;
            case 'i':
                miimon = atoi(optarg);
                break;
            case 'c':
                use_carrier = atoi(optarg);
                break;
            case 'u':
                updelay = atoi(optarg);
                break;
            case 'd':
                downdelay = atoi(optarg);
                break;

            default: 
                help();
                exit(0);
        }
    }

    slotid = getenv("slotid");
    if (slotid) {
        dev_bond_slot_id = atoi(slotid);
        if (dev_bond_slot_id == 0) {
            dev_bond_slot_id = 30;
        }
    }

    if (no_bondig == 3) {
        dev_bond_set_if_ip(base_if, dev_bond_get_bond_ip(dev_bond_slot_id));
        exit(0);
    } else if (no_bondig == 2) {
        fprintf(stdout, "no interface name\n");
        exit(1);
    }

    if ((bonding_mode[0] == 0 && add_flag == 1) || i == 0 || add_flag == -1 || base_if == NULL) {
        help();
        exit(0);
    }
        
    if (dev_bond_if_exist(base_if) == 0) {
        dev_bond_config_bond(base_if, salves, bonding_mode, add_flag);
        dev_bond_set_if_ip(base_if, dev_bond_get_bond_ip(dev_bond_slot_id));
    }

    do {
      free(salves[i--]);
    } while(i);

    return 0;
}
