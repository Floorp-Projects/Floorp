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

#ifndef _NS_FTPCONN_H_
#define _NS_FTPCONN_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netdb.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/param.h>
#include <arpa/inet.h>

#define CNTL_PORT 21
#define DATA_PORT 20

typedef int (*FTPGetCB)(int aBytesRd, int aTotal);

class nsFTPConn
{
public:
    nsFTPConn(char *aHost);
    ~nsFTPConn();

    /* ftp type */
    enum
    {
        ASCII = 0,
        BINARY
    };

    /* connection state */
    enum
    {
        OPEN = 0,
        GETTING,
        CLOSED
    };

    int     Open();
    int     Open(char *aHost);
    int     Get(char *aSrvPath, char *aLoclPath, int aType, int aOvWrite,
                FTPGetCB aCBFunc);
    int     Close();

    static float 
            CalcRate(struct timeval *aPre, struct timeval *aPost, int aBytes);

/*--------------------------------------------------------------------* 
 *  Errors 
 *--------------------------------------------------------------------*/
    enum
    {
        OK                  = 0,
        E_MEM               = -801, /* out of memory */
        E_PARAM             = -802, /* parameter null or incorrect */
        E_ALREADY_OPEN      = -803, /* connection already established */
        E_NOT_OPEN          = -804, /* connection not established, can't use */
        E_SOCK_OPEN         = -805, /* socket already open */
        E_SOCK_CLOSE        = -806, /* socket closed, can't use */
        E_CMD_ERR           = -807, /* ftp command error */
        E_CMD_FAIL          = -808, /* ftp command failed */
        E_CMD_UNEXPECTED    = -809, /* ftp command unexpected response */
        E_WRITE             = -810, /* write to socket/fd failed */
        E_READ              = -811, /* read on socket/fd failed */
        E_SMALL_BUF         = -812, /* buffer too small, provide bigger one */
        E_INVALID_HOST      = -813, /* failed to resolve hostname */
        E_CANT_OVWRITE      = -814, /* cannot overwrite existing file */
        E_BIND              = -815, /* bind() failed during cxn init */
        E_LISTEN            = -816, /* listen() failed during cxn init */
        E_ACCEPT            = -817, /* accept() failed */
        E_GETSOCKNAME       = -818, /* getsockname() failed */
        E_READ_MORE         =  819, /* more to read from this socket */
        E_LOCL_INIT         = -820, /* local file open/init failed */
        E_TIMEOUT           = -821, /* select() timed out */
        E_INVALID_ADDR      = -822  /* couldn't parse address/port */
    };

private:
    int         IssueCmd(char *aCmd, char *aResp, int aRespSize, int aFd);

/*--------------------------------------------------------------------* 
 *  "Raw" transport primitives
 *--------------------------------------------------------------------*/
    int         RawConnect(char *aHost, int aPort, int *aFd);   /* cli init */
    int         RawDataInit(char *aHost, int aPort, int *aFd);  /* srv init1 */
    int         RawDataConnect(int aDataFd, int *aConnFd);      /* srv init2 */
    int         RawClose(int aFd);
    int         RawSend(unsigned char *aBuf, int *aBufSize, int aFd);
    int         RawRecv(unsigned char *aBuf, int *aBufSize, int aFd);
    int         RawParseAddr(char *aBuf, char **aHost, int *aPort);

    char        *mHost;
    int         mState;
    int         mCntlFd;
    int         mDataFd;
    int         mEOFFound;
    int         mPassive;
};

#ifndef NULL
#define NULL (void*)0L
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef DUMP
#ifdef DEBUG
#define DUMP(_msg) printf("%s %d: %s\n", __FILE__, __LINE__, _msg);
#else
#define DUMP(_msg)  
#endif /* DEBUG */
#endif /* DUMP */

#ifndef ERR_CHECK
#define ERR_CHECK(_func)                                            \
do {                                                                \
    err = _func;                                                    \
    if (err != OK)                                                  \
        goto BAIL;                                                  \
} while(0);

#endif

#endif /* _NS_FTPCONN_H_ */

