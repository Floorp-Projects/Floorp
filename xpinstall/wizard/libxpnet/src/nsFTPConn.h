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

class nsSocket; 

typedef int (*FTPGetCB)(int aBytesRd, int aTotal);

class nsFTPConn
{
public:
    nsFTPConn(char *aHost);
    nsFTPConn(char *aHost, int (*aEventPumpCB)(void));
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
    int     ResumeOrGet(char *aSrvPath, char *aLoclPath, int aType, 
                int aOvWrite, FTPGetCB aCBFunc);
    int     Get(char *aSrvPath, char *aLoclPath, int aType, 
                int aOvWrite, FTPGetCB aCBFunc);
    int     Get(char *aSrvPath, char *aLoclPath, int aType, int aResumePos,
                int aOvWrite, FTPGetCB aCBFunc);
    int     Close();

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
        E_CMD_ERR           = -805, /* ftp command error */
        E_CMD_FAIL          = -806, /* ftp command failed */
        E_CMD_UNEXPECTED    = -807, /* ftp command unexpected response */
        E_WRITE             = -808, /* write to socket/fd failed */
        E_READ              = -809, /* read on socket/fd failed */
        E_SMALL_BUF         = -810, /* buffer too small, provide bigger one */
        E_CANT_OVWRITE      = -811, /* cannot overwrite existing file */
        E_LOCL_INIT         = -812, /* local file open/init failed */
        E_USER_CANCEL       = -813, /* user canceled the download */
        E_INVALID_ADDR      = -814  /* couldn't parse address/port */
    };

private:
    int         FlushCntlSock(nsSocket *aSock, int bailOnTimeOut = 1);
    int         IssueCmd(const char *aCmd, char *aResp, int aRespSize, 
                         nsSocket *aSock);
    int         ParseAddr(char *aBuf, char **aHost, int *aPort);
    int         DataInit(char *aHost, int aPort, nsSocket **aSock); 

    int         (*mEventPumpCB)(void);
    char        *mHost;
    int         mState;
    int         mPassive;
    nsSocket    *mCntlSock;
    nsSocket    *mDataSock;
};

#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifdef DUMP
#undef DUMP
#endif 

#if defined(DEBUG) || defined(DEBUG_sgehani)
#define DUMP(_msg) printf("%s %d: %s\n", __FILE__, __LINE__, _msg);
#else
#define DUMP(_msg)  
#endif /* DEBUG */

#ifndef ERR_CHECK
#define ERR_CHECK(_func)                                            \
do {                                                                \
    err = _func;                                                    \
    if (err != OK)                                                  \
        goto BAIL;                                                  \
} while(0);

#endif

#endif /* _NS_FTPCONN_H_ */

