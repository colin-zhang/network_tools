/* lscpu.c Copyright(C) Jason R Fink  see COPYING file */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXLEN 1024

/* the file handles we will be using */
static char *pfh[]=
{
	"/proc/cpuinfo",
	"/proc/sys/kernel/hostname",
	"/proc/sys/kernel/ostype",
	"/proc/sys/kernel/osrelease",
	"/proc/rtc",
	"/proc/driver/rtc",
};

/* Function Prototypes */
int get_kernel_version(void);
void error_output_mesg(char *locale);
void get_hostname_info(void);
void get_ostype_info(void);
void get_osrelease_info(void);
void get_cpu_info(void);
void get_rtc_info(void);

/* Using /proc/sys/kernel/osrelease this function returns:
  	0 if the kernel is a 2.2.X series kernel
  	1 if the kernel is a 2.3.X series kernel
  	2 if the not 2.2.X or 2.2.X series kernel
  	3 if the iteration is missed completely */
int get_kernel_version()
{
	FILE *osrfp;
	static char osrch[MAXLEN];

	if((osrfp = fopen(pfh[3], "r")) == NULL)
		error_output_mesg(pfh[3]);

	while(fgets(osrch, MAXLEN, osrfp) != NULL) {
		if(!strncmp(osrch, "2.2", 3)) {
			fclose(osrfp);
			return 0;
		} else if(!strncmp(osrch, "2.3", 3)) {
			fclose(osrfp);
			return 1;
		} else {
			fclose(osrfp);
			return 2;
		}
	}

	fclose(osrfp);

	return 3;
}

/* Simple strerror print function depends upon file handle    
   parameter to print out a file not found message */
void error_output_mesg(char *locale)
{
	fprintf(stderr, ("%s: %s\n"), locale, strerror(errno));
	exit (1);
}

/* Scans the hostname into a line and prints out the line */
void get_hostname_info()
{
	FILE *hostfp;
	static char hostch[MAXLEN];
	static char hostname[MAXLEN];

	if((hostfp = fopen(pfh[1], "r")) == NULL)
		error_output_mesg(pfh[1]);

	while(fgets(hostch, MAXLEN, hostfp) != NULL) {
		sscanf(hostch, "%s", hostname);
		printf("Processor Information for %s\n", hostname);
	}

	fclose(hostfp);
}

/* Scans the ostype into a line buffer and prints it out */
void get_ostype_info()
{
	FILE *osfp;
	static char osch[MAXLEN];
	static char ostype[MAXLEN];

	if((osfp = fopen(pfh[2], "r")) == NULL) 
		error_output_mesg(pfh[2]);

	while(fgets(osch, MAXLEN, osfp) != NULL) {
		sscanf(osch, "%s", ostype);
		printf("OS: %s", ostype);
	}

	fclose(osfp);
}

/* Scans the osrelease into a line buffer and prints it out */
void get_osrelease_info()
{
	FILE *osrfp;
	static char osrch[MAXLEN];
	static char osrelease[MAXLEN];

	if((osrfp = fopen(pfh[3], "r")) == NULL) 
		error_output_mesg(pfh[3]);

	while(fgets(osrch, MAXLEN, osrfp) != NULL) {
		sscanf(osrch, "%s", osrelease);
		printf(" version %s\n", osrelease);
	}

	fclose(osrfp);
}

/* Scans select information from /proc/cpuinfo
   into a set of buffers and prints them to std out
   AS EACH LINE is matched */
void get_cpu_info()
{
    FILE *cpufp;
    static char ch[MAXLEN];
    char line[MAXLEN];

    if ((cpufp = fopen(pfh[0], "r")) == NULL) 
		error_output_mesg(pfh[0]);

    while (fgets(ch, MAXLEN, cpufp) != NULL) {
		if (!strncmp(ch, "processor", 9)) {
		    sscanf(ch, "%*s %*s %s", line);
		    printf("CPU %s", line);
		} else if (!strncmp(ch, "vendor_id", 9)) {
		    sscanf(ch,"%*s %*s %s", line);
		    printf(" is Processor Type: %s ", line);
		} else if (!strncmp(ch, "model name", 10)) {
		    sscanf(ch, "%*s %*s %*s %s", line);
		    printf(" %s\n", line);
		} else if (!strncmp(ch, "cpu MHz", 7)) {
		    sscanf(ch, "%*s %*s %*s %s", line);
		    printf("Processor Speed in MHz: %s\n", line);
		} else if (!strncmp(ch, "cache size", 10)) {
		    sscanf(ch, "%*s %*s %*s %s", line);
		    printf("Processor Cache Size: %s\n", line);
		} else if (!strncmp(ch, "bogomips", 8)) {
		    sscanf(ch, "%*s %*s %s", line);
		    printf("Processor Speed in Bogomips: %s\n", line);
		} 
    }
    fclose(cpufp);
}

/* Checks the kernel version using /proc/sys/kernel/osrelease
   then based upon that determines where the rtc file is at.
   Finally scan and print out select info from the rtc file. */
void get_rtc_info()
{
	FILE *rtcfp;
	static char ch[MAXLEN];	
	char line[MAXLEN];
	char rtcinfo[MAXLEN];
	register int kernelv_flag;

	/* set kernel flag */
	kernelv_flag = get_kernel_version();

	/* 
	   Based upon the flag determine where the 
	   elusive rtc file is residing. If it is
	   indetermined print an error message
	   and exit out 
	 */
	if(kernelv_flag == 0) {
		strcpy(rtcinfo, pfh[4]);
	} else if(kernelv_flag == 1) {
		strcpy(rtcinfo, pfh[5]);
	} else {
		fprintf(stderr, "unknown");
		exit(2);	/* slightly higher magnitude */
	} 

    if ((rtcfp = fopen(rtcinfo, "r")) == NULL) 
		error_output_mesg(rtcinfo);

    /* Just grab stuff from a certain position and dump it to stdout */
    while (fgets(ch, MAXLEN, rtcfp) != NULL) {
		if (!strncmp(ch, "rtc_time", 8)) {
		    sscanf(ch, "%*s %*s %s", line);
		    printf("RTC Current Time: %s\t", line);
		} else if (!strncmp(ch, "rtc_date", 8)) {
		    sscanf(ch, "%*s %*s %s", line);
		    printf("RTC Date: %s\n", line);
		} else if (!strncmp(ch, "periodic_freq", 13)) {
		    sscanf(ch, "%*s %*s %s", line);
		    printf("RTC Periodic Frequency: %s\t", line); 
		} else if (!strncmp(ch, "batt_status", 11)) {
		    sscanf(ch, "%*s %*s %s", line);
		    printf("RTC Battery Status: %s\n", line);
		}
    }
    fclose(rtcfp);
}

/* Main */
int main(void)
{
	get_hostname_info();
	get_ostype_info();
	get_osrelease_info();
	get_cpu_info();
	get_rtc_info();

    return 0;
}
