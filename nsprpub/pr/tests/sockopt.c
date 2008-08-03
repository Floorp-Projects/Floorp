/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
        "PR_SockOpt_Broadcast",       /* Enable broadcast */
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
        PRUint32 segment = 1024;
        PRNetAddr addr;

        rv = PR_InitializeNetAddr(PR_IpAddrAny, 0, &addr);
        if (PR_FAILURE == rv) Failed("PR_InitializeNetAddr()", NULL);
        rv = PR_Bind(udp, &addr);
        if (PR_FAILURE == rv) Failed("PR_Bind()", NULL);
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
                case PR_SockOpt_Broadcast:
                    fd = udp; 
                    data.value.broadcast = PR_TRUE;         
                    break;    
                default: continue;
            }

			/*
			 * TCP_MAXSEG can only be read, not set
			 */
            if (option != PR_SockOpt_MaxSegment) {
#ifdef WIN32
            	if (option != PR_SockOpt_McastLoopback)
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
        PR_Close(udp);
        PR_Close(tcp);
    }
#ifndef XP_MAC
    PR_fprintf(err, "%s\n", (failed) ? "FAILED" : "PASSED");
#else
   printf("%s\n", (failed) ? "FAILED" : "PASSED");
#endif
    return (failed) ? 1 : 0;
}  /* main */

/* sockopt.c */

