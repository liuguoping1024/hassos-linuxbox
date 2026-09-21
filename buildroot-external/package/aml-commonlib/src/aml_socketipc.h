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

#ifndef AML_SOCKETIPC_H
#define AML_SOCKETIPC_H

#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * a lightweight IPC API implemented in a signel header file
 *
 * use DECLARE_IPC_FUNCTION to define the IPC function prototype and IPC stub
 * code i.e. below code will define an IPC call "foo" which takes two parameters
 * typed as int32_t and int64_t:
 *    DECLARE_IPC_FUNCTION(foo,int32_t, int64_t)
 *
 * internally it will declare "call_foo" C API so you can call this function on
 * an opened IPC channel, The stub code is generated when SIPC_IMPLEMENTATION is
 * defined, SIPC_IMPLEMENTATION shall be defined in one of your C code before
 * including this file
 *
 * On the peer side, use EXPORT_IPC_FUNCTION to generate stub code for the IPC
 * call handle:
 *     EXPORT_IPC_FUNCTION(foo,int32_t, int64_t)
 *
 * IPC calls are asynchronous, calls can be make on any thread, the IPC stub
 * code will place an dgram on the socket, the data will transfer to peer, and
 * received by ipc_socket_handle in peer application, it then dispatch the call
 * and call "on_foo"
 *
 */

/*
 * define the IPC type and C type mapping
 * this IPC library is UDP/unix socket based, it is meant to communicate
 * accross different host, so don't use those types whose length depends on ARCH
 * and compiler, such as int, long, use int32_t, int64_t instead
 */
// build-in types
#define SIPC_DEFINE_TYPE_i8 (int),0x01
#define SIPC_DEFINE_TYPE_u8 (int),0x02
#define SIPC_DEFINE_TYPE_i16 (int),0x03
#define SIPC_DEFINE_TYPE_u16 (int),0x04
#define SIPC_DEFINE_TYPE_i32 (int32_t),0x05
#define SIPC_DEFINE_TYPE_u32 (uint32_t),0x06
#define SIPC_DEFINE_TYPE_i64 (int64_t),0x07
#define SIPC_DEFINE_TYPE_u64 (uint64_t),0x08
#define SIPC_DEFINE_TYPE_str (char*),0x09
#define SIPC_DEFINE_TYPE_ptr (void*,int32_t),0x0a

// type alias
#define SIPC_DEFINE_TYPE_int8_t SIPC_DEFINE_TYPE_i8
#define SIPC_DEFINE_TYPE_char SIPC_DEFINE_TYPE_i8
#define SIPC_DEFINE_TYPE_uint8_t SIPC_DEFINE_TYPE_u8
#define SIPC_DEFINE_TYPE_uchar SIPC_DEFINE_TYPE_u8
#define SIPC_DEFINE_TYPE_byte SIPC_DEFINE_TYPE_u8
#define SIPC_DEFINE_TYPE_int32_t SIPC_DEFINE_TYPE_i32
#define SIPC_DEFINE_TYPE_uint32_t SIPC_DEFINE_TYPE_u32
#define SIPC_DEFINE_TYPE_int64_t SIPC_DEFINE_TYPE_i64
#define SIPC_DEFINE_TYPE_uint64_t SIPC_DEFINE_TYPE_u64


#define MACRO_CAT(a,...) a ## __VA_ARGS__
#define MACRO_CAT2(a,...) MACRO_CAT(a,__VA_ARGS__)
#define MACRO_EMPTY(...)
#define MACRO_CALL(M,...) M(__VA_ARGS__)
#define EXPR(...) __VA_ARGS__

#define MACRO_GET_16(dummy,_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,...) _16
#define MACRO_NARG(...) MACRO_GET_16(dummy,##__VA_ARGS__,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0)

#define SIPC_TYPE_CTYPE(t) MACRO_CALL(MACRO_GET_16,dummy,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,SIPC_DEFINE_TYPE_##t)
#define SIPC_TYPE_ID(t) MACRO_CALL(MACRO_GET_16,dummy,1,2,3,4,5,6,7,8,9,10,11,12,13,14,SIPC_DEFINE_TYPE_##t)

#define MACRO_RECURSE_0(I,E,...)
#define MACRO_RECURSE_1(I,E,_1,...) E(_1)
#define MACRO_RECURSE_2(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_1(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_3(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_2(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_4(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_3(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_5(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_4(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_6(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_5(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_7(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_6(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_8(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_7(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_9(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_8(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_10(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_9(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_11(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_10(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_12(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_11(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_13(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_12(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_14(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_13(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_15(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_14(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_16(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_15(I,E,##__VA_ARGS__)
#define MACRO_RECURSE_17(I,E,_1,...) I(_1,##__VA_ARGS__) MACRO_RECURSE_16(I,E,##__VA_ARGS__)

#define MACRO_MAP(M,...) MACRO_CAT2(MACRO_RECURSE_,MACRO_NARG(__VA_ARGS__))(M,M,##__VA_ARGS__)

#define SIPC_GEN_CTYPE_EXPR1(...) __VA_ARGS__
#define SIPC_GEN_CTYPE_EXPR2(...) __VA_ARGS__
#define SIPC_GEN_CTYPE_E(a,...) SIPC_GEN_CTYPE_EXPR2(SIPC_GEN_CTYPE_EXPR1 SIPC_TYPE_CTYPE(a))
#define SIPC_GEN_CTYPE_I(a,...) SIPC_GEN_CTYPE_EXPR2(SIPC_GEN_CTYPE_EXPR1 SIPC_TYPE_CTYPE(a)),
#define SIPC_EXPAND_CTYPE_LIST(...) MACRO_CAT2(MACRO_RECURSE_,MACRO_NARG(__VA_ARGS__))(SIPC_GEN_CTYPE_I,SIPC_GEN_CTYPE_E,##__VA_ARGS__)

#define SIPC_GEN_ARG_LIST(a,...) ,a MACRO_CAT2(_,MACRO_NARG(__VA_ARGS__))
#define SIPC_GEN_PARAM_LIST(a,...) ,MACRO_CAT2(_,MACRO_NARG(__VA_ARGS__))
#define SIPC_GEN_PARAM_REF_LIST(a,...) ,&MACRO_CAT2(_,MACRO_NARG(__VA_ARGS__))
#define SIPC_GEN_PARAM_TYPEID(a,...) SIPC_TYPE_ID(a),
#define SIPC_GEN_DECLARE_LIST(a,...) a MACRO_CAT2(_,MACRO_NARG(__VA_ARGS__));
#define SIPC_GEN_ARG_PLIST(a,...) ,a * MACRO_CAT2(_p,MACRO_NARG(__VA_ARGS__))
#define SIPC_GEN_PARAM_PLIST(a,...) ,MACRO_CAT2(_p,MACRO_NARG(__VA_ARGS__))
// generate parameter-list: a,b -> , ctype_a _1 ,ctype_b _2
#define SIPC_EXPAND_ARG_LIST(...) MACRO_CALL(MACRO_MAP,SIPC_GEN_ARG_LIST,SIPC_EXPAND_CTYPE_LIST(__VA_ARGS__))
// generate list of declarator: a,b -> , _1 , _2
#define SIPC_EXPAND_PARAM_LIST(...) MACRO_CALL(MACRO_MAP,SIPC_GEN_PARAM_LIST,SIPC_EXPAND_CTYPE_LIST(__VA_ARGS__))
// generate address of declarator list: a,b -> , &_1 , &_2
#define SIPC_EXPAND_PARAM_REF_LIST(...) MACRO_CALL(MACRO_MAP,SIPC_GEN_PARAM_REF_LIST,SIPC_EXPAND_CTYPE_LIST(__VA_ARGS__))
// generate typeid array: a,b -> SIPC_TYPE_ID_a, SIPC_TYPE_ID_b,
#define SIPC_EXPAND_PARAM_TYPEID(...) MACRO_MAP(SIPC_GEN_PARAM_TYPEID,##__VA_ARGS__)
// generate declaration-list: a,b -> SIPC_a _1; SIPC_b _2;
#define SIPC_EXPAND_PARAM_DECLARE_LIST(...) MACRO_CALL(MACRO_MAP,SIPC_GEN_DECLARE_LIST,SIPC_EXPAND_CTYPE_LIST(__VA_ARGS__))
// generate parameter-pointer list: a,b -> , ctype_a * _1 ,ctype_b * _2
#define SIPC_EXPAND_ARG_PLIST(...) MACRO_CALL(MACRO_MAP,SIPC_GEN_ARG_PLIST,SIPC_EXPAND_CTYPE_LIST(__VA_ARGS__))
// generate list of declarator: a,b -> , _p1 , _p2
#define SIPC_EXPAND_PARAM_PLIST(...) MACRO_CALL(MACRO_MAP,SIPC_GEN_PARAM_PLIST,SIPC_EXPAND_CTYPE_LIST(__VA_ARGS__))

#define SIPC_FUNCTION(n) g_ipc_func_##n

#define _DECLARE_IPC_FUNCTION(handle,...)\
    extern SIPC_Function * g_ipc_func_##handle;\
    extern int32_t call_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST(__VA_ARGS__));
#define _DECLARE_IPC_FUNCTION_SYNC(handle,argfmt,retfmt)\
    extern SIPC_Function * g_ipc_func_##handle;\
    extern int32_t call_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST argfmt SIPC_EXPAND_ARG_PLIST retfmt);
#define _DECLARE_IPC_EXPORT(handle,...)\
    extern SIPC_Function * g_ipc_func_##handle;\
    extern int32_t call_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST(__VA_ARGS__));\
    extern int32_t on_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST(__VA_ARGS__));
#define _DECLARE_IPC_EXPORT_SYNC(handle,argfmt,retfmt)\
    extern SIPC_Function * g_ipc_func_##handle;\
    extern int32_t call_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST argfmt SIPC_EXPAND_ARG_PLIST retfmt);\
    extern int32_t on_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST argfmt);\
    extern int32_t resp_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST retfmt);

#define _IMPLEMENT_IPC_FUNCTION(handle,...) \
    _DECLARE_IPC_FUNCTION(handle,##__VA_ARGS__)\
    static int32_t _typeid_call_##handle[] = {SIPC_EXPAND_PARAM_TYPEID(__VA_ARGS__) 0};\
    static SIPC_Function _ipc_imp_func_##handle = {#handle, (void*)0, _typeid_call_##handle, NULL, 0, NULL};\
    SIPC_Function *g_ipc_func_##handle __attribute__((section("ipc_function_import_table"))) = &_ipc_imp_func_##handle;\
    int32_t call_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST(__VA_ARGS__)) {\
        return ipc_call_fmt(ipc, SIPC_FUNCTION(handle) SIPC_EXPAND_PARAM_LIST(__VA_ARGS__));\
    }
#define _IMPLEMENT_IPC_FUNCTION_SYNC(handle,argfmt,retfmt)\
    _DECLARE_IPC_FUNCTION_SYNC(handle,argfmt,retfmt)\
    static int32_t _typeid_call_##handle[] = {SIPC_EXPAND_PARAM_TYPEID argfmt 0};\
    static int32_t _typeid_ret_##handle[] = {SIPC_EXPAND_PARAM_TYPEID retfmt 0};\
    static SIPC_Function _ipc_imp_func_##handle = {#handle, (void*)0, _typeid_call_##handle, _typeid_ret_##handle, 0, NULL};\
    SIPC_Function *g_ipc_func_##handle __attribute__((section("ipc_function_import_table"))) = &_ipc_imp_func_##handle;\
    int32_t call_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST argfmt SIPC_EXPAND_ARG_PLIST retfmt) {\
        return ipc_call_fmt(ipc, SIPC_FUNCTION(handle) SIPC_EXPAND_PARAM_LIST argfmt SIPC_EXPAND_PARAM_PLIST retfmt);\
    }

#define _IMPLEMENT_IPC_EXPORT(handle,...) \
    _DECLARE_IPC_EXPORT(handle,##__VA_ARGS__)\
    static int32_t _typeid_call_##handle[] = {SIPC_EXPAND_PARAM_TYPEID(__VA_ARGS__) 0};\
    static int32_t _ipc_handle_##handle(SIPC_Peer ipc, void * buf, int len) {\
        SIPC_EXPAND_PARAM_DECLARE_LIST(__VA_ARGS__) \
        int32_t r = ipc_upack_args(buf,len,_typeid_call_##handle SIPC_EXPAND_PARAM_REF_LIST(__VA_ARGS__));\
        if (r >= 0) \
            r = on_##handle(ipc SIPC_EXPAND_PARAM_LIST(__VA_ARGS__)); \
        return r;\
    }\
    static SIPC_Function _ipc_exp_func_##handle = {#handle, _ipc_handle_##handle, _typeid_call_##handle, NULL, 0, NULL};\
    SIPC_Function *g_ipc_func_##handle __attribute__((section("ipc_function_export_table"))) = &_ipc_exp_func_##handle;\
    int32_t call_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST(__VA_ARGS__)) {\
        return ipc_call_fmt(ipc, SIPC_FUNCTION(handle) SIPC_EXPAND_PARAM_LIST(__VA_ARGS__));\
    }
#define _IMPLEMENT_IPC_EXPORT_SYNC(handle,argfmt,retfmt) \
    _DECLARE_IPC_EXPORT_SYNC(handle,argfmt,retfmt)\
    static int32_t _typeid_call_##handle[] = {SIPC_EXPAND_PARAM_TYPEID argfmt 0};\
    static int32_t _typeid_ret_##handle[] = {SIPC_EXPAND_PARAM_TYPEID retfmt 0};\
    static int32_t _ipc_handle_##handle(SIPC_Peer ipc, void * buf, int len) {\
        SIPC_EXPAND_PARAM_DECLARE_LIST argfmt \
        int32_t r = ipc_upack_args(buf,len,_typeid_call_##handle SIPC_EXPAND_PARAM_REF_LIST argfmt);\
        if (r >= 0) \
            r = on_##handle(ipc SIPC_EXPAND_PARAM_LIST argfmt);\
        if (r != SIPC_NO_ERR) { \
            r = ipc_resp_fmt(ipc, NULL, r);\
        } \
        return r;\
    }\
    int32_t resp_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST retfmt) {\
        return ipc_resp_fmt(ipc, _typeid_ret_##handle, SIPC_NO_ERR SIPC_EXPAND_PARAM_LIST retfmt);\
    }\
    static SIPC_Function _ipc_exp_func_##handle = {#handle, _ipc_handle_##handle, _typeid_call_##handle, _typeid_ret_##handle, 0, NULL};\
    SIPC_Function *g_ipc_func_##handle __attribute__((section("ipc_function_export_table"))) = &_ipc_exp_func_##handle;\
    int32_t call_##handle(SIPC_Peer ipc SIPC_EXPAND_ARG_LIST argfmt SIPC_EXPAND_ARG_PLIST retfmt) {\
        return ipc_call_fmt(ipc, SIPC_FUNCTION(handle) SIPC_EXPAND_PARAM_LIST argfmt SIPC_EXPAND_PARAM_PLIST retfmt);\
    }

#if defined(SIPC_IMPLEMENTATION)
#define DECLARE_IPC_FUNCTION _IMPLEMENT_IPC_FUNCTION
#define DECLARE_IPC_FUNCTION_SYNC _IMPLEMENT_IPC_FUNCTION_SYNC
#define EXPORT_IPC_FUNCTION _IMPLEMENT_IPC_EXPORT
#define EXPORT_IPC_FUNCTION_SYNC _IMPLEMENT_IPC_EXPORT_SYNC
#else  // SIPC_IMPLEMENTATION
#define DECLARE_IPC_FUNCTION _DECLARE_IPC_FUNCTION
#define DECLARE_IPC_FUNCTION_SYNC _DECLARE_IPC_FUNCTION_SYNC
#define EXPORT_IPC_FUNCTION _DECLARE_IPC_EXPORT
#define EXPORT_IPC_FUNCTION_SYNC _DECLARE_IPC_EXPORT_SYNC
#endif // SIPC_IMPLEMENTATION

#define IPC_CALL(ipc,handle,...) call_##handle(ipc,##__VA_ARGS__)
#define IPC_RESP(ipc,handle,...) resp_##handle(ipc,##__VA_ARGS__)

typedef struct SIPC_HandleInternal* SIPC_Handle;
typedef struct SIPC_HandleInternal* SIPC_Peer;
typedef struct _SIPC_Function SIPC_Function;

struct _SIPC_Function {
    const char * name;
    int32_t (*handle)(SIPC_Peer , void *, int );
    int32_t * argfmt;
    int32_t * retfmt;
    int32_t call_id;
    struct _SIPC_Function * next;
};

// user data type can be registered
struct SIPC_DataType {
    int type_id;
    int len;
    int32_t (*pack)(struct SIPC_DataType*, void *, int , va_list *);
    int32_t (*upack)(struct SIPC_DataType*, void *, int , va_list *);
    void (*free)(void *obj);
};

#define SIPC_NO_ERR             0
#define SIPC_ERR_PARAM          -1
#define SIPC_ERR_IO             -2
#define SIPC_ERR_DATA           -3
#define SIPC_ERR_NO_FUNC        -4
#define SIPC_ERR_DUP_FUNC       -5
#define SIPC_ERR_NO_IMPL        -6
#define SIPC_ERR_TIMEOUT        -7
#define SIPC_ERR_UNKNOWN        -8

// default timeout for IPC call
#define SIPC_DEFAULT_TIMEOUT    1000000*3
// IPC message buffer size
#define SIPC_MAX_MESSAGE_SIZE   1024*8

// create an ipc channel that listen on bindaddr for incoming calls
SIPC_Handle ipc_create_channel(const char * bindaddr);
void ipc_close_channel(SIPC_Handle serv);
// open a peer channel to make IPC call
SIPC_Peer ipc_open_peer(SIPC_Handle serv, const char * remoteaddr);
void ipc_close_peer(SIPC_Peer ipc);
// open ipc channel
SIPC_Peer ipc_open(const char * bindaddr, const char * remoteaddr);
// close ipc channel
int32_t ipc_close(SIPC_Peer ipc);
// run the main loop, it will not return
int32_t ipc_run_loop(SIPC_Handle serv);
// exit the most inner loop, return loop level
int32_t ipc_exit_inner_loop(SIPC_Handle serv);
// get the socket fd which can be used in select/poll
// this makes it possible to avoid a dedicate thread to run ipc_run_loop
int ipc_socket_getfd(SIPC_Handle serv);
// call this function if select/poll success on the socket fd
int32_t ipc_socket_handle(SIPC_Handle serv);
// register function calls, the memory of SIPC_Function shall keep valid and untouched until ipc_close_channel
int32_t ipc_register_call(SIPC_Handle serv, SIPC_Function * func);
// set synchronous IPC call timeout, return previous timeout, default timeout is SIPC_DEFAULT_TIMEOUT
int ipc_set_timeout(SIPC_Peer ipc, int us);
// call IPC function with parameters
int32_t ipc_call_va(SIPC_Peer ipc, SIPC_Function * func, va_list* va);
int32_t ipc_call_fmt(SIPC_Peer ipc, SIPC_Function * func, ...);
// answer an IPC call
int32_t ipc_resp_va(SIPC_Peer ipc, int32_t * fmt, va_list* va);
int32_t ipc_resp_fmt(SIPC_Peer ipc, int32_t * fmt, ...);
// pack and unpack IPC parameters
int32_t ipc_pack_va(void * buf, int len, int32_t * fmt, va_list* va);
int32_t ipc_pack_args(void * buf, int len, int32_t * fmt, ...);
int32_t ipc_upack_va(void * buf, int len, int32_t * fmt, va_list* va);
int32_t ipc_upack_args(void * buf, int len, int32_t * fmt, ...);

DECLARE_IPC_FUNCTION_SYNC(_call,(i32,i32,str),(i32));
EXPORT_IPC_FUNCTION(_local,i32,ptr);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* end of include guard: AML_SOCKETIPC_H */
