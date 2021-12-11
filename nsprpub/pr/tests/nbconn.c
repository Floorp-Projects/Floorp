/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * A test for nonblocking connect.  Functions tested include PR_Connect,
 * PR_Poll, and PR_GetConnectStatus.
 *
 * The test should be invoked with a host name, for example:
 *     nbconn www.netscape.com
 * It will do a nonblocking connect to port 80 (HTTP) on that host,
 * and when connected, issue the "GET /" HTTP command.
 *
 * You should run this test in three ways:
 * 1. To a known web site, such as www.netscape.com.  The HTML of the
 *    top-level page at the web site should be printed.
 * 2. To a machine not running a web server at port 80.  This test should
 *    fail.  Ideally the error code should be PR_CONNECT_REFUSED_ERROR.
 *    But it is possible to return PR_UNKNOWN_ERROR on certain platforms.
 * 3. To an unreachable machine, for example, a machine that is off line.
 *    The test should fail after the connect times out.  Ideally the
 *    error code should be PR_IO_TIMEOUT_ERROR, but it is possible to
 *    return PR_UNKNOWN_ERROR on certain platforms.
 */

#include "nspr.h"
#include "plgetopt.h"
#include <stdio.h>
#include <string.h>

#define SERVER_MAX_BIND_COUNT        100
#define DATA_BUF_SIZE                256
#define TCP_SERVER_PORT            10000
#define TCP_UNUSED_PORT            211

typedef struct Server_Param {
    PRFileDesc *sp_fd;      /* server port */
} Server_Param;
static void PR_CALLBACK TCP_Server(void *arg);

int _debug_on;
#define DPRINTF(arg) if (_debug_on) printf arg

static PRIntn connection_success_test();
static PRIntn connection_failure_test();

int main(int argc, char **argv)
{
    PRHostEnt he;
    char buf[1024];
    PRNetAddr addr;
    PRPollDesc pd;
    PRStatus rv;
    PRSocketOptionData optData;
    const char *hostname = NULL;
    PRIntn default_case, n, bytes_read, bytes_sent;
    PRInt32 failed_already = 0;

    /*
     * -d           debug mode
     */

    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "d");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) {
            continue;
        }
        switch (opt->option)
        {
            case 0:  /* debug mode */
                hostname = opt->value;
                break;
            case 'd':  /* debug mode */
                _debug_on = 1;
                break;
            default:
                break;
        }
    }
    PL_DestroyOptState(opt);

    PR_STDIO_INIT();
    if (hostname) {
        default_case = 0;
    }
    else {
        default_case = 1;
    }

    if (default_case) {

        /*
         * In the default case the following tests are executed:
         *  1. successful connection: a server thread accepts a connection
         *     from the main thread
         *  2. unsuccessful connection: the main thread tries to connect to a
         *     nonexistent port and expects to get an error
         */
        rv = connection_success_test();
        if (rv == 0) {
            rv = connection_failure_test();
        }
        return rv;
    } else {
        PRFileDesc *sock;

        if (PR_GetHostByName(argv[1], buf, sizeof(buf), &he) == PR_FAILURE) {
            printf( "Unknown host: %s\n", argv[1]);
            exit(1);
        } else {
            printf( "host: %s\n", buf);
        }
        PR_EnumerateHostEnt(0, &he, 80, &addr);

        sock = PR_NewTCPSocket();
        optData.option = PR_SockOpt_Nonblocking;
        optData.value.non_blocking = PR_TRUE;
        PR_SetSocketOption(sock, &optData);
        rv = PR_Connect(sock, &addr, PR_INTERVAL_NO_TIMEOUT);
        if (rv == PR_FAILURE && PR_GetError() == PR_IN_PROGRESS_ERROR) {
            printf( "Connect in progress\n");
        }

        pd.fd = sock;
        pd.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
        n = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
        if (n == -1) {
            printf( "PR_Poll failed\n");
            exit(1);
        }
        printf( "PR_Poll returns %d\n", n);
        if (pd.out_flags & PR_POLL_READ) {
            printf( "PR_POLL_READ\n");
        }
        if (pd.out_flags & PR_POLL_WRITE) {
            printf( "PR_POLL_WRITE\n");
        }
        if (pd.out_flags & PR_POLL_EXCEPT) {
            printf( "PR_POLL_EXCEPT\n");
        }
        if (pd.out_flags & PR_POLL_ERR) {
            printf( "PR_POLL_ERR\n");
        }
        if (pd.out_flags & PR_POLL_NVAL) {
            printf( "PR_POLL_NVAL\n");
        }

        if (PR_GetConnectStatus(&pd) == PR_SUCCESS) {
            printf("PR_GetConnectStatus: connect succeeded\n");
            PR_Write(sock, "GET /\r\n\r\n", 9);
            PR_Shutdown(sock, PR_SHUTDOWN_SEND);
            pd.in_flags = PR_POLL_READ;
            while (1) {
                n = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
                printf( "poll returns %d\n", n);
                n = PR_Read(sock, buf, sizeof(buf));
                printf( "read returns %d\n", n);
                if (n <= 0) {
                    break;
                }
                PR_Write(PR_STDOUT, buf, n);
            }
        } else {
            if (PR_GetError() == PR_IN_PROGRESS_ERROR) {
                printf( "PR_GetConnectStatus: connect still in progress\n");
                exit(1);
            }
            printf( "PR_GetConnectStatus: connect failed: (%ld, %ld)\n",
                    PR_GetError(), PR_GetOSError());
        }
        PR_Close(sock);
        printf( "PASS\n");
        return 0;

    }
}


/*
 * TCP Server
 *    Server Thread
 *    Accept a connection from the client and write some data
 */
static void PR_CALLBACK
TCP_Server(void *arg)
{
    Server_Param *sp = (Server_Param *) arg;
    PRFileDesc *sockfd, *newsockfd;
    char data_buf[DATA_BUF_SIZE];
    PRIntn rv, bytes_read;

    sockfd = sp->sp_fd;
    if ((newsockfd = PR_Accept(sockfd, NULL,
                               PR_INTERVAL_NO_TIMEOUT)) == NULL) {
        fprintf(stderr,"ERROR - PR_Accept failed: (%d,%d)\n",
                PR_GetError(), PR_GetOSError());
        return;
    }
    bytes_read = 0;
    while (bytes_read != DATA_BUF_SIZE) {
        rv = PR_Read(newsockfd, data_buf + bytes_read,
                     DATA_BUF_SIZE - bytes_read);
        if (rv < 0) {
            fprintf(stderr,"Error - PR_Read failed: (%d, %d)\n",
                    PR_GetError(), PR_GetOSError());
            PR_Close(newsockfd);
            return;
        }
        PR_ASSERT(rv != 0);
        bytes_read += rv;
    }
    DPRINTF(("Bytes read from client - %d\n",bytes_read));
    rv = PR_Write(newsockfd, data_buf,DATA_BUF_SIZE);
    if (rv < 0) {
        fprintf(stderr,"Error - PR_Write failed: (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        PR_Close(newsockfd);
        return;
    }
    PR_ASSERT(rv == DATA_BUF_SIZE);
    DPRINTF(("Bytes written to client - %d\n",rv));
    PR_Close(newsockfd);
}


/*
 * test for successful connection using a non-blocking socket
 */
static PRIntn
connection_success_test()
{
    PRFileDesc *sockfd = NULL, *conn_fd = NULL;
    PRNetAddr netaddr;
    PRInt32 i, rv;
    PRPollDesc pd;
    PRSocketOptionData optData;
    PRThread *thr = NULL;
    Server_Param sp;
    char send_buf[DATA_BUF_SIZE], recv_buf[DATA_BUF_SIZE];
    PRIntn default_case, n, bytes_read, bytes_sent;
    PRIntn failed_already = 0;

    /*
     * Create a tcp socket
     */
    if ((sockfd = PR_NewTCPSocket()) == NULL) {
        fprintf(stderr,"Error - PR_NewTCPSocket failed\n");
        failed_already=1;
        goto def_exit;
    }
    memset(&netaddr, 0, sizeof(netaddr));
    netaddr.inet.family = PR_AF_INET;
    netaddr.inet.port = PR_htons(TCP_SERVER_PORT);
    netaddr.inet.ip = PR_htonl(PR_INADDR_ANY);
    /*
     * try a few times to bind server's address, if addresses are in
     * use
     */
    i = 0;
    while (PR_Bind(sockfd, &netaddr) < 0) {
        if (PR_GetError() == PR_ADDRESS_IN_USE_ERROR) {
            netaddr.inet.port += 2;
            if (i++ < SERVER_MAX_BIND_COUNT) {
                continue;
            }
        }
        fprintf(stderr,"ERROR - PR_Bind failed: (%d,%d)\n",
                PR_GetError(), PR_GetOSError());
        failed_already=1;
        goto def_exit;
    }

    if (PR_Listen(sockfd, 32) < 0) {
        fprintf(stderr,"ERROR - PR_Listen failed: (%d,%d)\n",
                PR_GetError(), PR_GetOSError());
        failed_already=1;
        goto def_exit;
    }

    if (PR_GetSockName(sockfd, &netaddr) < 0) {
        fprintf(stderr,"ERROR - PR_GetSockName failed: (%d,%d)\n",
                PR_GetError(), PR_GetOSError());
        failed_already=1;
        goto def_exit;
    }
    if ((conn_fd = PR_NewTCPSocket()) == NULL) {
        fprintf(stderr,"Error - PR_NewTCPSocket failed\n");
        failed_already=1;
        goto def_exit;
    }
    optData.option = PR_SockOpt_Nonblocking;
    optData.value.non_blocking = PR_TRUE;
    PR_SetSocketOption(conn_fd, &optData);
    rv = PR_Connect(conn_fd, &netaddr, PR_INTERVAL_NO_TIMEOUT);
    if (rv == PR_FAILURE) {
        if (PR_GetError() == PR_IN_PROGRESS_ERROR) {
            DPRINTF(("Connect in progress\n"));
        } else  {
            fprintf(stderr,"Error - PR_Connect failed: (%d, %d)\n",
                    PR_GetError(), PR_GetOSError());
            failed_already=1;
            goto def_exit;
        }
    }
    /*
     * Now create a thread to accept a connection
     */
    sp.sp_fd = sockfd;
    thr = PR_CreateThread(PR_USER_THREAD, TCP_Server, (void *)&sp,
                          PR_PRIORITY_NORMAL, PR_LOCAL_THREAD, PR_JOINABLE_THREAD, 0);
    if (thr == NULL) {
        fprintf(stderr,"Error - PR_CreateThread failed: (%d,%d)\n",
                PR_GetError(), PR_GetOSError());
        failed_already=1;
        goto def_exit;
    }
    DPRINTF(("Created TCP_Server thread [0x%x]\n",thr));
    pd.fd = conn_fd;
    pd.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
    n = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
    if (n == -1) {
        fprintf(stderr,"Error - PR_Poll failed: (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        failed_already=1;
        goto def_exit;
    }
    if (PR_GetConnectStatus(&pd) == PR_SUCCESS) {
        PRInt32 rv;

        DPRINTF(("Connection successful\n"));

        /*
         * Write some data, read it back and check data integrity to
         * make sure the connection is good
         */
        pd.in_flags = PR_POLL_WRITE;
        bytes_sent = 0;
        memset(send_buf, 'a', DATA_BUF_SIZE);
        while (bytes_sent != DATA_BUF_SIZE) {
            rv = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
            if (rv < 0) {
                fprintf(stderr,"Error - PR_Poll failed: (%d, %d)\n",
                        PR_GetError(), PR_GetOSError());
                failed_already=1;
                goto def_exit;
            }
            PR_ASSERT((rv == 1) && (pd.out_flags == PR_POLL_WRITE));
            rv = PR_Write(conn_fd, send_buf + bytes_sent,
                          DATA_BUF_SIZE - bytes_sent);
            if (rv < 0) {
                fprintf(stderr,"Error - PR_Write failed: (%d, %d)\n",
                        PR_GetError(), PR_GetOSError());
                failed_already=1;
                goto def_exit;
            }
            PR_ASSERT(rv > 0);
            bytes_sent += rv;
        }
        DPRINTF(("Bytes written to server - %d\n",bytes_sent));
        PR_Shutdown(conn_fd, PR_SHUTDOWN_SEND);
        pd.in_flags = PR_POLL_READ;
        bytes_read = 0;
        memset(recv_buf, 0, DATA_BUF_SIZE);
        while (bytes_read != DATA_BUF_SIZE) {
            rv = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
            if (rv < 0) {
                fprintf(stderr,"Error - PR_Poll failed: (%d, %d)\n",
                        PR_GetError(), PR_GetOSError());
                failed_already=1;
                goto def_exit;
            }
            PR_ASSERT((rv == 1) && (pd.out_flags == PR_POLL_READ));
            rv = PR_Read(conn_fd, recv_buf + bytes_read,
                         DATA_BUF_SIZE - bytes_read);
            if (rv < 0) {
                fprintf(stderr,"Error - PR_Read failed: (%d, %d)\n",
                        PR_GetError(), PR_GetOSError());
                failed_already=1;
                goto def_exit;
            }
            PR_ASSERT(rv != 0);
            bytes_read += rv;
        }
        DPRINTF(("Bytes read from server - %d\n",bytes_read));
        /*
         * verify the data read
         */
        if (memcmp(send_buf, recv_buf, DATA_BUF_SIZE) != 0) {
            fprintf(stderr,"ERROR - data corruption\n");
            failed_already=1;
            goto def_exit;
        }
        DPRINTF(("Data integrity verified\n"));
    } else {
        fprintf(stderr,"PR_GetConnectStatus: connect failed: (%ld, %ld)\n",
                PR_GetError(), PR_GetOSError());
        failed_already = 1;
        goto def_exit;
    }
def_exit:
    if (thr) {
        PR_JoinThread(thr);
        thr = NULL;
    }
    if (sockfd) {
        PR_Close(sockfd);
        sockfd = NULL;
    }
    if (conn_fd) {
        PR_Close(conn_fd);
        conn_fd = NULL;
    }
    if (failed_already) {
        return 1;
    }
    else {
        return 0;
    }

}

/*
 * test for connection to a nonexistent port using a non-blocking socket
 */
static PRIntn
connection_failure_test()
{
    PRFileDesc *sockfd = NULL, *conn_fd = NULL;
    PRNetAddr netaddr;
    PRInt32 i, rv;
    PRPollDesc pd;
    PRSocketOptionData optData;
    PRIntn n, failed_already = 0;

    /*
     * Create a tcp socket
     */
    if ((sockfd = PR_NewTCPSocket()) == NULL) {
        fprintf(stderr,"Error - PR_NewTCPSocket failed\n");
        failed_already=1;
        goto def_exit;
    }
    memset(&netaddr, 0, sizeof(netaddr));
    netaddr.inet.family = PR_AF_INET;
    netaddr.inet.port = PR_htons(TCP_SERVER_PORT);
    netaddr.inet.ip = PR_htonl(PR_INADDR_ANY);
    /*
     * try a few times to bind server's address, if addresses are in
     * use
     */
    i = 0;
    while (PR_Bind(sockfd, &netaddr) < 0) {
        if (PR_GetError() == PR_ADDRESS_IN_USE_ERROR) {
            netaddr.inet.port += 2;
            if (i++ < SERVER_MAX_BIND_COUNT) {
                continue;
            }
        }
        fprintf(stderr,"ERROR - PR_Bind failed: (%d,%d)\n",
                PR_GetError(), PR_GetOSError());
        failed_already=1;
        goto def_exit;
    }

    if (PR_GetSockName(sockfd, &netaddr) < 0) {
        fprintf(stderr,"ERROR - PR_GetSockName failed: (%d,%d)\n",
                PR_GetError(), PR_GetOSError());
        failed_already=1;
        goto def_exit;
    }
#ifdef AIX
    /*
     * On AIX, set to unused/reserved port
     */
    netaddr.inet.port = PR_htons(TCP_UNUSED_PORT);
#endif
    if ((conn_fd = PR_NewTCPSocket()) == NULL) {
        fprintf(stderr,"Error - PR_NewTCPSocket failed\n");
        failed_already=1;
        goto def_exit;
    }
    optData.option = PR_SockOpt_Nonblocking;
    optData.value.non_blocking = PR_TRUE;
    PR_SetSocketOption(conn_fd, &optData);
    rv = PR_Connect(conn_fd, &netaddr, PR_INTERVAL_NO_TIMEOUT);
    if (rv == PR_FAILURE) {
        DPRINTF(("PR_Connect to a non-listen port failed: (%d, %d)\n",
                 PR_GetError(), PR_GetOSError()));
    } else {
        PR_ASSERT(rv == PR_SUCCESS);
        fprintf(stderr,"Error - PR_Connect succeeded, expected to fail\n");
        failed_already=1;
        goto def_exit;
    }
    pd.fd = conn_fd;
    pd.in_flags = PR_POLL_WRITE | PR_POLL_EXCEPT;
    n = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
    if (n == -1) {
        fprintf(stderr,"Error - PR_Poll failed: (%d, %d)\n",
                PR_GetError(), PR_GetOSError());
        failed_already=1;
        goto def_exit;
    }
    if (PR_GetConnectStatus(&pd) == PR_SUCCESS) {
        PRInt32 rv;
        fprintf(stderr,"PR_GetConnectStatus succeeded, expected to fail\n");
        failed_already = 1;
        goto def_exit;
    }
    rv = PR_GetError();
    DPRINTF(("Connection failed, successfully with PR_Error %d\n",rv));
def_exit:
    if (sockfd) {
        PR_Close(sockfd);
        sockfd = NULL;
    }
    if (conn_fd) {
        PR_Close(conn_fd);
        conn_fd = NULL;
    }
    if (failed_already) {
        return 1;
    }
    else {
        return 0;
    }

}
