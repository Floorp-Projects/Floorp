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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "prio.h"
#include "prerror.h"
#include "prthread.h"
#include "prinrval.h"
#include "plstr.h"
#include "prprf.h"

#include "ipcConfig.h"
#include "ipcLog.h"
#include "ipcMessage.h"
#include "ipcClient.h"
#include "ipcModuleReg.h"
#include "ipcdPrivate.h"
#include "ipcd.h"

//-----------------------------------------------------------------------------
// ipc directory and locking...
//-----------------------------------------------------------------------------

static char *ipcDir;
static char *ipcSockFile;
static char *ipcLockFile;
static int   ipcLockFD;

static PRBool AcquireDaemonLock()
{
    const char lockName[] = "lock";

    int dirLen = strlen(ipcDir);
    int len = dirLen            // ipcDir
            + 1                 // "/"
            + sizeof(lockName); // "lock"

    ipcLockFile = (char *) malloc(len);
    memcpy(ipcLockFile, ipcDir, dirLen);
    ipcLockFile[dirLen] = '/';
    memcpy(ipcLockFile + dirLen + 1, lockName, sizeof(lockName));

    ipcLockFD = open(ipcLockFile, O_WRONLY|O_CREAT|O_TRUNC, S_IWUSR|S_IRUSR);
    if (ipcLockFD == -1)
        return PR_FALSE;

    //
    // we use fcntl for locking.  assumption: filesystem should be local.
    // this API is nice because the lock will be automatically released
    // when the process dies.  it will also be released when the file
    // descriptor is closed.
    //
    struct flock lock;
    lock.l_type = F_WRLCK;
    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_whence = SEEK_SET;
    if (fcntl(ipcLockFD, F_SETLK, &lock) == -1)
        return PR_FALSE;

    //
    // write our PID into the lock file (this just seems like a good idea...
    // no real purpose otherwise).
    //
    char buf[256];
    int nb = PR_snprintf(buf, sizeof(buf), "%u\n", (unsigned long) getpid());
    write(ipcLockFD, buf, nb);

    return PR_TRUE;
}

static PRBool InitDaemonDir(const char *socketPath)
{
    LOG(("InitDaemonDir [sock=%s]\n", socketPath));

    ipcSockFile = PL_strdup(socketPath);
    ipcDir      = PL_strdup(socketPath);

    //
    // make sure IPC directory exists (XXX this should be recursive)
    //
    char *p = strrchr(ipcDir, '/');
    if (p)
        p[0] = '\0';
    mkdir(ipcDir, 0700);

    //
    // if we can't acquire the daemon lock, then another daemon
    // must be active, so bail.
    //
    if (!AcquireDaemonLock())
        return PR_FALSE;

    //
    // delete an existing socket to prevent bind from failing.
    //
    unlink(socketPath);

    return PR_TRUE;
}

static void ShutdownDaemonDir()
{
    LOG(("ShutdownDaemonDir [sock=%s]\n", ipcSockFile));

    //
    // unlink files from directory before giving up lock; otherwise, we'd be
    // unable to delete it w/o introducing a race condition.
    //
    unlink(ipcLockFile);
    unlink(ipcSockFile);

    //
    // should be able to remove the ipc directory now.
    //
    rmdir(ipcDir);

    // 
    // this removes the advisory lock, allowing other processes to acquire it.
    //
    close(ipcLockFD);

    //
    // free allocated memory
    //
    PL_strfree(ipcDir);
    PL_strfree(ipcSockFile);
    free(ipcLockFile);

    ipcDir = NULL;
    ipcSockFile = NULL;
    ipcLockFile = NULL;
}

//-----------------------------------------------------------------------------
// poll list
//-----------------------------------------------------------------------------

//
// declared in ipcdPrivate.h
//
ipcClient *ipcClients;
int        ipcClientCount;

//
// the first element of this array is always zero; this is done so that the
// n'th element of ipcClientArray corresponds to the n'th element of
// ipcPollList.
//
static ipcClient ipcClientArray[IPC_MAX_CLIENTS + 1];

//
// element 0 contains the "server socket"
//
static PRPollDesc ipcPollList[IPC_MAX_CLIENTS + 1];

//-----------------------------------------------------------------------------

static int AddClient(PRFileDesc *fd)
{
    if (ipcClientCount == IPC_MAX_CLIENTS) {
        LOG(("reached maximum client limit\n"));
        return -1;
    }

    int pollCount = ipcClientCount + 1;

    ipcClientArray[pollCount].Init();

    ipcPollList[pollCount].fd = fd;
    ipcPollList[pollCount].in_flags = PR_POLL_READ;
    ipcPollList[pollCount].out_flags = 0;

    ++ipcClientCount;
    return 0;
}

static int RemoveClient(int clientIndex)
{
    PRPollDesc *pd = &ipcPollList[clientIndex];

    PR_Close(pd->fd);

    ipcClientArray[clientIndex].Finalize();

    //
    // keep the clients and poll_fds contiguous; move the last one into
    // the spot held by the one that is going away.
    //
    int toIndex = clientIndex;
    int fromIndex = ipcClientCount;
    if (fromIndex != toIndex) {
        memcpy(&ipcClientArray[toIndex], &ipcClientArray[fromIndex], sizeof(ipcClient));
        memcpy(&ipcPollList[toIndex], &ipcPollList[fromIndex], sizeof(PRPollDesc));
    }

    //
    // zero out the old entries.
    //
    memset(&ipcClientArray[fromIndex], 0, sizeof(ipcClient));
    memset(&ipcPollList[fromIndex], 0, sizeof(PRPollDesc));

    --ipcClientCount;
    return 0;
}

//-----------------------------------------------------------------------------

static void PollLoop(PRFileDesc *listenFD)
{
    ipcClients = ipcClientArray + 1;
    ipcClientCount = 0;

    ipcPollList[0].fd = listenFD;
    ipcPollList[0].in_flags = PR_POLL_EXCEPT | PR_POLL_READ;
    
    while (1) {
        PRInt32 rv;
        PRIntn i;

        int pollCount = ipcClientCount + 1;

        ipcPollList[0].out_flags = 0;

        //
        // poll
        //
        // timeout after 5 minutes.  if no connections after timeout, then
        // exit.  this timeout ensures that we don't stay resident when no
        // clients are interested in connecting after spawning the daemon.
        //
        // XXX add #define for timeout value
        //
        rv = PR_Poll(ipcPollList, pollCount, PR_SecondsToInterval(60 * 5));
        if (rv == -1) {
            LOG(("PR_Poll failed [%d]\n", PR_GetError()));
            return;
        }

        if (rv > 0) {
            //
            // process clients that are ready
            //
            for (i = 1; i < pollCount; ++i) {
                if (ipcPollList[i].out_flags != 0) {
                    ipcPollList[i].in_flags =
                        ipcClientArray[i].Process(ipcPollList[i].fd,
                                                  ipcPollList[i].out_flags);
                    ipcPollList[i].out_flags = 0;
                }
            }

            //
            // cleanup any dead clients (indicated by a zero in_flags)
            //
            for (i = pollCount - 1; i >= 1; --i) {
                if (ipcPollList[i].in_flags == 0)
                    RemoveClient(i);
            }

            //
            // check for new connection
            //
            if (ipcPollList[0].out_flags & PR_POLL_READ) {
                LOG(("got new connection\n"));

                PRNetAddr clientAddr;
                memset(&clientAddr, 0, sizeof(clientAddr));
                PRFileDesc *clientFD;

                clientFD = PR_Accept(listenFD, &clientAddr, PR_INTERVAL_NO_WAIT);
                if (clientFD == NULL) {
                    LOG(("PR_Accept failed [%d]\n", PR_GetError()));
                    return;
                }

                // make socket non-blocking
                PRSocketOptionData opt;
                opt.option = PR_SockOpt_Nonblocking;
                opt.value.non_blocking = PR_TRUE;
                PR_SetSocketOption(clientFD, &opt);

                if (AddClient(clientFD) != 0)
                    PR_Close(clientFD);
            }
        }

        //
        // shutdown if no clients
        //
        if (ipcClientCount == 0) {
            LOG(("shutting down\n"));
            break;
        }
    }
}

//-----------------------------------------------------------------------------

PRStatus
IPC_PlatformSendMsg(ipcClient *client, const ipcMessage *msg)
{
    LOG(("IPC_PlatformSendMsg\n"));

    //
    // must copy message onto send queue.
    //
    client->EnqueueOutboundMsg(msg->Clone());

    //
    // since our Process method may have already been called, we must ensure
    // that the PR_POLL_WRITE flag is set.
    //
    int clientIndex = client - ipcClientArray;
    ipcPollList[clientIndex].in_flags |= PR_POLL_WRITE;

    return PR_SUCCESS;
}

//-----------------------------------------------------------------------------

int main(int argc, char **argv)
{
    PRFileDesc *listenFD;
    PRNetAddr addr;

    //
    // ignore SIGINT so <ctrl-c> from terminal only kills the client
    // which spawned this daemon.
    //
    signal(SIGINT, SIG_IGN);

    // ensure strict file permissions
    umask(0077);

    IPC_InitLog("###");

    LOG(("daemon started...\n"));

    //XXX uncomment these lines to test slow starting daemon
    //LOG(("sleeping for 2 seconds...\n"));
    //PR_Sleep(PR_SecondsToInterval(2));

    listenFD = PR_OpenTCPSocket(PR_AF_LOCAL);
    if (!listenFD) {
        LOG(("PR_OpenUDPSocket failed [%d]\n", PR_GetError()));
        return -1;
    }

    const char *socket_path;
    if (argc < 2)
        socket_path = IPC_DEFAULT_SOCKET_PATH;
    else
        socket_path = argv[1];

    if (!InitDaemonDir(socket_path)) {
        PR_Close(listenFD);
        return 0;
    }

    addr.local.family = PR_AF_LOCAL;
    PL_strncpyz(addr.local.path, socket_path, sizeof(addr.local.path));

    if (PR_Bind(listenFD, &addr) != PR_SUCCESS) {
        LOG(("PR_Bind failed [%d]\n", PR_GetError()));
        return -1;
    }

    IPC_InitModuleReg(argv[0]);

    if (PR_Listen(listenFD, 5) != PR_SUCCESS) {
        LOG(("PR_Listen failed [%d]\n", PR_GetError()));
        return -1;
    }

    PollLoop(listenFD);

    IPC_ShutdownModuleReg();

    ShutdownDaemonDir();

    LOG(("closing socket\n"));
    PR_Close(listenFD);
    return 0;
}
