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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef XP_UNIX
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#endif

#include "prio.h"
#include "prerror.h"
#include "prthread.h"
#include "prinrval.h"
#include "plstr.h"

#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcMessage.h"
#include "ipcClient.h"
#include "ipcModuleReg.h"
#include "ipcModule.h"
#include "ipcd.h"

#ifdef IPC_USE_INET
#include "prnetdb.h"
#endif

//-----------------------------------------------------------------------------

// upper limit on the number of active connections
// XXX may want to make this more dynamic
#define MAX_CLIENTS 100

//
// the first element of this array is always zero; this is done so that the
// n'th element of |clients| corresponds to the n'th element of |poll_fds|.
//
static ipcClient clients[MAX_CLIENTS + 1];

static PRPollDesc poll_fds[MAX_CLIENTS + 1];
static int        poll_fd_count;

//-----------------------------------------------------------------------------

static int AddClient(PRFileDesc *fd)
{
    if (poll_fd_count == MAX_CLIENTS + 1) {
        LOG(("reached maximum client limit\n"));
        return -1;
    }

    poll_fds[poll_fd_count].fd = fd;
    poll_fds[poll_fd_count].in_flags = clients[poll_fd_count].Init();
    poll_fds[poll_fd_count].out_flags = 0;

    ++poll_fd_count;
    return 0;
}

static int RemoveClient(int client_index)
{
    PRPollDesc *pd = &poll_fds[client_index];

    PR_Close(pd->fd);

    clients[client_index].Finalize();

    //
    // keep the clients and poll_fds contiguous; move the last one into
    // the spot held by the one that is going away.
    //
    int to_idx = client_index;
    int from_idx = poll_fd_count - 1;
    if (from_idx != to_idx) {
        memcpy(&clients[to_idx], &clients[from_idx], sizeof(ipcClient));
        memcpy(&poll_fds[to_idx], &poll_fds[from_idx], sizeof(PRPollDesc));
    }

    //
    // zero out the old entries.
    //
    memset(&clients[from_idx], 0, sizeof(ipcClient));
    memset(&poll_fds[from_idx], 0, sizeof(PRPollDesc));

    --poll_fd_count;
    return 0;
}

//-----------------------------------------------------------------------------

static void Process(PRFileDesc *listen_fd)
{
    poll_fd_count = 1;

    poll_fds[0].fd = listen_fd;
    poll_fds[0].in_flags = PR_POLL_EXCEPT | PR_POLL_READ;
    
    while (1) {
        PRInt32 rv;
        PRIntn i;

        poll_fds[0].out_flags = 0;

        //
        // poll
        //
        // timeout after 5 minutes.  if no connections after timeout, then
        // exit.  this timeout ensures that we don't stay resident when no
        // clients are interested in connecting after spawning the daemon.
        //
        rv = PR_Poll(poll_fds, poll_fd_count, PR_SecondsToInterval(60 * 5));
        if (rv == -1) {
            LOG(("PR_Poll failed [%d]\n", PR_GetError()));
            return;
        }

        if (rv > 0) {
            //
            // process clients that are ready
            //
            for (i = 1; i < poll_fd_count; ++i) {
                if (poll_fds[i].out_flags != 0) {
                    poll_fds[i].in_flags = clients[i].Process(poll_fds[i].fd, poll_fds[i].out_flags);
                    poll_fds[i].out_flags = 0;
                }
            }

            //
            // cleanup any dead clients (indicated by a zero in_flags)
            //
            for (i = poll_fd_count - 1; i >= 1; --i) {
                if (poll_fds[i].in_flags == 0)
                    RemoveClient(i);
            }

            //
            // check for new connection
            //
            if (poll_fds[0].out_flags & PR_POLL_READ) {
                LOG(("got new connection\n"));

                PRNetAddr client_addr;
                memset(&client_addr, 0, sizeof(client_addr));
                PRFileDesc *client_fd;

                client_fd = PR_Accept(listen_fd, &client_addr, PR_INTERVAL_NO_WAIT);
                if (client_fd == NULL) {
                    LOG(("PR_Accept failed [%d]\n", PR_GetError()));
                    return;
                }

                // make socket non-blocking
                PRSocketOptionData opt;
                opt.option = PR_SockOpt_Nonblocking;
                opt.value.non_blocking = PR_TRUE;
                PR_SetSocketOption(client_fd, &opt);

                if (AddClient(client_fd) != 0)
                    PR_Close(client_fd);
            }
        }

        //
        // shutdown if no clients
        //
        if (poll_fd_count == 1) {
            LOG(("shutting down\n"));
            break;
        }
    }
}

//-----------------------------------------------------------------------------

static void InitModuleReg(const char *exePath)
{
    static const char modDir[] = "ipc/modules";

    char *p = PL_strrchr(exePath, IPC_PATH_SEP_CHAR);
    if (p == NULL) {
        LOG(("unexpected exe path\n"));
        return;
    }

    int baseLen = p - exePath;
    int finalLen = baseLen + 1 + sizeof(modDir);

    // build full path to ipc modules
    char *buf = (char*) malloc(finalLen);
    memcpy(buf, exePath, baseLen);
    buf[baseLen] = IPC_PATH_SEP_CHAR;
    memcpy(buf + baseLen + 1, modDir, sizeof(modDir));

    IPC_InitModuleReg(buf);
    free(buf);
}

int main(int argc, char **argv)
{
    PRFileDesc *listen_fd;
    PRNetAddr addr;

#ifdef XP_UNIX
    signal(SIGINT, SIG_IGN);
    umask(0077); // ensure strict file permissions
#endif

#ifdef DEBUG
    IPC_InitLog("###");
#endif

start:
#ifdef IPC_USE_INET
    listen_fd = PR_OpenTCPSocket(PR_AF_INET);
    if (!listen_fd) {
        LOG(("PR_OpenUDPSocket failed [%d]\n", PR_GetError()));
        return -1;
    }

    PR_InitializeNetAddr(PR_IpAddrLoopback, IPC_PORT, &addr);

#else
    const char *socket_path;
    if (argc < 2)
        socket_path = IPC_DEFAULT_SOCKET_PATH;
    else
        socket_path = argv[1];

    listen_fd = PR_OpenTCPSocket(PR_AF_LOCAL);
    if (!listen_fd) {
        LOG(("PR_OpenUDPSocket failed [%d]\n", PR_GetError()));
        return -1;
    }

    addr.local.family = PR_AF_LOCAL;
    PL_strncpyz(addr.local.path, socket_path, sizeof(addr.local.path));
#endif

    if (PR_Bind(listen_fd, &addr) != PR_SUCCESS) {
        LOG(("PR_Bind failed [%d]\n", PR_GetError()));
        //
        // failure here indicates that another process may be bound
        // to the socket already.  let's try connecting to that socket.
        // if connect fails, then the socket is probably stale.
        //
        if (PR_Connect(listen_fd, &addr, PR_SecondsToInterval(2)) == PR_SUCCESS) {
            //
            // looks like another process is active.  silently exit.
            //
            LOG(("looks like another instance of the daemon is active; sleeping...\n"));
            //
            // sleep here to avoid triggering the shutdown procedure in the
            // other daemon.  10 seconds should be long enough for the client
            // to have established a connection.
            //
            PR_Sleep(PR_SecondsToInterval(10));
            LOG(("exiting\n"));
            PR_Close(listen_fd);
            return 0;
        }
        //
        // OK, the socket is probably stale.
        //
        LOG(("socket appears to be stale\n"));
#ifdef IPC_USE_INET
        LOG(("waiting for TIMEWAIT period to expire...\n"));
        PR_Sleep(PR_SecondsToInterval(60));
#else
        LOG(("deleting socket at %s\n", socket_path));
        PR_Delete(socket_path);
#endif
        PR_Close(listen_fd);
        goto start;
    }

    InitModuleReg(argv[0]);

    if (PR_Listen(listen_fd, 5) != PR_SUCCESS) {
        LOG(("PR_Listen failed [%d]\n", PR_GetError()));
        return -1;
    }

    Process(listen_fd);

    IPC_ShutdownModuleReg();

    //
    // XXX enable this delay for startup testing
    //
    //LOG(("sleeping for 5 seconds...\n"));
    //PR_Sleep(PR_SecondsToInterval(5));

#ifndef IPC_USE_INET
    LOG(("deleting socket at %s\n", socket_path));
    //
    // we delete the file itself first to avoid a race between shutting
    // ourselves down and another instance of the daemon starting up.
    //
    if (PR_Delete(socket_path) != PR_SUCCESS) {
        LOG(("PR_Delete failed [%d]\n", PR_GetError()));
        return -1;
    }
#endif

    LOG(("closing socket\n"));

    if (PR_Close(listen_fd) != PR_SUCCESS) {
        LOG(("PR_Close failed [%d]\n", PR_GetError()));
        return -1;
    }

    return 0;
}

//-----------------------------------------------------------------------------
// IPC API
//-----------------------------------------------------------------------------

int IPC_DispatchMsg(ipcClient *client, const ipcMessage *msg)
{
    // lookup handler for this message's topic and forward message to it.
    ipcModule *module = IPC_GetModuleByID(msg->Target());
    if (module)
        module->HandleMsg(client, msg);
    else
        LOG(("no registered module; ignoring message\n"));
    return 0;
}

int IPC_SendMsg(ipcClient *client, ipcMessage *msg)
{
    if (client == NULL) {
        int i;
        //
        // walk clients array 
        //
        for (i = 1; i < poll_fd_count - 1; ++i)
            IPC_SendMsg(&clients[i], msg->Clone());

        // send to last client w/o cloning to avoid extra malloc
        IPC_SendMsg(&clients[i], msg);
    }
    else {
        if (client->EnqueueOutboundMsg(msg)) {
            //
            // the message was successfully enqueued...
            //
            // since this client's Process method may have already been 
            // called, we need to explicitly set the PR_POLL_WRITE flag.
            //
            int client_index = client - clients;
            poll_fds[client_index].in_flags |= PR_POLL_WRITE;
        }
    }
    return 0;
}

ipcClient *IPC_GetClientByID(int clientID)
{
    // linear search OK since number of clients should be small
    for (int i = 1; i < poll_fd_count; ++i) {
        if (clients[i].ID() == clientID)
            return &clients[i];
    }
    return NULL;
}

ipcClient *IPC_GetClientByName(const char *name)
{
    // linear search OK since number of clients should be small
    for (int i = 1; i < poll_fd_count; ++i) {
        if (clients[i].HasName(name))
            return &clients[i];
    }
    return NULL;
}

ipcClient *IPC_GetClients(int *count)
{
    *count = poll_fd_count - 1;
    return &clients[1];
}
