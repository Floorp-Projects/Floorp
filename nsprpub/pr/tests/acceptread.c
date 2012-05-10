/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <prio.h>
#include <prprf.h>
#include <prinit.h>
#include <prnetdb.h>
#include <prinrval.h>
#include <prthread.h>

#include <plerror.h>

#include <stdlib.h>

#define DEFAULT_PORT 12273
#define GET "GET / HTTP/1.0\n\n"
static PRFileDesc *std_out, *err_out;
static PRIntervalTime write_dally, accept_timeout;

static PRStatus PrintAddress(const PRNetAddr* address)
{
    char buffer[100];
    PRStatus rv = PR_NetAddrToString(address, buffer, sizeof(buffer));
    if (PR_FAILURE == rv) PL_FPrintError(err_out, "PR_NetAddrToString");
    else PR_fprintf(
        std_out, "Accepted connection from (0x%p)%s:%d\n",
        address, buffer, address->inet.port);
    return rv;
}  /* PrintAddress */

static void ConnectingThread(void *arg)
{
    PRInt32 nbytes;
#ifdef SYMBIAN
    char buf[256];
#else
    char buf[1024];
#endif
    PRFileDesc *sock;
    PRNetAddr peer_addr, *addr;

    addr = (PRNetAddr*)arg;

    sock = PR_NewTCPSocket();
    if (sock == NULL)
    {
        PL_FPrintError(err_out, "PR_NewTCPSocket (client) failed");
        PR_ProcessExit(1);
    }

    if (PR_Connect(sock, addr, PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE)
    {
        PL_FPrintError(err_out, "PR_Connect (client) failed");
        PR_ProcessExit(1);
    }
    if (PR_GetPeerName(sock, &peer_addr) == PR_FAILURE)
    {
        PL_FPrintError(err_out, "PR_GetPeerName (client) failed");
        PR_ProcessExit(1);
    }

    /*
    ** Then wait between the connection coming up and sending the expected
    ** data. At some point in time, the server should fail due to a timeou
    ** on the AcceptRead() operation, which according to the document is
    ** only due to the read() portion.
    */
    PR_Sleep(write_dally);

    nbytes = PR_Send(sock, GET, sizeof(GET), 0, PR_INTERVAL_NO_TIMEOUT);
    if (nbytes == -1) PL_FPrintError(err_out, "PR_Send (client) failed");

    nbytes = PR_Recv(sock, buf, sizeof(buf), 0, PR_INTERVAL_NO_TIMEOUT);
    if (nbytes == -1) PL_FPrintError(err_out, "PR_Recv (client) failed");
    else
    {
        PR_fprintf(std_out, "PR_Recv (client) succeeded: %d bytes\n", nbytes);
        buf[sizeof(buf) - 1] = '\0';
        PR_fprintf(std_out, "%s\n", buf);
    }

    if (PR_FAILURE == PR_Shutdown(sock, PR_SHUTDOWN_BOTH))
        PL_FPrintError(err_out, "PR_Shutdown (client) failed");

    if (PR_FAILURE == PR_Close(sock))
        PL_FPrintError(err_out, "PR_Close (client) failed");

    return;
}  /* ConnectingThread */

#define BUF_SIZE 117
static void AcceptingThread(void *arg)
{
    PRStatus rv;
    PRInt32 bytes;
    PRSize buf_size = BUF_SIZE;
    PRUint8 buf[BUF_SIZE + (2 * sizeof(PRNetAddr)) + 32];
    PRNetAddr *accept_addr, *listen_addr = (PRNetAddr*)arg;
    PRFileDesc *accept_sock, *listen_sock = PR_NewTCPSocket();
    PRSocketOptionData sock_opt;

    if (NULL == listen_sock)
    {
        PL_FPrintError(err_out, "PR_NewTCPSocket (server) failed");
        PR_ProcessExit(1);        
    }
    sock_opt.option = PR_SockOpt_Reuseaddr;
    sock_opt.value.reuse_addr = PR_TRUE;
    rv = PR_SetSocketOption(listen_sock, &sock_opt);
    if (PR_FAILURE == rv)
    {
        PL_FPrintError(err_out, "PR_SetSocketOption (server) failed");
        PR_ProcessExit(1);        
    }
    rv = PR_Bind(listen_sock, listen_addr);
    if (PR_FAILURE == rv)
    {
        PL_FPrintError(err_out, "PR_Bind (server) failed");
        PR_ProcessExit(1);        
    }
    rv = PR_Listen(listen_sock, 10);
    if (PR_FAILURE == rv)
    {
        PL_FPrintError(err_out, "PR_Listen (server) failed");
        PR_ProcessExit(1);        
    }
    bytes = PR_AcceptRead(
        listen_sock, &accept_sock, &accept_addr, buf, buf_size, accept_timeout);

    if (-1 == bytes) PL_FPrintError(err_out, "PR_AcceptRead (server) failed");
    else
    {
        PrintAddress(accept_addr);
        PR_fprintf(
            std_out, "(Server) read [0x%p..0x%p) %s\n",
            buf, &buf[BUF_SIZE], buf);
        bytes = PR_Write(accept_sock, buf, bytes);
        rv = PR_Shutdown(accept_sock, PR_SHUTDOWN_BOTH);
        if (PR_FAILURE == rv)
            PL_FPrintError(err_out, "PR_Shutdown (server) failed");
    }

    if (-1 != bytes)
    {
        rv = PR_Close(accept_sock);
        if (PR_FAILURE == rv)
            PL_FPrintError(err_out, "PR_Close (server) failed");
    }

    rv = PR_Close(listen_sock);
    if (PR_FAILURE == rv)
        PL_FPrintError(err_out, "PR_Close (server) failed");
}  /* AcceptingThread */

int main(int argc, char **argv)
{
    PRHostEnt he;
    PRStatus status;
    PRIntn next_index;
    PRUint16 port_number;
    char netdb_buf[PR_NETDB_BUF_SIZE];
    PRNetAddr client_addr, server_addr;
    PRThread *client_thread, *server_thread;
    PRIntervalTime delta = PR_MillisecondsToInterval(500);

    err_out = PR_STDERR;
    std_out = PR_STDOUT;
    accept_timeout = PR_SecondsToInterval(2);

    if (argc != 2 && argc != 3) port_number = DEFAULT_PORT;
    else port_number = (PRUint16)atoi(argv[(argc == 2) ? 1 : 2]);

    status = PR_InitializeNetAddr(PR_IpAddrAny, port_number, &server_addr);
    if (PR_SUCCESS != status)
    {
        PL_FPrintError(err_out, "PR_InitializeNetAddr failed");
        PR_ProcessExit(1);
    }
    if (argc < 3)
    {
        status = PR_InitializeNetAddr(
            PR_IpAddrLoopback, port_number, &client_addr);
        if (PR_SUCCESS != status)
        {
            PL_FPrintError(err_out, "PR_InitializeNetAddr failed");
            PR_ProcessExit(1);
        }
    }
    else
    {
        status = PR_GetHostByName(
            argv[1], netdb_buf, sizeof(netdb_buf), &he);
        if (status == PR_FAILURE)
        {
            PL_FPrintError(err_out, "PR_GetHostByName failed");
            PR_ProcessExit(1);
        }
        next_index = PR_EnumerateHostEnt(0, &he, port_number, &client_addr);
        if (next_index == -1)
        {
            PL_FPrintError(err_out, "PR_EnumerateHostEnt failed");
            PR_ProcessExit(1);
        }
    }

    for (
        write_dally = 0;
        write_dally < accept_timeout + (2 * delta);
        write_dally += delta)
    {
        PR_fprintf(
            std_out, "Testing w/ write_dally = %d msec\n",
            PR_IntervalToMilliseconds(write_dally));
        server_thread = PR_CreateThread(
            PR_USER_THREAD, AcceptingThread, &server_addr,
            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
        if (server_thread == NULL)
        {
            PL_FPrintError(err_out, "PR_CreateThread (server) failed");
            PR_ProcessExit(1);
        }

        PR_Sleep(delta);  /* let the server pot thicken */

        client_thread = PR_CreateThread(
            PR_USER_THREAD, ConnectingThread, &client_addr,
            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
        if (client_thread == NULL)
        {
            PL_FPrintError(err_out, "PR_CreateThread (client) failed");
            PR_ProcessExit(1);
        }

        if (PR_JoinThread(client_thread) == PR_FAILURE)
            PL_FPrintError(err_out, "PR_JoinThread (client) failed");

        if (PR_JoinThread(server_thread) == PR_FAILURE)
            PL_FPrintError(err_out, "PR_JoinThread (server) failed");
    }

    return 0;
}
