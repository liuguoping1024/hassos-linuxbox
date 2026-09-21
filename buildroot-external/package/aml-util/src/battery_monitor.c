/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 * This source code is subject to the terms and conditions defined
 * in the file 'LICENSE' which is part of this source code package.
 * Description: battery monitor file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <linux/netlink.h>

#define CONFIG_LOGLEVEL (log_level)

#define PRINT_ERROR_LEVEL   1
#define PRINT_WARNING_LEVEL 2
#define PRINT_INFO_LEVEL    3
#define PRINT_DEBUG_LEVEL   4

#define _printf(level, fmt, ...) ({level <= CONFIG_LOGLEVEL ? printf(fmt, ##__VA_ARGS__) : 0;})
#define pr_error(fmt, ...)   _printf(PRINT_ERROR_LEVEL, "[Battery Monitor][ERROR]" fmt, ##__VA_ARGS__)
#define pr_warning(fmt, ...) _printf(PRINT_WARNING_LEVEL, "[Battery Monitor][WARNING]" fmt, ##__VA_ARGS__)
#define pr_info(fmt, ...)    _printf(PRINT_INFO_LEVEL, "[Battery Monitor][INFO]" fmt, ##__VA_ARGS__)
#define pr_debug(fmt, ...)   _printf(PRINT_DEBUG_LEVEL, "[Battery Monitor][DEBUG]" fmt, ##__VA_ARGS__)

#define UEVENT_BUFFER_SIZE 2048

#define DEVICE_PATH "/sys/class/power_supply"
#define REPORT_PERIOD_SETTING_FILE "uevent_period"

/*battery state item*/
#define STATE_PRESENT        "POWER_SUPPLY_PRESENT="
#define STATE_HEALTH         "POWER_SUPPLY_HEALTH="
#define STATE_CAPACITY       "POWER_SUPPLY_CAPACITY="
#define STATE_VOLTAGE_NOW    "POWER_SUPPLY_VOLTAGE_NOW="
#define STATE_VOLTAGE_OCV    "POWER_SUPPLY_VOLTAGE_OCV="
#define STATE_CURRENT_NOW    "POWER_SUPPLY_CURRENT_NOW="
#define STATE_RESIST         "POWER_SUPPLY_RESIST="
#define STATE_CAPACITY_LEVEL "POWER_SUPPLY_CAPACITY_LEVEL="
#define STATE_STATUS         "POWER_SUPPLY_STATUS="
#define STATE_TEMPERATURE    "POWER_SUPPLY_TEMP="
#define STATE_UEVENT_PERIOD  "POWER_SUPPLY_UEVENT_PERIOD="
#define STATE__SERIAL_NUMBER "POWER_SUPPLY_SERIAL_NUMBER="

#define REPORT_TIME_MAX        90
#define REPORT_TIME_MIN        1
#define REPORT_TIME_DEFAULT    60
#define REPORT_TIME_LOW_ALARM  1

#define ALARM_CAPACITY_MAX     99
#define ALARM_CAPACITY_MIN     1
#define ALARM_CAPACITY_DEFAULT 1

#define DELTA_CAPACITY_ENTER_PRE_ALARM 2

typedef void (* fun)(char *, char *);

struct battery_state_item {
	char * name;
	fun fn;
};

static int set_report_time(int time);
static void state_capacity_analysis(char *buf, char *name);

static int report_time = REPORT_TIME_DEFAULT;
static int alarm_capacity = ALARM_CAPACITY_DEFAULT;
static int report_time_current = 0;
static char device_name[32] = {0};
static char log_level = PRINT_INFO_LEVEL;

static const struct battery_state_item battery_state[] = {
	{
		.name = STATE_PRESENT,
		.fn = NULL,
	},
	{
		.name = STATE_HEALTH,
		.fn = NULL,
	},
	{
		.name = STATE_CAPACITY,
		.fn = state_capacity_analysis,
	},
	{
		.name = STATE_VOLTAGE_NOW,
		.fn = NULL,
	},
	{
		.name = STATE_VOLTAGE_OCV,
		.fn = NULL,
	},
	{
		.name = STATE_CURRENT_NOW,
		.fn = NULL,
	},
	{
		.name = STATE_RESIST,
		.fn = NULL,
	},
	{
		.name = STATE_CAPACITY_LEVEL,
		.fn = NULL,
	},
	{
		.name = STATE_STATUS,
		.fn = NULL,
	},
	{
		.name = STATE_TEMPERATURE,
		.fn = NULL,
	},
	{
		.name = STATE_UEVENT_PERIOD,
		.fn = NULL,
	},
	{
		.name = STATE__SERIAL_NUMBER,
		.fn = NULL,
	},
};

static void state_capacity_analysis(char *buf, char *name)
{
	if (!buf || !name) {
		return;
	}

	char *val = buf + strlen(name);
	int capacity = atoi(val);

	if (capacity <= alarm_capacity) {
		pr_warning("Low power alarm!\n");
		system("/etc/adckey/adckey_function.sh longpresspower");
	} else {
		int delta= capacity - alarm_capacity;
		if (delta <= DELTA_CAPACITY_ENTER_PRE_ALARM) {
			if (report_time_current != REPORT_TIME_LOW_ALARM) {
				set_report_time(REPORT_TIME_LOW_ALARM);
			}
		} else {
			if (report_time_current != report_time) {
				set_report_time(report_time);
			}
		}
	}
}

static int battery_monitor_init(void)
{
	struct sockaddr_nl client;
	int fd = -1, ret = -1;
	int buffersize = 1024;
	fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (fd < 0) {
		pr_error("can't creat socket!\n");
		return -EPERM;
	}
	memset(&client, 0, sizeof(client));
	client.nl_family = AF_NETLINK;
	client.nl_pid = getpid();
	client.nl_groups = 1; /* receive broadcast message*/
	ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &buffersize, sizeof(buffersize));
	if (ret < 0) {
		pr_error("can't set socket!\n");
		close(fd);
		return -errno;
	}
	ret = bind(fd, (struct sockaddr*)&client, sizeof(client));
	if (ret < 0) {
		pr_error("can't bind socket!\n");
		close(fd);
		return -errno;
	}
	return fd;
}

static int battery_state_analysis(char * buf)
{
	if (!buf) {
		return -EINVAL;
	}

	for (int i = 0; i < sizeof(battery_state)/sizeof(battery_state[0]); i++) {
		if (!strncmp(buf, battery_state[i].name, strlen(battery_state[i].name))) {
			if (battery_state[i].fn)
				battery_state[i].fn(buf, battery_state[i].name);
		}
	}
}

static int battery_state_change(char *buf, int size)
{
	int len = 0;
	if (!buf) {
		return -EINVAL;
	}
	while (size > 0) {
		pr_debug("%s\n", buf);
		len = strlen(buf) + 1;
		battery_state_analysis(buf);
		buf += len;
		size -= len;
	}
	pr_debug("\n\n");
	return 0;
}

static int battery_state_read(int fd)
{
	int len = 0;
	char buf[UEVENT_BUFFER_SIZE] = {0};
	/* receive data */
	len = recv(fd, buf, sizeof(buf), 0);
	if (len > 0 && strstr(buf, device_name)) {
		buf[len] = 0;
		battery_state_change(buf, len);
	}
	return 0;
}

static int set_report_time(int time)
{
	char temp[64] = {0};
	sprintf(temp, "%s/%s/%s", DEVICE_PATH, device_name, REPORT_PERIOD_SETTING_FILE);
	FILE *fp = fopen(temp, "w");
	if (fp != NULL) {
		char buf[16] = {0};
		sprintf(buf, "%d", time);
		fwrite(buf, strlen(buf) + 1, 1, fp);
		fclose(fp);
		report_time_current = time;
	} else {
		pr_error("can't open pmic uevent setting file!\n");
	}
}

static void usage() {
	fprintf(stderr, "%s [-d <device>] [-r <time>] [-a <capacity>] [-p] [-h] \n"
			"  -d <device>   Monitored device name \n"
			"                Refer to the device name in the \'/sys/class/power_supply/\' directory \n"
			"  -r <time>     Time of reporting interval (1-90)s\n"
			"  -a <capacity> Capacity of low battery alarm (1-99)%%\n"
			"  -p            Print uevent to debug\n"
			"  -h            Print help\n", "battery_monitor");
	exit(1);
}

int main(int argc, char **argv)
{
	int battery_fd = -1;
	int nr = -1;
	int c = 0;
	struct pollfd pollfds = {0};
	char temp[64] = {0};

	while ((c = getopt(argc, argv, "d:r:a:ph")) != -1) {
		switch (c) {
			case 'd':
				strcpy(device_name, optarg);
				break;
			case 'r':
				report_time = atoi(optarg);
				if (report_time < REPORT_TIME_MIN || REPORT_TIME_MIN > REPORT_TIME_MAX)
					report_time = REPORT_TIME_DEFAULT;
				break;
			case 'a':
				alarm_capacity = atoi(optarg);
				if (alarm_capacity < ALARM_CAPACITY_MIN || alarm_capacity > ALARM_CAPACITY_MAX)
					alarm_capacity = ALARM_CAPACITY_DEFAULT;
				break;
			case 'p':
				log_level = PRINT_DEBUG_LEVEL;
				break;
			case 'h':
				usage();
				break;
			default:
				error(1, 0, "invalid option -%c", optopt);
		}
	}

	if (strlen(device_name) == 0) {
		pr_error("The entered device does not exist\n");
		exit(1);
	} else {
		sprintf(temp, "%s/%s", DEVICE_PATH, device_name);
		if (access(temp, F_OK) == -1) {
			pr_error("The entered device does not exist\n");
			exit(1);
		} else {
			sprintf(temp, "%s/%s/%s", DEVICE_PATH, device_name, REPORT_PERIOD_SETTING_FILE);
			if (access(temp, F_OK) == -1) {
				pr_error("The entered device does not support monitoring\n");
				exit(1);
			}
		}
	}

	pr_info("Device:%s, Report interval: %ds, Alarm capacity: %d%%\n", device_name, report_time, alarm_capacity);
	set_report_time(report_time);

	memset(&pollfds, 0, sizeof(pollfds));
	battery_fd = battery_monitor_init();
	if (battery_fd < 0) {
		pr_error("can't open battery monitor\n");
		exit(1);
	}
	pollfds.fd = battery_fd;
	pollfds.events = POLLIN;

	while (1) {
		nr = poll(&pollfds, 1, -1);
		if (nr > 0 && pollfds.revents & POLLIN) {
			battery_state_read(pollfds.fd);
		}
	}

	if (battery_fd > 0)
		close(battery_fd);
}
