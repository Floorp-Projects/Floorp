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

#include "plgetopt.h"

#include <stdlib.h>
#include <string.h>

#ifdef XP_MAC
#include "prlog.h"
#define printf PR_LogPrint
#endif


#ifndef IOV_MAX
#define IOV_MAX 16
#endif

#define BASE_PORT 9867

int PR_CALLBACK Writev(int argc, char **argv)
{

    PRStatus rv;
    PRNetAddr serverAddr;
    PRFileDesc *clientSock, *debug = NULL;

    char *buffer = NULL;
    PRIOVec *iov = NULL;
    PRBool passed = PR_TRUE;
    PRIntervalTime timein, elapsed, timeout;
    PRIntervalTime tmo_min = 0x7fffffff, tmo_max = 0, tmo_elapsed = 0;
    PRInt32 tmo_counted = 0, iov_index, loop, bytes, number_fragments;
    PRInt32 message_length = 100, fragment_length = 100, messages = 100;
    struct Descriptor { PRInt32 length; PRUint32 checksum; } descriptor;

    /*
     * USAGE
     * -h       dns name of host serving the connection (default = self)
     * -m       number of messages to send              (default = 100)
     * -s       size of each message                    (default = 100)
     * -f       size of each message fragment           (default = 100)
     */

	PLOptStatus os;
	PLOptState *opt = PL_CreateOptState(argc, argv, "dh:m:s:f:");

    PR_STDIO_INIT();
    rv = PR_InitializeNetAddr(PR_IpAddrLoopback, BASE_PORT, &serverAddr);
    PR_ASSERT(PR_SUCCESS == rv);

	while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'h':  /* the remote host */
            {
                PRIntn es = 0;
                PRHostEnt host;
                char buffer[1024];
                (void)PR_GetHostByName(opt->value, buffer, sizeof(buffer), &host);
                es = PR_EnumerateHostEnt(es, &host, BASE_PORT, &serverAddr);
                PR_ASSERT(es > 0);
            }
            break;
        case 'd':  /* debug mode */
            debug = PR_GetSpecialFD(PR_StandardError);
            break;
        case 'm':  /* number of messages to send */
            messages = atoi(opt->value);
            break;
        case 's':  /* total size of each message */
            message_length = atoi(opt->value);
            break;
        case 'f':  /* size of each message fragment */
            fragment_length = atoi(opt->value);
            break;
        default:
            break;
        }
    }
	PL_DestroyOptState(opt);

    buffer = (char*)malloc(message_length);

    number_fragments = (message_length + fragment_length - 1) / fragment_length + 1;
    while (IOV_MAX < number_fragments)
    {
        fragment_length = message_length / (IOV_MAX - 2);
        number_fragments = (message_length + fragment_length - 1) /
            fragment_length + 1;
        if (NULL != debug) PR_fprintf(debug, 
            "Too many fragments - reset fragment length to %ld\n", fragment_length);
    }
    iov = (PRIOVec*)malloc(number_fragments * sizeof(PRIOVec));

    iov[0].iov_base = (char*)&descriptor;
    iov[0].iov_len = sizeof(descriptor);
    for (iov_index = 1; iov_index < number_fragments; ++iov_index)
    {
        iov[iov_index].iov_base = buffer + (iov_index - 1) * fragment_length;
        iov[iov_index].iov_len = fragment_length;
    }

    for (bytes = 0; bytes < message_length; ++bytes)
        buffer[bytes] = (char)bytes;

    timeout = PR_SecondsToInterval(1);

    for (loop = 0; loop < messages; ++loop)
    {
        if (NULL != debug)
            PR_fprintf(debug, "[%d]socket ... ", loop);
        clientSock = PR_NewTCPSocket();
        if (clientSock)
        {
            timein = PR_IntervalNow();
            if (NULL != debug)
                PR_fprintf(debug, "connecting ... ");
            rv = PR_Connect(clientSock, &serverAddr, timeout);
            if (PR_SUCCESS == rv)
            {
                descriptor.checksum = 0;
                descriptor.length = (loop < (messages - 1)) ? message_length : 0;
                if (0 == descriptor.length) number_fragments = 1;
                else
                    for (iov_index = 0; iov_index < descriptor.length; ++iov_index)
                    {
                        PRUint32 overflow = descriptor.checksum & 0x80000000;
                        descriptor.checksum = (descriptor.checksum << 1);
                        if (0x00000000 != overflow) descriptor.checksum += 1;
                        descriptor.checksum += buffer[iov_index];
                    }
                if (NULL != debug) PR_fprintf(
                    debug, "sending %d bytes ... ", descriptor.length);

                /* then, at the last moment ... */
                descriptor.length = PR_ntohl(descriptor.length);
                descriptor.checksum = PR_ntohl(descriptor.checksum);

                bytes = PR_Writev(clientSock, iov, number_fragments, timeout);
                if (NULL != debug)
                    PR_fprintf(debug, "closing ... ");
                rv = PR_Shutdown(clientSock, PR_SHUTDOWN_BOTH);
                rv = PR_Close(clientSock);
                if (NULL != debug) PR_fprintf(
                    debug, "%s\n", ((PR_SUCCESS == rv) ? "good" : "bad"));
                elapsed = PR_IntervalNow() - timein;
                if (elapsed < tmo_min) tmo_min = elapsed;
                else if (elapsed > tmo_max) tmo_max = elapsed;
                tmo_elapsed += elapsed;
                tmo_counted += 1;
            }
            else
            {
                if (NULL != debug) PR_fprintf(
                    debug, "failed - retrying (%d, %d)\n",
                    PR_GetError(), PR_GetOSError());
                PR_Close(clientSock);
            }
        }
        else if (NULL != debug)
        {
            PR_fprintf(debug, "unable to create client socket\n");
            passed = PR_FALSE;
        }
    }
    if (NULL != debug) {
        if (0 == tmo_counted) {
            PR_fprintf(debug, "No connection made\n");
        } else {
        PR_fprintf(
            debug, "\nTimings: %d [%d] %d (microseconds)\n",
            PR_IntervalToMicroseconds(tmo_min),
            PR_IntervalToMicroseconds(tmo_elapsed / tmo_counted),
            PR_IntervalToMicroseconds(tmo_max));
	}
    }

    PR_DELETE(buffer);
    PR_DELETE(iov);

    PR_fprintf(
        PR_GetSpecialFD(PR_StandardError),
        "%s\n", (passed) ? "PASSED" : "FAILED");
    return (passed) ? 0 : 1;
}

int main(int argc, char **argv)
{
    return (PR_VersionCheck(PR_VERSION)) ?
        PR_Initialize(Writev, argc, argv, 4) : -1;
}  /* main */

/* writev.c */


