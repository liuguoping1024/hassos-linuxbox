/*
 * Copyright (C) 2014-2019 Amlogic, Inc. All rights reserved.
 *
 * All information contained herein is Amlogic confidential.
 *
 * This software is provided to you pursuant to Software License Agreement
 * (SLA) with Amlogic Inc ("Amlogic"). This software may be used
 * only in accordance with the terms of this agreement.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification is strictly prohibited without prior written permission from
 * Amlogic.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <pthread.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
//#define _GNU_SOURCE
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <sys/inotify.h>

#include "aml_log.h"

typedef enum {
  AML_DEBUG_LOG,
  AML_TRACE_LOG,
} AMLLogType_e;

extern char *__progname;
__thread int sc_level__ = 0;
static __thread int trace_seq = 0;
static int global_trace_seq = 0;
static pthread_mutex_t g_aml_log_lock = PTHREAD_MUTEX_INITIALIZER;
static struct AmlLogCat AML_LOG_LAST = {NULL, 0, 0, NULL};
struct AmlLogCat AML_LOG_DEFAULT = {"default", 1, 1, &AML_LOG_LAST};
static struct AmlLogCat *g_aml_log_cat_list = &AML_LOG_DEFAULT;

struct log_setting_node {
    struct log_setting_node *next;
    int level;
    regex_t pattern;
};

static struct log_setting_node *g_log_setting = NULL;
static struct log_setting_node *g_trace_setting = NULL;

// locked
static int get_log_level(const char *name) {
    struct log_setting_node *node = g_log_setting;
    for (; node != NULL; node = node->next) {
        if (regexec(&node->pattern, name, 0, NULL, 0) == 0)
            return node->level;
    }
    return AML_LOG_DEFAULT.log_level;
}

// locked
static int get_trace_level(const char *name) {
    struct log_setting_node *node = g_trace_setting;
    for (; node != NULL; node = node->next) {
        if (regexec(&node->pattern, name, 0, NULL, 0) == 0)
            return node->level;
    }
    return AML_LOG_DEFAULT.trace_level;
}

static void aml_log_add(struct AmlLogCat *cat) {
  pthread_mutex_lock(&g_aml_log_lock);
  cat->next = g_aml_log_cat_list;
  g_aml_log_cat_list = cat;
  cat->log_level = get_log_level(cat->name);
  cat->trace_level = get_trace_level(cat->name);
  pthread_mutex_unlock(&g_aml_log_lock);
}

static int level_str_to_level(const char *level_str) {
  char *log_level[] = { "LOG_QUIET", "LOG_ERR", "LOG_WARNING", "LOG_INFO",
                        "LOG_DEBUG", "LOG_VERBOSE", NULL };
  int level = 0;

  while (log_level[level]) {
    if (!strncmp(log_level[level], level_str, strlen(log_level[level])))
      break;
    level++;
  }

  if (log_level[level] == NULL) {
    printf("level str error: %s\n", level_str);
    level = -1;
  }

  return level;
}

static void get_setting_from_string(AMLLogType_e type, const char *str) {
    const char *key = str;
    const char *val = NULL;
    char buf[256];
    struct log_setting_node *newhead = NULL;
    struct log_setting_node *tail = NULL;
    struct log_setting_node *node, *node_prev = NULL, *tmp_node;
    struct log_setting_node *head;
    struct AmlLogCat *cat = g_aml_log_cat_list;

    if (type == AML_DEBUG_LOG)
      head = g_log_setting;
    else
      head = g_trace_setting;

    for (const char *p = str;; ++p) {
        if (key) {
            if (*p == ':') {
                snprintf(buf, sizeof(buf), "%.*s", (int)(p - key), key);
                key = NULL;
                val = p + 1;
                //printf("name:%s\n", buf);
            }
        } else {
            if (*p == ',' || *p == '\0') {
                node = malloc(sizeof(*node));
                if (regcomp(&node->pattern, buf, REG_NOSUB) != 0) {
                    printf("failed to compile regex '%s'\n", buf);
                    free(node);
                } else {
                    snprintf(buf, sizeof(buf), "%.*s", (int)(p - val), val);
                    //printf("level:%s\n", buf);
                    //node->level = atoi(buf);
                    node->level = level_str_to_level(buf);
                    if (node->level < 0) {
                        regfree(&node->pattern);
                        free(node);
                    } else {
                        node->next = NULL;
                        if (newhead) {
                            tail->next = node;
                        } else {
                            newhead = node;
                        }
                        tail = node;
                    }
                }
                key = p + 1;
            }
        }
        if (*p == '\0')
            break;
    }

    if (newhead == NULL) {
      printf("not match log level setting\n");
      return;
    }

    pthread_mutex_lock(&g_aml_log_lock);

    node = newhead;
    // update all module
    if (regexec(&node->pattern, "all", 0, NULL, 0) == 0) {
      for (cat = g_aml_log_cat_list; cat != NULL; cat = cat->next) {
        if (cat->name) {
          if (type == AML_DEBUG_LOG) {
            cat->log_level = node->level;
            //printf("change name:%s, debug_level:%d\n", cat->name, cat->log_level);
          } else {
            cat->trace_level = node->level;
            //printf("change name:%s, trace_level:%d\n", cat->name, cat->trace_level);
          }
        }
      }
      node = node->next;
    }

    // update sub module
    for (; node != NULL;) {
      for (cat = g_aml_log_cat_list; cat != NULL; cat = cat->next) {
        if (cat->name) {
          //printf("%s get name:%s, debug_level:%d, trace_level:%d\n",
          //  __progname, cat->name, cat->log_level, cat->trace_level);
          if (regexec(&node->pattern, cat->name, 0, NULL, 0) == 0) {
            if (type == AML_DEBUG_LOG) {
              cat->log_level = node->level;
              //printf("change name:%s, debug_level:%d\n", cat->name, cat->log_level);
            } else {
              cat->trace_level = node->level;
              //printf("name:%s, trace_level:%d\n", cat->name, cat->trace_level);
            }
            break;
          }
        }
      }

      // no match in cat_list, save the node
      if (cat == NULL) {
        if (node_prev == NULL)
          newhead = node->next;
        else
          node_prev->next = node->next;

        tmp_node = node;
        node = node->next;

        tmp_node->next = head;
        head = tmp_node;
      } else {
        node_prev = node;
        node = node->next;
      }
    }

    if (type == AML_DEBUG_LOG)
      g_log_setting = head;
    else
      g_trace_setting = head;

    pthread_mutex_unlock(&g_aml_log_lock);

    // release newhead
    while (newhead) {
        node = newhead;
        newhead = node->next;
        regfree(&node->pattern);
        free(node);
    }
}

static void aml_log_sync_config_file(const char *level_str) {
  char file_name[64];
  FILE *file_fd = NULL;

  snprintf(file_name, sizeof(file_name), "/tmp/AML_LOG_%s", __progname);
  file_fd = fopen(file_name, "w");
  if (file_fd) {
    //printf("output str \"%s\" to file %s\n", level_str, file_name);
    fputs(level_str, file_fd);
    fclose(file_fd);
  }
}

char level_str[256] = {0};
void aml_log_set_from_string(const char *str) {
    if (!strncmp(level_str, str, strlen(str))) return;
    strncpy(level_str, str, 256);
    aml_log_sync_config_file(str);
    get_setting_from_string(AML_DEBUG_LOG, str);
}

void aml_trace_set_from_string(const char *str) {
    get_setting_from_string(AML_TRACE_LOG, str);
}

void aml_log_parse(void) {
  char file_name[64];
  FILE *file_fd = NULL;
  int size = 0;
  char *buf;

  snprintf(file_name, sizeof(file_name), "/tmp/AML_LOG_%s", __progname);
  file_fd = fopen(file_name, "r");
  if (file_fd == NULL) {
    return;
  }

  fseek(file_fd, 0, SEEK_END);
  size = ftell(file_fd);
  rewind(file_fd);

  buf = (char*)calloc(1, sizeof(char)*size+1);
  if (fread(buf, 1, size, file_fd) > 0) {
    // printf("buf: %s\n", buf);
    aml_log_set_from_string(buf);
  }
  free(buf);

  fclose(file_fd);
  return;
}

#define EVENT_SIZE  ( sizeof (struct inotify_event) )
#define BUF_LEN     ( 16 * ( EVENT_SIZE + 16 ) )
static void *aml_log_threadloop(void *arg) {
  char file_name[64];
  FILE *file_fd = NULL;
  char buffer[BUF_LEN];
  int notify_fd;
  int watch_desc;
  unsigned int watch_flag = IN_CLOSE_WRITE | IN_CREATE | IN_DELETE |
                            IN_DELETE_SELF | IN_MOVE | IN_MOVE_SELF | IN_IGNORED;

  (void)arg;

  pthread_detach(pthread_self());

  snprintf(file_name, sizeof(file_name), "/tmp/AML_LOG_%s", __progname);
  file_fd = fopen(file_name, "a+");
  if (file_fd) {
    int size = 0;

    fseek(file_fd, 0, SEEK_END);
    size = ftell(file_fd);
    if (size == 0) {
      //printf("output str \"all:LOG_ERR\" to file %s\n", file_name);
      fputs("all:LOG_ERR", file_fd);
    }
    fclose(file_fd);
    aml_log_parse();
  }

  notify_fd = inotify_init();
  if (notify_fd < 0) {
    perror("inotify_init");
    pthread_exit(NULL);
  }

  watch_desc = inotify_add_watch(notify_fd, file_name, watch_flag);
  if (watch_desc < 0) {
    perror("inotify_add_watch");
    pthread_exit(NULL);
  }

  while (1) {
    if (read(notify_fd, buffer, BUF_LEN) < 0) {
      printf("read error\n");
    } else {
      aml_log_parse();
    }
  }

  pthread_exit(NULL);
}

static const char *log_level_name[] = {"ERR", "WARNING", "INFO", "DEBUG", "VERBOSE",
                                       "VV",  "VVV",  "",     "",      ""};

static FILE *log_fp = NULL;
void aml_log_set_output_file(FILE *fp) { log_fp = fp; }
pthread_t aml_log_pthread_id = 0;

void aml_log_msg(struct AmlLogCat *cat, int level, const char *file, const char *function,
                     int lineno, const char *fmt, ...) {
    if (aml_log_pthread_id == 0) {
      pthread_create(&aml_log_pthread_id, NULL, aml_log_threadloop, NULL);
    }

    if (cat->next == NULL) {
        aml_log_add(cat);
        if (!AML_LOG_CAT_ENABLE(cat, level))
            return;
    }

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    char buf[4096];
    //const char *fname = strrchr(file, '/');
    //fname = (fname == NULL) ? file : (fname + 1);
    int len = sprintf(buf, "%ld.%06ld %s[%d] %s %s tid:%ld (%s %d %s): ", ts.tv_sec, (ts.tv_nsec / 1000) % 1000000,
                      __progname, getpid(), cat->name, log_level_name[level],
                      syscall(SYS_gettid), file, lineno, function);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(&buf[len], sizeof(buf) - len, fmt, ap);
    va_end(ap);
    fprintf(log_fp ?: stdout, "%s", buf);
}

static FILE *trace_json_fp = NULL;
void aml_trace_set_json_output(FILE *fp) {
    if (fp != NULL)
        fprintf(fp, "[");
    ++global_trace_seq;
    trace_json_fp = fp;
}

static void trace_thread_name(FILE * fp) {
    char tname[32];
    prctl(PR_GET_NAME, tname, NULL, NULL, NULL);
    fprintf(fp,
            "{\"name\":\"thread_name\",\"ph\":\"M\",\"pid\":%ld,\"tid\":%ld,"
            "\"args\":{\"name\":\"%s\"}},\n",
            syscall(SYS_getpid), syscall(SYS_gettid), tname);
}

void stop_scope_timer__(struct AmlScopeTimer **pptimer) {
    --sc_level__;
    if (*pptimer == NULL)
        return;
    struct AmlScopeTimer *timer = *pptimer;
    struct AmlLogCat *cat = timer->cat;
    if (cat->next == NULL) {
        aml_log_add(cat);
    }
    if (!AML_TRACE_CAT_ENABLE(cat, timer->level))
        return;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    int64_t time_ns = (ts.tv_sec - timer->start_time.tv_sec) * 1000000000LL +
                      (ts.tv_nsec - timer->start_time.tv_nsec);
    if (trace_json_fp) {
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
        int64_t ttime_ns = (ts.tv_sec - timer->start_ttime.tv_sec) * 1000000000LL +
                           (ts.tv_nsec - timer->start_ttime.tv_nsec);
        if (global_trace_seq != trace_seq) {
            trace_seq = global_trace_seq;
            trace_thread_name(trace_json_fp);
        }
        fprintf(trace_json_fp,
                "{\"name\":\"%s\",\"cat\":\"%s\",\"ph\":\"X\",\"ts\":%lld,\"tts\":%lld,\"dur\":%d,"
                "\"tdur\":%d,\"pid\":%ld,\"tid\":%ld,\"args\":{\"text\":\"%s\"}},\n",
                timer->func, cat->name,
                timer->start_time.tv_sec * 1000000LL + timer->start_time.tv_nsec / 1000,
                timer->start_ttime.tv_sec * 1000000LL + timer->start_ttime.tv_nsec / 1000,
                (int)(time_ns / 1000), (int)(ttime_ns / 1000), syscall(SYS_getpid),
                syscall(SYS_gettid), timer->buf);
        return;
    }
    fprintf(log_fp ?: stdout, "%ld.%06ld %d %s[%d] %s %s tid:%ld (%s %d %s): SCTIME %d.%06d ms %s\n",
            ts.tv_sec, (ts.tv_nsec / 1000) % 1000000, sc_level__,
            __progname, getpid(), cat->name, log_level_name[timer->level],
            syscall(SYS_gettid), timer->file, timer->line, timer->func,
            (int)(time_ns / 1000000LL), (int)(time_ns % 1000000LL), timer->buf);
}

void aml_trace_log(struct AmlLogCat *cat, int level, const char *evcat, const char *evname,
                       int evph, void *evid, const char *fmt, ...) {
    if (cat->next == NULL) {
        aml_log_add(cat);
        if (!AML_TRACE_CAT_ENABLE(cat, level))
            return;
    }
    if (trace_json_fp) {
        if (global_trace_seq != trace_seq) {
            trace_seq = global_trace_seq;
            trace_thread_name(trace_json_fp);
        }
        struct timespec ts,tts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &tts);
        char buf[4096];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        fprintf(trace_json_fp,
                "{\"name\":\"%s\",\"cat\":\"%s\",\"ph\":\"%c\",\"ts\":%lld,\"tts\":%lld,\"pid\":%"
                "ld,\"tid\":%ld,\"id\":\"%p\",\"args\":{\"text\":\"%s\"}},\n",
                evname, evcat, evph, ts.tv_sec * 1000000LL + ts.tv_nsec / 1000,
                tts.tv_sec * 1000000LL + tts.tv_nsec / 1000, syscall(SYS_getpid),
                syscall(SYS_gettid), evid, buf);
    }
}
