#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#define UEVENT_BUFFER_SIZE 2048
#define MIN_SIZE           30
#define USB_STATE_FILE     "/sys/class/android_usb/android0/state"
#define USB_CONFIG_ADB_CMD     "echo ff400000.dwc2_a > /sys/kernel/config/usb_gadget/amlogic/UDC"
#define USB_CONFIG_ADB_UDC     "/sys/kernel/config/usb_gadget/amlogic/UDC"
#define USB_CONFIG_ADB_UDC_VAL_FILE     "/etc/adb_udc_file"
#define ERROR_GET_CONTENT  -1
#define ERROR_USB_CONFIG_ADB  -2
#define SUCCESS            0

int UDC_config()
{
    char UdcValue[128] = "ff400000.dwc2_a";
    char UdcValue_check[MIN_SIZE] = {0};
    FILE *fp = NULL;
    fp = fopen(USB_CONFIG_ADB_UDC_VAL_FILE, "r");
    if (fp != NULL)
    {
        fgets(UdcValue, 128, fp);
        fclose(fp);
    }
    fp = NULL;

    fp = fopen(USB_CONFIG_ADB_UDC, "wb");
    if (fp != NULL)
    {
      //printf("udc config data!\n");
      if ( fputs("none", fp) < 0)
        printf("udc config data failure\n");
      fclose(fp);
    }
    fp = NULL;
    fp = fopen(USB_CONFIG_ADB_UDC, "wb");
    if (fp != NULL)
    {
      //printf("udc config data!\n");
      if ( fputs(UdcValue, fp) < 0)
        printf("udc config data failure\n");
      fclose(fp);
    }
    fp = NULL;

    fp = fopen(USB_CONFIG_ADB_UDC, "wb");
    if (fp != NULL)
    {
      //printf("udc config data!\n");
      if ( fputs(UdcValue, fp) < 0)
        printf("udc config data failure\n");
      fclose(fp);
    }
    fp = NULL;
    return 0;
}

int usb_configure()
{
    /*open android0 state node*/
    char FileName[MIN_SIZE] = {0};
    int flag = -1, ret = -1;
    FILE *fp = fopen(USB_STATE_FILE, "r");
    if (fp != NULL)
    {
        if (fgets(FileName, MIN_SIZE, fp) == NULL)
        {
            printf("get android0 state  content failure!\n");
            fclose(fp);
            ret = ERROR_GET_CONTENT;
        }
        else
        {
            //printf("content: %s\n", FileName);
            if(!strncmp("DISCONNECTED", (const char*)FileName, 12))
            {
                usleep(200*1000);   //Delay 200ms
                static time_t last = 0;
                time_t now;
                time(&now);
                if (now - last >= 1) {
                  UDC_config();
                } else {
                  printf("filter disconnected event in short time\n");
                }
                last = now;
                //system("echo ff400000.dwc2_a > /sys/kernel/config/usb_gadget/amlogic/UDC");
                ret = SUCCESS;
                //printf("config usb successfull!\n");
            }
             fclose(fp);
        }
    }
    else
        ret = ERROR_USB_CONFIG_ADB;
    fp = NULL;
    return ret;
}

int main(void)
{
    struct sockaddr_nl client;
    struct timeval tv;
    int Usbmonitor, rcvlen, ret;
    fd_set fds;
    int buffersize = 1024;
    Usbmonitor = socket(AF_NETLINK, SOCK_RAW, NETLINK_KOBJECT_UEVENT);
    if (0 > Usbmonitor) {
      return -1;
    }
    memset(&client, 0, sizeof(client));
    client.nl_family = AF_NETLINK;
    client.nl_pid = getpid();
    client.nl_groups = 1; /* receive broadcast message*/
    ret = setsockopt(Usbmonitor, SOL_SOCKET, SO_RCVBUF, &buffersize, sizeof(buffersize));
    if (0 > ret) {
      return -1;
    }
    ret = bind(Usbmonitor, (struct sockaddr*)&client, sizeof(client));
    if (0 > ret) {
      return -1;
    }
    while (1) {
        char buf[UEVENT_BUFFER_SIZE] = { 0 };
        FD_ZERO(&fds);
        FD_SET(Usbmonitor, &fds);
        tv.tv_sec = 0;
        tv.tv_usec = 100 * 1000;
        ret = select(Usbmonitor + 1, &fds, NULL, NULL, &tv);
        if (ret < 0) {
            continue;
        }
        if (!(ret > 0 && FD_ISSET(Usbmonitor, &fds))) {
              continue;
        }
        /* receive data */
        rcvlen = recv(Usbmonitor, &buf, sizeof(buf), 0);
        if (rcvlen > 0 && strstr(buf,"usb") ) {
          //printf("%s\n", buf);
          usb_configure();
        }
    }
    close(Usbmonitor);
    return 0;
}
