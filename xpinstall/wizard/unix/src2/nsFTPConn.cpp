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

#include "nsFTPConn.h"
#include "platform.h" // XXX hack to get SOLARIS define
                      // XXX TODO: replace with autoconf rule

#define MAXSOCKADDR 128

#ifdef SOLARIS
#define socklen_t int
#endif

nsFTPConn::nsFTPConn(char *aHost) :
    mHost(aHost),
    mState(CLOSED),
    mCntlFd(-1),
    mDataFd(-1),
    mEOFFound(FALSE)
{
}

nsFTPConn::~nsFTPConn()
{
    // don't release mHost cause we don't own it
}

#define CMD_BUF_SIZE 64 + MAXPATHLEN
#define RESP_BUF_SIZE 1024

int
nsFTPConn::Open()
{
    int err = OK;
    char cmd[CMD_BUF_SIZE], resp[RESP_BUF_SIZE];
    int respBufSize = RESP_BUF_SIZE;

    if (!mHost)
        return E_PARAM;
    if (mState != CLOSED)
        return E_ALREADY_OPEN;

    /* open control connection on port 21 */
    ERR_CHECK(RawConnect(mHost, CNTL_PORT, &mCntlFd));
    ERR_CHECK(RawRecv((unsigned char *)resp, &respBufSize, mCntlFd));
    DUMP(resp);

    /* issue USER command on control connection */
    sprintf(cmd, "USER anonymous\r\n");
    err = IssueCmd(cmd, resp, RESP_BUF_SIZE, mCntlFd);

    /* issue PASS command on control connection */
    sprintf(cmd, "PASS linux@installer.sbg\r\n");
    ERR_CHECK(IssueCmd(cmd, resp, RESP_BUF_SIZE, mCntlFd));

    mState = OPEN;

    return err;

BAIL:
    if (mCntlFd > 0)
        RawClose(mCntlFd);
    if (mDataFd > 0)
        RawClose(mDataFd);
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

#define DL_BUF_SIZE 1024
int
nsFTPConn::Get(char *aSrvPath, char *aLoclPath, int aType, int aOvWrite,
               FTPGetCB aCBFunc)
{
    struct stat dummy;
    int err = OK, connfd = 0, wrote = 0, totBytesRd = 0;
    char cmd[CMD_BUF_SIZE], resp[RESP_BUF_SIZE];
    int fileSize = 0, respBufSize = RESP_BUF_SIZE;
    socklen_t clilen;
    struct sockaddr cliaddr;
    FILE *loclfd = NULL;

    if (!aSrvPath || !aLoclPath)
        return E_PARAM;
    if (mState != OPEN)
        return E_NOT_OPEN;

    /* stat local path and verify aOvWrite is set if file already exists */
    err = stat(aLoclPath, &dummy);
    if (err != -1 && aOvWrite == FALSE)
        return E_CANT_OVWRITE;

    mState = GETTING;

    // XXX rename to RawDataInit()
    /* initialize data connection */
    ERR_CHECK(RawListen(mHost, DATA_PORT, &mDataFd));

    /* issue SIZE command on control connection */
    sprintf(cmd, "SIZE %s\r\n", aSrvPath);
    err = IssueCmd(cmd, resp, RESP_BUF_SIZE, mCntlFd); /* non-fatal */
    if (err == OK && (resp[0] == '2'))
        fileSize = atoi(&resp[4]);

    /* issue TYPE command on control connection */
    sprintf(cmd, "TYPE %s\r\n", aType==BINARY ? "I" : "A");
    ERR_CHECK(IssueCmd(cmd, resp, RESP_BUF_SIZE, mCntlFd));

    /* issue RETR command on control connection */
    sprintf(cmd, "RETR %s\r\n", aSrvPath);
    ERR_CHECK(IssueCmd(cmd, resp, RESP_BUF_SIZE, mCntlFd));

    // XXX move this to RawDataConnect()
    /* get file contents on data connection */
    clilen = sizeof(cliaddr);
    connfd = accept(mDataFd, (struct sockaddr *) &cliaddr, &clilen);
    if (connfd < 0)
    {
        err = E_ACCEPT;
        goto BAIL;
    }

    /* initialize locl file */
    if (!(loclfd = fopen(aLoclPath, aType==BINARY ? "wb" : "w")) ||
        (fseek(loclfd, 0, SEEK_SET) != 0))
    {
        err = E_LOCL_INIT;
        goto BAIL;
    }

    totBytesRd = 0;
    mEOFFound = FALSE;
    do 
    {
        respBufSize = DL_BUF_SIZE;
        err = RawRecv((unsigned char *)resp, &respBufSize, connfd);
        if (err != E_READ_MORE && err != OK)
            goto BAIL;
        if (!mEOFFound)
            err = E_READ_MORE;
        totBytesRd += respBufSize;
        if (err == E_READ_MORE && aCBFunc)
            aCBFunc(totBytesRd, fileSize);
            
        /* append to local file */
        wrote = fwrite((void *)resp, 1, respBufSize, loclfd);
        if (wrote != respBufSize)
        {   
            err = E_WRITE;
            goto BAIL;
        }
    }
    while (err == E_READ_MORE);

BAIL:
    /* close locl file if open */
    if (loclfd)
        fclose(loclfd);

    /* kill data connection if it exists */
    if (mDataFd > 0)
    {
        RawClose(mDataFd);
        mDataFd = -1;
    }
    
    mState = OPEN;

    return err;
}

int 
nsFTPConn::Close()
{
    int err = OK;
    char cmd[CMD_BUF_SIZE], resp[RESP_BUF_SIZE];

    if (mState != OPEN)
        return E_NOT_OPEN;

    sprintf(cmd, "QUIT\r\n");
    IssueCmd(cmd, resp, RESP_BUF_SIZE, mCntlFd);
    
    /* close sockets */
    if (mCntlFd > 0)
    {
        ERR_CHECK(RawClose(mCntlFd));
        mCntlFd = -1;
    }
    if (mDataFd > 0)
    {
        ERR_CHECK(RawClose(mDataFd));
        mDataFd = -1;
    }

BAIL:
    return err;
}

int
nsFTPConn::IssueCmd(char *aCmd, char *aResp, int aRespSize, int aFd)
{
    int err = OK;
    int len;

    DUMP("IssueCmd");

    /* send command */
    len = strlen(aCmd);
    ERR_CHECK(RawSend((unsigned char *)aCmd, &len, aFd));
    DUMP(aCmd);

    /* receive response */
    ERR_CHECK(RawRecv((unsigned char *)aResp, &aRespSize, aFd));
    DUMP(aResp);

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
nsFTPConn::RawConnect(char *aHost, int aPort, int *aFd)
{
    int err = OK;
    int sockfd;
    struct sockaddr_in servaddr;
    struct hostent *hptr = NULL;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(aPort);

    if ( (hptr = gethostbyname(aHost)) == NULL )
        return E_INVALID_HOST;
    
    memcpy(&servaddr.sin_addr, (struct in_addr **) hptr->h_addr_list[0],
           sizeof(struct in_addr));

    err = connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (err < 0)
    {
#ifdef DEBUG
        printf("ETIMEDOUT: %d\n", ETIMEDOUT);
        printf("ECONNREFUSED: %d\n", ECONNREFUSED);
        printf("EHOSTUNREACH: %d\n", EHOSTUNREACH);
        printf("ENETUNREACH: %d\n", ENETUNREACH);

        printf("connect error: %d\n", errno);
#endif
        return E_SOCK_OPEN;
    }

    *aFd = sockfd;
    return err;
}

int
nsFTPConn::RawListen(char *aHost, int aPort, int *aFd)
{
    int err = OK;
    struct sockaddr_in servaddr;
    socklen_t salen;
    int listenfd = 0;
    char cmd[CMD_BUF_SIZE], resp[RESP_BUF_SIZE];
    
    /* param check */
    if (!aHost || !aFd)
        return E_PARAM;

    // XXX TODO: handle PASV mode
    // XXX       * issue PASV command
    // XXX       * if passive command returns an error use active mode
    // XXX       * else connect to supplied port

    /* init data socket making it listen */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    salen = MAXSOCKADDR;
    if ((getsockname(mCntlFd, (struct sockaddr *) &servaddr, &salen)) < 0)
        return E_GETSOCKNAME;
    servaddr.sin_port = 0;
    
    if ((bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) != 0)
        return E_BIND;

    if ((listen(listenfd, SOMAXCONN)) != 0)
        return E_LISTEN;
     
    salen = MAXSOCKADDR;
    if ((getsockname(listenfd, (struct sockaddr *) &servaddr, &salen)) < 0)
        return E_GETSOCKNAME;

    sprintf(cmd, "PORT %d,%d,%d,%d,%d,%d\r\n",
      (int)((char*)&servaddr.sin_addr)[0] & 0xFF,
      (int)((char*)&servaddr.sin_addr)[1] & 0xFF,
      (int)((char*)&servaddr.sin_addr)[2] & 0xFF,
      (int)((char*)&servaddr.sin_addr)[3] & 0xFF,
      (int)((char*)&servaddr.sin_port)[0] & 0xFF,
      (int)((char*)&servaddr.sin_port)[1] & 0xFF);
    ERR_CHECK(IssueCmd(cmd, resp, RESP_BUF_SIZE, mCntlFd));

    *aFd = listenfd;

BAIL:
    return err;
}

int
nsFTPConn::RawClose(int aFd)
{
    int err = OK;

#if 0
    err = shutdown(aFd, SHUT_RDWR);
    if (err != 0) err = E_SOCK_CLOSE;
#endif

    return err;
}

#define USECS_PER_SEC 1000000
#define TIMEOUT_THRESHOLD_USECS 120 * USECS_PER_SEC
#define TIMEOUT_SELECT_USECS 100000
int
nsFTPConn::RawSend(unsigned char *aBuf, int *aBufSize, int aFd)
{
    int err = OK;
    struct timeval seltime;
    int timeout = 0; 
    fd_set selset;

    DUMP("RawSend");

    if (!aBuf || aBufSize <= 0)
        return E_PARAM;

    while (timeout < TIMEOUT_THRESHOLD_USECS)
    {
        FD_ZERO(&selset);
        FD_SET(aFd, &selset);
        seltime.tv_sec = 0;
        seltime.tv_usec = TIMEOUT_SELECT_USECS;

        err = select(aFd+1, NULL, &selset, NULL, &seltime);
        switch (err)
        {
            case -1:            /* error occured! */
                return errno;
            case 0:             /* timeout; retry */
                timeout += TIMEOUT_SELECT_USECS;
                continue;
            default:            /* ready to write */
                break;
        }

        if (!FD_ISSET(aFd, &selset))
        {
            timeout += TIMEOUT_SELECT_USECS;
            continue;           /* not ready to write; retry */
        }
        else
            break;
    }
    if (err == 0)
        return E_TIMEOUT;

    err = write(aFd, aBuf, *aBufSize);
    if (err <= 0)
        err = E_WRITE;
    else
    {
        *aBufSize = err;
        err = OK;
    }

    return err;
}

#define READ_BUF_SIZE 1024
int
nsFTPConn::RawRecv(unsigned char *aBuf, int *aBufSize, int aFd)
{
    int err = OK;
    unsigned char lbuf[READ_BUF_SIZE]; /* function local buffer */
    int bytesrd = 0;
    struct timeval seltime;
    fd_set selset;

    if (!aBuf || *aBufSize <= 0)
        return E_PARAM;
    memset(aBuf, 0, *aBufSize);

    for ( ; ; )
    {
        /* return if we anticipate overflowing caller's buffer */
        if (bytesrd + READ_BUF_SIZE > *aBufSize)
            return E_READ_MORE;

        memset(&lbuf, 0, READ_BUF_SIZE); 

        FD_ZERO(&selset);
        FD_SET(aFd, &selset);
        seltime.tv_sec = 0;
        seltime.tv_usec = TIMEOUT_SELECT_USECS;

        err = select(aFd+1, &selset, NULL, NULL, &seltime);
        switch (err)
        {
            case -1:            /* error occured! */
                return errno;
            case 0:             /* timeout; retry */
                continue;
            default:            /* ready to read */
                break;
        }
        
        // XXX TODO: prevent inf loop retruning at TIMEOUT_THREASHOLD_USECS
        if (!FD_ISSET(aFd, &selset))
            continue;           /* not ready to read; retry */
            
        err = read(aFd, lbuf, READ_BUF_SIZE);
        if (err == 0) /* EOF encountered */
        {
            err = OK;
            mEOFFound = TRUE;
            break;
        }
        if (err < 0)
        {
            err = E_READ;
            break;
        }

        if (*aBufSize >= bytesrd + err)
        {
            memcpy(aBuf + bytesrd, lbuf, err);
            bytesrd += err;
            if (err < READ_BUF_SIZE)
            {
                err = OK;
                break;
            }
        }
        else
        {
            err = E_SMALL_BUF;
            break;  
        }
    }
    *aBufSize = bytesrd;
    return err;
}

#define KILOBYTE 1024
float
nsFTPConn::CalcRate(struct timeval *aPre, struct timeval *aPost, int aBytes)
{
    float diff_usecs, rate;

    /* param check */
    if (!aPre || !aPost || aBytes <= 0)
        return 0;
    
    diff_usecs = (aPost->tv_sec - aPre->tv_sec) * USECS_PER_SEC;
    diff_usecs += (float)aPost->tv_usec - (float)aPre->tv_usec;
    rate = ((float)(aBytes/KILOBYTE))/((float)(diff_usecs/USECS_PER_SEC));

    return rate;
}

#ifdef TEST_NSFTPCONN
static struct timeval init;
int
TestFTPGetCB(int aBytesRd, int aTotal)
{
    struct timeval now;
    float rate;

    gettimeofday(&now, NULL);
    rate = nsFTPConn::CalcRate(&init, &now, aBytesRd);
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

    if (leaf = strrchr(argv[2], '/')) leaf++;
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

