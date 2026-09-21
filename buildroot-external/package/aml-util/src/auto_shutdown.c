/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 * This source code is subject to the terms and conditions defined
 * in the file 'LICENSE' which is part of this source code package.
 * Description: auto shutdown file
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/input.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <dirent.h>

#define INFO(fmt, args...) \
	printf("[Auto Shutdown][%s] " fmt, __func__, ##args)

/*timer*/
#define POWER_OFF_TIME_DEFAULT           (45)                        //45min
#define POWER_OFF_TIME_MIN               (5)                         //5min
#define POWER_OFF_TIME_MAX               (90)                        //90min
#define MIN_TO_SEC                       (60)                        //60s
#define POWER_OFF_TIME_TO_SEC_DEFAULT    (POWER_OFF_TIME_DEFAULT * MIN_TO_SEC)

/*usb*/
#define UEVENT_BUFFER_SIZE 2048
#define MIN_SIZE           30
#define USB_STATE_FILE     "/sys/class/android_usb/android0/state"

/*bt*/
#define CTL_SOCKET_PATH "/etc/bluetooth/hfp_ctl_sk"
#define HFP_EVENT_CONNECTION 1
#define HFP_EVENT_CALL       2
#define HFP_EVENT_CALLSETUP  3
#define HFP_EVENT_VGS        4
#define HFP_EVENT_VGM        5

#define HFP_IND_DEVICE_DISCONNECTED 0
#define HFP_IND_DEVICE_CONNECTED    1

#define HFP_IND_CALL_NONE           0
/* at least one call is in progress */
#define HFP_IND_CALL_ACTIVE         1
/* currently not in call set up */
#define HFP_IND_CALLSETUP_NONE      0
/* an incoming call process ongoing */
#define HFP_IND_CALLSETUP_IN        1
/* an outgoing call set up is ongoing */
#define HFP_IND_CALLSETUP_OUT       2
/* remote party being alerted in an outgoing call */
#define HFP_IND_CALLSETUP_OUT_ALERT 3

/*adc key*/
#define ADC_INPUT_EVENT_NAME "adc_keypad"

#define USB_DISCONNECT 0
#define USB_CONNECT  1

#define HFP_STATE_DISCONNECT        0                  //handfree  disconnect
#define HFP_STATE_CONNECT           1                  //handfree  connect
#define HFP_STATE_CALL_IN           2                  //have call in
#define HFP_STATE_CALL_OUT          3                  //have call out
#define HFP_STATE_CALL_OUT_RINGING  4                  //call out,ringing
#define HFP_STATE_CALL_IN_OUT_OVER  5                  //call in and out over
#define HFP_STATE_CALL_START        6                  //call start
#define HFP_STATE_CALL_ENDED        7                  //call over

static int usb_state = USB_CONNECT;
static int bt_state = HFP_STATE_DISCONNECT;
static int timing_time_sec = POWER_OFF_TIME_TO_SEC_DEFAULT;

static int stop_timer(void);
static int check_to_start_timer(void);

static int usb_monitor_init(void)
{
	struct sockaddr_nl client;
	int usb_monitor, ret;
	int buffersize = 1024;
	usb_monitor = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
	if (0 > usb_monitor) {
		return -1;
	}

	memset(&client, 0, sizeof(client));
	client.nl_family = AF_NETLINK;
	client.nl_pid = getpid();
	client.nl_groups = 1; /* receive broadcast message*/
	ret = setsockopt(usb_monitor, SOL_SOCKET, SO_RCVBUF, &buffersize, sizeof(buffersize));
	if (0 > ret) {
		return -1;
	}
	ret = bind(usb_monitor, (struct sockaddr*)&client, sizeof(client));
	if (0 > ret) {
		return -1;
	}
	return usb_monitor;
}

static int usb_state_change(void)
{
	char usb_state_buffer[MIN_SIZE] = {0};
	FILE *fp = fopen(USB_STATE_FILE, "r");
	if (fp != NULL) {
		if (fgets(usb_state_buffer, MIN_SIZE, fp) == NULL) {
			INFO("get android0 state  content failure!\n");
			fclose(fp);
		} else {
			if (!strncmp("DISCONNECTED", (const char*)usb_state_buffer, strlen("DISCONNECTED"))) {
				INFO("usb is disconnected!\n");
				usb_state = USB_DISCONNECT;
				check_to_start_timer();
			} else if (!strncmp("CONFIGURED", (const char*)usb_state_buffer, strlen("CONFIGURED"))) {
				INFO("usb is connected!\n");
				usb_state = USB_CONNECT;
				stop_timer();
			}
			fclose(fp);
		}
	} else {
		INFO("can't open usb state file!\n");
	}
	fp = NULL;
	return 0;
}

static int usb_state_read(int fd)
{
	int rcvlen;
	int buffersize = 1024;
	char buf[UEVENT_BUFFER_SIZE] = {0};
	/* receive data */
	rcvlen = recv(fd, &buf, sizeof(buf), 0);
	if (rcvlen > 0 && strstr(buf,"usb") ) {
		//printf("%s\n", buf);
		usb_state_change();
	}
	return 0;
}

static int bt_monitor_init(void)
{
	struct sockaddr_un address;
	int sockfd;

	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		INFO("can't creat socket, errno:%s", strerror(errno));
		return -1;
	}

	address.sun_family = AF_UNIX;
	strncpy (address.sun_path, CTL_SOCKET_PATH, sizeof(address.sun_path) - 1);

	if (connect(sockfd, (struct sockaddr *)&address, sizeof (address)) == -1) {
		/*if socket is not avialable or not ready, don't report error*/
		if (errno != 111 && errno != 2)
			INFO("connect server: %s", strerror(errno));
		close(sockfd);
		return -1;
	}
	return sockfd;
}

static void msg_handler(int event, int value)
{
	switch (event) {
		case HFP_EVENT_CONNECTION:
			switch (value) {
				case HFP_IND_DEVICE_DISCONNECTED:
					INFO("HFP disconnected!!!\n");
					bt_state = HFP_STATE_DISCONNECT;
					check_to_start_timer();
					break;

				case HFP_IND_DEVICE_CONNECTED:
					INFO("HFP connected!!!\n");
					bt_state = HFP_STATE_CONNECT;
					stop_timer();
					break;

				default:
					break;
			}
			break;

		case HFP_EVENT_CALL:
			switch (value) {
				case HFP_IND_CALL_NONE:
					INFO("Call stopped!!!\n");
					bt_state = HFP_STATE_CALL_START;
					break;

				case HFP_IND_CALL_ACTIVE :
					INFO("Call active!!!\n");
					bt_state = HFP_STATE_CALL_ENDED;
					break;

				default:
					break;
			}
			break;

		case HFP_EVENT_CALLSETUP:
			switch (value) {
				case HFP_IND_CALLSETUP_NONE:
					INFO("Callsetup stopped!!!\n");
					if (bt_state != HFP_STATE_CALL_START)
						bt_state = HFP_STATE_CALL_IN_OUT_OVER;
					break;

				case HFP_IND_CALLSETUP_IN :
					INFO("An incomming Callsetup!!!\n");
					bt_state = HFP_STATE_CALL_IN;
					break;

				case HFP_IND_CALLSETUP_OUT :
					INFO("An outgoing Callsetup!!!\n");
					bt_state = HFP_STATE_CALL_OUT;
					break;

				case HFP_IND_CALLSETUP_OUT_ALERT :
					INFO("Remote device being altered!!!\n");
					bt_state = HFP_STATE_CALL_OUT_RINGING;
					break;

				default:
					break;
			}
			break;

		default:
			break;
	}
}

static int bt_state_read(int fd)
{
	char msg[64];
	int byte, i, value = -1, event =-1;
	if (fd < 0) {
		INFO("BT fd is error!\n");
		return -1;
	}
	memset(msg, 0, sizeof(msg));
	byte = recv(fd, msg, sizeof(msg), 0);
	if (byte < 0) {
		INFO("rcv error!\n");
	} else if (byte == 0) {
		INFO("the server is offline, need to reconnect!\n");
		bt_state = HFP_STATE_DISCONNECT;
		check_to_start_timer();
		return -1;
	} else if (byte >= 2) {
		INFO("incoming msg of %d bytes\n", byte);
		for (i = 0; i + 1 < byte; i += 2) {
			event = msg[i];
			value = msg[i + 1];
			INFO("event = %d, value = %d\n", event, value);
			msg_handler(event, value);
		}
	}
	return 0;
}

static void *bt_monitor_pthread(void * arc)
{
	int bt_fd, res;
	struct pollfd pollfds;
init:
	if ((bt_fd = bt_monitor_init()) < 0) {
		sleep(5);
		goto init;
	}

	pollfds.fd = bt_fd;
	pollfds.events = POLLIN;
	while (1) {
		poll(&pollfds, 1, -1);
		if (pollfds.revents & POLLIN) {
			pollfds.revents = 0;
			res=bt_state_read(bt_fd);
			if (res < 0) {
				close(bt_fd);
				goto init;
			}
		}
	}
	close(bt_fd);
	return NULL;
}

static int key_monitor_init(void)
{
	int fd = -1;
	char name[256];
	DIR* dir = opendir("/dev/input");
	if (dir != NULL) {
		struct dirent* de;
		while ((de = readdir(dir))) {
			if (strncmp(de->d_name, "event", 5)) continue;
			fd = openat(dirfd(dir), de->d_name, O_RDONLY);
			if (fd < 0) continue;
			ioctl(fd, EVIOCGNAME(sizeof(name)), name);
			if (!strcmp(ADC_INPUT_EVENT_NAME, name))
				break;
			close(fd);
			fd = -1;
		}
		closedir(dir);
	}
	return fd;
}

static int key_state_read(int fd)
{
	int rd, i;
	struct input_event ev[5];

	if (fd < 0) {
		INFO("ADC-KEY fd is error!\n");
		return -1;
	}

	memset(ev, 0, sizeof(ev));
	rd = read(fd, ev, sizeof(ev));

	if (rd < (int) sizeof(struct input_event)) {
		INFO("expected %d bytes, got %d\n", (int) sizeof(struct input_event), rd);
		perror("\nevtest: error reading");
		return 1;
	}

	for (i = 0; i < rd / sizeof(struct input_event); i++) {
		if (ev[i].type !=0 ) {
			if (ev[i].value == 1) {
				INFO("Have a key press!\n");
				stop_timer();
			} else if (ev[i].value == 0) {
				INFO("Have a key release!\n");
				check_to_start_timer();
			}
		}
	}
	return 0;
}

static void timer_handle(int sig)
{
	system("/etc/adckey/adckey_function.sh longpresspower");
}

static int set_timer_handle(void (*fun)(int))
{
	if (fun == NULL)
		return -1;
	signal(SIGALRM, fun);
	return 0;
}

static int start_timer(unsigned int nsec)
{
	struct itimerval tick;
	memset(&tick, 0, sizeof(tick));
	tick.it_value.tv_sec = nsec;
	tick.it_value.tv_usec = 0;

	tick.it_interval.tv_sec = 0;
	tick.it_interval.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &tick, NULL) < 0) {
		INFO("Set timer failed!\n");
		return -1;
	}
	INFO("Start timer!\n");
	return 0;
}

static int stop_timer(void)
{
	struct itimerval tick;
	memset(&tick, 0, sizeof(tick));
	tick.it_value.tv_sec = 0;
	tick.it_value.tv_usec = 0;

	tick.it_interval.tv_sec = 0;
	tick.it_interval.tv_usec = 0;

	if (setitimer(ITIMER_REAL, &tick, NULL) < 0) {
		INFO("Set timer failed!\n");
		return -1;
	}
	INFO("Stop timer!\n");
	return 0;
}

static int check_to_start_timer(void)
{
	if (usb_state == USB_DISCONNECT
		&& bt_state == HFP_STATE_DISCONNECT)
		start_timer(timing_time_sec);
	return 0;
}

int main(int argc, char **argv)
{
	int usb_fd, key_fd;
	int timing_time_min;
	pthread_t bt_thread_id;
	struct pollfd pollfds[2];
	const char *cmd_usage = {
		"Usage:\n"
		"\t Automatic shutdown when idle \n"
		"\t auto_shutdown <Timing time> \n"
		"\t <Timing time>: 5 ~ 90\n"
		"\t MIN:5min  MAX:90min  Default:45min\n"
	};

	if (argc != 2) {
		printf("%s", cmd_usage);
		return -1;
	}

	if ((timing_time_min = atoi(argv[1])) != 0) {
		if (timing_time_min < POWER_OFF_TIME_MIN || timing_time_min > POWER_OFF_TIME_MAX)
			timing_time_min = POWER_OFF_TIME_DEFAULT;
	} else {
		timing_time_min = POWER_OFF_TIME_DEFAULT;
	}
	INFO("Timing time: %d min\n", timing_time_min);
	timing_time_sec = timing_time_min * MIN_TO_SEC;

	memset(pollfds, 0, sizeof(pollfds));
	/*usb*/
	usb_fd = usb_monitor_init();
	if (usb_fd > 0) {
		pollfds[0].fd = usb_fd;
		pollfds[0].events = POLLIN;
		usb_state_change();
		check_to_start_timer();
	}
	/*adc-key*/
	key_fd = key_monitor_init();
	if (key_fd > 0) {
		pollfds[1].fd = key_fd;
		pollfds[1].events = POLLIN;
	}
	/*bt*/
	int res = pthread_create(&bt_thread_id, NULL, bt_monitor_pthread, NULL);
	if (res < 0)
		INFO("bt monitor pthread creat fail!\n");

	set_timer_handle(timer_handle);
	while (1)
	{
		poll(pollfds, 2, -1);

		for (int i = 0; i < 2; i++) {
			if (pollfds[i].revents & POLLIN) {
				pollfds[i].revents = 0;
				if (pollfds[i].fd == usb_fd)
					usb_state_read(usb_fd);
				else if (pollfds[i].fd == key_fd)
					key_state_read(key_fd);
			}
		}
	}
	if (key_fd > 0)
		close(key_fd);
	if (usb_fd > 0)
		close(usb_fd);
}
