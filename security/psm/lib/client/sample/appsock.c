/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#include "cmtcmn.h"
#include "appsock.h"

#ifdef XP_UNIX
#include <netinet/tcp.h>
#include <errno.h>
#endif

CMT_SocketFuncs socketFuncs = {
    APP_GetSocket,
    APP_Connect,
    APP_VerifyUnixSocket,
    APP_Send,
    APP_Select,
    APP_Receive,
    APP_Shutdown,
    APP_Close
};

CMTSocket APP_GetSocket(int unixSock)
{
    APPSocket *sock;
    int on = 1;

#ifndef XP_UNIX
    if (unixSock) {
        return NULL;
    }
#endif

    sock = malloc(sizeof(APPSocket));
    if (sock == NULL) {
        return NULL;
    }
    if (unixSock) {
        sock->sock = socket(AF_UNIX, SOCK_STREAM, 0);
    } else {
        sock->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    }
    if (sock->sock < 0) {
        free(sock);
        return NULL;
    }
    if (!unixSock && 
        setsockopt(sock->sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&on,
                   sizeof(on))) {
        free(sock);
        return NULL;
    }

    sock->isUnix = unixSock;
#ifdef XP_UNIX
    memset (&sock->servAddr, 0, sizeof(struct sockaddr_un));
#endif
    return (CMTSocket)sock;
}

CMTStatus APP_Connect(CMTSocket sock, short port, char *path)
{
    APPSocket *cmSock = (APPSocket*)sock;
    struct sockaddr_in iServAddr;
    const struct sockaddr *servAddr;
    size_t addrLen;
    int error;

    if (cmSock->isUnix){
#ifndef XP_UNIX
        return CMTFailure;
#else
        cmSock->servAddr.sun_family = AF_UNIX;
	memcpy(&cmSock->servAddr.sun_path, path, strlen(path)+1);
        servAddr = (const struct sockaddr*)&cmSock->servAddr;
        addrLen = sizeof(cmSock->servAddr);
#endif
    } else {
        iServAddr.sin_family = AF_INET;
        iServAddr.sin_port = htons(port);
        iServAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        servAddr = (const struct sockaddr*)&iServAddr;
        addrLen = sizeof(struct sockaddr_in);
    }
    while (connect(cmSock->sock, servAddr, addrLen) != 0) {
#ifdef WIN32
        error = WSAGetLastError();
        if (error == WSAEISCONN) {
            break;
        }
        if ((error != WSAEINPROGRESS) && (error != WSAEWOULDBLOCK) && 
            (error!= WSAEINVAL)) {
            goto loser;
        }
#else
        error = errno;
        if (error == EISCONN) {
            break;
        }
        if (error != EINPROGRESS) {
            goto loser;
        }
#endif
    }
    return CMTSuccess;
 loser:
    return CMTFailure;
}

CMTStatus APP_VerifyUnixSocket(CMTSocket sock)
{
#ifndef XP_UNIX
    return CMTFailure;
#else
    APPSocket *cmSock = (APPSocket*)sock;
    int rv;
    struct stat statbuf;

    if (!cmSock->isUnix) {
        return CMTFailure;
    }
    rv = stat(cmSock->servAddr.sun_path, &statbuf);
    if (rv < 0) {
        goto loser;
    }
    if (statbuf.st_uid != geteuid()) {
        goto loser;
    }
    return CMTSuccess;
 loser:
    close(cmSock->sock);
    free(cmSock);
    return CMTFailure;
#endif
}

size_t APP_Send(CMTSocket sock, void *buffer, size_t length)
{
    APPSocket *cmSock = (APPSocket*) sock;

    return send(cmSock->sock, buffer, length, 0);
}

CMTSocket APP_Select(CMTSocket *socks, int numsocks, int poll)
{
    APPSocket **sockArr = (APPSocket**)socks;
    SOCKET nsocks = 0;
    int i, rv;
    struct timeval timeout;
    fd_set readfds;

#ifdef WIN32
win_startover:
#endif
    FD_ZERO(&readfds);
    for (i=0; i<numsocks; i++) {
        FD_SET(sockArr[i]->sock, &readfds);
        if (sockArr[i]->sock > nsocks) {
            nsocks = sockArr[i]->sock;
        }
    }
    if (poll) {
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
    }
    rv = select(nsocks+1, &readfds, NULL, NULL, (poll) ? &timeout : NULL);

#ifdef WIN32
    /* XXX Win95/98 Bug (Q177346)
     *     select() with no timeout might return even if there is no data
     *     pending or no error has occurred.  To get around this problem,
     *     we loop if these erroneous conditions happen.
     */
    if (poll == 0 && rv == 0) {
        goto win_startover;
    }
#endif

    /* Figure out which socket was selected */
    if (rv == -1 || rv == 0) {
        goto loser;
    }
    for (i=0; i<numsocks; i++) {
        if (FD_ISSET(sockArr[i]->sock, &readfds)) {
            return (CMTSocket)sockArr[i];
        }
    }
 loser:
    return NULL;
}

size_t APP_Receive(CMTSocket sock, void *buffer, size_t bufSize)
{
    APPSocket *cmSock = (APPSocket*)sock;

    return recv(cmSock->sock, buffer, bufSize, 0);
}

CMTStatus APP_Shutdown(CMTSocket sock)
{
    APPSocket *cmSock = (APPSocket*)sock;
    int rv;

    rv = shutdown(cmSock->sock, 1);
    return (rv == 0) ? CMTSuccess : CMTFailure;
}

CMTStatus APP_Close(CMTSocket sock)
{
    APPSocket *cmSock = (APPSocket*)sock;
    int rv;

#ifdef XP_UNIX
    rv = close(cmSock->sock);
#else
    rv = closesocket(cmSock->sock);
#endif
    free(cmSock);
    return (rv == 0) ? CMTSuccess : CMTFailure;
}
