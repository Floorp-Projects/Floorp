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

/* Platform-specific headers for socket functionality */
#ifdef __unix
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

#include "platform.h" // for SOLARIS define
#if defined(SOLARIS) || defined(_WINDOWS)
#define socklen_t int
#endif

#ifndef SHUT_RDWR
#define SHUT_RDWR 2
#endif

const int kUsecsPerSec = 1000000;
const int kTimeoutThresholdUsecs = 120 * kUsecsPerSec;
const int kTimeoutSelectUsecs = 100000;
const int kKilobyte = 1024;
const int kReadBufSize = 1024;

nsSocket::nsSocket(char *aHost, int aPort) :
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
    WORD wVersionRequested = MAKEWORD(4, 0);
    err = WSAStartup(wVersionRequested, &wsaData);
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
		if (mFd == INVALID_SOCKET)
		{
			printf("Last error: %d\n", WSAGetLastError());
		}
#endif

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(mPort);

    if ( (hptr = gethostbyname(mHost)) == NULL )
        return E_INVALID_HOST;

    memcpy(&servaddr.sin_addr, (struct in_addr **) hptr->h_addr_list[0],
           sizeof(struct in_addr));

    rv = connect(mFd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (rv < 0)
    {
#if defined(DEBUG) && defined(__unix)
        printf("ETIMEDOUT: %d\n", ETIMEDOUT);
        printf("ECONNREFUSED: %d\n", ECONNREFUSED);
        printf("EHOSTUNREACH: %d\n", EHOSTUNREACH);
        printf("ENETUNREACH: %d\n", ENETUNREACH);

        printf("connect error: %d\n", errno);
#endif /* DEBUG && __unix */
        mFd = -1;
        rv = E_SOCK_OPEN;
    }
    else
        rv = OK;

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

    if (!aBuf || aBufSize <= 0 || mFd < 0)
        return E_PARAM;

    while (timeout < kTimeoutThresholdUsecs)
    {
        FD_ZERO(&selset);
        FD_SET(mFd, &selset);
        seltime.tv_sec = 0;
        seltime.tv_usec = kTimeoutSelectUsecs;

        rv = select(mFd+1, NULL, &selset, NULL, &seltime);
        switch (rv)
        {
            case -1:            /* error occured! */
                return errno;
            case 0:             /* timeout; retry */
                timeout += kTimeoutSelectUsecs;
                continue;
            default:            /* ready to write */
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
    int  rv = OK;
    unsigned char lbuf[kReadBufSize]; /* function local buffer */
    int bytesrd = 0;
    struct timeval seltime;
    fd_set selset;
    int bufsize;

    if (!aBuf || *aBufSize <= 0 || mFd < 0)
        return E_PARAM;
    memset(aBuf, 0, *aBufSize);

    for ( ; ; )
    {
        /* return if we anticipate overflowing caller's buffer */
        if (bytesrd >= *aBufSize)
            return E_READ_MORE;

        memset(&lbuf, 0, kReadBufSize);

        FD_ZERO(&selset);
        FD_SET(mFd, &selset);
        seltime.tv_sec = 0;
        seltime.tv_usec = kTimeoutSelectUsecs;

        rv = select(mFd+1, &selset, NULL, NULL, &seltime);
        switch (rv)
        {
            case -1:            /* error occured! */
                return errno;
            case 0:             /* timeout; retry */
                continue;
            default:            /* ready to read */
                break;
        }

        // XXX TODO: prevent inf loop returning at kTimeoutThresholdUsecs
        if (!FD_ISSET(mFd, &selset))
            continue;           /* not ready to read; retry */

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
    *aBufSize = bytesrd;
    return rv;
}

int
nsSocket::Close()
{
    int rv = OK, rv1 = OK, rv2 = OK;

    rv1 = shutdown(mFd, SHUT_RDWR);
    if (mListenFd > 0)
        rv2 = shutdown(mListenFd, SHUT_RDWR);
    
    if (rv1 != 0 || rv2 != 0)
       rv = E_SOCK_CLOSE; 

/* funky windows shutdown of winsock */
#ifdef _WINDOWS
    int wsaErr = WSACleanup();
    if (wsaErr != 0)
        rv = wsaErr;
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

float
nsSocket::CalcRate(struct timeval *aPre, struct timeval *aPost, int aBytes)
{
    float diffUsecs, rate;

    /* param check */
    if (!aPre || !aPost || aBytes <= 0)
        return 0;
    
    diffUsecs = (float)(aPost->tv_sec - aPre->tv_sec) * kUsecsPerSec;
    diffUsecs += (float)aPost->tv_usec - (float)aPre->tv_usec;
    rate = ((float)(aBytes/kKilobyte))/
           ((float)(diffUsecs/kUsecsPerSec));

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
