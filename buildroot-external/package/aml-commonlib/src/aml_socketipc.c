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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#include "aml_socketipc.h"

static int SIPC_LOG_LEVEL = -1;
#define SIPC_LOG(lev,fmt, ...) do {\
    if (SIPC_LOG_LEVEL < 0) {\
        const char * level = getenv("SIPC_LOG_LEVEL");\
        SIPC_LOG_LEVEL= level?atoi(level):2;\
    }\
    if (lev<SIPC_LOG_LEVEL)\
        printf("SIPC " fmt "\n", ## __VA_ARGS__);\
} while (0)
#define SIPC_LOG0(fmt, ...) SIPC_LOG(0, fmt, ## __VA_ARGS__)
#define SIPC_LOG1(fmt, ...) SIPC_LOG(1, fmt, ## __VA_ARGS__)
#define SIPC_LOG2(fmt, ...) SIPC_LOG(2, fmt, ## __VA_ARGS__)
#define SIPC_LOG3(fmt, ...) SIPC_LOG(3, fmt, ## __VA_ARGS__)
#define SIPC_LOG4(fmt, ...) SIPC_LOG(4, fmt, ## __VA_ARGS__)

struct SIPC_HandleInternal {
    char buffer[SIPC_MAX_MESSAGE_SIZE];
    struct sockaddr_un addr;
    socklen_t addrlen;
    int32_t ipc_id;
    int sockfd;
    int loop_level;
    int timeout_us;
    SIPC_Function * func_table;
    SIPC_Peer next;
    SIPC_Handle serv;
    SIPC_Handle next_serv;
};

static SIPC_Handle serv_list = NULL;
static pthread_t serv_pthread_id;
static pthread_mutex_t serv_mutex = PTHREAD_MUTEX_INITIALIZER;
static bool serv_status = false;

static int32_t _ipc_pack_number(struct SIPC_DataType * type, void * buf, int len, va_list * va) {
    if (len < type->len)
        return SIPC_ERR_PARAM;
    uint64_t val64 = (type->len == 8)?va_arg(*va, uint64_t):va_arg(*va, uint32_t);
    uint8_t * p = (uint8_t*)&val64;
    for (int n = type->len; n--; ) {
        ((uint8_t*)buf)[n] = *p++;
    }
    return type->len;
}

static int32_t _ipc_upack_number(struct SIPC_DataType * type, void * buf, int len, va_list *va) {
    if (len < type->len)
        return SIPC_ERR_PARAM;
    uint8_t * pval = va_arg(*va, void*);
    for (int n = type->len; n--; ) {
        *pval++ = ((uint8_t*)buf)[n];
    }
    return type->len;
}

static int32_t _ipc_pack_pointer(struct SIPC_DataType * type, void * buf, int len, va_list *va) {
    void* ptr = va_arg(*va, void*);
    int32_t tlen = type->type_id == SIPC_TYPE_ID(str) ? (ptr ? strlen(ptr) + 1 : 0) : va_arg(*va, int32_t);
    if ((tlen<0) || (len < tlen+4))
        return SIPC_ERR_PARAM;
    ((uint8_t*)buf)[0] = ((uint8_t*)&tlen)[3];
    ((uint8_t*)buf)[1] = ((uint8_t*)&tlen)[2];
    ((uint8_t*)buf)[2] = ((uint8_t*)&tlen)[1];
    ((uint8_t*)buf)[3] = ((uint8_t*)&tlen)[0];
    if (tlen) memcpy((uint8_t *)buf + 4, ptr, tlen);
    return tlen+4;
}

static int32_t _ipc_upack_pointer(struct SIPC_DataType * type, void * buf, int len, va_list *va) {
    int32_t tlen = ((uint32_t)((uint8_t*)buf)[0] << 24) | ((uint32_t)((uint8_t*)buf)[1] << 16) | ((uint32_t)((uint8_t*)buf)[2] << 8) | ((uint32_t)((uint8_t*)buf)[3]);
    if ((tlen<0) && (len < tlen+4))
        return SIPC_ERR_PARAM;
    void ** ptr = va_arg(*va, void**);
    *ptr = tlen ? buf + 4 : NULL;
    if (type->type_id == SIPC_TYPE_ID(ptr)) {
        int32_t * plen = va_arg(*va, int32_t*);
        *plen = tlen;
    }
    return tlen+4;
}

static struct SIPC_DataType g_ipc_buildin_types[] = {
    {0},
    {SIPC_TYPE_ID(i8),1,_ipc_pack_number,_ipc_upack_number,0},
    {SIPC_TYPE_ID(u8),1,_ipc_pack_number,_ipc_upack_number,0},
    {SIPC_TYPE_ID(i16),2,_ipc_pack_number,_ipc_upack_number,0},
    {SIPC_TYPE_ID(u16),2,_ipc_pack_number,_ipc_upack_number,0},
    {SIPC_TYPE_ID(i32),4,_ipc_pack_number,_ipc_upack_number,0},
    {SIPC_TYPE_ID(u32),4,_ipc_pack_number,_ipc_upack_number,0},
    {SIPC_TYPE_ID(i64),8,_ipc_pack_number,_ipc_upack_number,0},
    {SIPC_TYPE_ID(u64),8,_ipc_pack_number,_ipc_upack_number,0},
    {SIPC_TYPE_ID(str),4,_ipc_pack_pointer,_ipc_upack_pointer,0},
    {SIPC_TYPE_ID(ptr),4,_ipc_pack_pointer,_ipc_upack_pointer,0},
};

int32_t ipc_pack_args(void * buf, int len, int32_t * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int32_t r = ipc_pack_va(buf,len,fmt,&ap);
    va_end(ap);
    return r;
}

int32_t ipc_pack_va(void * buf, int len, int32_t * fmt, va_list* va)
{
    int tlen = 0;
    int32_t ret = 0;
    int32_t ch;
    while ((ch=*fmt++) != 0) {
        if ((ch >= SIPC_TYPE_ID(i8)) && (ch <= SIPC_TYPE_ID(ptr))) {
            tlen = g_ipc_buildin_types[ch].pack(&g_ipc_buildin_types[ch], buf, len, va);
        } else {
            SIPC_LOG1("TODO: customer type");
            tlen = SIPC_ERR_NO_IMPL;
        }
        if (tlen <= 0)
            break;
        len -= tlen;
        buf += tlen;
        ret += tlen;
    }
    return ret;
}

int32_t ipc_upack_args(void * buf, int len, int32_t * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int32_t r = ipc_upack_va(buf,len,fmt,&ap);
    va_end(ap);
    return r;
}

int32_t ipc_upack_va(void * buf, int len, int32_t * fmt, va_list* va)
{
    int ch;
    int tlen = 0;
    int32_t ret = 0;
    while ((ch=*fmt++) != 0) {
        if ((ch >= SIPC_TYPE_ID(i8)) && (ch <= SIPC_TYPE_ID(ptr))) {
            tlen = g_ipc_buildin_types[ch].upack(&g_ipc_buildin_types[ch], buf, len,va);
        } else {
            SIPC_LOG1("TODO: customer type");
            tlen = SIPC_ERR_NO_IMPL;
        }
        if (tlen <= 0)
            break;
        len -= tlen;
        buf += tlen;
        ret += tlen;
    }
    return ret;
}

void* serv_pthread_func(void *param) {
  SIPC_Handle serv;
  struct timeval tv = {1, 0};
  int retval;
  fd_set serv_rfds;
  int serv_maxfd = 0;

  while (serv_status) {
    pthread_mutex_lock(&serv_mutex);
    FD_ZERO(&serv_rfds);
    for (serv = serv_list; serv != NULL; serv = serv->next_serv) {
      FD_SET(serv->sockfd, &serv_rfds);
      if (serv->sockfd > serv_maxfd) {
        serv_maxfd = serv->sockfd;
      }
    }
    do {
      retval = select(serv_maxfd+1, &serv_rfds, NULL, NULL, &tv);
    } while ((retval == -1) && (errno == EINTR));
    if (retval == -1) {
      printf("select error %d %s\n", errno, strerror(errno));
    } else if (retval) {
      for (serv = serv_list; serv != NULL; serv = serv->next_serv) {
        if (FD_ISSET(serv->sockfd, &serv_rfds)) {
          ipc_socket_handle(serv);
        }
      }
    }
    pthread_mutex_unlock(&serv_mutex);
  }

  pthread_exit(NULL);
}

void ipc_init(void)
{
  SIPC_LOG1("ipc_init");

  serv_status = true;
  pthread_create(&serv_pthread_id, NULL, serv_pthread_func, NULL);
}

void ipc_exit(void)
{
  serv_status = false;
  pthread_join(serv_pthread_id, NULL);
  pthread_mutex_destroy(&serv_mutex);

  SIPC_LOG1("ipc_exit");
}

SIPC_Handle ipc_create_channel(const char * bindaddr)
{
    int unix_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (unix_socket < 0) {
        SIPC_LOG1("failed to open socket %d %s", errno, strerror(errno));
        goto error;
    }
    struct sockaddr_un addr;
    socklen_t addrlen;
    addr.sun_family = AF_UNIX;
    if (bindaddr) {
        unlink(bindaddr);
        addrlen = snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", bindaddr) + 1 + offsetof(struct sockaddr_un, sun_path);
    } else {
        addrlen = sizeof(sa_family_t);
    }
    if (bind(unix_socket, (struct sockaddr *)&addr, addrlen) < 0) {
        SIPC_LOG1("bind to %s fail %d %s", bindaddr, errno, strerror(errno));
        goto error;
    }
    struct SIPC_HandleInternal * serv = malloc(sizeof(*serv));
    memset(serv, 0, sizeof(*serv));
    serv->sockfd = unix_socket;
    serv->timeout_us = SIPC_DEFAULT_TIMEOUT;
    serv->addrlen = sizeof(serv->addr);
    if (getsockname(unix_socket, (struct sockaddr *)&serv->addr, &serv->addrlen)) {
        SIPC_LOG1("getsockname fail %d %s", errno, strerror(errno));
    } else {
        SIPC_LOG1("bind to addr %.*s addrlen %d", serv->addrlen-3, &serv->addr.sun_path[1], serv->addrlen);
    }
    return serv;

error:
    if (unix_socket >= 0) {
        close(unix_socket);
    }
    return NULL;
}

SIPC_Peer ipc_open_peer(SIPC_Handle serv, const char * remoteaddr)
{
    SIPC_Peer ipc;
    if (serv) {
        for (ipc=serv->next; ipc; ipc=ipc->next) {
            if (strcmp(ipc->addr.sun_path, remoteaddr) == 0)
                return ipc;
        }
    }
    ipc = ipc_create_channel(NULL);
    ipc->addr.sun_family = AF_UNIX;
    ipc->addrlen = snprintf(ipc->addr.sun_path, sizeof(ipc->addr.sun_path), "%s", remoteaddr) + 1 + offsetof(struct sockaddr_un, sun_path);
    if (serv) {
        ipc->serv = serv;
        ipc->next = serv->next;
        serv->next = ipc;
    }
    return ipc;
}

SIPC_Peer ipc_open(const char * bindaddr, const char * remoteaddr)
{
  SIPC_Handle serv = NULL;
  SIPC_Peer ipc = NULL;

  serv = ipc_create_channel(bindaddr);
  ipc = ipc_open_peer(serv, remoteaddr);

  pthread_mutex_lock(&serv_mutex);
  if (serv_list == NULL) ipc_init();

  if (serv) {
    if (serv_list) {
      serv->next_serv = serv_list;
      serv_list = serv;
    } else {
      serv->next_serv = NULL;
      serv_list = serv;
    }
  }
  pthread_mutex_unlock(&serv_mutex);

  return ipc;
}

void ipc_close_peer(SIPC_Peer ipc)
{
    if (ipc->serv) {
        SIPC_Handle ch = ipc->serv->next;
        if (ch == ipc)
            ipc->serv->next = ch->next;
        else if (ch) {
            for (; ch->next; ch=ch->next) {
                if (ch->next == ipc) {
                    ch->next = ipc->next;
                    break;
                }
            }
        }
    }
    if (ipc->sockfd >= 0) {
        close(ipc->sockfd);
    }
    free(ipc);
}

void ipc_close_channel(SIPC_Handle serv)
{
    if (serv->sockfd >= 0) {
        close(serv->sockfd);
        serv->sockfd = -1;
    }
    while (serv->next) {
        SIPC_Peer ipc = serv->next;
        serv->next = ipc->next;
        ipc_close_peer(ipc);
    }
    free(serv);
}

int32_t ipc_close(SIPC_Peer ipc)
{
  SIPC_Handle tmp, tmp_prev;

  if (ipc == NULL) return SIPC_ERR_PARAM;

  pthread_mutex_lock(&serv_mutex);
  for (tmp = serv_list; tmp != NULL; tmp = tmp->next_serv) {
    if (tmp == ipc->serv) {
      if (tmp_prev == NULL)
        serv_list = tmp->next_serv;
      else
        tmp_prev = tmp->next_serv;
      break;
    } else {
      tmp_prev = tmp;
    }
  }

  if (serv_list == NULL) ipc_exit();
  pthread_mutex_unlock(&serv_mutex);

  ipc_close_channel(ipc->serv);

  return SIPC_NO_ERR;
}

int32_t ipc_run_loop(SIPC_Handle serv)
{
    int cur_loop = serv->loop_level++;
    while (cur_loop < serv->loop_level) {
        int r = ipc_socket_handle(serv);
        (void)r;
    }
    return 0;
}

int32_t ipc_exit_inner_loop(SIPC_Handle serv)
{
    serv->loop_level--;
    call__local(serv,0,NULL,0);
    return 0;
}

int ipc_socket_getfd(SIPC_Handle serv) { return serv->sockfd; }

// clientid,callid,name
int32_t ipc_socket_handle(SIPC_Handle serv)
{
    char * buf = serv->buffer;
    struct SIPC_HandleInternal tempChannel;
    tempChannel.addrlen = sizeof(tempChannel.addr);
    tempChannel.sockfd = serv->sockfd;
    tempChannel.serv = serv;
    int len = recvfrom(serv->sockfd, buf, SIPC_MAX_MESSAGE_SIZE, MSG_NOSIGNAL, (struct sockaddr *)&tempChannel.addr, &tempChannel.addrlen);
    int32_t client_id;
    int32_t call_id;
    char * name;
    int len1 = ipc_upack_args(buf, len, SIPC_FUNCTION(_call)->argfmt, &client_id, &call_id, &name);
    if (len1 <= 0) {
        SIPC_LOG2("data format error");
        return SIPC_ERR_DATA;
    }
    SIPC_LOG3("received call %s %d bytes from %.*s", name, len, tempChannel.addrlen-3, &tempChannel.addr.sun_path[1]);
    if (SIPC_LOG_LEVEL > 4) {
        int i;
        char msg[len*3 + 1], *p = msg;
        for (i=0; i<len; ++i) {
            p += sprintf(p, "%02x ", (uint8_t)buf[i]);
        }
        SIPC_LOG4("%s",msg);
    }
    SIPC_Peer ipc = NULL;
    if (client_id == 0) {
        ipc = &tempChannel;
        ipc->serv = serv;
        ipc->next = NULL;
        ipc->ipc_id = 0;
        ipc->func_table = NULL;
    } else {
        for (ipc=serv->next; ipc; ipc=ipc->next) {
            if (ipc->ipc_id == client_id)
                break;
        }
    }
    if (ipc == NULL) {
        SIPC_LOG2("client_id %d not found", client_id);
    }
    extern SIPC_Function *__start_ipc_function_export_table;
    extern SIPC_Function *__stop_ipc_function_export_table;
    //extern SIPC_Function *__start_ipc_function_import_table;
    //extern SIPC_Function *__stop_ipc_function_import_table;
    SIPC_Function * f = NULL;
    SIPC_Function ** ppfunc = &__start_ipc_function_export_table;
    for (;ppfunc<&__stop_ipc_function_export_table;++ppfunc) {
        if (strcmp((*ppfunc)->name, name) == 0) {
            f = *ppfunc;
            break;
        }
    }
    if (f == NULL) {
        for (f=serv->func_table; f; f=f->next) {
            if (strcmp(f->name, name) == 0) {
                break;
            }
        }
    }
    if (f == NULL) {
        SIPC_LOG2("can't find function %s", (char*)name);
        return SIPC_ERR_NO_FUNC;
    }
    return f->handle(ipc, buf+len1, len-len1);
}

int32_t ipc_register_call(SIPC_Handle serv, SIPC_Function * func)
{
    SIPC_Function *f;
    for (f=serv->func_table; f != NULL; f = f->next) {
        if ((f == func) || (strcmp(f->name, func->name) == 0)) {
            SIPC_LOG1("try to register IPC function %s multiple time, %p %p\n", func->name, f, func);
            return SIPC_ERR_DUP_FUNC;
        }
    }
    func->next = serv->func_table;
    serv->func_table = func;
    return 0;
}

static int socket_have_data_to_read(int sockfd, int us)
{
    struct timeval tv, *ptv;
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
    if (us == -1) {
        ptv = NULL;
    } else {
        ptv = &tv;
        ptv->tv_sec = us / 1000000;
        ptv->tv_usec = us % 1000000;
    }
    int retval;
    do {
       retval = select(sockfd+1, &rfds, NULL, NULL, ptv);
    } while ((retval==-1) && (errno==EINTR));
    return retval;
}

int32_t ipc_call_va(SIPC_Peer ipc, SIPC_Function * func, va_list* va)
{
#if 0
    if (func->retfmt) {
        struct sockaddr_un addr;
        socklen_t addrlen = sizeof(addr);
        // flush the socket
        while (socket_have_data_to_read(ipc->sockfd, 0) > 0) {
            int rlen = recvfrom(ipc->sockfd, ipc->buffer, sizeof(ipc->buffer), MSG_NOSIGNAL, (struct sockaddr *)&addr, &addrlen);
            if (rlen > 0) {
                SIPC_LOG1("drop %d bytes from %.*s", rlen, addrlen-3, &addr.sun_path[1]);
            }
        }
    }
#endif
    if (ipc == NULL) {
      SIPC_LOG2("call ipc_call_va SIPC_Peer is NULL");
      return SIPC_ERR_PARAM;
    }
    char *buf = ipc->buffer;
    int len = sizeof(ipc->buffer);
    int len1 = ipc_pack_args(buf, len, SIPC_FUNCTION(_call)->argfmt, ipc->ipc_id, func->call_id, func->name);
    if (len1 <= 0) {
        SIPC_LOG2("call ipc_pack_args header failed");
        return len1;
    }
    int len2 = ipc_pack_va(buf+len1, len-len1, func->argfmt, va);
    if (len2 < 0) {
        SIPC_LOG2("call ipc_pack_args params failed");
        return len2;
    }
    int wlen = sendto(ipc->sockfd, buf, len1+len2, MSG_NOSIGNAL, (struct sockaddr *)&ipc->addr, ipc->addrlen);
    SIPC_LOG3("call %s, send %d bytes to %.*s, return %d", func->name, len1+len2, ipc->addrlen-3, &ipc->addr.sun_path[1], wlen);
    if (SIPC_LOG_LEVEL > 3) {
        int i = len1+len2;
        char msg[i*3 + 1], *p = msg;
        for (i=0; i<len1+len2; ++i) {
            p += sprintf(p, "%02x ", (uint8_t)buf[i]);
        }
        SIPC_LOG4("%s",msg);
    }
    if (wlen != len1+len2) {
        SIPC_LOG2("send data fail %d != %d + %d, err %d %s", wlen, len1, len2, errno, strerror(errno));
        return SIPC_ERR_IO;
    }
    if (func->retfmt == NULL)
        return SIPC_NO_ERR;
    int r = socket_have_data_to_read(ipc->sockfd, ipc->timeout_us);
    if (r > 0) {
        struct sockaddr_un addr;
        socklen_t addrlen = sizeof(addr);
        int rlen = recvfrom(ipc->sockfd, ipc->buffer, sizeof(ipc->buffer), MSG_NOSIGNAL, (struct sockaddr *)&addr, &addrlen);
        if (rlen <= 0) {
            SIPC_LOG2("call %s, response len %d", func->name, rlen);
            r = SIPC_ERR_IO;
        } else {
            SIPC_LOG3("receive %d bytes from %.*s", rlen, addrlen-3, &addr.sun_path[1]);
            if (SIPC_LOG_LEVEL > 3) {
                int i = rlen;
                char msg[i*3 + 1], *p = msg;
                for (i=0; i<rlen; ++i) {
                    p += sprintf(p, "%02x ", (uint8_t)ipc->buffer[i]);
                }
                SIPC_LOG4("%s",msg);
            }
            len1 = ipc_upack_args(ipc->buffer, rlen, SIPC_FUNCTION(_call)->retfmt, &r);
            if (r == SIPC_NO_ERR) {
                len2 = ipc_upack_va(ipc->buffer+len1, rlen-len1, func->retfmt, va);
            }
            SIPC_LOG3("call %s return %d", func->name, r);
        }
    } else if (r == 0) {
        SIPC_LOG2("call %s response timeout", func->name);
        r = SIPC_ERR_TIMEOUT;
    } else {
        SIPC_LOG2("call %s, failed to get response", func->name);
        r = SIPC_ERR_UNKNOWN;
    }
    return r;
}

int32_t ipc_call_fmt(SIPC_Peer ipc, SIPC_Function * func, ...) {
    va_list ap;
    va_start(ap, func);
    int32_t r = ipc_call_va(ipc,func,&ap);
    va_end(ap);
    return r;
}

int32_t ipc_resp_va(SIPC_Peer ipc, int32_t * fmt, va_list* va) {
    int len1 = ipc_pack_va(ipc->buffer, sizeof(ipc->buffer), SIPC_FUNCTION(_call)->retfmt, va);
    if (len1 <= 0) {
        SIPC_LOG2("call ipc_pack_args header failed");
        return len1;
    }
    int len2 = fmt ? ipc_pack_va(ipc->buffer + len1, sizeof(ipc->buffer) - len1, fmt, va) : 0;
    if (len2 < 0) {
        SIPC_LOG2("call ipc_pack_args params failed");
        return len2;
    }
    int wlen = sendto(ipc->sockfd, ipc->buffer, len1+len2, MSG_NOSIGNAL, (struct sockaddr *)&ipc->addr, ipc->addrlen);
    SIPC_LOG3("send %d bytes to %.*s, return %d", len1+len2, ipc->addrlen-3, &ipc->addr.sun_path[1], wlen);
    if (SIPC_LOG_LEVEL > 3) {
        int i = len1+len2;
        char msg[i*3 + 1], *p = msg;
        for (i=0; i<len1+len2; ++i) {
            p += sprintf(p, "%02x ", (uint8_t)ipc->buffer[i]);
        }
        SIPC_LOG4("%s",msg);
    }
    if (wlen != len1+len2) {
        SIPC_LOG2("send data fail %d != %d + %d, err %d %s", wlen, len1, len2, errno, strerror(errno));
        return SIPC_ERR_IO;
    }
    return SIPC_NO_ERR;
}

int32_t ipc_resp_fmt(SIPC_Peer ipc, int32_t * fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int32_t r = ipc_resp_va(ipc,fmt,&ap);
    va_end(ap);
    return r;
}

int32_t on__local(SIPC_Peer ipc, int32_t cmd, void * data, int32_t len)
{
    SIPC_LOG3("client %p call local %d %p %d", ipc, cmd, data, len);
    return 0;
}

int ipc_set_timeout(SIPC_Peer ipc, int us) {
    int old = ipc->timeout_us;
    ipc->timeout_us = us;
    return old;
}
