#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cpuid.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/sysinfo.h>
#include <sys/queue.h>

//https://github.com/linux-test-project/ltp/blob/master/testcases/kernel/controllers/cpuset/cpuset_lib/meminfo.c
//online
const char* kSysNodeOnlinePath="/sys/devices/system/node";
const char* kSysPciDevicePath="/sys/bus/pci/devices";
const char* kVendor="vendor";
const char* kDevice="device";

const char cpu_list[][32] = 
{
    "2650",
    "2678",
    "2620",
};

typedef struct PciId
{
    char name[32];
    uint32_t type;
    uint32_t vendor_id;
    uint32_t device_id;
    //uint32_t subsystem_device_id;
    char description[1024];
} PciId;

const PciId KnownPciId[] = {
    {"X710", 10, 0x8086, 0x1572,  "Ethernet Controller X710 for 10GbE SFP+"},   //
    {"X710", 10, 0x8086, 0x1581,  "Ethernet Controller X710 for 10GbE backplane"},
    {"X710", 10, 0x8086, 0x1585,  "Ethernet Controller X710 for 10GbE QSFP+"},
    {"X710", 10, 0x8086, 0x1586,  "Ethernet Controller X710 for 10GBASE-T"},
    {"X710", 10, 0x8086, 0x1589,  "Ethernet Controller X710/X557-AT 10GBASE-T"},

    {"82599", 10, 0x8086, 0x10fb,  "82599ES 10-Gigabit SFI/SFP+ Network Connection"}, //
    {"82599", 10, 0x8086, 0x10f9,  "82599 10 Gigabit Dual Port Network Connection"},
    {"82599", 10, 0x8086, 0x10fc,  "82599 10 Gigabit Dual Port Network Connection"},
    {"82599", 10, 0x8086, 0x1517,  "82599ES 10 Gigabit Network Connection"},
    {"82599", 10, 0x8086, 0x1529,  "82599 10 Gigabit Dual Port Network Connection with FCoE"},
    {"82599", 10, 0x8086, 0x152a,  "82599 10 Gigabit Dual Port Backplane Connection with FCoE"},
    {"82599", 10, 0x8086, 0x1557,  "82599 10 Gigabit Network Connection"},
    {"82599", 10, 0x8086, 0x10a6,  "82599EB 10-Gigabit Dummy Function"},
    {"82599", 10, 0x8086, 0x10d8,  "82599EB 10 Gigabit Unprogrammed"},
    {"82599", 10, 0x8086, 0x10ed,  "82599 Ethernet Controller Virtual Function"},
    {"82599", 10, 0x8086, 0x10f8,  "82599 10 Gigabit Dual Port Backplane Connection"},
};

typedef struct PciDirNode {
    uint32_t vendor_id;
    uint32_t device_id;
    char path[1024];
    TAILQ_ENTRY(PciDirNode) entries;
} PciDirNode;

static uint32_t read_hex(const char* dir, const char* file) 
{   
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s/%s", dir, file);
    FILE* f = fopen(buf, "r");
    if (f == NULL) {
        fprintf(stderr, "read_hex :%s\n", strerror(errno));
        return 0;
    }
    uint32_t value;
    fscanf(f, "%x", &value);
    fclose(f);
    return value;
}

int read_eth_device()
{
    TAILQ_HEAD(, PciDirNode) pci_dir_list;
    TAILQ_INIT(&pci_dir_list);
    struct PciDirNode* item;
    int cap = 0;

    DIR* dir = opendir(kSysPciDevicePath);
    if (NULL == dir) {
        fprintf(stderr, "read_eth_device :%s\n", strerror(errno));
        return -1;
    }

    struct dirent* sub_dir = NULL;
    while ((sub_dir = readdir(dir)) != NULL) {
        if (sub_dir->d_type == DT_DIR || 
            sub_dir->d_type == DT_LNK) {
            if (strlen(sub_dir->d_name) > 4) {
                item = (PciDirNode*)malloc(sizeof(PciDirNode));
                item->vendor_id = 0;
                item->device_id = 0;
                snprintf(item->path, sizeof(item->path), "%s/%s", kSysPciDevicePath, sub_dir->d_name);
                TAILQ_INSERT_TAIL(&pci_dir_list, item, entries);
            }
        }
    }
    closedir(dir);

    TAILQ_FOREACH(item, &pci_dir_list, entries) {
        uint32_t device_id = read_hex(item->path, kDevice);
        uint32_t vendor_id = read_hex(item->path, kVendor);
        for (size_t i = 0; i < sizeof(KnownPciId)/sizeof(KnownPciId[0]); i++) {
            if (vendor_id == KnownPciId[i].vendor_id &&
                device_id == KnownPciId[i].device_id) {
                //printf("%s, %x %x \n", KnownPciId[i].description, vendor_id, device_id);
                cap += KnownPciId[i].type;
            }
        }
    }

    while (item = TAILQ_FIRST(&pci_dir_list)) {
        TAILQ_REMOVE(&pci_dir_list, item, entries);
        free(item);
    }
    return cap;
}

//int numa_num_configured_nodes();
int read_nodes(void)
{
    DIR* dir = opendir(kSysNodeOnlinePath);
    if (NULL == dir) {
        fprintf(stderr, "read_nodes :%s\n", strerror(errno));
        return -1;
    }

    struct dirent* sub_dir = NULL;
    int node = 0;
    while ((sub_dir = readdir(dir)) != NULL) {
        if (sub_dir->d_type == DT_DIR || 
            sub_dir->d_type == DT_LNK) {
            if (memcmp("node", sub_dir->d_name, 4) == 0) {
                node++;
            }
        }
    }
    closedir(dir);
    return node;
}

void getcpuid(uint32_t cpu_info[4], uint32_t info_type)
{
    __cpuid(info_type, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
}

int get_cpu_brand(char* brand)
{
    uint32_t buf[4];
    if (NULL == brand) return 0;
    getcpuid(buf, 0x80000000U);
    if (buf[0] < 0x80000004U) return 0;
    getcpuid((uint32_t*)&brand[0], 0x80000002U);
    getcpuid((uint32_t*)&brand[16], 0x80000003U);
    getcpuid((uint32_t*)&brand[32], 0x80000004U);
    brand[48] = '\0';
    return 48;
}

int extract_cpu_brand(char* brand, size_t len)
{
    char* buf = (char*)malloc(len);
    if (buf == NULL) {
        snprintf(brand, len, "%s", "unkown");
        return -1;
    }
    snprintf(buf, len, "%s", brand);
    snprintf(brand, len, "%s", "unkown");
    size_t n = 0;

    for (size_t i = 0; i < sizeof(cpu_list)/sizeof(cpu_list[0]); i++) {
        char* p = strstr(buf, cpu_list[i]);
        if (p != NULL) {
            n = snprintf(brand, len, "%s", cpu_list[i]);
            while (*p) {
                if (*p == 'v' || *p == 'V') {
                    char* q = p;
                    while (*p) {
                        if (*p == ' ') {
                            *p = '\0';
                            snprintf(brand + n, len - n , "%s", q);
                            break;
                        }
                        p++;
                    }
                    break;
                }
                p++;
            }
            break;
        }
    }
    free(buf);
    return 0;
}

int get_total_mem(void)
{
    //not right
    struct sysinfo info;
    sysinfo(&info);
    return info.totalram / (1 << 30);
    
    //TODO
    /*
    long long physical_mem_bytes = (long long) sysconf (_SC_PHYS_PAGES) * sysconf (_SC_PAGESIZE);
    https://github.com/lyonel/lshw/blob/master/src/core/mem.cc
    
    */
    
    /*
    FILE* f = popen("dmidecode -t memory | grep Size", "r");
    if (f == NULL) {
        return -1;
    }
    char buf[1024] = {0};
    size_t cap = 0;
    while (fgets(buf, 1024, f)) {
        int len = strlen(buf);
        for (int i = 0; i < len; i++) {
            if (isdigit(buf[i])) {
                cap += atoi(&buf[i]);
                break;
            }
        }
    }
    pclose(f);
    return cap >> 10;
    
    */
}

char* sys_info_str(void)
{
    int ethcap = read_eth_device();
    int nodes = read_nodes();
    int memsize = get_total_mem();
    char brand[1024] = {0 };
    get_cpu_brand(brand);
    int ret = extract_cpu_brand(brand, 1024);

    if (ethcap < 0 || 
        nodes < 0 ||  
        memsize < 0 || 
        ret < 0) {
        return "";
    }

    if (strcmp("unkown", brand) == 0) {
        return "";
    }

    char* sysinfo = (char*)malloc(1024);
    if (sysinfo == NULL) {
        return "";
    }
    snprintf(sysinfo, 1024, 
            "%s*%d_%dGB_%dGbps"
            , brand, nodes, memsize, ethcap);
    return sysinfo;
}

int main(int argc, char const *argv[])
{
    printf("%s\n", sys_info_str());
    return 0;
}
