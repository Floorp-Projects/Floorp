/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef MKGETURL_H
#define MKGETURL_H

#include "xp.h"


/* fix Mac warnings about missing prototypes */
MODULE_PRIVATE int PR_CALLBACK 
NET_PrefChangedFunc(const char *pref, void *data);

/* Debugging routine prints an URL (and string "header")
 */
PR_BEGIN_EXTERN_C
#ifdef DEBUG
extern void TraceURL (URL_Struct *url, char *header);
#else
#define TraceURL(U,M)
#endif /* DEBUG */
PR_END_EXTERN_C

/* forward declared; see below */
typedef struct _NET_ProtoImpl NET_ProtoImpl;

/* structure for maintaining multiple active data transfers
 */
typedef struct _ActiveEntry {
    URL_Struct    *URL_s;           /* the URL data */
    int            status;          /* current status */
    int32          bytes_received;  /* number of bytes received so far */
    PRFileDesc    *socket;          /* data sock */
    PRFileDesc    *con_sock;        /* socket waiting for connection */
    Bool           local_file;      /* are we reading a local file */
	Bool           memory_file;     /* are we reading from memory? */
    int            protocol;        /* protocol used for transfer */
	NET_ProtoImpl *proto_impl;	    /* handle to protocol implemenation */
    void          *con_data;        /* data about the transfer connection and status */
                                    /* routine to call when finished */
    Net_GetUrlExitFunc *exit_routine;
    MWContext  * window_id;         /* a unique window id */
    FO_Present_Types format_out;    /* the output format */

	NET_StreamClass	* save_stream; /* used for cacheing of partial docs
									* The file code opens this stream
									* and writes part of the file down it.
									* Then the stream is saved
									* and the rest is loaded from the
									* network
									*/
    Bool      busy;

    char *    proxy_conf;	/* Proxy autoconfig string */
    char *    proxy_addr;	/* Proxy address in host:port format */
    u_long    socks_host;	/* SOCKS host IP address */
    short     socks_port;	/* SOCKS port number */

} ActiveEntry;

/* typedefs of protocol implementation functions
 *
 * All these currently take an ActiveEntry Struct but
 * should probably be abstracted out considerably more
 */
typedef int32 NET_ProtoInitFunc(ActiveEntry *ce);
typedef int32 NET_ProtoProcessFunc(ActiveEntry *ce);
typedef int32 NET_ProtoInterruptFunc(ActiveEntry *ce);
typedef int32 NET_ProtoCleanupFunc(void);

/* a structure to hold the registered implementation of
 * a protocol converter
 */
struct _NET_ProtoImpl {
	int32 (*init) (ActiveEntry *ce);
	int32 (*process)   (ActiveEntry *ce);
	int32 (*interrupt) (ActiveEntry *ce);
	void  (*cleanup)   (void);   /* note that cleanup can be called more 
				      			  * than once, when we need to shut down 
				      			  * connections or free up memory
				      			  */
};

PR_BEGIN_EXTERN_C
extern int NET_TotalNumberOfOpenConnections;
extern int NET_MaxNumberOfOpenConnections;
extern CacheUseEnum NET_CacheUseMethod;
extern time_t NET_StartupTime;  /* time we began the program */
extern PRBool NET_ProxyAcLoaded;
/*
 * Silently Interrupts all transfers in progress that have the same
 * window id as the one passed in.
 */
extern int NET_SilentInterruptWindow(MWContext * window_id);
/* cause prefs to be read or updated */
extern void NET_SetupPrefs(const char * prefChanged);

extern NET_ProxyStyle NET_GetProxyStyle(void);
extern const char * net_GetPACUrl(void);
extern void net_SetPACUrl(char *u);

/* return a proxy server host and port to the caller or NULL
 */
extern char * NET_FindProxyHostForUrl(int urltype, char *urladdress);

/* registers a protocol impelementation for a particular url_type
 * see NET_URL_Type() for types
 */
extern void NET_RegisterProtocolImplementation(NET_ProtoImpl *impl, int for_url_type);

PR_END_EXTERN_C
#endif /* not MKGetURL_H */
