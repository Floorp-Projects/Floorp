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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#if defined(__unix) || defined(__unix__)
#include <sys/param.h>
#elif defined(_WINDOWS)
#include <windows.h>
#define MAXPATHLEN MAX_PATH
#endif

#include "nsSocket.h"
#include "nsFTPConn.h"

const int kCntlPort = 21;
const int kDataPort = 20;
const int kCmdBufSize = 64 + MAXPATHLEN;
const int kRespBufSize = 1024;
const int kKilobyte = 1024;
const int kUsecsPerSec = 1000000;
const int kDlBufSize = 1024;

nsFTPConn::nsFTPConn(char *aHost) :
    mHost(aHost),
    mState(CLOSED),
    mPassive(FALSE),
    mCntlSock(NULL),
    mDataSock(NULL)
{
}

nsFTPConn::~nsFTPConn()
{
    // don't release mHost cause we don't own it
}

int
nsFTPConn::Open()
{
    int err = OK;
    char cmd[kCmdBufSize], resp[kRespBufSize];
    int respBufSize = kRespBufSize;

    if (!mHost)
        return E_PARAM;
    if (mState != CLOSED)
        return E_ALREADY_OPEN;

    /* open control connection on port 21 */
    mCntlSock = new nsSocket(mHost, kCntlPort);
    if (!mCntlSock)
        return E_MEM;
    ERR_CHECK(mCntlSock->Open());
    ERR_CHECK(mCntlSock->Recv((unsigned char *)resp, &respBufSize));
    DUMP(resp);

    /* issue USER command on control connection */
    sprintf(cmd, "USER anonymous\r\n");
    err = IssueCmd(cmd, resp, kRespBufSize, mCntlSock);

    /* issue PASS command on control connection */
    sprintf(cmd, "PASS -linux@installer.sbg\r\n");
    ERR_CHECK(IssueCmd(cmd, resp, kRespBufSize, mCntlSock));

    mState = OPEN;

    return err;

BAIL:
    if (mCntlSock)
    {
        mCntlSock->Close();
        delete mCntlSock;
        mCntlSock = NULL;
    }
    if (mDataSock)
    {
        mDataSock->Close();
        delete mDataSock;
        mDataSock = NULL;
    }
    return err;
}

int
nsFTPConn::Open(char *aHost)
{
    if (!aHost)
        return E_PARAM;

    mHost = aHost;
    return Open();
}

int
nsFTPConn::Get(char *aSrvPath, char *aLoclPath, int aType, int aOvWrite,
               FTPGetCB aCBFunc)
{
    struct stat dummy;
    int err = OK, wrote = 0, totBytesRd = 0;
    char cmd[kCmdBufSize], resp[kRespBufSize];
    int fileSize = 0, respBufSize = kRespBufSize;
    FILE *loclfd = NULL;

    if (!aSrvPath || !aLoclPath)
        return E_PARAM;
    if (mState != OPEN || !mCntlSock)
        return E_NOT_OPEN;

    /* stat local path and verify aOvWrite is set if file already exists */
    err = stat(aLoclPath, &dummy);
    if (err != -1 && aOvWrite == FALSE)
        return E_CANT_OVWRITE;

    mState = GETTING;

    /* initialize data connection */
    ERR_CHECK(DataInit(mHost, kDataPort, &mDataSock));

    /* issue SIZE command on control connection */
    sprintf(cmd, "SIZE %s\r\n", aSrvPath);
    err = IssueCmd(cmd, resp, kRespBufSize, mCntlSock); /* non-fatal */
    if (err == OK && (resp[0] == '2'))
        fileSize = atoi(&resp[4]);

    /* issue TYPE command on control connection */
    sprintf(cmd, "TYPE %s\r\n", aType==BINARY ? "I" : "A");
    ERR_CHECK(IssueCmd(cmd, resp, kRespBufSize, mCntlSock));

    /* issue RETR command on control connection */
    sprintf(cmd, "RETR %s\r\n", aSrvPath);
    ERR_CHECK(IssueCmd(cmd, resp, kRespBufSize, mCntlSock));

    /* get file contents on data connection */
    if (!mPassive)
        ERR_CHECK(mDataSock->SrvAccept());

    /* initialize locl file */
    if (!(loclfd = fopen(aLoclPath, aType==BINARY ? "wb" : "w")) ||
        (fseek(loclfd, 0, SEEK_SET) != 0))
    {
        err = E_LOCL_INIT;
        goto BAIL;
    }

    totBytesRd = 0;
    do 
    {
        respBufSize = kDlBufSize;
        err = mDataSock->Recv((unsigned char *)resp, &respBufSize);
        if (err != nsSocket::E_READ_MORE && 
            err != nsSocket::E_EOF_FOUND &&
            err != nsSocket::OK)
            goto BAIL;
        totBytesRd += respBufSize;
        if (err == nsSocket::E_READ_MORE && aCBFunc)
            if ((err = aCBFunc(totBytesRd, fileSize)) == E_USER_CANCEL)
              goto BAIL;
            
        /* append to local file */
        wrote = fwrite((void *)resp, 1, respBufSize, loclfd);
        if (wrote != respBufSize)
        {   
            err = E_WRITE;
            goto BAIL;
        }
    }
    while (err == nsSocket::E_READ_MORE || err == nsSocket::OK);
    if (err == nsSocket::E_EOF_FOUND)
        err = OK;

BAIL:
    /* close locl file if open */
    if (loclfd)
        fclose(loclfd);

    /* kill data connection if it exists */
    if (mDataSock)
    {
        mDataSock->Close();
        delete mDataSock;
        mDataSock = NULL;
    }

    mState = OPEN;
    mPassive = FALSE;

    return err;
}

int 
nsFTPConn::Close()
{
    int err = OK;

    if (mState != OPEN)
        return E_NOT_OPEN;

    /* close sockets */
    if (mCntlSock)
    {
        ERR_CHECK(mCntlSock->Close());
        delete mCntlSock;
        mCntlSock = NULL;
    }
    if (mDataSock)
    {
        ERR_CHECK(mDataSock->Close());
        delete mDataSock;
        mDataSock = NULL;
    }

BAIL:
    return err;
}

int
nsFTPConn::IssueCmd(char *aCmd, char *aResp, int aRespSize, nsSocket *aSock)
{
    int err = OK;
    int len;

    /* param check */
    if (!aSock || !aCmd || !aResp || aRespSize <= 0)
        return E_PARAM;

    /* send command */
    len = strlen(aCmd);
    ERR_CHECK(aSock->Send((unsigned char *)aCmd, &len));
    DUMP(aCmd);

    /* receive response */
    do
    {
        err = aSock->Recv((unsigned char *)aResp, &aRespSize);
        if (err != nsSocket::OK && 
            err != nsSocket::E_READ_MORE && 
            err != nsSocket::E_EOF_FOUND)
            goto BAIL;
        DUMP(aResp);
    }
    while (err == nsSocket::E_READ_MORE);

    /* alternate interpretation of err codes */
    if ( (strncmp(aCmd, "APPE", 4) == 0) ||
         (strncmp(aCmd, "LIST", 4) == 0) ||
         (strncmp(aCmd, "NLST", 4) == 0) ||
         (strncmp(aCmd, "REIN", 4) == 0) ||
         (strncmp(aCmd, "RETR", 4) == 0) ||
         (strncmp(aCmd, "STOR", 4) == 0) ||
         (strncmp(aCmd, "STOU", 4) == 0) )
    {
        switch (*aResp)
        {
            case '1':   /* exception: 100 series is OK */
            case '2':
                break;
            case '3':
                err = E_CMD_ERR;
                break;
            case '4':
            case '5':
                err = E_CMD_FAIL;
                break;
            default:
                err = E_CMD_UNEXPECTED;
                break;
        }
    }

    /* regular interpretation of err codes */
    else
    {
        switch (*aResp)
        {
            case '2':
                break;
            case '1':
            case '3':
                err = E_CMD_ERR;
                break;
            case '4':
            case '5':
                err = E_CMD_FAIL;
                break;
            default:
                err = E_CMD_UNEXPECTED;
                break;
        }
    }

BAIL:
    return err;
}

int
nsFTPConn::ParseAddr(char *aBuf, char **aHost, int *aPort)
{
    int err = OK;
    char *c;
    int addr[6];

    /* param check */
    if (!aBuf || !aHost || !aPort)
        return E_PARAM;

    c = aBuf + strlen("227 "); /* pass by return code */
    while (!isdigit((int)(*c)))
    {
        if (*c == '\0')
            return E_INVALID_ADDR;
        c++;
    }

    if (sscanf(c, "%d,%d,%d,%d,%d,%d", 
        &addr[0], &addr[1], &addr[2], &addr[3], &addr[4], &addr[5]) != 6)
        return E_INVALID_ADDR;

    *aHost = (char *)malloc(strlen("XXX.XXX.XXX.XXX"));
    sprintf(*aHost, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);

    *aPort = ((addr[4] & 0xFF) << 8) | (addr[5] & 0xFF);

#ifdef DEBUG
    printf("%s %d: PASV response: %d,%d,%d,%d,%d,%d\n", __FILE__, __LINE__,
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
    printf("%s %d: Host = %s\tPort = %d\n", __FILE__, __LINE__, *aHost, *aPort);
#endif
    
    return err;
}

int
nsFTPConn::DataInit(char *aHost, int aPort, nsSocket **aSock)
{
    int err = OK;
    char cmd[kCmdBufSize], resp[kRespBufSize];
    char *srvHost = NULL; 
    int srvPort = 0;
    char *hostPort = NULL;

    /* param check */
    if (!aHost || !aSock)
        return E_PARAM;

    /* issue PASV command */
    sprintf(cmd, "PASV\r\n");
    err = IssueCmd(cmd, resp, kRespBufSize, mCntlSock);
    if (err != OK)
    {
        err = OK;
        goto ACTIVE; /* failover to active mode */
    }
       
    mPassive = TRUE;

    ERR_CHECK(ParseAddr(resp, &srvHost, &srvPort));
    *aSock = new nsSocket(srvHost, srvPort);
    if (!*aSock)
    {
        err = E_MEM;
        goto BAIL;
    }
    
    ERR_CHECK((*aSock)->Open());

    if (srvHost) 
    {
        free(srvHost);
        srvHost = NULL;
    }

    return err;

ACTIVE:

    *aSock = new nsSocket(aHost, aPort);
    if (!*aSock)
    {
        err = E_MEM;
        goto BAIL;
    }

    /* init data socket making it listen */
    ERR_CHECK((*aSock)->SrvOpen());

    ERR_CHECK((*aSock)->GetHostPortString(&hostPort)); // allocates
    if (!hostPort)
    {
        err = E_MEM;
        goto BAIL;
    }

    sprintf(cmd, "PORT %s\r\n", hostPort);
    ERR_CHECK(IssueCmd(cmd, resp, kRespBufSize, mCntlSock));

BAIL:
    if (mPassive && err != OK)
        mPassive = FALSE;

    if (err != OK && (*aSock))
    {
        delete *aSock;
        *aSock = NULL;
    }

    if (srvHost)
    {
        free(srvHost);
        srvHost = NULL;
    }

    if (hostPort)
    {
        free(hostPort);
        hostPort = NULL;
    }

    return err;
}

#ifdef TEST_NSFTPCONN
static struct timeval init;
int
TestFTPGetCB(int aBytesRd, int aTotal)
{
    struct timeval now;
    float rate;

    gettimeofday(&now, NULL);
    rate = nsSocket::CalcRate(&init, &now, aBytesRd);
    printf("br=%d\ttot=%d\trt=%f\tirt=%d\n",aBytesRd, aTotal, rate, (int)rate);
    return 0;
}

int
main(int argc, char **argv)
{
    int err = nsFTPConn::OK;
    nsFTPConn *conn = 0;
    char *leaf = NULL;
    
    if (argc < 2)
    {
        printf("usage: %s <host> <path/on/server>\n", argv[0]);
        exit(0);
    }

    if ((leaf = strrchr(argv[2], '/'))) leaf++;
    else leaf = argv[2];

    conn = new nsFTPConn(argv[1]);

    printf("Opening connection to %s...\n", argv[1]);
    err = conn->Open();
    if (err != nsFTPConn::OK) { printf("error: %d\n", err); exit(err); }

    printf("Getting binary file %s...\n", argv[2]);
    gettimeofday(&init, NULL);
    err = conn->Get(argv[2], leaf, nsFTPConn::BINARY, TRUE, TestFTPGetCB);
    if (err != nsFTPConn::OK) { printf("error: %d\n", err); exit(err); }

    printf("Closing connection to %s...\n", argv[1]);
    err = conn->Close();
    if (err != nsFTPConn::OK) { printf("error: %d\n", err); exit(err); }

    printf("Test successful!\n");
    exit(err);
}
#endif /* TEST_NSFTPCONN */

