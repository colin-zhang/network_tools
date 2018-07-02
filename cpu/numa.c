//
// Created by colin on 18-6-1.
//
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sched.h>
#include <sys/types.h>

#include "numa.h"   

void printmask(char *name, struct bitmask *mask)
{
	int i;
	printf("%s, size = %ld :", name, mask->size);
	for (i = 0; i < mask->size; i++)
		if (numa_bitmask_isbitset(mask, i))
			printf("%d ", i);
	putchar('\n');
}

void test1()
{
    printf("numa: \n");
    printf("numa_max_possible_node: %d \n", numa_max_possible_node());
    printf("numa_num_possible_nodes: %d \n", numa_num_possible_nodes());
    printf("numa_max_node: %d \n", numa_max_node());
    printf("numa_num_configured_nodes: %d \n", numa_num_configured_nodes());
    printf("numa_num_configured_cpus: %d \n", numa_num_configured_cpus());
    printf("numa_num_task_cpus: %d \n", numa_num_task_cpus());
    //printf("numa_node_size: %ld \n", numa_node_size());
    
    struct bitmask* mask = numa_get_run_node_mask();
    struct bitmask* mask_cpu = numa_allocate_cpumask();
    
    int ret;
    pid_t pid = getpid();
    
    ret = numa_sched_getaffinity(pid, mask_cpu);
    
    printmask("numa_get_run_node_mask", mask);
    if (ret >= 0) {
        printmask("numa_sched_getaffinity", mask_cpu);
    } else {
        printf("ret = %d, %s \n", ret, strerror(errno));
    }

    numa_free_cpumask(mask_cpu);
    //struct bitmask *numa_allocate_nodemask();
    //void numa_free_nodemask();
    
    printf("numa_preferred : %d \n", numa_preferred());
    
    printf("numa_get_interleave_node : %d \n", numa_get_interleave_node());
    
}



int main()
{
    printf("getpagesize: %d\n", getpagesize());
    if (numa_available() < 0) {
        printf("Your system does not support NUMA API\n");
        return 0;
    }

    struct bitmask* mask_cpu = numa_allocate_cpumask();
    numa_bitmask_clearall(mask_cpu);
    numa_bitmask_setbit(mask_cpu, 3);
    numa_bitmask_setbit(mask_cpu, 4);
    printmask("mask_cpu set", mask_cpu);

    numa_sched_setaffinity(getpid(), mask_cpu);

    numa_bitmask_clearall(mask_cpu);
    int ret = numa_sched_getaffinity(getpid(), mask_cpu);
    printmask("mask_cpu get", mask_cpu);


    printf("node of cpu:%d\n", numa_node_of_cpu(20));

    numa_free_cpumask(mask_cpu);
    return 0;
}
