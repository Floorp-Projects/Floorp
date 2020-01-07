/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:        thruput.c
** Description: Test server's throughput capability comparing various
**              implmentation strategies.
**
** Note:        Requires a server machine and an aribitrary number of
**              clients to bang on it. Trust the numbers on the server
**              more than those being displayed by the various clients.
*/

#include "prerror.h"
#include "prinrval.h"
#include "prinit.h"
#include "prio.h"
#include "prlock.h"
#include "prmem.h"
#include "prnetdb.h"
#include "prprf.h"
#include "prthread.h"
#include "pprio.h"
#include "plerror.h"
#include "plgetopt.h"

#define ADDR_BUFFER 100

#ifdef DEBUG
#define PORT_INC_DO +100
#else
#define PORT_INC_DO
#endif
#ifdef IS_64
#define PORT_INC_3264 +200
#else
#define PORT_INC_3264
#endif

#define PORT_NUMBER 51877 PORT_INC_DO PORT_INC_3264

#define SAMPLING_INTERVAL 10
#define BUFFER_SIZE (32 * 1024)

static PRInt32 domain = PR_AF_INET;
static PRInt32 protocol = 6;  /* TCP */
static PRFileDesc *err = NULL;
static PRIntn concurrency = 1;
static PRInt32 xport_buffer = -1;
static PRUint32 initial_streams = 1;
static PRInt32 buffer_size = BUFFER_SIZE;
static PRThreadScope thread_scope = PR_LOCAL_THREAD;

typedef struct Shared
{
    PRLock *ml;
    PRUint32 sampled;
    PRUint32 threads;
    PRIntervalTime timein;
    PRNetAddr server_address;
} Shared;

static Shared *shared = NULL;

static PRStatus PrintAddress(const PRNetAddr* address)
{
    char buffer[ADDR_BUFFER];
    PRStatus rv = PR_NetAddrToString(address, buffer, sizeof(buffer));
    if (PR_SUCCESS == rv) {
        PR_fprintf(err, "%s:%u\n", buffer, PR_ntohs(address->inet.port));
    }
    else {
        PL_FPrintError(err, "PR_NetAddrToString");
    }
    return rv;
}  /* PrintAddress */


static void PR_CALLBACK Clientel(void *arg)
{
    PRStatus rv;
    PRFileDesc *xport;
    PRInt32 bytes, sampled;
    PRIntervalTime now, interval;
    PRBool do_display = PR_FALSE;
    Shared *shared = (Shared*)arg;
    char *buffer = (char*)PR_Malloc(buffer_size);
    PRNetAddr *server_address = &shared->server_address;
    PRIntervalTime connect_timeout = PR_SecondsToInterval(5);
    PRIntervalTime sampling_interval = PR_SecondsToInterval(SAMPLING_INTERVAL);

    PR_fprintf(err, "Client connecting to ");
    (void)PrintAddress(server_address);

    do
    {
        xport = PR_Socket(domain, PR_SOCK_STREAM, protocol);
        if (NULL == xport)
        {
            PL_FPrintError(err, "PR_Socket");
            return;
        }

        if (xport_buffer != -1)
        {
            PRSocketOptionData data;
            data.option = PR_SockOpt_RecvBufferSize;
            data.value.recv_buffer_size = (PRSize)xport_buffer;
            rv = PR_SetSocketOption(xport, &data);
            if (PR_FAILURE == rv) {
                PL_FPrintError(err, "PR_SetSocketOption - ignored");
            }
            data.option = PR_SockOpt_SendBufferSize;
            data.value.send_buffer_size = (PRSize)xport_buffer;
            rv = PR_SetSocketOption(xport, &data);
            if (PR_FAILURE == rv) {
                PL_FPrintError(err, "PR_SetSocketOption - ignored");
            }
        }

        rv = PR_Connect(xport, server_address, connect_timeout);
        if (PR_FAILURE == rv)
        {
            PL_FPrintError(err, "PR_Connect");
            if (PR_IO_TIMEOUT_ERROR != PR_GetError()) {
                PR_Sleep(connect_timeout);
            }
            PR_Close(xport);  /* delete it and start over */
        }
    } while (PR_FAILURE == rv);

    do
    {
        bytes = PR_Recv(
                    xport, buffer, buffer_size, 0, PR_INTERVAL_NO_TIMEOUT);
        PR_Lock(shared->ml);
        now = PR_IntervalNow();
        shared->sampled += bytes;
        interval = now - shared->timein;
        if (interval > sampling_interval)
        {
            sampled = shared->sampled;
            shared->timein = now;
            shared->sampled = 0;
            do_display = PR_TRUE;
        }
        PR_Unlock(shared->ml);

        if (do_display)
        {
            PRUint32 rate = sampled / PR_IntervalToMilliseconds(interval);
            PR_fprintf(err, "%u streams @ %u Kbytes/sec\n", shared->threads, rate);
            do_display = PR_FALSE;
        }

    } while (bytes > 0);
}  /* Clientel */

static void Client(const char *server_name)
{
    PRStatus rv;
    PRHostEnt host;
    char buffer[PR_NETDB_BUF_SIZE];
    PRIntervalTime dally = PR_SecondsToInterval(60);
    PR_fprintf(err, "Translating the name %s\n", server_name);
    rv = PR_GetHostByName(server_name, buffer, sizeof(buffer), &host);
    if (PR_FAILURE == rv) {
        PL_FPrintError(err, "PR_GetHostByName");
    }
    else
    {
        if (PR_EnumerateHostEnt(
                0, &host, PORT_NUMBER, &shared->server_address) < 0) {
            PL_FPrintError(err, "PR_EnumerateHostEnt");
        }
        else
        {
            do
            {
                shared->threads += 1;
                (void)PR_CreateThread(
                    PR_USER_THREAD, Clientel, shared,
                    PR_PRIORITY_NORMAL, thread_scope,
                    PR_UNJOINABLE_THREAD, 8 * 1024);
                if (shared->threads == initial_streams)
                {
                    PR_Sleep(dally);
                    initial_streams += 1;
                }
            } while (PR_TRUE);
        }
    }
}

static void PR_CALLBACK Servette(void *arg)
{
    PRInt32 bytes, sampled;
    PRIntervalTime now, interval;
    PRBool do_display = PR_FALSE;
    PRFileDesc *client = (PRFileDesc*)arg;
    char *buffer = (char*)PR_Malloc(buffer_size);
    PRIntervalTime sampling_interval = PR_SecondsToInterval(SAMPLING_INTERVAL);

    if (xport_buffer != -1)
    {
        PRStatus rv;
        PRSocketOptionData data;
        data.option = PR_SockOpt_RecvBufferSize;
        data.value.recv_buffer_size = (PRSize)xport_buffer;
        rv = PR_SetSocketOption(client, &data);
        if (PR_FAILURE == rv) {
            PL_FPrintError(err, "PR_SetSocketOption - ignored");
        }
        data.option = PR_SockOpt_SendBufferSize;
        data.value.send_buffer_size = (PRSize)xport_buffer;
        rv = PR_SetSocketOption(client, &data);
        if (PR_FAILURE == rv) {
            PL_FPrintError(err, "PR_SetSocketOption - ignored");
        }
    }

    do
    {
        bytes = PR_Send(
                    client, buffer, buffer_size, 0, PR_INTERVAL_NO_TIMEOUT);

        PR_Lock(shared->ml);
        now = PR_IntervalNow();
        shared->sampled += bytes;
        interval = now - shared->timein;
        if (interval > sampling_interval)
        {
            sampled = shared->sampled;
            shared->timein = now;
            shared->sampled = 0;
            do_display = PR_TRUE;
        }
        PR_Unlock(shared->ml);

        if (do_display)
        {
            PRUint32 rate = sampled / PR_IntervalToMilliseconds(interval);
            PR_fprintf(err, "%u streams @ %u Kbytes/sec\n", shared->threads, rate);
            do_display = PR_FALSE;
        }
    } while (bytes > 0);
}  /* Servette */

static void Server(void)
{
    PRStatus rv;
    PRNetAddr server_address, client_address;
    PRFileDesc *xport = PR_Socket(domain, PR_SOCK_STREAM, protocol);

    if (NULL == xport)
    {
        PL_FPrintError(err, "PR_Socket");
        return;
    }

    rv = PR_InitializeNetAddr(PR_IpAddrAny, PORT_NUMBER, &server_address);
    if (PR_FAILURE == rv) {
        PL_FPrintError(err, "PR_InitializeNetAddr");
    }
    else
    {
        rv = PR_Bind(xport, &server_address);
        if (PR_FAILURE == rv) {
            PL_FPrintError(err, "PR_Bind");
        }
        else
        {
            PRFileDesc *client;
            rv = PR_Listen(xport, 10);
            PR_fprintf(err, "Server listening on ");
            (void)PrintAddress(&server_address);
            do
            {
                client = PR_Accept(
                             xport, &client_address, PR_INTERVAL_NO_TIMEOUT);
                if (NULL == client) {
                    PL_FPrintError(err, "PR_Accept");
                }
                else
                {
                    PR_fprintf(err, "Server accepting from ");
                    (void)PrintAddress(&client_address);
                    shared->threads += 1;
                    (void)PR_CreateThread(
                        PR_USER_THREAD, Servette, client,
                        PR_PRIORITY_NORMAL, thread_scope,
                        PR_UNJOINABLE_THREAD, 8 * 1024);
                }
            } while (PR_TRUE);

        }
    }
}  /* Server */

static void Help(void)
{
    PR_fprintf(err, "Usage: [-h] [<server>]\n");
    PR_fprintf(err, "\t-s <n>   Initial # of connections        (default: 1)\n");
    PR_fprintf(err, "\t-C <n>   Set 'concurrency'               (default: 1)\n");
    PR_fprintf(err, "\t-b <nK>  Client buffer size              (default: 32k)\n");
    PR_fprintf(err, "\t-B <nK>  Transport recv/send buffer size (default: sys)\n");
    PR_fprintf(err, "\t-G       Use GLOBAL threads              (default: LOCAL)\n");
    PR_fprintf(err, "\t-X       Use XTP transport               (default: TCP)\n");
    PR_fprintf(err, "\t-6       Use IPv6                        (default: IPv4)\n");
    PR_fprintf(err, "\t-h       This message and nothing else\n");
    PR_fprintf(err, "\t<server> DNS name of server\n");
    PR_fprintf(err, "\t\tIf <server> is not specified, this host will be\n");
    PR_fprintf(err, "\t\tthe server and not act as a client.\n");
}  /* Help */

int main(int argc, char **argv)
{
    PLOptStatus os;
    const char *server_name = NULL;
    PLOptState *opt = PL_CreateOptState(argc, argv, "hGX6C:b:s:B:");

    err = PR_GetSpecialFD(PR_StandardError);

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option)
        {
            case 0:  /* Name of server */
                server_name = opt->value;
                break;
            case 'G':  /* Globular threads */
                thread_scope = PR_GLOBAL_THREAD;
                break;
            case 'X':  /* Use XTP as the transport */
                protocol = 36;
                break;
            case '6':  /* Use IPv6 */
                domain = PR_AF_INET6;
                break;
            case 's':  /* initial_streams */
                initial_streams = atoi(opt->value);
                break;
            case 'C':  /* concurrency */
                concurrency = atoi(opt->value);
                break;
            case 'b':  /* buffer size */
                buffer_size = 1024 * atoi(opt->value);
                break;
            case 'B':  /* buffer size */
                xport_buffer = 1024 * atoi(opt->value);
                break;
            case 'h':  /* user wants some guidance */
            default:
                Help();  /* so give him an earful */
                return 2;  /* but not a lot else */
        }
    }
    PL_DestroyOptState(opt);

    shared = PR_NEWZAP(Shared);
    shared->ml = PR_NewLock();

    PR_fprintf(err,
               "This machine is %s\n",
               (NULL == server_name) ? "the SERVER" : "a CLIENT");

    PR_fprintf(err,
               "Transport being used is %s\n",
               (6 == protocol) ? "TCP" : "XTP");

    if (PR_GLOBAL_THREAD == thread_scope)
    {
        if (1 != concurrency)
        {
            PR_fprintf(err, "  **Concurrency > 1 and GLOBAL threads!?!?\n");
            PR_fprintf(err, "  **Ignoring concurrency\n");
            concurrency = 1;
        }
    }

    if (1 != concurrency)
    {
        PR_SetConcurrency(concurrency);
        PR_fprintf(err, "Concurrency set to %u\n", concurrency);
    }

    PR_fprintf(err,
               "All threads will be %s\n",
               (PR_GLOBAL_THREAD == thread_scope) ? "GLOBAL" : "LOCAL");

    PR_fprintf(err, "Client buffer size will be %u\n", buffer_size);

    if (-1 != xport_buffer)
        PR_fprintf(
            err, "Transport send & receive buffer size will be %u\n", xport_buffer);


    if (NULL == server_name) {
        Server();
    }
    else {
        Client(server_name);
    }

    return 0;
}  /* main */

/* thruput.c */

