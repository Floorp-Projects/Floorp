/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

#include "nspr.h"
#include "prio.h"
#include "prinit.h"
#include "prprf.h"
#ifdef XP_MAC
#include "probslet.h"
#else
#include "obsolete/probslet.h"
#endif

#include "plerror.h"

static PRFileDesc *err = NULL;
static PRBool failed = PR_FALSE;

#ifndef XP_MAC
static void Failed(const char *msg1, const char *msg2)
{
    if (NULL != msg1) PR_fprintf(err, "%s ", msg1);
    PL_FPrintError(err, msg2);
    failed = PR_TRUE;
}  /* Failed */

#else
#include "prlog.h"
#define printf PR_LogPrint
extern void SetupMacPrintfLog(char *logFile);
static void Failed(const char *msg1, const char *msg2)
{
    if (NULL != msg1) printf("%s ", msg1);
    printf (msg2);
    failed |= PR_TRUE;
}  /* Failed */

#endif

static PRSockOption Incr(PRSockOption *option)
{
    PRIntn val = ((PRIntn)*option) + 1;
    *option = (PRSockOption)val;
    return (PRSockOption)val;
}  /* Incr */

PRIntn main(PRIntn argc, char *argv)
{
    PRStatus rv;
    PRFileDesc *udp = PR_NewUDPSocket();
    PRFileDesc *tcp = PR_NewTCPSocket();
    const char *tag[] =
    {
        "PR_SockOpt_Nonblocking",     /* nonblocking io */
        "PR_SockOpt_Linger",          /* linger on close if data present */
        "PR_SockOpt_Reuseaddr",       /* allow local address reuse */
        "PR_SockOpt_Keepalive",       /* keep connections alive */
        "PR_SockOpt_RecvBufferSize",  /* send buffer size */
        "PR_SockOpt_SendBufferSize",  /* receive buffer size */

        "PR_SockOpt_IpTimeToLive",    /* time to live */
        "PR_SockOpt_IpTypeOfService", /* type of service and precedence */

        "PR_SockOpt_AddMember",       /* add an IP group membership */
        "PR_SockOpt_DropMember",      /* drop an IP group membership */
        "PR_SockOpt_McastInterface",  /* multicast interface address */
        "PR_SockOpt_McastTimeToLive", /* multicast timetolive */
        "PR_SockOpt_McastLoopback",   /* multicast loopback */

        "PR_SockOpt_NoDelay",         /* don't delay send to coalesce packets */
        "PR_SockOpt_MaxSegment",      /* maximum segment size */
        "PR_SockOpt_Last"
    };

    err = PR_GetSpecialFD(PR_StandardError);
    PR_STDIO_INIT();

#ifdef XP_MAC
	SetupMacPrintfLog("sockopt.log");
#endif

    if (NULL == udp) Failed("PR_NewUDPSocket()", NULL);
    else if (NULL == tcp) Failed("PR_NewTCPSocket()", NULL);
    else
    {
        PRSockOption option;
        PRLinger linger;
        PRUint32 ttl = 20;
        PRBool boolean = PR_TRUE;
        PRUint32 segment = 1024;
        for(option = PR_SockOpt_Linger; option < PR_SockOpt_Last; Incr(&option))
        {
            void *value;
            PRInt32 *size;
            PRInt32 ttlsize = sizeof(ttl);
            PRInt32 segmentsize = sizeof(segment);
            PRInt32 booleansize = sizeof(boolean);
            PRInt32 lingersize = sizeof(linger);

            PRFileDesc *socket = udp;
            switch (option)
            {
            case PR_SockOpt_Linger:          /* linger on close if data present */
                socket = tcp;
                linger.polarity = PR_TRUE;
                linger.linger = PR_SecondsToInterval(1);
                value = &linger;
                size = &lingersize;
                break;
            case PR_SockOpt_NoDelay:         /* don't delay send to coalesce packets */
            case PR_SockOpt_Keepalive:       /* keep connections alive */
                socket = tcp;
            case PR_SockOpt_Reuseaddr:       /* allow local address reuse */
                value = &boolean;
                size = &booleansize;
                break;
#ifndef WIN32
            case PR_SockOpt_MaxSegment:      /* maximum segment size */
                socket = tcp;
#endif
            case PR_SockOpt_RecvBufferSize:  /* send buffer size */
            case PR_SockOpt_SendBufferSize:  /* receive buffer size */
                value = &segment;
                size = &segmentsize;
                break;

            case PR_SockOpt_IpTimeToLive:    /* time to live */
                value = &ttl;
                size = &ttlsize;
                break;
            case PR_SockOpt_IpTypeOfService: /* type of service and precedence */
            case PR_SockOpt_AddMember:       /* add an IP group membership */
            case PR_SockOpt_DropMember:      /* drop an IP group membership */
            case PR_SockOpt_McastInterface:  /* multicast interface address */
            case PR_SockOpt_McastTimeToLive: /* multicast timetolive */
            case PR_SockOpt_McastLoopback:   /* multicast loopback */
            default:
                continue;
            }
			/*
			 * TCP_MAXSEG can only be read, not set
			 */
            if (option != PR_SockOpt_MaxSegment) {
#ifdef WIN32
            	if ((option != PR_SockOpt_McastTimeToLive) &&
								(option != PR_SockOpt_McastLoopback))
#endif
				{
            		rv = PR_SetSockOpt(socket, option, value, *size);
            		if (PR_FAILURE == rv) Failed("PR_SetSockOpt()",
														tag[option]);
				}
			}
            
            rv = PR_GetSockOpt(socket, option, &value, size);
            if (PR_FAILURE == rv) Failed("PR_GetSockOpt()", tag[option]);
        }
        for(option = PR_SockOpt_Linger; option < PR_SockOpt_Last; Incr(&option))
        {
            PRSocketOptionData data;
            PRFileDesc *fd = tcp;
            data.option = option;
            switch (option)
            {
                case PR_SockOpt_Nonblocking:
                    data.value.non_blocking = PR_TRUE;
                    break;    
                case PR_SockOpt_Linger:
                    data.value.linger.polarity = PR_TRUE;
                    data.value.linger.linger = PR_SecondsToInterval(2);          
                    break;    
                case PR_SockOpt_Reuseaddr:
                    data.value.reuse_addr = PR_TRUE;      
                    break;    
                case PR_SockOpt_Keepalive:       
                    data.value.keep_alive = PR_TRUE;      
                    break;    
                case PR_SockOpt_RecvBufferSize:
                    data.value.recv_buffer_size = segment;  
                    break;    
                case PR_SockOpt_SendBufferSize:  
                    data.value.send_buffer_size = segment;  
                    break;    
                case PR_SockOpt_IpTimeToLive:
                    data.value.ip_ttl = 64;  
                    break;    
                case PR_SockOpt_IpTypeOfService:
                    data.value.tos = 0; 
                    break;    
                case PR_SockOpt_McastTimeToLive:
                    fd = udp; 
                    data.value.mcast_ttl = 4; 
                    break;    
                case PR_SockOpt_McastLoopback:
                    fd = udp; 
                    data.value.mcast_loopback = PR_TRUE; 
                    break;    
                case PR_SockOpt_NoDelay:
                    data.value.no_delay = PR_TRUE;         
                    break;    
#ifndef WIN32
                case PR_SockOpt_MaxSegment:
                    data.value.max_segment = segment;      
                    break;    
#endif
                default: continue;
            }

			/*
			 * TCP_MAXSEG can only be read, not set
			 */
            if (option != PR_SockOpt_MaxSegment) {
#ifdef WIN32
            	if ((option != PR_SockOpt_McastTimeToLive) &&
								(option != PR_SockOpt_McastLoopback))
#endif
				{
            		rv = PR_SetSocketOption(fd, &data);
            		if (PR_FAILURE == rv)
							Failed("PR_SetSocketOption()", tag[option]);
				}
			}

            rv = PR_GetSocketOption(fd, &data);
            if (PR_FAILURE == rv) Failed("PR_GetSocketOption()", tag[option]);
        }
    }
#ifndef XP_MAC
    PR_fprintf(err, "%s\n", (failed) ? "FAILED" : "PASSED");
#else
   printf("%s\n", (failed) ? "FAILED" : "PASSED");
#endif
    return (failed) ? 1 : 0;
}  /* main */

/* sockopt.c */

