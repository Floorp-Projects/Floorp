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

/***********************************************************************
**
** Name: tmocon.c
**
** Description: test client socket connection.
**
** Modification History:
** 19-May-97 AGarcia- Converted the test to accomodate the debug_mode flag.
**	         The debug mode will print all of the printfs associated with this test.
**			 The regress mode will be the default mode. Since the regress tool limits
**           the output to a one line status:PASS or FAIL,all of the printf statements
**			 have been handled with an if (debug_mode) statement. 
***********************************************************************/

/***********************************************************************
** Includes
***********************************************************************/
/* Used to get the command line option */
#include "plgetopt.h"

#include "nspr.h"
#include "pprio.h"

#include "plerror.h"
#include "plgetopt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* for getcwd */
#if defined(XP_UNIX) || defined (XP_OS2_EMX)
#include <unistd.h>
#elif defined(XP_PC)
#include <direct.h>
#endif

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
#endif


#define BASE_PORT 9867

#define DEFAULT_DALLY 1
#define DEFAULT_THREADS 1
#define DEFAULT_TIMEOUT 10
#define DEFAULT_MESSAGES 100
#define DEFAULT_MESSAGESIZE 100

static PRFileDesc *debug_out = NULL;

typedef struct Shared
{
    PRBool random;
    PRBool failed;
    PRBool intermittant;
    PRIntn debug;
    PRInt32 messages;
    PRIntervalTime dally;
    PRIntervalTime timeout;
    PRInt32 message_length;
    PRNetAddr serverAddress;
} Shared;

static PRIntervalTime Timeout(const Shared *shared)
{
    PRIntervalTime timeout = shared->timeout;
    if (shared->random)
    {
        PRIntervalTime quarter = timeout >> 2;  /* one quarter of the interval */
        PRUint32 random = rand() % quarter;  /* something in[0..timeout / 4) */
        timeout = (((3 * quarter) + random) >> 2) + quarter;  /* [75..125)% */
    }
    return timeout;
}  /* Timeout */

static void CauseTimeout(const Shared *shared)
{
    if (shared->intermittant) PR_Sleep(Timeout(shared));
}  /* CauseTimeout */

static PRStatus MakeReceiver(Shared *shared)
{
    PRStatus rv = PR_FAILURE;
#if defined(_PR_INET6)
    if (IN6_IS_ADDR_LOOPBACK(&shared->serverAddress.ipv6.ip))
#else
    if (PR_htonl(PR_INADDR_LOOPBACK) == shared->serverAddress.inet.ip)
#endif
    {
        char *argv[3];
        char path[1024 + sizeof("/tmoacc")];
        (void)getcwd(path, sizeof(path));
        (void)strcat(path, "/tmoacc");
#ifdef XP_PC
        (void)strcat(path, ".exe");
#endif
        argv[0] = path;
        if (shared->debug > 0)
        {
            argv[1] = "-d";
            argv[2] = NULL;
        }
        else argv[1] = NULL;
        if (shared->debug > 1)
            PR_fprintf(debug_out, " creating accept process %s ...", path);
        fflush(stdout);
        rv = PR_CreateProcessDetached(path, argv, NULL, NULL);
        if (PR_SUCCESS == rv)
        {
            if (shared->debug > 1)
                PR_fprintf(debug_out, " wait 5 seconds");
            if (shared->debug > 1)
                PR_fprintf(debug_out, " before connecting to accept process ...");
            fflush(stdout);
            PR_Sleep(PR_SecondsToInterval(5));
            return rv;
        }
        shared->failed = PR_TRUE;
        if (shared->debug > 0)
            PL_FPrintError(debug_out, "PR_CreateProcessDetached failed");
    }
    return rv;
}  /* MakeReceiver */

static void Connect(void *arg)
{
    PRStatus rv;
    char *buffer = NULL;
    PRFileDesc *clientSock;
    Shared *shared = (Shared*)arg;
    PRInt32 loop, bytes, flags = 0;
    struct Descriptor { PRInt32 length; PRUint32 checksum; } descriptor;
    debug_out = (0 == shared->debug) ? NULL : PR_GetSpecialFD(PR_StandardError);

    buffer = (char*)PR_MALLOC(shared->message_length);

    for (bytes = 0; bytes < shared->message_length; ++bytes)
        buffer[bytes] = (char)bytes;

    descriptor.checksum = 0;
    for (bytes = 0; bytes < shared->message_length; ++bytes)
    {
        PRUint32 overflow = descriptor.checksum & 0x80000000;
        descriptor.checksum = (descriptor.checksum << 1);
        if (0x00000000 != overflow) descriptor.checksum += 1;
        descriptor.checksum += buffer[bytes];
    }
    descriptor.checksum = PR_htonl(descriptor.checksum);

    for (loop = 0; loop < shared->messages; ++loop)
    {
        if (shared->debug > 1)
            PR_fprintf(debug_out, "[%d]socket ... ", loop);
        clientSock = PR_NewTCPSocket();
        if (clientSock)
        {
            /*
             * We need to slow down the rate of generating connect requests,
             * otherwise the listen backlog queue on the accept side may
             * become full and we will get connection refused or timeout
             * error.
             */

            PR_Sleep(shared->dally);
            if (shared->debug > 1) PR_fprintf(debug_out, "connecting ... ");
            rv = PR_Connect(
                clientSock, &shared->serverAddress, Timeout(shared));
            if (PR_SUCCESS == rv)
            {
                PRInt32 descriptor_length = (loop < (shared->messages - 1)) ?
                    shared->message_length : 0;
                descriptor.length = PR_htonl(descriptor_length);
                if (shared->debug > 1)
                    PR_fprintf(
                        debug_out, "sending %d bytes ... ", descriptor_length);
                CauseTimeout(shared);  /* might cause server to timeout */
                bytes = PR_Send(
                    clientSock, &descriptor, sizeof(descriptor),
                    flags, Timeout(shared));
                if (bytes != sizeof(descriptor))
                {
                    shared->failed = PR_TRUE;
                    if (shared->debug > 0)
                        PL_FPrintError(debug_out, "PR_Send failed");
                }
                if (0 != descriptor_length)
                {
                    CauseTimeout(shared);
                    bytes = PR_Send(
                        clientSock, buffer, descriptor_length,
                        flags, Timeout(shared));
                    if (bytes != descriptor_length)
                    {
                        shared->failed = PR_TRUE;
                        if (shared->debug > 0)
                            PL_FPrintError(debug_out, "PR_Send failed");
                    }
                }
                if (shared->debug > 1) PR_fprintf(debug_out, "closing ... ");
                rv = PR_Shutdown(clientSock, PR_SHUTDOWN_BOTH);
                rv = PR_Close(clientSock);
                if (shared->debug > 1)
                {
                    if (PR_SUCCESS == rv) PR_fprintf(debug_out, "\n");
                    else PL_FPrintError(debug_out, "shutdown failed");
                }
            }
            else
            {
                if (shared->debug > 1) PL_FPrintError(debug_out, "connect failed");
                PR_Close(clientSock);
                if ((loop == 0) && (PR_GetError() == PR_CONNECT_REFUSED_ERROR))
                {
                    if (MakeReceiver(shared) == PR_FAILURE) break;
                }
                else
                {
                    if (shared->debug > 1) PR_fprintf(debug_out, " exiting\n");
                    break;
                }
            }
        }
        else
        {
            shared->failed = PR_TRUE;
            if (shared->debug > 0) PL_FPrintError(debug_out, "create socket");
            break;
        }
    }

    PR_DELETE(buffer);
}  /* Connect */

#ifdef _PR_INET6

/*
 * Below are some of the IPv6 hosts at Netscape and their IPv4
 * addresses.  We will use their IPv4-compatible IPv6 addresses
 * for automatic tunneling.
 */

static const char *ipv6_host[] = {
    "dijkstra",
    "wirth",
    "gandalf",
    "raven"
};

static const unsigned char ipv6_host_ipv4_addr[] = {
    208, 12, 62, 49,
    208, 12, 62, 98,
    208, 12, 62, 55,
    206, 222, 228, 80
};

/*
 * Return PR_TRUE if 'host' is an IPv6 host at Netscape in the table
 * above, and fill 'addr->ipv6.ip' with its IPv4-compatible IPv6 address
 * for automatic tunneling.
 */

static PRBool
IPv6AutoTunnelHost(const char *host, PRNetAddr *addr)
{
    int nHosts = sizeof(ipv6_host) / sizeof(ipv6_host[0]);
    int i;

    for (i = 0; i < nHosts; i++)
    {
        if (strcmp(host, ipv6_host[i]) == 0)
        {
            memset(&addr->ipv6.ip, 0, 12);
            memcpy(((unsigned char *) &addr->ipv6.ip) + 12,
                &ipv6_host_ipv4_addr[4 * i], 4);
            PR_ASSERT(IN6_IS_ADDR_V4COMPAT(&addr->ipv6.ip));
            return PR_TRUE;
        }
    }
    return PR_FALSE;
}
#endif /* _PR_INET6 */

int Tmocon(int argc, char **argv)
{
    /*
     * USAGE
     * -d       turn on debugging output                (default = off)
     * -v       turn on verbose output                  (default = off)
     * -h <n>   dns name of host serving the connection (default = self)
     * -i       dally intermittantly to cause timeouts  (default = off)
     * -m <n>   number of messages to send              (default = 100)
     * -s <n>   size of each message                    (default = 100)
     * -t <n>   number of threads sending               (default = 1)
     * -G       use global threads                      (default = local)
     * -T <n>   timeout on I/O operations (seconds)     (default = 10)
     * -D <n>   dally between connect requests (seconds)(default = 0)
     * -R       randomize the dally types around 'T'    (default = no)
     */

    PRStatus rv;
    PLOptStatus os;
    Shared *shared = NULL;
    PRThread **thread = NULL;
    PRIntn index, threads = DEFAULT_THREADS;
    PRThreadScope thread_scope = PR_LOCAL_THREAD;
    PRInt32 dally = DEFAULT_DALLY, timeout = DEFAULT_TIMEOUT;
    PLOptState *opt = PL_CreateOptState(argc, argv, "divGRh:m:s:t:T:D:");

#ifdef _PR_INET6
    PR_SetIPv6Enable(PR_TRUE);
#endif
    shared = PR_NEWZAP(Shared);  /* this is leaked */

    shared->debug = 0;
    shared->failed = PR_FALSE;
    shared->random = PR_FALSE;
    shared->messages = DEFAULT_MESSAGES;
    shared->message_length = DEFAULT_MESSAGESIZE;

    PR_STDIO_INIT();
    rv = PR_InitializeNetAddr(PR_IpAddrLoopback, BASE_PORT, &shared->serverAddress);
    PR_ASSERT(PR_SUCCESS == rv);
    
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':
            if (0 == shared->debug) shared->debug = 1;
            break;
        case 'v':
            if (0 == shared->debug) shared->debug = 2;
            break;
        case 'i':
            shared->intermittant = PR_TRUE;
            break;
        case 'R':
            shared->random = PR_TRUE;
            break;
        case 'G':
            thread_scope = PR_GLOBAL_THREAD;
            break;
        case 'h':  /* the value for backlock */
            {
#ifdef _PR_INET6
                if (!IPv6AutoTunnelHost(opt->value, &shared->serverAddress))
                {
#endif
                    PRIntn es = 0;
                    PRHostEnt host;
                    char buffer[1024];
                    (void)PR_GetHostByName(
                        opt->value, buffer, sizeof(buffer), &host);
                    es = PR_EnumerateHostEnt(
                        es, &host, BASE_PORT, &shared->serverAddress);
                    PR_ASSERT(es > 0);
#ifdef _PR_INET6
                }
#endif
            }
            break;
        case 'm':  /* number of messages to send */
            shared->messages = atoi(opt->value);
            break;
        case 't':  /* number of threads sending */
            threads = atoi(opt->value);
            break;
        case 'D':  /* dally time between transmissions */
            dally = atoi(opt->value);
            break;
        case 'T':  /* timeout on I/O operations */
            timeout = atoi(opt->value);
            break;
        case 's':  /* total size of each message */
            shared->message_length = atoi(opt->value);
            break;
        default:
            break;
        }
    }
        PL_DestroyOptState(opt);

    if (0 == timeout) timeout = DEFAULT_TIMEOUT;
    if (0 == threads) threads = DEFAULT_THREADS;
    if (0 == shared->messages) shared->messages = DEFAULT_MESSAGES;
    if (0 == shared->message_length) shared->message_length = DEFAULT_MESSAGESIZE;

    shared->dally = PR_SecondsToInterval(dally);
    shared->timeout = PR_SecondsToInterval(timeout);

    thread = (PRThread**)PR_CALLOC(threads * sizeof(PRThread*));

    for (index = 0; index < threads; ++index)
        thread[index] = PR_CreateThread(
            PR_USER_THREAD, Connect, shared,
            PR_PRIORITY_NORMAL, thread_scope,
            PR_JOINABLE_THREAD, 0);
    for (index = 0; index < threads; ++index)
        rv = PR_JoinThread(thread[index]);

    PR_DELETE(thread);

    PR_fprintf(
        PR_GetSpecialFD(PR_StandardError), "%s\n",
        ((shared->failed) ? "FAILED" : "PASSED"));
    return (shared->failed) ? 1 : 0;
}

int main(int argc, char **argv)
{
    return (PR_VersionCheck(PR_VERSION)) ?
        PR_Initialize(Tmocon, argc, argv, 4) : -1;
}  /* main */

/* tmocon.c */


