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
#ifndef MKTCP_H
#define MKTCP_H

PR_BEGIN_EXTERN_C

/* state machine data for host connect */
typedef struct _TCP_ConData TCP_ConData;  

/* the socks host in U long form
 */
extern u_long NET_SocksHost;
extern short NET_SocksPort;
extern char *NET_SocksHostName;

/* socket buffer and socket buffer size
 * used for reading from sockets
 */
extern char * NET_Socket_Buffer;
extern int    NET_Socket_Buffer_Size;

extern int NET_InGetHostByName;  /* global semaphore */

/* free any cached data in preperation
 * for shutdown or to free up memory
 */
extern void NET_CleanupTCP (void);

/* 
 * do a standard netwrite but echo it to stderr as well
 */
extern int 
NET_DebugNetWrite (PRFileDesc *fildes, CONST void *buf, unsigned nbyte);

/* make sure the whole buffer is written in one call
 */
extern int32
NET_BlockingWrite (PRFileDesc *filedes, CONST void * buf, unsigned int nbyte);


/*  print an IP address from a sockaddr struct
 *
 *  This routine is only used for TRACE messages
 */
#if defined(XP_WIN) || defined(XP_OS2)
extern char *CONST
NET_IpString (struct sockaddr_in* sin);
#else
extern const char *
NET_IpString (struct sockaddr_in* sin);
#endif

/* return the local hostname
*/
#ifdef XP_WIN
extern char *CONST
NET_HostName (void);
#else
extern CONST char * 
NET_HostName (void);
#endif

/* free left over tcp connection data if there is any
 */
extern void NET_FreeTCPConData(TCP_ConData * data);

/*
 * Non blocking connect begin function.  It will most likely
 * return negative. If it does the NET_ConnectFinish() will
 * need to be called repeatably until a connect is established
 *
 * return's negative on error
 * return's MK_WAITING_FOR_CONNECTION when continue is neccessary
 * return's MK_CONNECTED on true connect
 */ 
extern int 
NET_BeginConnect  (CONST char   *url,
				   char         *ip_address_string,
				   char         *protocol,
				   int           default_port,
				   PRFileDesc  **s, 
				   Bool       use_security, 
				   TCP_ConData **tcp_con_data, 
				   MWContext    *window_id,
				   char        **error_msg,
				   u_long        socks_host,
				   short         socks_port);


/*
 * Non blocking connect finishing function.  This routine polls
 * the socket until a connection finally takes place or until
 * an error occurs. NET_ConnectFinish() will need to be called 
 * repeatably until a connect is established.
 *
 * return's negative on error
 * return's MK_WAITING_FOR_CONNECTION when continue is neccessary
 * return's MK_CONNECTED on true connect
 */
extern int 
NET_FinishConnect (CONST char   *url,
				   char         *protocol,
				   int           default_port,
				   PRFileDesc  **s,  
				   TCP_ConData **tcp_con_data, 
				   MWContext    *window_id,
				   char        **error_msg);

/* 
 * Echo to stderr as well as the socket
 */
extern int 
NET_DebugNetRead (PRFileDesc *fildes, void *buf, unsigned nbyte);

/* this is a very standard network write routine.
 * 
 * the only thing special about it is that
 * it returns MK_HTTP_TYPE_CONFLICT on special error codes
 *
 */
extern int 
NET_HTTPNetWrite (PRFileDesc *fildes, CONST void * buf, unsigned nbyte);

/* 
 *
 * trys to get a line of data from the socket or from the buffer
 * that was passed in
 */
extern int NET_BufferedReadLine(PRFileDesc *sock, 
				char ** line, 
				char ** buffer, 
				int32 * buffer_size, 
				Bool * pause_for_next_read);


extern void NET_SanityCheckDNS (MWContext *context);

PR_END_EXTERN_C
#endif   /* MKTCP_H */
