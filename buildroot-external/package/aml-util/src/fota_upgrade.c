#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

#include "../fota/dmgr_api.h"
#include "../aml_utility/unifykey.h"
#include "aml_device_property.h"

#define CHECK_NETWORK_DEALY 20 //60
#define INFO_LEN  128

struct test_context {
  char DEVINFO_OEM[INFO_LEN];
  char DEVINFO_DEVICETYPE[INFO_LEN];
  char DEVINFO_PLATFORM[INFO_LEN];
  char DEVINFO_MODELS[INFO_LEN];
  char DEVINFO_VERSION[INFO_LEN];
  char DEVINFO_PRODUCTID[INFO_LEN];
  char DEVINFO_PRODUCTSECRET[INFO_LEN];
  const char *DEVINFO_APPVERSION;
  const char *DEVINFO_NETWORKTYPE;
  const char *DEVINFO_CHK_URL;
  char DEVINFO_MID[256];

  int DEVINFO_SERVER_OPT;

  struct notifier_block nb_checkversion;
  struct notifier_block nb_download;

  int has_new_version;
  struct version_info cloned_version;
  int download_exit;

};

#define INIT_DEFAULT_TEST_CONTEXT() \
{ \
  .DEVINFO_OEM        = "Amlogic", \
  .DEVINFO_DEVICETYPE = "smarthome", \
  .DEVINFO_PLATFORM   = "A113D", \
  .DEVINFO_MODELS     = "OTT", \
  .DEVINFO_MID        = "20220809043042",\
  .DEVINFO_VERSION    = "0.5.0", \
  .DEVINFO_PRODUCTID  = "1660115992", \
  .DEVINFO_PRODUCTSECRET = "fa7bfc8d755c4c0b8c52fdb00d431d3f", \
  .DEVINFO_APPVERSION = "2", \
  .DEVINFO_NETWORKTYPE= "WIFI", \
  .DEVINFO_CHK_URL    = NULL, \
  .DEVINFO_SERVER_OPT = SERVER_TRANSOPT_IOT| SERVER_TRANSOPT_HTTP, \
  .has_new_version = 0, \
  .download_exit = 0, \
}

#define TEST_ASSERT_EQUAL_INT(expected, actual) do{ if(expected != actual) return -1;}while(0)

#ifndef CONTAINER_OF
#define CONTAINER_OF(ptr, type, member) \
  (type *)((char *)(ptr) - (char *) &((type *)0)->member)
#endif

#define FOTA_WEB_SERVER "www.abupdate.com"
#define AML_SYS_SAVE_PATH  "/data"

static char *__save_file_path = "/data/software.swu";
static char *__version_file_path = "/version.txt";
static char *__new_version_file = "new_version";

static int write_new_version(struct version_info *ver) {
  FILE *ver_fd = NULL;
  char *save_path = AML_SYS_SAVE_PATH;
  char cmd_buf[200];

  snprintf(cmd_buf, 200, "%s/%s", save_path, __new_version_file);
  if (access(cmd_buf, F_OK) != -1) {
    snprintf(cmd_buf, 200, "rm %s/%s", save_path, __new_version_file);
    system(cmd_buf);
  }

  snprintf(cmd_buf, 200, "%s/%s", save_path, __new_version_file);
  ver_fd = fopen(cmd_buf, "w+");
  if (NULL == ver_fd) {
    printf("Cannot open version file %s\n", cmd_buf);
    return -1;
  }

  fprintf(ver_fd, "version_name:%s\n", ver->version_name);
  //fprintf(ver_fd, "file_size:%d\n", ver->file_size);
  //fprintf(ver_fd, "delta_id:%s\n", ver->delta_id);
  //fprintf(ver_fd, "md5sum:%s\n", ver->md5sum);
  //fprintf(ver_fd, "delta_url:%s\n", ver->delta_url);
  //fprintf(ver_fd, "release_note:%s\n", ver->release_note);
  //fprintf(ver_fd, "upgrade_from_time:%s\n", ver->upgrade_from_time);
  //fprintf(ver_fd, "upgrade_to_time:%s\n", ver->upgrade_to_time);
  //fprintf(ver_fd, "upgrade_gap:%s\n", ver->upgrade_gap);
  //fprintf(ver_fd, "meta_data:%s\n", ver->meta_data);
  //fprintf(ver_fd, "event_id:%s\n", ver->event_id);

  fclose(ver_fd);
  return 0;
}

static char *simple_strndup(const char *s, size_t n)
{
  size_t max_len = strnlen(s, n);

  char *dest = malloc(max_len + 1);
  if (dest) {
    memcpy(dest, s, max_len);
    dest[max_len] = '\0';
  }

  return dest;
}

static char *simple_strdup(const char *s)
{
  return simple_strndup(s, 0x7fffff);
}

static void clone_version(struct test_context *test_ctx, struct version_info *info)
{
  test_ctx->cloned_version.file_size = info->file_size;
  test_ctx->cloned_version.flags = info->flags;

  if (info->version_name) {
    test_ctx->cloned_version.version_name = simple_strdup(info->version_name);
  }
  if (info->delta_id) {
    test_ctx->cloned_version.delta_id = simple_strdup(info->delta_id);
  }
  if (info->md5sum) {
    test_ctx->cloned_version.md5sum = simple_strdup(info->md5sum);
  }
  if (info->delta_url) {
    test_ctx->cloned_version.delta_url = simple_strdup(info->delta_url);
  }
  if (info->release_note) {
    test_ctx->cloned_version.release_note = simple_strdup(info->release_note);
  }
  if (info->upgrade_from_time) {
    test_ctx->cloned_version.upgrade_from_time = strdup(info->upgrade_from_time);
  }
  if (info->upgrade_to_time) {
    test_ctx->cloned_version.upgrade_to_time = strdup(info->upgrade_to_time);
  }
  if (info->upgrade_gap) {
    test_ctx->cloned_version.upgrade_gap = strdup(info->upgrade_gap);
  }
  if (info->meta_data) {
    test_ctx->cloned_version.meta_data = simple_strdup(info->meta_data);
  }
  if (info->event_id) {
    test_ctx->cloned_version.event_id = strdup(info->event_id);
  }
}

static void free_cloned_version(struct test_context *test_ctx)
{
  struct version_info *info = &test_ctx->cloned_version;

  if (!test_ctx->has_new_version) {
    return;
  }

  if (info->version_name) {
    free(info->version_name);
  }
  if (info->delta_id) {
    free(info->delta_id);
  }
  if (info->md5sum) {
    free(info->md5sum);
  }
  if (info->delta_url) {
    free(info->delta_url);
  }
  if (info->release_note) {
    free(info->release_note);
  }
  if (info->upgrade_from_time) {
    free(info->upgrade_from_time);
  }
  if (info->upgrade_to_time) {
    free(info->upgrade_to_time);
  }
  if (info->upgrade_gap) {
    free(info->upgrade_gap);
  }
  if (info->meta_data) {
    free(info->meta_data);
  }
  if (info->event_id) {
    free(info->event_id);
  }

  memset(info, 0x00, sizeof(*info));
  test_ctx->has_new_version = 0;
}

static int checkversion_notifier(struct notifier_block *nb,
                unsigned long action,
                void *data)
{
  struct test_context *test_ctx = CONTAINER_OF(nb, struct test_context, nb_checkversion);

  switch (action) {
  case NOTIFY_CHECKVERSION_PREPARE:
    printf("[callback: checkversion_prepare]\n");
    break;

  case NOTIFY_CHECKVERSION_ABORTED:
    printf("[callback: checkversion_abored]\n");
    break;

  case NOTIFY_CHECKVERSION_FINISHED:
    printf("[callback: checkversion_finished]\n");
    if (data) {
      printf("[callback: new version %s]\n", ((struct version_info *)data)->version_name);
      test_ctx->has_new_version = 1;
      clone_version(test_ctx, data);
    }
    break;
  }

  return 0;
}

static int download_notifier(struct notifier_block *nb,
              unsigned long action,
              void *data)
{
  struct test_context *test_ctx = CONTAINER_OF(nb, struct test_context, nb_download);
  notifier_data_t *notify_data = (notifier_data_t *)data;
  long total_Kbytes = notify_data->u.dlinfo.total_bytes/1024;
  long saved_Kbytes = notify_data->u.dlinfo.saved_bytes/1024;
  unsigned int percent = (saved_Kbytes * 100)/total_Kbytes;
  static unsigned int  pre_percent = 0;

  //printf("[download_notifier] percent:%d, total_Kbytes:%d, saved_Kbytes:%d\n", percent, total_Kbytes, saved_Kbytes);

  switch (action) {
  case NOTIFY_DOWNLOAD_PREPARE:
    printf("[callback: download_prepare]\n");
    pre_percent = 0;
    break;

  case NOTIFY_DOWNLOAD_ABORTED:
    printf("[callback: download_abored]\n");
    test_ctx->download_exit = -1;
    break;

  case NOTIFY_DOWNLOAD_TRANSLATING:
    if (percent < 100) {
      if (pre_percent != percent) {
        pre_percent = percent;
        printf("[callback: download_translating] percent:%d\n", percent);

        char cmd_buf[100];
        snprintf(cmd_buf, 100, "echo Downloading %d > /tmp/fota_stat", percent);
        system(cmd_buf);
      }
    }
    break;

  case NOTIFY_DOWNLOAD_FINISHED:
    test_ctx->download_exit = 1;
    percent = 100;
    printf("[callback: download_finished], percent:%d\n", percent);
    break;

  case NOTIFY_DOWNLOAD_PARTTIAL_FINISHED:
    printf("[callback: download_partial_finished]\n");
    test_ctx->download_exit = 1;
    break;
  }

  return 0;
}

static int init_notifier(dmgr_t dm_id, struct test_context *test_ctx)
{
  test_ctx->nb_checkversion.notifier_call = checkversion_notifier;
  test_ctx->nb_checkversion.priority = 0;

  test_ctx->nb_download.notifier_call = download_notifier;
  test_ctx->nb_download.priority = 0;

  TEST_ASSERT_EQUAL_INT(0, dmgr_register_notifier(dm_id,
            &test_ctx->nb_checkversion,
            NOTIFY_ID_CHECK_VERSION));

  TEST_ASSERT_EQUAL_INT(0, dmgr_register_notifier(dm_id,
            &test_ctx->nb_download,
            NOTIFY_ID_DOWNLOAD));

  return 0;
}

static void WAIT_DOWNLOAD_COMPLETED(struct test_context *test_ctx)
{
  if (!test_ctx) {
    return;
  }
  for (;;) {
    if (test_ctx->download_exit != 0) {
      printf(">>>>>download_exit = %d\n", test_ctx->download_exit);
      break;
    }
    sleep(1);
  }
}

static int getMid(char* readbuf)
{
  #define DEVICE_NAME "deviceid"
  int fd,ret = -1;

  ret = Aml_Util_UnifyKeyInit(UNIFYKEYS_PATH);
  if (0 > ret) {
    printf("%s() %d, init unifykey failed @ %d!\n",
      __func__, __LINE__, ret);
  }

  ret = Aml_Util_UnifyKeyRead(UNIFYKEYS_PATH, DEVICE_NAME, readbuf);
  return ret;
}

static int get_sw_version(char *sw_version) {
  FILE *ver_fd = NULL;
  char line[200];

  ver_fd = fopen(__version_file_path, "r");
  if (NULL == ver_fd) {
    printf("Cannot open version file %s\n", __version_file_path);
    return -1;
  }

  while (fgets(line, sizeof(line), ver_fd))
  {
    char *pLine = strstr(line, "imagename");
    if (NULL != pLine)
    {
      // 20220809043042
      memcpy(sw_version, pLine+strlen(pLine)-15, 14);
      break;
    }
  }

  fclose(ver_fd);

  return 0;
}

static int get_deviceinfo(struct test_context *test_ctx) {
  int ret;
  char out_value[INFO_LEN];

  ret = get_sw_version(test_ctx->DEVINFO_VERSION);

  // SDK_VERSION of device.properties, only can sync time base on clean build
  //ret = AmlDeviceGetProperty("SDK_VERSION", out_value, INFO_LEN);
  //if (ret == AMLDEVICE_SUCCESS) {
  //  memcpy(test_ctx->DEVINFO_VERSION, out_value, strlen(out_value));
  //}

  ret = AmlDeviceGetProperty("DEVINFO_OEM", out_value, INFO_LEN);
  if (ret == AMLDEVICE_SUCCESS) {
    memcpy(test_ctx->DEVINFO_OEM, out_value, strlen(out_value));
  }

  ret = AmlDeviceGetProperty("DEVINFO_DEVICETYPE", out_value, INFO_LEN);
  if (ret == AMLDEVICE_SUCCESS) {
    memcpy(test_ctx->DEVINFO_DEVICETYPE, out_value, strlen(out_value));
  }

  ret = AmlDeviceGetProperty("DEVINFO_PLATFORM", out_value, INFO_LEN);
  if (ret == AMLDEVICE_SUCCESS) {
    memcpy(test_ctx->DEVINFO_PLATFORM, out_value, strlen(out_value));
  }

  ret = AmlDeviceGetProperty("DEVINFO_MODELS", out_value, INFO_LEN);
  if (ret == AMLDEVICE_SUCCESS) {
    memcpy(test_ctx->DEVINFO_MODELS, out_value, strlen(out_value));
  }

  ret = AmlDeviceGetProperty("DEVINFO_PRODUCTID", out_value, INFO_LEN);
  if (ret == AMLDEVICE_SUCCESS) {
    memcpy(test_ctx->DEVINFO_PRODUCTID, out_value, strlen(out_value));
  }

  ret = AmlDeviceGetProperty("DEVINFO_PRODUCTSECRET", out_value, INFO_LEN);
  if (ret == AMLDEVICE_SUCCESS) {
    memcpy(test_ctx->DEVINFO_PRODUCTSECRET, out_value, strlen(out_value));
  }

  return ret;
}

static int report_upgraded_version(dmgr_t dm_id, char *ver) {
  struct version_info cur_ver;
  int ret;

  if (ver == NULL) return -1;

  memset(&cur_ver, 0, sizeof(struct version_info));
  cur_ver.version_name = ver;
  ret = dmgr_report_upgraded_version(dm_id, &cur_ver, "Upgrade done");
  printf("dmgr_report_upgraded_version %s ret = %d\n", ver, ret);

  return ret;
}

#define STR_VALUE(val) #val
#define STR(name) STR_VALUE(name)

#define PATH_LEN 256
#define MD5_LEN 32

static int CalcFileMD5(char *file_name, char *md5_sum)
{
#define MD5SUM_CMD_FMT "md5sum %." STR(PATH_LEN) "s 2>/dev/null"
  char cmd[PATH_LEN + sizeof (MD5SUM_CMD_FMT)];
  sprintf(cmd, MD5SUM_CMD_FMT, file_name);
#undef MD5SUM_CMD_FMT

  FILE *p = popen(cmd, "r");
  if (p == NULL) return 0;

  int i, ch;
  for (i = 0; i < MD5_LEN && isxdigit(ch = fgetc(p)); i++) {
    *md5_sum++ = ch;
  }

  *md5_sum = '\0';
  pclose(p);

  return i == MD5_LEN;
}

static int check_md5(char *md5sum) {
  char md5_str[MD5_LEN+1];

  if (!CalcFileMD5(__save_file_path, md5_str)) {
    printf("Cannot calculate the md5 sum\n");
    return -1;
  }

  printf("Calculate the md5 sum = %s\n", md5_str);
  if (!strncmp(md5_str, md5sum, MD5_LEN)) {
    printf("Check md5sum OK\n");
    return 0;
  }

  return -1;
}

int checkNet(const char *web_server) {
  char buffer[200];
  FILE *fp;
  char *p, num[3];

  snprintf(buffer, 200, "ping %s -c 1 -w 2 > /tmp/netlog", web_server);
  //printf("command: %s\n", buffer);
  system(buffer);
  //sleep(2);
  usleep(50000);

  fp = fopen("/tmp/netlog", "r");
  if (fp < 0) {
    printf("open file failed.\n");
    return 0;
  }

  fseek(fp, 0, SEEK_SET);
  memset(buffer, 0, sizeof(buffer));
  // read penultimate row, 1 packets transmitted, 1 received, 0% packet loss, time 0ms
  while (fgets(buffer, 200, fp) != NULL) {
    p = strstr(buffer, "received,");
    if (p) {
      //printf("string is: %s", p);
      memcpy(num, p+10, 3);
      //printf("num: %s\n", num);
      if (atoi(num) == 0)
        return 1;
      else
        return 0;
    }
    memset(buffer, 0, sizeof(buffer));
  }
  fclose(fp);

  system("rm /tmp/netlog");
  return 0;
}

int main(void)
{
  dmgr_t dm_id,ret;
  char rbuf[64];
  struct test_context test_ctx = INIT_DEFAULT_TEST_CONTEXT();
  struct policy_info policy = POLICY_INFO_INIT(policy);
  policy.sstate_cached_path = "/var";
  policy.sub_certificates  = "/var/client.crt";
  policy.server_transfer_opt = SERVER_TRANSOPT_IOT | SERVER_TRANSOPT_HTTP;

  ret = getMid(rbuf);
  if ((0 != strcmp(rbuf,"")) && (0 < ret)) {
    strcpy(test_ctx.DEVINFO_MID, rbuf);
  } else {
    printf("get mid fail\n");
    return -1;
  }

  ret = get_deviceinfo(&test_ctx);
  if (ret < 0) {
    printf("get deviceinfo fail\n");
    return -1;
  }

#if 1
  printf(" %s:  DEVINFO_OEM = %s \n",__func__,test_ctx.DEVINFO_OEM);
  printf(" %s:  DEVINFO_DEVICETYPE = %s \n",__func__,test_ctx.DEVINFO_DEVICETYPE);
  printf(" %s:  DEVINFO_PLATFORM = %s \n",__func__,test_ctx.DEVINFO_PLATFORM);
  printf(" %s:  DEVINFO_MODELS = %s \n",__func__,test_ctx.DEVINFO_MODELS);
  printf(" %s:  DEVINFO_MID = %s \n",__func__,test_ctx.DEVINFO_MID);
  printf(" %s:  DEVINFO_VERSION = %s \n",__func__,test_ctx.DEVINFO_VERSION);
  printf(" %s:  DEVINFO_PRODUCTID = %s \n",__func__,test_ctx.DEVINFO_PRODUCTID);
  printf(" %s:  DEVINFO_PRODUCTSECRET = %s \n",__func__,test_ctx.DEVINFO_PRODUCTSECRET);
  printf(" %s:  DEVINFO_APPVERSION = %s \n",__func__,test_ctx.DEVINFO_APPVERSION);
  printf(" %s:  DEVINFO_NETWORKTYPE = %s \n",__func__,test_ctx.DEVINFO_NETWORKTYPE);
#endif

  system("echo Checking_NewVersion > /tmp/fota_stat");
  while (1) {
    if (0 == checkNet(FOTA_WEB_SERVER)) {
      printf(" network not connect \n ");
      system("echo network_not_connect > /tmp/fota_stat");
      //sleep(CHECK_NETWORK_DEALY);
      return -1;
    }

    printf(" network  connected \n ");
    dm_id = dmgr_alloc(&policy, LOG_INFO, LOGGER_STDIO, stdout);
    if (0 >= dm_id) {
      return -1;
    }

    if (0 != dmgr_register_deviceinfo(dm_id,
        "oem", test_ctx.DEVINFO_OEM) ||
      0 != dmgr_register_deviceinfo(dm_id,
          "deviceType",test_ctx.DEVINFO_DEVICETYPE) ||
      0 != dmgr_register_deviceinfo(dm_id,
          "platform", test_ctx.DEVINFO_PLATFORM) ||
      0 != dmgr_register_deviceinfo(dm_id,
          "models", test_ctx.DEVINFO_MODELS) ||
      0 != dmgr_register_deviceinfo(dm_id,
          "mid", test_ctx.DEVINFO_MID) ||
      0 != dmgr_register_deviceinfo(dm_id,
          "version", test_ctx.DEVINFO_VERSION) ||
      0 != dmgr_register_deviceinfo(dm_id,
          "productId", test_ctx.DEVINFO_PRODUCTID) ||
      0 != dmgr_register_deviceinfo(dm_id,
          "productSecret", test_ctx.DEVINFO_PRODUCTSECRET) ||
      0 != dmgr_register_deviceinfo(dm_id,
          "appversion", test_ctx.DEVINFO_APPVERSION) ||
      0 != dmgr_register_deviceinfo(dm_id,
        "networkType", test_ctx.DEVINFO_NETWORKTYPE)) {
      TEST_ASSERT_EQUAL_INT(0, dmgr_free(dm_id));
      continue;
    }

    if (test_ctx.DEVINFO_CHK_URL) {
      if (0 != dmgr_register_serverinfo(dm_id,
                  SERI_CHK_URL,
                  test_ctx.DEVINFO_CHK_URL)) {
        TEST_ASSERT_EQUAL_INT(0, dmgr_free(dm_id));
        continue;
      }
    }

    init_notifier(dm_id, &test_ctx);
    if (0 != dmgr_check_version(dm_id)) {
      TEST_ASSERT_EQUAL_INT(0, dmgr_free(dm_id));
      printf(">>>> no new version detect\n");
      system("echo Check_NewVersion Failed> /tmp/fota_stat");
      break;
    }
    if (0 == test_ctx.has_new_version) {
      if ((access("/data/software.swu",F_OK)) != -1) {
        system("rm /data/software.swu");
      }
      TEST_ASSERT_EQUAL_INT(0, dmgr_free(dm_id));
      printf(" no new_version \n");
      system("echo No_NewVersion > /tmp/fota_stat");
      ret = 0;
      break;
    } else {
      printf(" ADUPS : has_new_version \n");
      system("echo NewVersion > /tmp/fota_stat");
    }

    system("echo downloading > /tmp/fota_stat");
    if (0 != dmgr_async_download_version(dm_id,
        &test_ctx.cloned_version,
        __save_file_path,
        DOWNLOAD_FILE_SEEK_END,
        0,
        -1)) {
        TEST_ASSERT_EQUAL_INT(0, dmgr_free(dm_id));
        system("echo Download_FAILED > /tmp/fota_stat");
        ret = -1;
        break;
    }

    WAIT_DOWNLOAD_COMPLETED(&test_ctx);

    if (test_ctx.has_new_version &&
      (test_ctx.download_exit == 1)) {
      write_new_version(&test_ctx.cloned_version);
      // ota package not match md5sum, remove software.swu
      if (check_md5(test_ctx.cloned_version.md5sum) == 0) {
        printf("secure upgrade check pass!\n");
        system("echo package_check_SUCCESS > /tmp/fota_stat");
        system("reboot recovery");
        ret = 1;
      } else {
        system("rm /data/software.swu");
        system("sync");
        TEST_ASSERT_EQUAL_INT(0, dmgr_free(dm_id));
        system("echo package_check_FAILED > /tmp/fota_stat");
        printf("ota package not match md5sum!\n");
        ret = -1;
      }
    }

    free_cloned_version(&test_ctx);
    break;
  }

  return ret;
}
