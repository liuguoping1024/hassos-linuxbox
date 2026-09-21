#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

void simulate_key(int fd, int kval) {
  struct input_event event;
  int ret;

  gettimeofday(&event.time, 0);
  event.type = EV_KEY;
  event.code = kval;
  event.value = 1;
  ret = write(fd, &event, sizeof(event));

  event.type = EV_SYN;
  event.code = SYN_REPORT;
  event.value = 0;
  ret = write(fd, &event, sizeof(event));

  memset(&event, 0, sizeof(event));
  gettimeofday(&event.time, NULL);
  event.type = EV_KEY;
  event.code = kval;
  event.value = 0;
  ret = write(fd, &event, sizeof(event));

  event.type = EV_SYN;
  event.code = SYN_REPORT;
  event.value = 0;
  ret = write(fd, &event, sizeof(event));
}

int main(int argc, char *argv[]) {
  unsigned int keycode;
  int fd_kbd;
  char eventname[32];
  FILE *fp;
  const char *deviceInfo = "/proc/bus/input/devices";

  char *p;
  char *buf = 0;
  size_t readLen = 0;

  unsigned char foundDev = 0;

  if (argc != 2) {
    printf("Usage: simulate_key keycode\n");
    return -1;
  }

  fp = fopen(deviceInfo, "r");
  if (fp == NULL) {
    printf("error to open input devices info\n");
    return -1;
  }

  while ((readLen = getline(&buf, &readLen, fp)) != -1) {
    if (buf) {
      if (!foundDev) {
        p = strstr(buf, "N: Name=\"ir_keypad\"");
        if (p != NULL) {
          foundDev = 1;
        }
      } else {
        p = strstr(buf, "H: Handlers=");
        if (p != NULL) {
          p = strstr(buf, "event");
          char event[8];
          sscanf(p, "%s", event);
          sprintf(eventname, "/dev/input/%s", event);
          break;
        }
      }
    }
  }

  if (buf) {
    free(buf);
    buf = NULL;
  }

  fclose(fp);

  if (!foundDev) {
    printf("no input device found\n");
    return -1;
  }

  fd_kbd = open(eventname, O_RDWR);

  if (fd_kbd <= 0) {
    printf("error open keyboard %s\n", eventname);
    return -1;
  }

  keycode = strtoul(argv[1], NULL, 0);
  printf("send simulate event %s keycode: %d (0x%x)!\n", eventname, keycode,
         keycode);
  simulate_key(fd_kbd, keycode);

  close(fd_kbd);
}
