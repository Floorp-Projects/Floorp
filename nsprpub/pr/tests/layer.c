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
#include "prprf.h"
#include "prlog.h"
#include "prnetdb.h"
#include "prthread.h"

#include "plerror.h"
#include "plgetopt.h"
#include "prwin16.h"

#include <stdlib.h>

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

typedef enum Verbosity {silent, quiet, chatty, noisy} Verbosity;

static PRIntn minor_iterations = 5;
static PRIntn major_iterations = 1;
static Verbosity verbosity = quiet;
static PRUint16 default_port = 12273;

static PRFileDesc *PushLayer(PRFileDesc *stack)
{
    PRFileDesc *layer = PR_CreateIOLayerStub(identity, &myMethods);
    PRStatus rv = PR_PushIOLayer(stack, PR_GetLayersIdentity(stack), layer);
    if (verbosity > quiet)
        PR_fprintf(logFile, "Pushed layer(0x%x) onto stack(0x%x)\n", layer, stack);
    PR_ASSERT(PR_SUCCESS == rv);
    return stack;
}  /* PushLayer */

#if 0
static PRFileDesc *PopLayer(PRFileDesc *stack)
{
    PRFileDesc *popped = PR_PopIOLayer(stack, identity);
    if (verbosity > quiet)
        PR_fprintf(logFile, "Popped layer(0x%x) from stack(0x%x)\n", popped, stack);
    popped->dtor(popped);
    
    return stack;
}  /* PopLayer */
#endif

static void PR_CALLBACK Client(void *arg)
{
    PRStatus rv;
    PRUint8 buffer[100];
    PRIntn empty_flags = 0;
    PRIntn bytes_read, bytes_sent;
    PRFileDesc *stack = (PRFileDesc*)arg;

    /* Initialize the buffer so that Purify won't complain */
    memset(buffer, 0, sizeof(buffer));

    rv = PR_Connect(stack, &server_address, PR_INTERVAL_NO_TIMEOUT);
    PR_ASSERT(PR_SUCCESS == rv);
    while (minor_iterations-- > 0)
    {
        bytes_sent = PR_Send(
            stack, buffer, sizeof(buffer), empty_flags, PR_INTERVAL_NO_TIMEOUT);
        PR_ASSERT(sizeof(buffer) == bytes_sent);
        if (verbosity > chatty)
            PR_fprintf(logFile, "Client sending %d bytes\n", bytes_sent);
        bytes_read = PR_Recv(
            stack, buffer, bytes_sent, empty_flags, PR_INTERVAL_NO_TIMEOUT);
        if (verbosity > chatty)
            PR_fprintf(logFile, "Client receiving %d bytes\n", bytes_read);
        PR_ASSERT(bytes_read == bytes_sent);
    }

    if (verbosity > quiet)
        PR_fprintf(logFile, "Client shutting down stack\n");
    
    rv = PR_Shutdown(stack, PR_SHUTDOWN_BOTH); PR_ASSERT(PR_SUCCESS == rv);
}  /* Client */

static void PR_CALLBACK Server(void *arg)
{
    PRStatus rv;
    PRUint8 buffer[100];
    PRFileDesc *service;
    PRUintn empty_flags = 0;
    PRIntn bytes_read, bytes_sent;
    PRFileDesc *stack = (PRFileDesc*)arg;
    PRNetAddr any_address, client_address;

    rv = PR_InitializeNetAddr(PR_IpAddrAny, default_port, &any_address);
    PR_ASSERT(PR_SUCCESS == rv);

    rv = PR_Bind(stack, &any_address); PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_Listen(stack, 10); PR_ASSERT(PR_SUCCESS == rv);

    service = PR_Accept(stack, &client_address, PR_INTERVAL_NO_TIMEOUT);
    if (verbosity > quiet)
        PR_fprintf(logFile, "Server accepting connection\n");

    do
    {
        bytes_read = PR_Recv(
            service, buffer, sizeof(buffer), empty_flags, PR_INTERVAL_NO_TIMEOUT);
        if (0 != bytes_read)
        {
            if (verbosity > chatty)
                PR_fprintf(logFile, "Server receiving %d bytes\n", bytes_read);
            PR_ASSERT(bytes_read > 0);
            bytes_sent = PR_Send(
                service, buffer, bytes_read, empty_flags, PR_INTERVAL_NO_TIMEOUT);
            if (verbosity > chatty)
                PR_fprintf(logFile, "Server sending %d bytes\n", bytes_sent);
            PR_ASSERT(bytes_read == bytes_sent);
        }

    } while (0 != bytes_read);

    if (verbosity > quiet)
        PR_fprintf(logFile, "Server shutting down and closing stack\n");
    rv = PR_Shutdown(service, PR_SHUTDOWN_BOTH); PR_ASSERT(PR_SUCCESS == rv);
    rv = PR_Close(service); PR_ASSERT(PR_SUCCESS == rv);

}  /* Server */

static PRInt32 PR_CALLBACK MyRecv(
    PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    char *b = (char*)buf;
    PRFileDesc *lo = fd->lower;
    PRInt32 rv, readin = 0, request = 0;
    rv = lo->methods->recv(lo, &request, sizeof(request), flags, timeout);
    if (verbosity > chatty) PR_fprintf(
        logFile, "MyRecv sending permission for %d bytes\n", request);
    if (0 < rv)
    {
        if (verbosity > chatty) PR_fprintf(
            logFile, "MyRecv received permission request for %d bytes\n", request);
        rv = lo->methods->send(
            lo, &request, sizeof(request), flags, timeout);
        if (0 < rv)
        {
            if (verbosity > chatty) PR_fprintf(
                logFile, "MyRecv sending permission for %d bytes\n", request);
            while (readin < request)
            {
                rv = lo->methods->recv(
                    lo, b + readin, amount - readin, flags, timeout);
                if (rv <= 0) break;
                if (verbosity > chatty) PR_fprintf(
                    logFile, "MyRecv received %d bytes\n", rv);
                readin += rv;
            }
            rv = readin;
        }
    }
    return rv;
}  /* MyRecv */

static PRInt32 PR_CALLBACK MySend(
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    PRFileDesc *lo = fd->lower;
    const char *b = (const char*)buf;
    PRInt32 rv, wroteout = 0, request;
    if (verbosity > chatty) PR_fprintf(
        logFile, "MySend asking permission to send %d bytes\n", amount);
    rv = lo->methods->send(lo, &amount, sizeof(amount), flags, timeout);
    if (0 < rv)
    {
        rv = lo->methods->recv(
            lo, &request, sizeof(request), flags, timeout);
        if (0 < rv)
        {
            PR_ASSERT(request == amount);
            if (verbosity > chatty) PR_fprintf(
                logFile, "MySend got permission to send %d bytes\n", request);
            while (wroteout < request)
            {
                rv = lo->methods->send(
                    lo, b + wroteout, request - wroteout, flags, timeout);
                if (rv <= 0) break;
                if (verbosity > chatty) PR_fprintf(
                    logFile, "MySend wrote %d bytes\n", rv);
                wroteout += rv;
            }
            rv = amount;
        }
    }
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
    PRIntn mits;
    PLOptStatus os;
    PRFileDesc *client, *service;
    const char *server_name = NULL;
    const PRIOMethods *stubMethods;
    PRThread *client_thread, *server_thread;
    PRThreadScope thread_scope = PR_LOCAL_THREAD;
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

    /* one type w/o layering */

    mits = minor_iterations;
    while (major_iterations-- > 0)
    {
        if (verbosity > silent)
            PR_fprintf(logFile, "Beginning non-layered test\n");
        client = PR_NewTCPSocket(); PR_ASSERT(NULL != client);
        service = PR_NewTCPSocket(); PR_ASSERT(NULL != service);

        minor_iterations = mits;
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

        minor_iterations = mits;
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

        rv = PR_Close(client); PR_ASSERT(PR_SUCCESS == rv);
        rv = PR_Close(service); PR_ASSERT(PR_SUCCESS == rv);
        if (verbosity > silent)
            PR_fprintf(logFile, "Ending layered test\n");
    }
    return 0;
}  /* main */

/* layer.c */
