/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prio.h"
#include "prprf.h"
#include "prlog.h"
#include "prmem.h"
#include "pratom.h"
#include "prlock.h"
#include "prmwait.h"
#include "prclist.h"
#include "prerror.h"
#include "prinrval.h"
#include "prnetdb.h"
#include "prthread.h"

#include "plstr.h"
#include "plerror.h"
#include "plgetopt.h"

#include <string.h>

typedef struct Shared
{
    const char *title;
    PRLock *list_lock;
    PRWaitGroup *group;
    PRIntervalTime timeout;
} Shared;

typedef enum Verbosity {silent, quiet, chatty, noisy} Verbosity;

static PRFileDesc *debug = NULL;
static PRInt32 desc_allocated = 0;
static PRUint16 default_port = 12273;
static enum Verbosity verbosity = quiet;
static PRInt32 ops_required = 1000, ops_done = 0;
static PRThreadScope thread_scope = PR_LOCAL_THREAD;
static PRIntn client_threads = 20, worker_threads = 2, wait_objects = 50;

#if defined(DEBUG)
#define MW_ASSERT(_expr) \
    ((_expr)?((void)0):_MW_Assert(# _expr,__FILE__,__LINE__))
static void _MW_Assert(const char *s, const char *file, PRIntn ln)
{
    if (NULL != debug) PL_FPrintError(debug, NULL);
    PR_Assert(s, file, ln);
}  /* _MW_Assert */
#else
#define MW_ASSERT(_expr)
#endif

static void PrintRecvDesc(PRRecvWait *desc, const char *msg)
{
    const char *tag[] = {
        "PR_MW_INTERRUPT", "PR_MW_TIMEOUT",
        "PR_MW_FAILURE", "PR_MW_SUCCESS", "PR_MW_PENDING"};
    PR_fprintf(
        debug, "%s: PRRecvWait(@0x%x): {fd: 0x%x, outcome: %s, tmo: %u}\n",
        msg, desc, desc->fd, tag[desc->outcome + 3], desc->timeout);
}  /* PrintRecvDesc */

static Shared *MakeShared(const char *title)
{
    Shared *shared = PR_NEWZAP(Shared);
    shared->group = PR_CreateWaitGroup(1);
    shared->timeout = PR_SecondsToInterval(1);
    shared->list_lock = PR_NewLock();
    shared->title = title;
    return shared;
}  /* MakeShared */

static void DestroyShared(Shared *shared)
{
    PRStatus rv;
    if (verbosity > quiet)
        PR_fprintf(debug, "%s: destroying group\n", shared->title);
    rv = PR_DestroyWaitGroup(shared->group);
    MW_ASSERT(PR_SUCCESS == rv);
    PR_DestroyLock(shared->list_lock);
    PR_DELETE(shared);
}  /* DestroyShared */

static PRRecvWait *CreateRecvWait(PRFileDesc *fd, PRIntervalTime timeout)
{
    PRRecvWait *desc_out = PR_NEWZAP(PRRecvWait);
    MW_ASSERT(NULL != desc_out);

    MW_ASSERT(NULL != fd);
    desc_out->fd = fd;
    desc_out->timeout = timeout;
    desc_out->buffer.length = 120;
    desc_out->buffer.start = PR_CALLOC(120);

    PR_AtomicIncrement(&desc_allocated);

    if (verbosity > chatty)
        PrintRecvDesc(desc_out, "Allocated");
    return desc_out;
}  /* CreateRecvWait */

static void DestroyRecvWait(PRRecvWait *desc_out)
{
    if (verbosity > chatty)
        PrintRecvDesc(desc_out, "Destroying");
    PR_Close(desc_out->fd);
    if (NULL != desc_out->buffer.start)
        PR_DELETE(desc_out->buffer.start);
    PR_Free(desc_out);
    (void)PR_AtomicDecrement(&desc_allocated);
}  /* DestroyRecvWait */

static void CancelGroup(Shared *shared)
{
    PRRecvWait *desc_out;

    if (verbosity > quiet)
        PR_fprintf(debug, "%s Reclaiming wait descriptors\n", shared->title);

    do
    {
        desc_out = PR_CancelWaitGroup(shared->group);
        if (NULL != desc_out) DestroyRecvWait(desc_out);
    } while (NULL != desc_out);

    MW_ASSERT(0 == desc_allocated);
    MW_ASSERT(PR_GROUP_EMPTY_ERROR == PR_GetError());
}  /* CancelGroup */

static void PR_CALLBACK ClientThread(void* arg)
{
    PRStatus rv;
    PRInt32 bytes;
    PRIntn empty_flags = 0;
    PRNetAddr server_address;
    unsigned char buffer[100];
    Shared *shared = (Shared*)arg;
    PRFileDesc *server = PR_NewTCPSocket();
    if ((NULL == server)
    && (PR_PENDING_INTERRUPT_ERROR == PR_GetError())) return;
    MW_ASSERT(NULL != server);

    if (verbosity > chatty)
        PR_fprintf(debug, "%s: Server socket @0x%x\n", shared->title, server);

    /* Initialize the buffer so that Purify won't complain */
    memset(buffer, 0, sizeof(buffer));

    rv = PR_InitializeNetAddr(PR_IpAddrLoopback, default_port, &server_address);
    MW_ASSERT(PR_SUCCESS == rv);

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: Client opening connection\n", shared->title);
    rv = PR_Connect(server, &server_address, PR_INTERVAL_NO_TIMEOUT);

    if (PR_FAILURE == rv)
    {
        if (verbosity > silent) PL_FPrintError(debug, "Client connect failed");
        return;
    }

    while (ops_done < ops_required)
    {
        bytes = PR_Send(
            server, buffer, sizeof(buffer), empty_flags, PR_INTERVAL_NO_TIMEOUT);
        if ((-1 == bytes) && (PR_PENDING_INTERRUPT_ERROR == PR_GetError())) break;
        MW_ASSERT(sizeof(buffer) == bytes);
        if (verbosity > chatty)
            PR_fprintf(
                debug, "%s: Client sent %d bytes\n",
                shared->title, sizeof(buffer));
        bytes = PR_Recv(
            server, buffer, sizeof(buffer), empty_flags, PR_INTERVAL_NO_TIMEOUT);
        if (verbosity > chatty)
            PR_fprintf(
                debug, "%s: Client received %d bytes\n",
                shared->title, sizeof(buffer));
        if ((-1 == bytes) && (PR_PENDING_INTERRUPT_ERROR == PR_GetError())) break;
        MW_ASSERT(sizeof(buffer) == bytes);
        PR_Sleep(shared->timeout);
    }
    rv = PR_Close(server);
    MW_ASSERT(PR_SUCCESS == rv);

}  /* ClientThread */

static void OneInThenCancelled(Shared *shared)
{
    PRStatus rv;
    PRRecvWait *desc_out, *desc_in = PR_NEWZAP(PRRecvWait);

    shared->timeout = PR_INTERVAL_NO_TIMEOUT;

    desc_in->fd = PR_NewTCPSocket();
    desc_in->timeout = shared->timeout;

    if (verbosity > chatty) PrintRecvDesc(desc_in, "Adding desc");

    rv = PR_AddWaitFileDesc(shared->group, desc_in);
    MW_ASSERT(PR_SUCCESS == rv);

    if (verbosity > chatty) PrintRecvDesc(desc_in, "Cancelling");
    rv = PR_CancelWaitFileDesc(shared->group, desc_in);
    MW_ASSERT(PR_SUCCESS == rv);

    desc_out = PR_WaitRecvReady(shared->group);
    MW_ASSERT(desc_out == desc_in);
    MW_ASSERT(PR_MW_INTERRUPT == desc_out->outcome);
    MW_ASSERT(PR_PENDING_INTERRUPT_ERROR == PR_GetError());
    if (verbosity > chatty) PrintRecvDesc(desc_out, "Ready");

    rv = PR_Close(desc_in->fd);
    MW_ASSERT(PR_SUCCESS == rv);

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: destroying group\n", shared->title);

    PR_DELETE(desc_in);
}  /* OneInThenCancelled */

static void OneOpOneThread(Shared *shared)
{
    PRStatus rv;
    PRRecvWait *desc_out, *desc_in = PR_NEWZAP(PRRecvWait);

    desc_in->fd = PR_NewTCPSocket();
    desc_in->timeout = shared->timeout;

    if (verbosity > chatty) PrintRecvDesc(desc_in, "Adding desc");

    rv = PR_AddWaitFileDesc(shared->group, desc_in);
    MW_ASSERT(PR_SUCCESS == rv);
    desc_out = PR_WaitRecvReady(shared->group);
    MW_ASSERT(desc_out == desc_in);
    MW_ASSERT(PR_MW_TIMEOUT == desc_out->outcome);
    MW_ASSERT(PR_IO_TIMEOUT_ERROR == PR_GetError());
    if (verbosity > chatty) PrintRecvDesc(desc_out, "Ready");

    rv = PR_Close(desc_in->fd);
    MW_ASSERT(PR_SUCCESS == rv);

    PR_DELETE(desc_in);
}  /* OneOpOneThread */

static void ManyOpOneThread(Shared *shared)
{
    PRStatus rv;
    PRIntn index;
    PRRecvWait *desc_in;
    PRRecvWait *desc_out;

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: adding %d descs\n", shared->title, wait_objects);

    for (index = 0; index < wait_objects; ++index)
    {
        desc_in = CreateRecvWait(PR_NewTCPSocket(), shared->timeout);

        rv = PR_AddWaitFileDesc(shared->group, desc_in);
        MW_ASSERT(PR_SUCCESS == rv);
    }

    while (ops_done < ops_required)
    {
        desc_out = PR_WaitRecvReady(shared->group);
        MW_ASSERT(PR_MW_TIMEOUT == desc_out->outcome);
        MW_ASSERT(PR_IO_TIMEOUT_ERROR == PR_GetError());
        if (verbosity > chatty) PrintRecvDesc(desc_out, "Ready/readding");
        rv = PR_AddWaitFileDesc(shared->group, desc_out);
        MW_ASSERT(PR_SUCCESS == rv);
        (void)PR_AtomicIncrement(&ops_done);
    }

    CancelGroup(shared);
}  /* ManyOpOneThread */

static void PR_CALLBACK SomeOpsThread(void *arg)
{
    PRRecvWait *desc_out;
    PRStatus rv = PR_SUCCESS;
    Shared *shared = (Shared*)arg;
    do  /* until interrupted */
    {
        desc_out = PR_WaitRecvReady(shared->group);
        if (NULL == desc_out)
        {
            MW_ASSERT(PR_PENDING_INTERRUPT_ERROR == PR_GetError());
            if (verbosity > quiet) PR_fprintf(debug, "Aborted\n");
            break;
        }
        MW_ASSERT(PR_MW_TIMEOUT == desc_out->outcome);
        MW_ASSERT(PR_IO_TIMEOUT_ERROR == PR_GetError());
        if (verbosity > chatty) PrintRecvDesc(desc_out, "Ready");

        if (verbosity > chatty) PrintRecvDesc(desc_out, "Re-Adding");
        desc_out->timeout = shared->timeout;
        rv = PR_AddWaitFileDesc(shared->group, desc_out);
        PR_AtomicIncrement(&ops_done);
        if (ops_done > ops_required) break;
    } while (PR_SUCCESS == rv);
    MW_ASSERT(PR_SUCCESS == rv);
}  /* SomeOpsThread */

static void SomeOpsSomeThreads(Shared *shared)
{
    PRStatus rv;
    PRThread **thread;
    PRIntn index;
    PRRecvWait *desc_in;

    thread = (PRThread**)PR_CALLOC(sizeof(PRThread*) * worker_threads);

    /* Create some threads */

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: creating threads\n", shared->title);
    for (index = 0; index < worker_threads; ++index)
    {
        thread[index] = PR_CreateThread(
            PR_USER_THREAD, SomeOpsThread, shared,
            PR_PRIORITY_HIGH, thread_scope,
            PR_JOINABLE_THREAD, 16 * 1024);
    }

    /* then create some operations */
    if (verbosity > quiet)
        PR_fprintf(debug, "%s: creating desc\n", shared->title);
    for (index = 0; index < wait_objects; ++index)
    {
        desc_in = CreateRecvWait(PR_NewTCPSocket(), shared->timeout);
        rv = PR_AddWaitFileDesc(shared->group, desc_in);
        MW_ASSERT(PR_SUCCESS == rv);
    }

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: sleeping\n", shared->title);
    while (ops_done < ops_required) PR_Sleep(shared->timeout);

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: interrupting/joining threads\n", shared->title);
    for (index = 0; index < worker_threads; ++index)
    {
        rv = PR_Interrupt(thread[index]);
        MW_ASSERT(PR_SUCCESS == rv);
        rv = PR_JoinThread(thread[index]);
        MW_ASSERT(PR_SUCCESS == rv);
    }
    PR_DELETE(thread);

    CancelGroup(shared);
}  /* SomeOpsSomeThreads */

static PRStatus ServiceRequest(Shared *shared, PRRecvWait *desc)
{
    PRInt32 bytes_out;

    if (verbosity > chatty)
        PR_fprintf(
            debug, "%s: Service received %d bytes\n",
            shared->title, desc->bytesRecv);

    if (0 == desc->bytesRecv) goto quitting;
    if ((-1 == desc->bytesRecv)
    && (PR_PENDING_INTERRUPT_ERROR == PR_GetError())) goto aborted;

    bytes_out = PR_Send(
        desc->fd, desc->buffer.start, desc->bytesRecv, 0, shared->timeout);
    if (verbosity > chatty)
        PR_fprintf(
            debug, "%s: Service sent %d bytes\n",
            shared->title, bytes_out);

    if ((-1 == bytes_out)
    && (PR_PENDING_INTERRUPT_ERROR == PR_GetError())) goto aborted;
    MW_ASSERT(bytes_out == desc->bytesRecv);

    return PR_SUCCESS;

aborted:
quitting:
    return PR_FAILURE;
}  /* ServiceRequest */

static void PR_CALLBACK ServiceThread(void *arg)
{
    PRStatus rv = PR_SUCCESS;
    PRRecvWait *desc_out = NULL;
    Shared *shared = (Shared*)arg;
    do  /* until interrupted */
    {
        if (NULL != desc_out)
        {
            desc_out->timeout = PR_INTERVAL_NO_TIMEOUT;
            if (verbosity > chatty)
                PrintRecvDesc(desc_out, "Service re-adding");
            rv = PR_AddWaitFileDesc(shared->group, desc_out);
            MW_ASSERT(PR_SUCCESS == rv);
        }

        desc_out = PR_WaitRecvReady(shared->group);
        if (NULL == desc_out)
        {
            MW_ASSERT(PR_PENDING_INTERRUPT_ERROR == PR_GetError());
            break;
        }

        switch (desc_out->outcome)
        {
            case PR_MW_SUCCESS:
            {
                PR_AtomicIncrement(&ops_done);
                if (verbosity > chatty)
                    PrintRecvDesc(desc_out, "Service ready");
                rv = ServiceRequest(shared, desc_out);
                break;
            }
            case PR_MW_INTERRUPT:
                MW_ASSERT(PR_PENDING_INTERRUPT_ERROR == PR_GetError());
                rv = PR_FAILURE;  /* if interrupted, then exit */
                break;
            case PR_MW_TIMEOUT:
                MW_ASSERT(PR_IO_TIMEOUT_ERROR == PR_GetError());
            case PR_MW_FAILURE:
                if (verbosity > silent)
                    PL_FPrintError(debug, "RecvReady failure");
                break;
            default:
                break;
        }
    } while (PR_SUCCESS == rv);

    if (NULL != desc_out) DestroyRecvWait(desc_out);

}  /* ServiceThread */

static void PR_CALLBACK EnumerationThread(void *arg)
{
    PRStatus rv;
    PRIntn count;
    PRRecvWait *desc;
    Shared *shared = (Shared*)arg;
    PRIntervalTime five_seconds = PR_SecondsToInterval(5);
    PRMWaitEnumerator *enumerator = PR_CreateMWaitEnumerator(shared->group);
    MW_ASSERT(NULL != enumerator);

    while (PR_SUCCESS == PR_Sleep(five_seconds))
    {
        count = 0;
        desc = NULL;
        while (NULL != (desc = PR_EnumerateWaitGroup(enumerator, desc)))
        {
            if (verbosity > chatty) PrintRecvDesc(desc, shared->title);
            count += 1;
        }
        if (verbosity > silent)
            PR_fprintf(debug,
                "%s Enumerated %d objects\n", shared->title, count);
    }

    MW_ASSERT(PR_PENDING_INTERRUPT_ERROR == PR_GetError());


    rv = PR_DestroyMWaitEnumerator(enumerator);
    MW_ASSERT(PR_SUCCESS == rv);
}  /* EnumerationThread */

static void PR_CALLBACK ServerThread(void *arg)
{
    PRStatus rv;
    PRIntn index;
    PRRecvWait *desc_in;
    PRThread **worker_thread;
    Shared *shared = (Shared*)arg;
    PRFileDesc *listener, *service;
    PRNetAddr server_address, client_address;

    worker_thread = (PRThread**)PR_CALLOC(sizeof(PRThread*) * worker_threads);
    if (verbosity > quiet)
        PR_fprintf(debug, "%s: Server creating worker_threads\n", shared->title);
    for (index = 0; index < worker_threads; ++index)
    {
        worker_thread[index] = PR_CreateThread(
            PR_USER_THREAD, ServiceThread, shared,
            PR_PRIORITY_HIGH, thread_scope,
            PR_JOINABLE_THREAD, 16 * 1024);
    }

    rv = PR_InitializeNetAddr(PR_IpAddrAny, default_port, &server_address);
    MW_ASSERT(PR_SUCCESS == rv);

    listener = PR_NewTCPSocket(); MW_ASSERT(NULL != listener);
    if (verbosity > chatty)
        PR_fprintf(
            debug, "%s: Server listener socket @0x%x\n",
            shared->title, listener);
    rv = PR_Bind(listener, &server_address); MW_ASSERT(PR_SUCCESS == rv);
    rv = PR_Listen(listener, 10); MW_ASSERT(PR_SUCCESS == rv);
    while (ops_done < ops_required)
    {
        if (verbosity > quiet)
            PR_fprintf(debug, "%s: Server accepting connection\n", shared->title);
        service = PR_Accept(listener, &client_address, PR_INTERVAL_NO_TIMEOUT);
        if (NULL == service)
        {
            if (PR_PENDING_INTERRUPT_ERROR == PR_GetError()) break;
            PL_PrintError("Accept failed");
            MW_ASSERT(PR_FALSE && "Accept failed");
        }
        else
        {
            desc_in = CreateRecvWait(service, shared->timeout);
            desc_in->timeout = PR_INTERVAL_NO_TIMEOUT;
            if (verbosity > chatty)
                PrintRecvDesc(desc_in, "Service adding");
            rv = PR_AddWaitFileDesc(shared->group, desc_in);
            MW_ASSERT(PR_SUCCESS == rv);
        }
    }

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: Server interrupting worker_threads\n", shared->title);
    for (index = 0; index < worker_threads; ++index)
    {
        rv = PR_Interrupt(worker_thread[index]);
        MW_ASSERT(PR_SUCCESS == rv);
        rv = PR_JoinThread(worker_thread[index]);
        MW_ASSERT(PR_SUCCESS == rv);
    }
    PR_DELETE(worker_thread);

    PR_Close(listener);

    CancelGroup(shared);

}  /* ServerThread */

static void RealOneGroupIO(Shared *shared)
{
    /*
    ** Create a server that listens for connections and then services
    ** requests that come in over those connections. The server never
    ** deletes a connection and assumes a basic RPC model of operation.
    **
    ** Use worker_threads threads to service how every many open ports
    ** there might be.
    **
    ** Oh, ya. Almost forget. Create (some) clients as well.
    */
    PRStatus rv;
    PRIntn index;
    PRThread *server_thread, *enumeration_thread, **client_thread;

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: creating server_thread\n", shared->title);

    server_thread = PR_CreateThread(
        PR_USER_THREAD, ServerThread, shared,
        PR_PRIORITY_HIGH, thread_scope,
        PR_JOINABLE_THREAD, 16 * 1024);

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: creating enumeration_thread\n", shared->title);

    enumeration_thread = PR_CreateThread(
        PR_USER_THREAD, EnumerationThread, shared,
        PR_PRIORITY_HIGH, thread_scope,
        PR_JOINABLE_THREAD, 16 * 1024);

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: snoozing before creating clients\n", shared->title);
    PR_Sleep(5 * shared->timeout);

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: creating client_threads\n", shared->title);
    client_thread = (PRThread**)PR_CALLOC(sizeof(PRThread*) * client_threads);
    for (index = 0; index < client_threads; ++index)
    {
        client_thread[index] = PR_CreateThread(
            PR_USER_THREAD, ClientThread, shared,
            PR_PRIORITY_NORMAL, thread_scope,
            PR_JOINABLE_THREAD, 16 * 1024);
    }

    while (ops_done < ops_required) PR_Sleep(shared->timeout);

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: interrupting/joining client_threads\n", shared->title);
    for (index = 0; index < client_threads; ++index)
    {
        rv = PR_Interrupt(client_thread[index]);
        MW_ASSERT(PR_SUCCESS == rv);
        rv = PR_JoinThread(client_thread[index]);
        MW_ASSERT(PR_SUCCESS == rv);
    }
    PR_DELETE(client_thread);

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: interrupting/joining enumeration_thread\n", shared->title);
    rv = PR_Interrupt(enumeration_thread);
    MW_ASSERT(PR_SUCCESS == rv);
    rv = PR_JoinThread(enumeration_thread);
    MW_ASSERT(PR_SUCCESS == rv);

    if (verbosity > quiet)
        PR_fprintf(debug, "%s: interrupting/joining server_thread\n", shared->title);
    rv = PR_Interrupt(server_thread);
    MW_ASSERT(PR_SUCCESS == rv);
    rv = PR_JoinThread(server_thread);
    MW_ASSERT(PR_SUCCESS == rv);
}  /* RealOneGroupIO */

static void RunThisOne(
    void (*func)(Shared*), const char *name, const char *test_name)
{
    Shared *shared;
    if ((NULL == test_name) || (0 == PL_strcmp(name, test_name)))
    {
        if (verbosity > silent)
            PR_fprintf(debug, "%s()\n", name);
        shared = MakeShared(name);
        ops_done = 0;
        func(shared);  /* run the test */
        MW_ASSERT(0 == desc_allocated);
        DestroyShared(shared);
    }
}  /* RunThisOne */

static Verbosity ChangeVerbosity(Verbosity verbosity, PRIntn delta)
{
    PRIntn verbage = (PRIntn)verbosity;
    return (Verbosity)(verbage += delta);
}  /* ChangeVerbosity */

int main(int argc, char **argv)
{
    PLOptStatus os;
    const char *test_name = NULL;
    PLOptState *opt = PL_CreateOptState(argc, argv, "dqGc:o:p:t:w:");

    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 0:
            test_name = opt->value;
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
        case 'c':  /* number of client threads */
            client_threads = atoi(opt->value);
            break;
        case 'o':  /* operations to compelete */
            ops_required = atoi(opt->value);
            break;
        case 'p':  /* default port */
            default_port = atoi(opt->value);
            break;
        case 't':  /* number of threads waiting */
            worker_threads = atoi(opt->value);
            break;
        case 'w':  /* number of wait objects */
            wait_objects = atoi(opt->value);
            break;
        default:
            break;
        }
    }
    PL_DestroyOptState(opt);

    if (verbosity > 0)
        debug = PR_GetSpecialFD(PR_StandardError);

    RunThisOne(OneInThenCancelled, "OneInThenCancelled", test_name);
    RunThisOne(OneOpOneThread, "OneOpOneThread", test_name);
    RunThisOne(ManyOpOneThread, "ManyOpOneThread", test_name);
    RunThisOne(SomeOpsSomeThreads, "SomeOpsSomeThreads", test_name);
    RunThisOne(RealOneGroupIO, "RealOneGroupIO", test_name);
    return 0;
}  /* main */

/* multwait.c */
