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
#include <errno.h>
#include <ctype.h>
#include <time.h>

/* Platform-specific headers for socket functionality */
#if defined(__unix) || defined(__unix__) || defined(macintosh)
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#elif defined(_WINDOWS)
#define read(_socket, _buf, _len) \
        recv(_socket, (char *) _buf, _len, 0);
#define write(_socket, _buf, _len) \
        send(_socket, (char *) _buf, _len, 0);
#include <winsock2.h>
#endif

#include "nsSocket.h"

#define MAXSOCKADDR 128

#ifndef macintosh
#include "platform.h" // for SOLARIS define
#endif
#if defined(SOLARIS) || defined(_WINDOWS) || defined(IRIX)
#define socklen_t int
#endif

#ifndef SHUT_RD
#define SHUT_RD 0
#endif
#ifndef SHUT_WR
#define SHUT_WR 1
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

const int kUsecsPerSec = 1000000;
const int kRecvTimeoutThresholdUsecs = 30 * kUsecsPerSec;
const int kTimeoutThresholdUsecs = 120 * kUsecsPerSec;
const int kTimeoutSelectUsecs = 100000;
const int kKilobyte = 1024;
const int kUsecsPerKBFactor = (kUsecsPerSec/kKilobyte);
const int kReadBufSize = 1024;

#ifdef _WINDOWS
static int sbWinSockInited = FALSE;
#endif

nsSocket::nsSocket(char *aHost, int aPort, int (*aEventPumpCB)(void)) :
    mEventPumpCB( aEventPumpCB ),
    mHost(aHost),
    mPort(aPort),
    mFd(-1),
    mListenFd(-1)
{
}

nsSocket::nsSocket(char *aHost, int aPort) :
    mEventPumpCB( NULL ),
    mHost(aHost),
    mPort(aPort),
    mFd(-1),
    mListenFd(-1)
{
}

nsSocket::~nsSocket()
{
    // don't release mHost cause we don't own it
}

int
nsSocket::Open()
{
#ifdef _WINDOWS
    /* funky windows initialization of winsock */

    int err;
    WSADATA wsaData;
    WORD wVersionRequested;
    
    /* We don't care which version we get because we're not
     * doing any specific to a particular winsock version. */
    /* Request for version 2.2 */
    wVersionRequested = MAKEWORD(2, 2);
    err = WSAStartup(wVersionRequested, &wsaData);
    if (err == WSAVERNOTSUPPORTED)
    {
        /* Request for version 1.1 */
        wVersionRequested = MAKEWORD(1, 1);
        err = WSAStartup(wVersionRequested, &wsaData);
        if (err == WSAVERNOTSUPPORTED)
        {
            /* Request for version 1.0 */
            wVersionRequested = MAKEWORD(0, 1);
            err = WSAStartup(wVersionRequested, &wsaData);
            if (err == WSAVERNOTSUPPORTED)
            {
                /* Request for version 0.4 */
                wVersionRequested = MAKEWORD(4, 0);
                err = WSAStartup(wVersionRequested, &wsaData);
            }
        }
    }

    if (err != 0)
    {
        return E_WINSOCK;
    }
#endif

    int rv = OK;
    struct sockaddr_in servaddr;
    struct hostent *hptr = NULL;

    mFd = socket(AF_INET, SOCK_STREAM, 0);
#ifdef _WINDOWS
	if (mFd == INVALID_SOCKET) {
			printf("Last error: %d\n", WSAGetLastError());
    }
    if ( mFd != INVALID_SOCKET ) {
#else
    if ( mFd != -1 ) {
#endif

        int windowSize = 32768;   // we could tune this, but for now...
        socklen_t newTCPWin, len;
       
        len = sizeof( newTCPWin );
 
        setsockopt( mFd, SOL_SOCKET, SO_RCVBUF, (char*) &windowSize, sizeof( windowSize ));
#ifdef DEBUG
        getsockopt( mFd, SOL_SOCKET, SO_RCVBUF, (char*) &newTCPWin, &len );
#endif
 
        setsockopt( mFd, SOL_SOCKET, SO_SNDBUF, (char*) &windowSize, sizeof( windowSize ));
#ifdef DEBUG
        getsockopt( mFd, SOL_SOCKET, SO_RCVBUF, (char*) &newTCPWin, &len );
#endif

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(mPort);

        if ((hptr = gethostbyname(mHost)) == NULL )
        {
            if (IsIPAddress(mHost) == OK)
            {
                unsigned long netAddr;

                netAddr = inet_addr(mHost);
                if ((hptr = gethostbyaddr((const char *)&netAddr, sizeof(unsigned long), AF_INET)) == NULL )
                    return E_INVALID_HOST;
            }
            else
            {
                return E_INVALID_HOST;
            }
        }

        memcpy(&servaddr.sin_addr, (struct in_addr **) hptr->h_addr_list[0], sizeof(struct in_addr));

        rv = connect(mFd, (struct sockaddr *) &servaddr, sizeof(servaddr));
        if (rv < 0)
        {
#if defined(DEBUG) && (defined(__unix) || defined(__unix__))
            printf("ETIMEDOUT: %d\n", ETIMEDOUT);
            printf("ECONNREFUSED: %d\n", ECONNREFUSED);
            printf("EHOSTUNREACH: %d\n", EHOSTUNREACH);
            printf("ENETUNREACH: %d\n", ENETUNREACH);

            printf("connect error: %d\n", errno);
#endif /* DEBUG && (__unix || __unix__) */
            mFd = -1;
            rv = E_SOCK_OPEN;
        }
        else
            rv = OK;
    } else
        rv = E_SOCK_OPEN;

    return rv;
}

int
nsSocket::SrvOpen()
{
    int rv = OK;
    struct sockaddr_in servaddr;

    /* init data socket making it listen */
    mListenFd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); /* wildcard */
    servaddr.sin_port = 0; /* let kernel bind an ephemeral port */
    
    if ((bind(mListenFd, (struct sockaddr *) &servaddr, sizeof(servaddr))) != 0)
        return E_BIND;

    if ((listen(mListenFd, SOMAXCONN)) != 0)
        return E_LISTEN;
    return rv;
}

int 
nsSocket::SrvAccept()
{
    int rv = OK;
    struct sockaddr cliaddr;
    socklen_t clilen;

    if (mListenFd < 0)
        return E_PARAM;

    clilen = sizeof(cliaddr);
    mFd = accept(mListenFd, (struct sockaddr *) &cliaddr, &clilen);
    if (mFd < 0)
        rv = E_ACCEPT;

    return rv;
}

int 
nsSocket::Send(unsigned char *aBuf, int *aBufSize)
{
    int rv = OK;
    struct timeval seltime;
    int timeout = 0;
    fd_set selset;

    if (!aBuf || (aBufSize && (*aBufSize <= 0)) || mFd < 0)
        return E_PARAM;

    while (timeout < kTimeoutThresholdUsecs)
    {
        FD_ZERO(&selset);
        FD_SET(mFd, &selset);
        seltime.tv_sec = 0;
        seltime.tv_usec = kTimeoutSelectUsecs;

        if ( mEventPumpCB != NULL )
            if (mEventPumpCB() == E_USER_CANCEL)
                return E_USER_CANCEL;

        rv = select(mFd+1, NULL, &selset, NULL, &seltime);
        switch (rv)
        {
            case -1:            /* error occured! */
                return errno;
            case 0:             /* timeout; retry */
                timeout += kTimeoutSelectUsecs;
                continue;
            default:            /* ready to write */
                timeout = 0;    /* reset the time out counter */
                break;
        }

        if (!FD_ISSET(mFd, &selset))
        {
            timeout += kTimeoutSelectUsecs;
            continue;           /* not ready to write; retry */
        }
        else
            break;
    }
    if (rv == 0)
        return E_TIMEOUT;

    rv = write(mFd, aBuf, *aBufSize);
    if (rv <= 0)
        rv = E_WRITE;
    else
    {
        *aBufSize = rv;
        rv = OK;
    }

    return rv;
}

int
nsSocket::Recv(unsigned char *aBuf, int *aBufSize)
{
  return(Recv(aBuf, aBufSize, kRecvTimeoutThresholdUsecs));
}

int
nsSocket::Recv(unsigned char *aBuf, int *aBufSize, int aTimeoutThresholdUsecs)
{
    int  rv = OK;
    unsigned char lbuf[kReadBufSize]; /* function local buffer */
    int bytesrd = 0;
    struct timeval seltime;
    fd_set selset;
    int bufsize;
    int timeout;

    if (!aBuf || (aBufSize && (*aBufSize <= 0)) || mFd < 0)
        return E_PARAM;
    memset(aBuf, 0, *aBufSize);

    timeout = 0;
    while (timeout < aTimeoutThresholdUsecs)
    {
        /* return if we anticipate overflowing caller's buffer */
        if (bytesrd >= *aBufSize)
            return E_READ_MORE;

        memset(&lbuf, 0, kReadBufSize);

        FD_ZERO(&selset);
        FD_SET(mFd, &selset);
        seltime.tv_sec = 0;
        seltime.tv_usec = kTimeoutSelectUsecs;

        if ( mEventPumpCB != NULL )
            if (mEventPumpCB() == E_USER_CANCEL)
                return E_USER_CANCEL;

        rv = select(mFd+1, &selset, NULL, NULL, &seltime);
        switch (rv)
        {
            case -1:            /* error occured! */
                return errno;
            case 0:             /* timeout; retry */
                timeout += kTimeoutSelectUsecs;
                continue;
            default:            /* ready to read */
                timeout = 0;    /* reset the time out counter */
                break;
        }

        if (!FD_ISSET(mFd, &selset))
        {
            timeout += kTimeoutSelectUsecs;
            continue;           /* not ready to read; retry */
        }

        bufsize = *aBufSize - bytesrd;
        rv = read(mFd, lbuf, bufsize);
        if (rv == 0) /* EOF encountered */
        {
            rv = E_EOF_FOUND;
            break;
        }
        if (rv < 0)
        {
            rv = E_READ;
            break;
        }

        if (*aBufSize >= bytesrd + rv)
        {
            memcpy(aBuf + bytesrd, lbuf, rv);
            bytesrd += rv;
            if (rv <= bufsize)
            {
                FD_ZERO(&selset);
                FD_SET(mFd, &selset);
                seltime.tv_sec = 0;
                seltime.tv_usec = kTimeoutSelectUsecs;

                /* check if we still need to read from this socket */
                rv = select(mFd+1, &selset, NULL, NULL, &seltime);
                if (rv == 1)
                    rv = E_READ_MORE;
                else
                    rv = OK;
                break;
            }
        }
        else
        {
            rv = E_SMALL_BUF;
            break;
        }
    }
    if (timeout >= aTimeoutThresholdUsecs)
        return E_TIMEOUT;

    *aBufSize = bytesrd;
    return rv;
}

int
nsSocket::Close()
{
    int rv = OK, rv1 = OK, rv2 = OK;

/* funky windows shutdown of winsock */
#ifdef _WINDOWS
    closesocket(mFd);
    if (mListenFd > 0)
        closesocket(mListenFd);
    
    if (rv1 != 0 || rv2 != 0)
        rv = E_SOCK_CLOSE; 

    int wsaErr = WSACleanup();
    if (wsaErr != 0)
        rv = wsaErr;
#else /* unix or mac */
    rv1 = close(mFd);
    if (mListenFd > 0)
        rv2 = close(mListenFd);
    if (rv1 != 0 || rv2 != 0)
        rv = E_SOCK_CLOSE; 
#endif

    return rv;
}

int
nsSocket::GetHostPortString(char **aHostPort)
{
    int rv = OK;
    socklen_t salen;
    struct sockaddr_in servaddr;
    int hpsLen; // host-port string length

    if (!aHostPort)
        return E_PARAM;

    salen = MAXSOCKADDR;
    if ((getsockname(mListenFd, (struct sockaddr *) &servaddr, &salen)) < 0)
    {
        *aHostPort = NULL;
        return E_GETSOCKNAME;
    }

    hpsLen = strlen("AA1,AA2,AA3,AA4,PP1,PP2");
    *aHostPort = (char *) malloc(hpsLen);
    if (!*aHostPort)
       return E_MEM;
 
    memset(*aHostPort, 0, hpsLen);
    sprintf(*aHostPort, "%d,%d,%d,%d,%d,%d",
      (int)((char*)&servaddr.sin_addr)[0] & 0xFF,
      (int)((char*)&servaddr.sin_addr)[1] & 0xFF,
      (int)((char*)&servaddr.sin_addr)[2] & 0xFF,
      (int)((char*)&servaddr.sin_addr)[3] & 0xFF,
      (int)((char*)&servaddr.sin_port)[0] & 0xFF,
      (int)((char*)&servaddr.sin_port)[1] & 0xFF);

    return rv;    
}

int
nsSocket::IsIPAddress(char *aAddress)
{
    int addr[4];
    int numDots = 0;
    
    for (unsigned int i=0; i < strlen(aAddress); ++i)
    {
        if (isspace(aAddress[i]))
            return E_INVALID_ADDR;
        if (aAddress[i] == '.')
        {
            ++numDots;
            if (numDots > 3)
                return E_INVALID_ADDR;
        }
        else if (!isdigit(aAddress[i]))
            return E_INVALID_ADDR;
    }

    if (sscanf(aAddress, "%d.%d.%d.%d", 
        &addr[0], &addr[1], &addr[2], &addr[3]) != 4)
        return E_INVALID_ADDR;

    if ((addr[0] > 255) || 
        (addr[1] > 255) || 
        (addr[2] > 255) || 
        (addr[3] > 255))
        return E_INVALID_ADDR;

    return OK;
}

float
nsSocket::CalcRate(struct timeval *aPre, struct timeval *aPost, int aBytes)
{
    float diffUsecs, rate;

    /* param check */
    if (!aPre || !aPost || aBytes <= 0)
        return 0;
    
    diffUsecs = (float)(aPost->tv_sec - aPre->tv_sec) * kUsecsPerSec;
    diffUsecs += (float)aPost->tv_usec - (float)aPre->tv_usec;
    rate = ((float)aBytes)/((float)diffUsecs) * kUsecsPerKBFactor;

    return rate;
}

#ifdef TEST_NSSOCKET

void 
my_nprintf(char *buf, int len)
{
    printf("buf size = %d\n", len);
    for (int i = 0; i < len; ++i)
    {
        printf("%c", *(buf+i));
    }
    printf("\n");
}

const int kTestBufSize = 1024;

int
main(int argc, char **argv)
{
    DUMP(("*** %s: A self-test for nsSocket.\n", argv[0]));
    
    if (argc < 4)
    {
        fprintf(stderr, "usage: %s <host> <port> <http_url>\n", 
                argv[0]);
        exit(1);
    }

    int rv = nsSocket::OK;
    nsSocket *sock = new nsSocket(argv[1], atoi(argv[2]));
    char buf[kTestBufSize];
    int bufSize;
    memset(buf, 0, kTestBufSize);
    
    // open socket
    rv = sock->Open();
    DUMP(("nsSocket::Open returned: %d\n", rv));

    // prepare http request str
    sprintf(buf, "GET %s HTTP/1.1\r\n\r\n", argv[3]);
    bufSize = strlen(buf) + 1; // add 1 for NULL termination

    // make request
    rv = sock->Send((unsigned char *)buf, &bufSize);
    DUMP(("nsSocket::Send returned: %d\t and sent: %d bytes\n", rv, bufSize));
    
    // get response
    do {
        // prepare response buf
        memset(buf, 0, kTestBufSize);
        bufSize = kTestBufSize;

        rv = sock->Recv((unsigned char *)buf, &bufSize);

        DUMP(("nsSocket::Recv returned: %d\t and recd: %d bytes\n", 
               rv, bufSize));
        // DUMP(("%s\n", buf));
        my_nprintf(buf, bufSize);
    } while (rv == nsSocket::E_READ_MORE);

    // close socket
    rv = sock->Close();
    DUMP(("nsSocket::Close returned: %d\n", rv));

    return 0;
}
#endif /* TEST_NSSOCKET */
