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

#include "prio.h"
#include "prmem.h"
#include "prprf.h"
#include "prlog.h"
#include "prerror.h"
#include "prnetdb.h"
#include "prthread.h"

#include "plerror.h"
#include "plgetopt.h"
#include "prwin16.h"

#include <stdlib.h>
#include <string.h>

/*
** Testing layering of I/O
**
**      The layered server
** A thread that acts as a server. It creates a TCP listener with a dummy
** layer pushed on top. Then listens for incoming connections. Each connection
** request for connection will be layered as well, accept one request, echo
** it back and close.
**
**      The layered client
** Pretty much what you'd expect.
*/

static PRFileDesc *logFile;
static PRDescIdentity identity;
static PRNetAddr server_address;

static PRIOMethods myMethods;

typedef enum {rcv_get_debit, rcv_send_credit, rcv_data} RcvState;
typedef enum {xmt_send_debit, xmt_recv_credit, xmt_data} XmtState;

struct PRFilePrivate
{
    RcvState rcvstate;
    XmtState xmtstate;
    PRInt32 rcvreq, rcvinprogress;
    PRInt32 xmtreq, xmtinprogress;
};

typedef enum Verbosity {silent, quiet, chatty, noisy} Verbosity;

static PRIntn minor_iterations = 5;
static PRIntn major_iterations = 1;
static Verbosity verbosity = quiet;
static PRUint16 default_port = 12273;

static PRFileDesc *PushLayer(PRFileDesc *stack)
{
    PRStatus rv;
    PRFileDesc *layer = PR_CreateIOLayerStub(identity, &myMethods);
    layer->secret = PR_NEWZAP(PRFilePrivate);
    rv = PR_PushIOLayer(stack, PR_GetLayersIdentity(stack), layer);
    PR_ASSERT(PR_SUCCESS == rv);
    if (verbosity > quiet)
        PR_fprintf(logFile, "Pushed layer(0x%x) onto stack(0x%x)\n", layer, stack);
    return stack;
}  /* PushLayer */

static PRFileDesc *PopLayer(PRFileDesc *stack)
{
    PRFileDesc *popped = PR_PopIOLayer(stack, identity);
    if (verbosity > quiet)
        PR_fprintf(logFile, "Popped layer(0x%x) from stack(0x%x)\n", popped, stack);
    PR_DELETE(popped->secret);
    popped->dtor(popped);
    return stack;
}  /* PopLayer */

static void PR_CALLBACK Client(void *arg)
{
    PRStatus rv;
    PRIntn mits;
    PRInt32 ready;
    PRUint8 buffer[100];
    PRPollDesc polldesc;
    PRIntn empty_flags = 0;
    PRIntn bytes_read, bytes_sent;
    PRFileDesc *stack = (PRFileDesc*)arg;

    /* Initialize the buffer so that Purify won't complain */
    memset(buffer, 0, sizeof(buffer));

    rv = PR_Connect(stack, &server_address, PR_INTERVAL_NO_TIMEOUT);
    if ((PR_FAILURE == rv) && (PR_IN_PROGRESS_ERROR == PR_GetError()))
    {
        if (verbosity > quiet)
            PR_fprintf(logFile, "Client connect 'in progress'\n");
        do
        {
            polldesc.fd = stack;
            polldesc.out_flags = 0;
            polldesc.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
            ready = PR_Poll(&polldesc, 1, PR_INTERVAL_NO_TIMEOUT);
            if ((1 != ready)  /* if not 1, then we're dead */
            || (0 == (polldesc.in_flags & polldesc.out_flags)))
                { PR_ASSERT(!"Whoa!"); break; }
            if (verbosity > quiet)
                PR_fprintf(
                    logFile, "Client connect 'in progress' [0x%x]\n",
                    polldesc.out_flags);
            rv = PR_GetConnectStatus(&polldesc);
            if ((PR_FAILURE == rv)
            && (PR_IN_PROGRESS_ERROR != PR_GetError())) break;
        } while (PR_FAILURE == rv);
    }
    PR_ASSERT(PR_SUCCESS == rv);
    if (verbosity > chatty)
        PR_fprintf(logFile, "Client created connection\n");

    for (mits = 0; mits < minor_iterations; ++mits)
    {
        bytes_sent = 0;
        if (verbosity > quiet)
            PR_fprintf(logFile, "Client sending %d bytes\n", sizeof(buffer));
        do
        {
            if (verbosity > chatty)
                PR_fprintf(
                    logFile, "Client sending %d bytes\n",
                    sizeof(buffer) - bytes_sent);
            ready = PR_Send(
                stack, buffer + bytes_sent, sizeof(buffer) - bytes_sent,
                empty_flags, PR_INTERVAL_NO_TIMEOUT);
            if (verbosity > chatty)
                PR_fprintf(logFile, "Client send status [%d]\n", ready);
            if (0 < ready) bytes_sent += ready;
            else if ((-1 == ready) && (PR_WOULD_BLOCK_ERROR == PR_GetError()))
            {
                polldesc.fd = stack;
                polldesc.out_flags = 0;
                polldesc.in_flags = PR_POLL_WRITE;
                ready = PR_Poll(&polldesc, 1, PR_INTERVAL_NO_TIMEOUT);
                if ((1 != ready)  /* if not 1, then we're dead */
                || (0 == (polldesc.in_flags & polldesc.out_flags)))
                    { PR_ASSERT(!"Whoa!"); break; }
            }
            else break;
        } while (bytes_sent < sizeof(buffer));
        PR_ASSERT(sizeof(buffer) == bytes_sent);

        bytes_read = 0;
        do
        {
            if (verbosity > chatty)
                PR_fprintf(
                    logFile, "Client receiving %d bytes\n",
                    bytes_sent - bytes_read);
            ready = PR_Recv(
                stack, buffer + bytes_read, bytes_sent - bytes_read,
                empty_flags, PR_INTERVAL_NO_TIMEOUT);
            if (verbosity > chatty)
                PR_fprintf(
                    logFile, "Client receive status [%d]\n", ready);
            if (0 < ready) bytes_read += ready;
            else if ((-1 == ready) && (PR_WOULD_BLOCK_ERROR == PR_GetError()))
            {
                polldesc.fd = stack;
                polldesc.out_flags = 0;
                polldesc.in_flags = PR_POLL_READ;
                ready = PR_Poll(&polldesc, 1, PR_INTERVAL_NO_TIMEOUT);
                if ((1 != ready)  /* if not 1, then we're dead */
                || (0 == (polldesc.in_flags & polldesc.out_flags)))
                    { PR_ASSERT(!"Whoa!"); break; }
            }
            else break;
        } while (bytes_read < bytes_sent);
        if (verbosity > chatty)
            PR_fprintf(logFile, "Client received %d bytes\n", bytes_read);
        PR_ASSERT(bytes_read == bytes_sent);
    }

    if (verbosity > quiet)
        PR_fprintf(logFile, "Client shutting down stack\n");
    
    rv = PR_Shutdown(stack, PR_SHUTDOWN_BOTH); PR_ASSERT(PR_SUCCESS == rv);
}  /* Client */

static void PR_CALLBACK Server(void *arg)
{
    PRStatus rv;
    PRInt32 ready;
    PRUint8 buffer[100];
    PRFileDesc *service;
    PRUintn empty_flags = 0;
    struct PRPollDesc polldesc;
    PRIntn bytes_read, bytes_sent;
    PRFileDesc *stack = (PRFileDesc*)arg;
    PRNetAddr any_address, client_address;

    rv = PR_InitializeNetAddr(PR_IpAddrAny, default_port, &any_address);
    PR_ASSERT(PR_SUCCESS == rv);

    rv = PR_Bind(stack, &any_address); PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_Listen(stack, 10); PR_ASSERT(PR_SUCCESS == rv);

    do
    {
        if (verbosity > chatty)
            PR_fprintf(logFile, "Server accepting connection\n");
        service = PR_Accept(stack, &client_address, PR_INTERVAL_NO_TIMEOUT);
        if (verbosity > chatty)
            PR_fprintf(logFile, "Server accept status [0x%p]\n", service);
        if ((NULL == service) && (PR_WOULD_BLOCK_ERROR == PR_GetError()))
        {
            polldesc.fd = stack;
            polldesc.out_flags = 0;
            polldesc.in_flags = PR_POLL_READ | PR_POLL_EXCEPT;
            ready = PR_Poll(&polldesc, 1, PR_INTERVAL_NO_TIMEOUT);
            if ((1 != ready)  /* if not 1, then we're dead */
            || (0 == (polldesc.in_flags & polldesc.out_flags)))
                { PR_ASSERT(!"Whoa!"); break; }
        }
    } while (NULL == service);
    PR_ASSERT(NULL != service);
        
    if (verbosity > quiet)
        PR_fprintf(logFile, "Server accepting connection\n");

    do
    {
        bytes_read = 0;
        do
        {
            if (verbosity > chatty)
                PR_fprintf(
                    logFile, "Server receiving %d bytes\n",
                    sizeof(buffer) - bytes_read);
            ready = PR_Recv(
                service, buffer + bytes_read, sizeof(buffer) - bytes_read,
                empty_flags, PR_INTERVAL_NO_TIMEOUT);
            if (verbosity > chatty)
                PR_fprintf(logFile, "Server receive status [%d]\n", ready);
            if (0 < ready) bytes_read += ready;
            else if ((-1 == ready) && (PR_WOULD_BLOCK_ERROR == PR_GetError()))
            {
                polldesc.fd = service;
                polldesc.out_flags = 0;
                polldesc.in_flags = PR_POLL_READ;
                ready = PR_Poll(&polldesc, 1, PR_INTERVAL_NO_TIMEOUT);
                if ((1 != ready)  /* if not 1, then we're dead */
                || (0 == (polldesc.in_flags & polldesc.out_flags)))
                    { PR_ASSERT(!"Whoa!"); break; }
            }
            else break;
        } while (bytes_read < sizeof(buffer));

        if (0 != bytes_read)
        {
            if (verbosity > chatty)
                PR_fprintf(logFile, "Server received %d bytes\n", bytes_read);
            PR_ASSERT(bytes_read > 0);

            bytes_sent = 0;
            do
            {
                ready = PR_Send(
                    service, buffer + bytes_sent, bytes_read - bytes_sent,
                    empty_flags, PR_INTERVAL_NO_TIMEOUT);
                if (0 < ready)
                {
                    bytes_sent += ready;
                }
                else if ((-1 == ready) && (PR_WOULD_BLOCK_ERROR == PR_GetError()))
                {
                    polldesc.fd = service;
                    polldesc.out_flags = 0;
                    polldesc.in_flags = PR_POLL_WRITE;
                    ready = PR_Poll(&polldesc, 1, PR_INTERVAL_NO_TIMEOUT);
                    if ((1 != ready)  /* if not 1, then we're dead */
                    || (0 == (polldesc.in_flags & polldesc.out_flags)))
                        { PR_ASSERT(!"Whoa!"); break; }
                }
                else break;
            } while (bytes_sent < bytes_read);
            PR_ASSERT(bytes_read == bytes_sent);
            if (verbosity > chatty)
                PR_fprintf(logFile, "Server sent %d bytes\n", bytes_sent);
        }
    } while (0 != bytes_read);

    if (verbosity > quiet)
        PR_fprintf(logFile, "Server shutting down stack\n");
    rv = PR_Shutdown(service, PR_SHUTDOWN_BOTH); PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_Close(service); PR_ASSERT(PR_SUCCESS == rv);

}  /* Server */

static PRStatus PR_CALLBACK MyClose(PRFileDesc *fd)
{
    PR_DELETE(fd->secret);  /* manage my secret file object */
    return (PR_GetDefaultIOMethods())->close(fd);  /* let him do all the work */
}  /* MyClose */

static PRInt16 PR_CALLBACK MyPoll(
    PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    PRInt16 my_flags, new_flags;
    PRFilePrivate *mine = (PRFilePrivate*)fd->secret;
    if (0 != (PR_POLL_READ & in_flags))
    {
        /* client thinks he's reading */
        switch (mine->rcvstate)
        {
            case rcv_send_credit:
                my_flags = (in_flags & ~PR_POLL_READ) | PR_POLL_WRITE;
                break;
            case rcv_data:
            case rcv_get_debit:
                my_flags = in_flags;
            default: break;
        }
    }
    else if (0 != (PR_POLL_WRITE & in_flags))
    {
        /* client thinks he's writing */
        switch (mine->xmtstate)
        {
            case xmt_recv_credit:
                my_flags = (in_flags & ~PR_POLL_WRITE) | PR_POLL_READ;
                break;
            case xmt_send_debit:
            case xmt_data:
                my_flags = in_flags;
            default: break;
        }
    }
    else PR_ASSERT(!"How'd I get here?");
    new_flags = (fd->lower->methods->poll)(fd->lower, my_flags, out_flags);
    if (verbosity > chatty)
        PR_fprintf(
            logFile, "Poll [i: 0x%x, m: 0x%x, o: 0x%x, n: 0x%x]\n",
            in_flags, my_flags, *out_flags, new_flags);
    return new_flags;
}  /* MyPoll */

static PRInt32 PR_CALLBACK MyRecv(
    PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    char *b;
    PRInt32 rv;
    PRFileDesc *lo = fd->lower;
    PRFilePrivate *mine = (PRFilePrivate*)fd->secret;

    do
    {
        switch (mine->rcvstate)
        {
        case rcv_get_debit:
            b = (char*)&mine->rcvreq;
            mine->rcvreq = amount;
            rv = lo->methods->recv(
                lo, b + mine->rcvinprogress,
                sizeof(mine->rcvreq) - mine->rcvinprogress, flags, timeout);
            if (0 == rv) goto closed;
            if ((-1 == rv) && (PR_WOULD_BLOCK_ERROR == PR_GetError())) break;
            mine->rcvinprogress += rv;  /* accumulate the read */
            if (mine->rcvinprogress < sizeof(mine->rcvreq)) break;  /* loop */
            mine->rcvstate = rcv_send_credit;
            mine->rcvinprogress = 0;
        case rcv_send_credit:
            b = (char*)&mine->rcvreq;
            rv = lo->methods->send(
                lo, b + mine->rcvinprogress,
                sizeof(mine->rcvreq) - mine->rcvinprogress, flags, timeout);
            if ((-1 == rv) && (PR_WOULD_BLOCK_ERROR == PR_GetError())) break;
            mine->rcvinprogress += rv;  /* accumulate the read */
            if (mine->rcvinprogress < sizeof(mine->rcvreq)) break;  /* loop */
            mine->rcvstate = rcv_data;
            mine->rcvinprogress = 0;
        case rcv_data:
            b = (char*)buf;
            rv = lo->methods->recv(
                lo, b + mine->rcvinprogress,
                mine->rcvreq - mine->rcvinprogress, flags, timeout);
            if (0 == rv) goto closed;
            if ((-1 == rv) && (PR_WOULD_BLOCK_ERROR == PR_GetError())) break;
            mine->rcvinprogress += rv;  /* accumulate the read */
            if (mine->rcvinprogress < amount) break;  /* loop */
            mine->rcvstate = rcv_get_debit;
            mine->rcvinprogress = 0;
            return mine->rcvreq;  /* << -- that's it! */
        default:
            break;
        }
    } while (-1 != rv);
    return rv;
closed:
    mine->rcvinprogress = 0;
    mine->rcvstate = rcv_get_debit;
    return 0;
}  /* MyRecv */

static PRInt32 PR_CALLBACK MySend(
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    char *b;
    PRInt32 rv;
    PRFileDesc *lo = fd->lower;
    PRFilePrivate *mine = (PRFilePrivate*)fd->secret;

    do
    {
        switch (mine->xmtstate)
        {
        case xmt_send_debit:
            b = (char*)&mine->xmtreq;
            mine->xmtreq = amount;
            rv = lo->methods->send(
                lo, b - mine->xmtinprogress,
                sizeof(mine->xmtreq) - mine->xmtinprogress, flags, timeout);
            if ((-1 == rv) && (PR_WOULD_BLOCK_ERROR == PR_GetError())) break;
            mine->xmtinprogress += rv;
            if (mine->xmtinprogress < sizeof(mine->xmtreq)) break;
            mine->xmtstate = xmt_recv_credit;
            mine->xmtinprogress = 0;
        case xmt_recv_credit:
             b = (char*)&mine->xmtreq;
             rv = lo->methods->recv(
                lo, b + mine->xmtinprogress,
                sizeof(mine->xmtreq) - mine->xmtinprogress, flags, timeout);
            if ((-1 == rv) && (PR_WOULD_BLOCK_ERROR == PR_GetError())) break;
            mine->xmtinprogress += rv;
            if (mine->xmtinprogress < sizeof(mine->xmtreq)) break;
            mine->xmtstate = xmt_data;
            mine->xmtinprogress = 0;
        case xmt_data:
            b = (char*)buf;
            rv = lo->methods->send(
                lo, b + mine->xmtinprogress,
                mine->xmtreq - mine->xmtinprogress, flags, timeout);
            if ((-1 == rv) && (PR_WOULD_BLOCK_ERROR == PR_GetError())) break;
            mine->xmtinprogress += rv;
            if (mine->xmtinprogress < amount) break;
            mine->xmtstate = xmt_send_debit;
            mine->xmtinprogress = 0;
            return mine->xmtreq;  /* <<-- That's the one! */
        default:
            break;
        }
    } while (-1 != rv);
    return rv;
}  /* MySend */

static Verbosity ChangeVerbosity(Verbosity verbosity, PRIntn delta)
{
    PRIntn verbage = (PRIntn)verbosity + delta;
    if (verbage < (PRIntn)silent) verbage = (PRIntn)silent;
    else if (verbage > (PRIntn)noisy) verbage = (PRIntn)noisy;
    return (Verbosity)verbage;
}  /* ChangeVerbosity */

PRIntn main(PRIntn argc, char **argv)
{
    PRStatus rv;
    PLOptStatus os;
    PRFileDesc *client, *service;
    const char *server_name = NULL;
    const PRIOMethods *stubMethods;
    PRThread *client_thread, *server_thread;
    PRThreadScope thread_scope = PR_LOCAL_THREAD;
    PRSocketOptionData socket_noblock, socket_nodelay;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dqGC:c:p:");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 0:
            server_name = opt->value;
            break;
        case 'd':  /* debug mode */
            if (verbosity < noisy)
                verbosity = ChangeVerbosity(verbosity, 1);
            break;
        case 'q':  /* debug mode */
            if (verbosity > silent)
                verbosity = ChangeVerbosity(verbosity, -1);
            break;
        case 'G':  /* use global threads */
            thread_scope = PR_GLOBAL_THREAD;
            break;
        case 'C':  /* number of threads waiting */
            major_iterations = atoi(opt->value);
            break;
        case 'c':  /* number of client threads */
            minor_iterations = atoi(opt->value);
            break;
        case 'p':  /* default port */
            default_port = atoi(opt->value);
            break;
        default:
            break;
        }
    }
    PL_DestroyOptState(opt);
    PR_STDIO_INIT();

    logFile = PR_GetSpecialFD(PR_StandardError);
    identity = PR_GetUniqueIdentity("Dummy");
    stubMethods = PR_GetDefaultIOMethods();

    /*
    ** The protocol we're going to implement is one where in order to initiate
    ** a send, the sender must first solicit permission. Therefore, every
    ** send is really a send - receive - send sequence.
    */
    myMethods = *stubMethods;  /* first get the entire batch */
    myMethods.recv = MyRecv;  /* then override the ones we care about */
    myMethods.send = MySend;  /* then override the ones we care about */
    myMethods.close = MyClose;  /* then override the ones we care about */
    myMethods.poll = MyPoll;  /* then override the ones we care about */

    if (NULL == server_name)
        rv = PR_InitializeNetAddr(
            PR_IpAddrLoopback, default_port, &server_address);
    else
    {
        rv = PR_StringToNetAddr(server_name, &server_address);
        PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_InitializeNetAddr(
            PR_IpAddrNull, default_port, &server_address);
    }
    PR_ASSERT(PR_SUCCESS == rv);

    socket_noblock.value.non_blocking = PR_TRUE;
    socket_noblock.option = PR_SockOpt_Nonblocking;
    socket_nodelay.value.no_delay = PR_TRUE;
    socket_nodelay.option = PR_SockOpt_NoDelay;

    /* one type w/o layering */

    while (major_iterations-- > 0)
    {
        if (verbosity > silent)
            PR_fprintf(logFile, "Beginning non-layered test\n");

        client = PR_NewTCPSocket(); PR_ASSERT(NULL != client);
        service = PR_NewTCPSocket(); PR_ASSERT(NULL != service);

        rv = PR_SetSocketOption(client, &socket_noblock);
        PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_SetSocketOption(service, &socket_noblock);
        PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_SetSocketOption(client, &socket_nodelay);
        PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_SetSocketOption(service, &socket_nodelay);
        PR_ASSERT(PR_SUCCESS == rv);

        server_thread = PR_CreateThread(
            PR_USER_THREAD, Server, service,
            PR_PRIORITY_HIGH, thread_scope,
            PR_JOINABLE_THREAD, 16 * 1024);
        PR_ASSERT(NULL != server_thread);

        client_thread = PR_CreateThread(
            PR_USER_THREAD, Client, client,
            PR_PRIORITY_NORMAL, thread_scope,
            PR_JOINABLE_THREAD, 16 * 1024);
        PR_ASSERT(NULL != client_thread);

        rv = PR_JoinThread(client_thread);
        PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_JoinThread(server_thread);
        PR_ASSERT(PR_SUCCESS == rv);

        rv = PR_Close(client); PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_Close(service); PR_ASSERT(PR_SUCCESS == rv);
        if (verbosity > silent)
            PR_fprintf(logFile, "Ending non-layered test\n");

        /* with layering */
        if (verbosity > silent)
            PR_fprintf(logFile, "Beginning layered test\n");
        client = PR_NewTCPSocket(); PR_ASSERT(NULL != client);
        service = PR_NewTCPSocket(); PR_ASSERT(NULL != service);

        rv = PR_SetSocketOption(client, &socket_noblock);
        PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_SetSocketOption(service, &socket_noblock);
        PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_SetSocketOption(client, &socket_nodelay);
        PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_SetSocketOption(service, &socket_nodelay);
        PR_ASSERT(PR_SUCCESS == rv);

        server_thread = PR_CreateThread(
            PR_USER_THREAD, Server, PushLayer(service),
            PR_PRIORITY_HIGH, thread_scope,
            PR_JOINABLE_THREAD, 16 * 1024);
        PR_ASSERT(NULL != server_thread);

        client_thread = PR_CreateThread(
            PR_USER_THREAD, Client, PushLayer(client),
            PR_PRIORITY_NORMAL, thread_scope,
            PR_JOINABLE_THREAD, 16 * 1024);
        PR_ASSERT(NULL != client_thread);

        rv = PR_JoinThread(client_thread);
        PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_JoinThread(server_thread);
        PR_ASSERT(PR_SUCCESS == rv);

        rv = PR_Close(PopLayer(client)); PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_Close(PopLayer(service)); PR_ASSERT(PR_SUCCESS == rv);
        if (verbosity > silent)
            PR_fprintf(logFile, "Ending layered test\n");
    }
    return 0;
}  /* main */

/* nblayer.c */
