#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/ip_icmp.h>

typedef struct sockaddr_in  SA;
typedef struct in_addr      IA;

#define IP4_HEADER_LEN      20
#define ICMP_HEADER_LEN      8
#define ICMP_BODY_LEN        0
#define ICMP_FULL_LEN       (IP4_HEADER_LEN + ICMP_HEADER_LEN + ICMP_BODY_LEN)


#define Print(fmt, args...) { \
            fprintf(stdout, ":%d:"fmt, __LINE__, ##args); \
            fflush(stdout); \
        } \

enum error_code
{
    PING_OK = 1,
    PING_SEND_FAIL,
    PING_ERROR_OCCUR,
    PING_NOT_RECEIVE,
};

struct v4_ping_addr
{
    IA ia;
    int res;
};

struct v4_ping
{
    int sock_fd;
    char send_buf[256];
    int num;
    int offset;
    struct v4_ping_addr *ping_addr;
};

struct v4_ping  *g_ping_ptr = NULL;
static unsigned int seq_num = 0;
static uint64_t pack_num = 0;

static int set_nonblocking(int fd, bool if_noblock) 
{
    int flags;
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) {
        Print("setnonblocking error");
        return -1;
    }
    if (if_noblock) {
        flags = flags | O_NONBLOCK;
    } else {
        flags = flags & ~O_NONBLOCK;
    }
    return fcntl(fd, F_SETFL, flags);
}

static int socket_set(int fd)
{
    int size = 0;
    int ttl = 0;

    size = (1024) * 1024;
    ttl = 10;
   
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0) {
        Print("setsockopt:%s\n", strerror(errno));
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0) {
        Print("setsockopt:%s\n", strerror(errno));
    }
    if (setsockopt(fd, IPPROTO_IP, IP_TTL, (char *)&ttl, sizeof(ttl)) < 0) {
        Print("setsockopt:%s\n", strerror(errno));
    } 
    return 0;
}

static int ping_socket_creat()
{
    int skt = 0;
    skt = socket(AF_INET, SOCK_RAW, 1); //1 is icmp
    if (skt < 0) {
        Print("socket error , %d, %s\n", errno, strerror(errno));
        exit(1);
    }
    socket_set(skt);
    set_nonblocking(skt, 1);
    return skt;
}

struct v4_ping* ping_addr_init(int num)
{
    struct v4_ping* ptr;

    ptr = calloc(1, sizeof(*ptr));
    if (ptr == NULL) {
        Print("%s \n", "calloc");
        return NULL;
    }
    ptr->num = num;
    ptr->offset = 0;
    ptr->sock_fd = ping_socket_creat();
   
    ptr->ping_addr = calloc(num, sizeof(*ptr->ping_addr));
    if (ptr->ping_addr == NULL) {
        Print("%s \n", "calloc 2");
        free(ptr);
        ptr = NULL;
    }
    return ptr;
}


void ping_addr_free(struct v4_ping *ptr)
{
    if (ptr != NULL) {
        close(ptr->sock_fd);
        if (ptr->ping_addr != NULL) {
            free(ptr->ping_addr);
        }
    }
}

/* ascending order */
int ping_addr_cmp(const void *a, const void *b)
{
    return ((IA *)a)->s_addr > ((IA * )b)->s_addr;
}

int ping_addr_cmp2(IA *a, IA *b)
{
    if (a->s_addr == b->s_addr) {
        return 0;
    } else if (a->s_addr > b->s_addr) {
        return 1;
    }  else {
        return -1;
    }
}

int ping_addr_binary_search(struct v4_ping *ptr, IA *target) {
    int left = 0;
    int right = ptr->offset - 1;
    while (left <= right) {
        int mid = left + ((right - left) >> 1);
        if (ping_addr_cmp2(&ptr->ping_addr[mid].ia, target) == 0) {
            return mid;
        }
        else if (ping_addr_cmp2(&ptr->ping_addr[mid].ia, target) > 0) {
            right = mid - 1;
        }
        else {
            left = mid + 1;
        }
    }
    return -1;
}

/* ascending order */
int ping_addr_sort(struct v4_ping *ptr)
{
    qsort(ptr->ping_addr, ptr->offset, sizeof(struct v4_ping_addr), ping_addr_cmp);
    return 0;
}

int ping_addr_double(struct v4_ping* ptr)
{
    struct v4_ping_addr *d = NULL;
    d = calloc(ptr->num << 1, sizeof(*d));
    if (NULL == d) {
        return -1;
    }
    memcpy(d, ptr->ping_addr, ptr->num*sizeof(*ptr->ping_addr));
    free(ptr->ping_addr);
    ptr->ping_addr = d;
    ptr->num = ptr->num << 1;    
}

int ping_addr_add(struct v4_ping* ptr, IA *target_ia)
{
    if (ptr->offset >= ptr->num) {
        if (ping_addr_double(ptr) < 0) {
            return -1;
        }
    }
    if (ping_addr_binary_search(ptr, target_ia) >= 0) {
        return 0;
    }
    ptr->ping_addr[ptr->offset].ia.s_addr = target_ia->s_addr;
    ptr->offset++;
    ping_addr_sort(ptr);
    return 0;
}

int ping_addr_del(struct v4_ping* ptr, IA *target_ia)
{
    int index = 0;
    index = ping_addr_binary_search(ptr, target_ia);
    if (index < 0) {
        return 0;
    }
    inet_aton("255.255.255.255", &ptr->ping_addr[index].ia);
    ping_addr_sort(ptr);
    ptr->offset--;
    return 0;
}

int ping_add_get_resp(struct v4_ping* ptr, IA *target_ia)
{
    int index = 0;
    index = ping_addr_binary_search(ptr, target_ia);
    if (index < 0) {
        return 1;
    }
    if (ptr->ping_addr[index].res == PING_OK) {
        return 1;
    } else {
        return 0;
    }
}

static int calc_chsum(unsigned short *addr,int len)
{
    int sum = 0,n = len;
    unsigned short answer = 0;
    unsigned short *p = addr; 
    while (n > 1) {
        sum += *p ++;
        n -= 2;
    }    
    if (n == 1) {
        *((unsigned char *)&answer) = *(unsigned char *)p;
        sum += answer;
    }    
    sum = (sum >> 16) + (sum & 0xffff);    
    sum += sum >> 16;
    answer = ~sum;
    return answer;
}

static int pack(char *send_buf)
{
    int packsize = 0;
    struct icmp *icmp = NULL;
    icmp = (struct icmp *)send_buf;
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_cksum = 0;
    icmp->icmp_id = htons(getpid());
    icmp->icmp_seq = ++seq_num;
    packsize = ICMP_HEADER_LEN + ICMP_BODY_LEN;
    icmp->icmp_cksum = calc_chsum((unsigned short *)icmp, packsize);    
    return packsize;
}

static int send_packet(int sockfd, IA *iaddr, char *send_buf, int buf_len)
{
    int size;    
    SA saddr;
    memset(&saddr, 0, sizeof(saddr));
    memcpy(&saddr.sin_addr, iaddr, sizeof(*iaddr));
    memset(send_buf, 'a', buf_len);
    size = pack(send_buf);
    if (sendto(sockfd, send_buf, size, 0, (struct sockaddr *)&saddr, sizeof(struct sockaddr)) < 0) {
        Print("sendto failed, %d, %s \n", errno, strerror(errno));
        return -1;
    }
    return 0;
}

static int ping_send(struct v4_ping* ptr)
{
    int i = 0, ret = 0;
    for (i = 0; i < ptr->offset; i++) {
        ret = send_packet(ptr->sock_fd, &ptr->ping_addr[i].ia, ptr->send_buf, sizeof(ptr->send_buf));
        pack_num++;
        if (ret < 0) {
            ptr->ping_addr[i].res = PING_SEND_FAIL;
        }
        if (pack_num % 1024 == 0) {
            //msleep(1000);
        }
    }
}

int ping_recv(struct v4_ping* ptr)
{
    int addr_len, n;
    struct ip *ip;
    struct icmp *icmp;
    struct sockaddr from_addr;
    struct sockaddr_in *frm_p;
    char recv_buf[1024] = {0};
    int ip_head_len, ip_len;
    int index = 0;
    
    addr_len = sizeof(struct sockaddr);
    n = recvfrom(ptr->sock_fd, recv_buf, ICMP_FULL_LEN, 0, &from_addr, &addr_len);
    if (n < 0) {
        Print("n = %d , %d, %s \n", n, errno, strerror(errno));
        return -1;
    }

    ip = (struct ip *)recv_buf;
    ip_head_len = ip->ip_hl << 2;
    ip_len = ntohs(ip->ip_len);

    frm_p = (struct sockaddr_in *)&from_addr;
    index = ping_addr_binary_search(ptr, &ip->ip_src);
    if (index < 0) {
        return -1;
    }
    icmp = (struct icmp *)(recv_buf + ip_head_len);
    if (ntohs(icmp->icmp_id) == getpid() && 
        icmp->icmp_type == ICMP_ECHOREPLY &&
        icmp->icmp_code == 0) {
        ptr->ping_addr[index].res = PING_OK;
    } else {
        ptr->ping_addr[index].res = PING_ERROR_OCCUR;
    }

    return 0;
}

int 
fd_poll(int fd, int timeout_ms) 
{
    struct pollfd fds[1];
    int ret;
    
    fds[0].fd = fd;
    fds[0].events = POLLIN | POLLPRI;
    if ((ret = poll(fds, 1, timeout_ms)) < 0)
        return -1;
    if (ret)
        return 1;
    // time out 
    return 0;
}

int 
get_ip_address(const char *ifn, char *ip, int ip_len) 
{
    int fd;
    struct sockaddr_in addr;
    struct ifreq ifr;

    if (ip == NULL || ifn == NULL) {
        return -1;
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        Print("%s , %s\n", "socket", strerror(errno));
        return -1;
    }

    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, ifn, sizeof(ifr.ifr_name) - 1);

    if (ioctl(fd, SIOCGIFADDR, &ifr) < 0 ) {
        Print("%s , %s\n", "ioctl", strerror(errno));
        close(fd);
        return -1;
    } else {
        memcpy(&addr, &ifr.ifr_addr, sizeof(addr));
        inet_ntop(AF_INET, &addr.sin_addr, ip, ip_len); 
    }
    close(fd);
    return 0;
}

void
ping_show_res(struct v4_ping* ptr)
{
    int i = 0;
    int num_ok = 0, num_send_fail = 0, num_error = 0, num_no_recv = 0;
    for (i = 0; i < ptr->offset; i++) {
        //Print("ip = %s , res = %d\n", inet_ntoa(ptr->ping_addr[i].ia), ptr->ping_addr[i].res);
        if (PING_OK == ptr->ping_addr[i].res) {
            num_ok++;
        } else if (PING_ERROR_OCCUR == ptr->ping_addr[i].res) {
            num_error++;
        } else if (PING_SEND_FAIL == ptr->ping_addr[i].res) {
            num_send_fail++;
        } else if (PING_NOT_RECEIVE == ptr->ping_addr[i].res) {
            num_no_recv++;
        }
    }
    Print("total=%d, ok=%d, error=%d, send_fail=%d, not_recv=%d\n", ptr->offset, num_ok, num_error, num_send_fail, num_no_recv);
}

test(void)
{
    struct v4_ping* ptr;
    char ip[32] = {0};
    int a, b, c, d, i;
    IA ia;

    ptr = ping_addr_init(64);
    if (NULL == ptr) {
        Print("ping addr init failed\n");
        exit(1);
    }

    get_ip_address("eth0", ip, sizeof(ip));
    sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d);
    for (i = 1; i < 255; i++) {
        if (i == d) continue;
        sprintf(ip, "%d.%d.%d.%d", a, b, c, i);
        inet_pton(AF_INET, ip, &ia);
        ping_addr_add(ptr, &ia);
    }

    ping_send(ptr);
    while (fd_poll(ptr->sock_fd, 1000)) {
        ping_recv(ptr);
    }
    ping_show_res(ptr);

    for (i = 1; i < 200; i++){
        if (i == d) continue;
        sprintf(ip, "%d.%d.%d.%d", a, b, c, i);
        inet_pton(AF_INET, ip, &ia);
        ping_addr_del(ptr, &ia);
    }

    ping_send(ptr);
    while (fd_poll(ptr->sock_fd, 1000)) {
        ping_recv(ptr);
    }
    ping_show_res(ptr);
    inet_pton(AF_INET, "192.168.11.204", &ia);
    int res = ping_add_get_resp(ptr, &ia);
    Print("204, res = %d \n", res);

    ping_addr_free(ptr);
}

int main(int argc, char *argv[])
{
    test();
    return 0;
}