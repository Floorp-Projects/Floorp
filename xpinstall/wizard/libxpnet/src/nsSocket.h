/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */

#ifndef _NS_SOCKET_H_
#define _NS_SOCKET_H_

#ifndef _WINDOWS
#include <sys/time.h>
#endif

class nsSocket
{
public:
    nsSocket(char *aHost, int aPort);
    nsSocket(char *aHost, int aPort, int (*aEventPumpCB)(void) );
    ~nsSocket();

//----------------------------------------------------------------------
//   Errors
//----------------------------------------------------------------------
    enum
    {
        OK              = 0,
        E_USER_CANCEL   = -813, /* user canceled the download */
        E_PARAM         = -1001,
        E_MEM           = -1002,
        E_INVALID_HOST  = -1003,
        E_SOCK_OPEN     = -1004,
        E_SOCK_CLOSE    = -1005,
        E_TIMEOUT       = -1006,
        E_WRITE         = -1007,
        E_READ_MORE     = -1008,
        E_READ          = -1009,
        E_SMALL_BUF     = -1010,
        E_EOF_FOUND     = -1011,
        E_BIND          = -1012,
        E_LISTEN        = -1014,
        E_ACCEPT        = -1015,
        E_GETSOCKNAME   = -1016,
        E_WINSOCK       = -1017,
        E_INVALID_ADDR  = -1018
    };

//----------------------------------------------------------------------
//   Public interface
//----------------------------------------------------------------------
    int Open();
    int SrvOpen(); // server alternate to client Open()
    int SrvAccept(); // must be called after SrvOpen()
    int Send(unsigned char *aBuf, int *aBufSize);
    int Recv(unsigned char *aBuf, int *aBufSize);
    int Recv(unsigned char *aBuf, int *aBufSize, int aTimeoutThresholdUsecs);
    int Close();

    int GetHostPortString(char **aHostPort); 

    static 
    float CalcRate(struct timeval *aPre, struct timeval *aPost, int aBytes);

private:
    int   (*mEventPumpCB)(void);
    char  *mHost;
    int   mPort;
    int   mFd; // connected socket
    int   mListenFd; // listening socket (only if SrvOpen() was called)

    int   IsIPAddress(char *aAddress);
};

//----------------------------------------------------------------------
//   Macro definitions
//----------------------------------------------------------------------
#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE 
#define FALSE 0
#endif

#ifndef NULL
#define NULL 0
#endif

#ifdef DUMP
#undef DUMP
#endif
#if defined(DEBUG) || defined(DEBUG_sgehani)
#define DUMP(_vargs)                       \
do {                                       \
    printf("%s %d: ", __FILE__, __LINE__); \
    printf _vargs;                         \
} while (0);
#else
#define DUMP(_vargs) 
#endif

#endif /* _NS_SOCKET_H_ */
