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
#ifndef _APPSOCK_H_
#define _APPSOCK_H_
#include "cmtcmn.h"

#ifdef XP_UNIX
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/un.h>
#include <sys/stat.h>

typedef int SOCKET;
#endif

typedef struct APPSocketStr {
    SOCKET sock;
    int    isUnix;
#ifdef XP_UNIX
    struct sockaddr_un servAddr;
#endif 
} APPSocket; 

extern CMT_SocketFuncs socketFuncs;

CMTStatus APP_Close(CMTSocket sock);
CMTStatus APP_Shutdown(CMTSocket sock);
size_t APP_Receive(CMTSocket sock, void *buffer, size_t bufSize);
CMTSocket APP_Select(CMTSocket *socks, int numsocks, int poll);
size_t APP_Send(CMTSocket sock, void *buffer, size_t length);
CMTStatus APP_VerifyUnixSocket(CMTSocket sock);
CMTStatus APP_Connect(CMTSocket sock, short port, char *path);
CMTSocket APP_GetSocket(int unixSock);


#endif /* _APPSOCK_H_ */
