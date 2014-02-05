/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: socket.c
**
** Description: Test socket functionality.
**
** Modification History:
*/
#include "primpl.h"

#include "plgetopt.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef XP_UNIX
#include <sys/mman.h>
#endif
#if defined(_PR_PTHREADS) && !defined(_PR_DCETHREADS)
#include <pthread.h>
#endif

#ifdef WIN32
#include <process.h>
#endif

static int _debug_on = 0;
static int test_cancelio = 0;

#include "obsolete/prsem.h"

#ifdef XP_PC
#define mode_t int
#endif

#define DPRINTF(arg) if (_debug_on) printf arg

#ifdef XP_PC
char *TEST_DIR = "prdir";
char *SMALL_FILE_NAME = "prsmallf";
char *LARGE_FILE_NAME = "prlargef";
#elif defined(SYMBIAN)
char *TEST_DIR = "c:\\data\\prsocket";
char *SMALL_FILE_NAME = "c:\\data\\prsocket\\small_file";
char *LARGE_FILE_NAME = "c:\\data\\prsocket\\large_file";
#else
char *TEST_DIR = "/tmp/prsocket_test_dir";
char *SMALL_FILE_NAME = "/tmp/prsocket_test_dir/small_file";
char *LARGE_FILE_NAME = "/tmp/prsocket_test_dir/large_file";
#endif
#define SMALL_FILE_SIZE				(3 * 1024)        /* 3 KB        */
#define SMALL_FILE_OFFSET_1			(512)
#define SMALL_FILE_LEN_1			(1 * 1024)        /* 1 KB        */
#define SMALL_FILE_OFFSET_2			(75)
#define SMALL_FILE_LEN_2			(758)
#define SMALL_FILE_OFFSET_3			(1024)
#define SMALL_FILE_LEN_3			(SMALL_FILE_SIZE - SMALL_FILE_OFFSET_3)
#define SMALL_FILE_HEADER_SIZE    	(64)            /* 64 bytes    */
#define SMALL_FILE_TRAILER_SIZE   	(128)           /* 128 bytes    */

#define LARGE_FILE_SIZE				(3 * 1024 * 1024)    /* 3 MB        */
#define LARGE_FILE_OFFSET_1			(0)
#define LARGE_FILE_LEN_1			(2 * 1024 * 1024)    /* 2 MB        */
#define LARGE_FILE_OFFSET_2			(64)
#define LARGE_FILE_LEN_2			(1 * 1024 * 1024 + 75)
#define LARGE_FILE_OFFSET_3			(2 * 1024 * 1024 - 128)
#define LARGE_FILE_LEN_3			(LARGE_FILE_SIZE - LARGE_FILE_OFFSET_3)
#define LARGE_FILE_OFFSET_4			PR_GetPageSize()
#define LARGE_FILE_LEN_4			769
#define LARGE_FILE_HEADER_SIZE    	(512)
#define LARGE_FILE_TRAILER_SIZE   	(64)

#define    BUF_DATA_SIZE    (2 * 1024)
#define TCP_MESG_SIZE    1024
/*
 * set UDP datagram size small enough that datagrams sent to a port on the
 * local host will not be lost
 */
#define UDP_DGRAM_SIZE            128
#define NUM_TCP_CLIENTS            5	/* for a listen queue depth of 5 */
#define NUM_UDP_CLIENTS            10

#ifdef SYMBIAN
#define NUM_TRANSMITFILE_CLIENTS    1
#else
#define NUM_TRANSMITFILE_CLIENTS    4
#endif

#define NUM_TCP_CONNECTIONS_PER_CLIENT    5
#define NUM_TCP_MESGS_PER_CONNECTION    10
#define NUM_UDP_DATAGRAMS_PER_CLIENT    5
#define TCP_SERVER_PORT            10000
#define UDP_SERVER_PORT            TCP_SERVER_PORT
#define SERVER_MAX_BIND_COUNT        100

#ifdef WINCE
#define perror(s)
#endif

static PRInt32 num_tcp_clients = NUM_TCP_CLIENTS;
static PRInt32 num_udp_clients = NUM_UDP_CLIENTS;
static PRInt32 num_transmitfile_clients = NUM_TRANSMITFILE_CLIENTS;
static PRInt32 num_tcp_connections_per_client = NUM_TCP_CONNECTIONS_PER_CLIENT;
static PRInt32 tcp_mesg_size = TCP_MESG_SIZE;
static PRInt32 num_tcp_mesgs_per_connection = NUM_TCP_MESGS_PER_CONNECTION;
static PRInt32 num_udp_datagrams_per_client = NUM_UDP_DATAGRAMS_PER_CLIENT;
static PRInt32 udp_datagram_size = UDP_DGRAM_SIZE;

static PRInt32 thread_count;
PRUint16 server_domain = PR_AF_INET, client_domain = PR_AF_INET;

/* an I/O layer that uses the emulated senfile method */
static PRDescIdentity emuSendFileIdentity;
static PRIOMethods emuSendFileMethods;

int failed_already=0;
typedef struct buffer {
    char    data[BUF_DATA_SIZE];
} buffer;

PRNetAddr tcp_server_addr, udp_server_addr;

typedef struct Serve_Client_Param {
    PRFileDesc *sockfd;    /* socket to read from/write to    */
    PRInt32    datalen;    /* bytes of data transfered in each read/write */
} Serve_Client_Param;

typedef struct Server_Param {
    PRSemaphore *addr_sem;    /* sem to post on, after setting up the address */
    PRMonitor *exit_mon;    /* monitor to signal on exit            */
    PRInt32 *exit_counter;    /* counter to decrement, before exit        */
    PRInt32    datalen;    /* bytes of data transfered in each read/write    */
} Server_Param;


typedef struct Client_Param {
    PRNetAddr server_addr;
    PRMonitor *exit_mon;    /* monitor to signal on exit */
    PRInt32 *exit_counter;    /* counter to decrement, before exit */
    PRInt32    datalen;
    PRInt32    udp_connect;    /* if set clients connect udp sockets */
} Client_Param;

/* the sendfile method in emuSendFileMethods */
static PRInt32 PR_CALLBACK
emu_SendFile(PRFileDesc *sd, PRSendFileData *sfd,
    PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    return PR_EmulateSendFile(sd, sfd, flags, timeout);
}

/* the transmitfile method in emuSendFileMethods */
static PRInt32 PR_CALLBACK
emu_TransmitFile(PRFileDesc *sd, PRFileDesc *fd, const void *headers,
    PRInt32 hlen, PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    PRSendFileData sfd;

    sfd.fd = fd;
    sfd.file_offset = 0;
    sfd.file_nbytes = 0;
    sfd.header = headers;
    sfd.hlen = hlen;
    sfd.trailer = NULL;
    sfd.tlen = 0;
    return emu_SendFile(sd, &sfd, flags, timeout);
}

/*
 * readn
 *    read data from sockfd into buf
 */
static PRInt32
readn(PRFileDesc *sockfd, char *buf, int len)
{
    int rem;
    int bytes;
    int offset = 0;
	int err;
	PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT;

	if (test_cancelio)
		timeout = PR_SecondsToInterval(2);

    for (rem=len; rem; offset += bytes, rem -= bytes) {
        DPRINTF(("thread = 0x%lx: calling PR_Recv, bytes = %d\n",
            PR_GetCurrentThread(), rem));
retry:
        bytes = PR_Recv(sockfd, buf + offset, rem, 0,
            	timeout);
        DPRINTF(("thread = 0x%lx: returning from PR_Recv, bytes = %d\n",
            PR_GetCurrentThread(), bytes));
        if (bytes < 0) {
#ifdef WINNT
			printf("PR_Recv: error = %d oserr = %d\n",(err = PR_GetError()),
									PR_GetOSError());
			if ((test_cancelio) && (err == PR_IO_TIMEOUT_ERROR)) {
				if (PR_NT_CancelIo(sockfd) != PR_SUCCESS)
					printf("PR_NT_CancelIO: error = %d\n",PR_GetError());
				timeout = PR_INTERVAL_NO_TIMEOUT;
				goto retry;
			}
#endif
			return -1;
		}	
    }
    return len;
}

/*
 * writen
 *    write data from buf to sockfd
 */
static PRInt32
writen(PRFileDesc *sockfd, char *buf, int len)
{
    int rem;
    int bytes;
    int offset = 0;

    for (rem=len; rem; offset += bytes, rem -= bytes) {
        DPRINTF(("thread = 0x%lx: calling PR_Send, bytes = %d\n",
            PR_GetCurrentThread(), rem));
        bytes = PR_Send(sockfd, buf + offset, rem, 0,
            PR_INTERVAL_NO_TIMEOUT);
        DPRINTF(("thread = 0x%lx: returning from PR_Send, bytes = %d\n",
            PR_GetCurrentThread(), bytes));
        if (bytes <= 0)
            return -1;
    }
    return len;
}

/*
 * Serve_Client
 *    Thread, started by the server, for serving a client connection.
 *    Reads data from socket and writes it back, unmodified, and
 *    closes the socket
 */
static void PR_CALLBACK
Serve_Client(void *arg)
{
    Serve_Client_Param *scp = (Serve_Client_Param *) arg;
    PRFileDesc *sockfd;
    buffer *in_buf;
    PRInt32 bytes, j;

    sockfd = scp->sockfd;
    bytes = scp->datalen;
    in_buf = PR_NEW(buffer);
    if (in_buf == NULL) {
        fprintf(stderr,"prsocket_test: failed to alloc buffer struct\n");
        failed_already=1;
        goto exit;
    }


    for (j = 0; j < num_tcp_mesgs_per_connection; j++) {
        /*
         * Read data from client and send it back to the client unmodified
         */
        if (readn(sockfd, in_buf->data, bytes) < bytes) {
            fprintf(stderr,"prsocket_test: ERROR - Serve_Client:readn\n");
            failed_already=1;
            goto exit;
        }
        /* Shutdown only RCV will cause error on Symbian OS */
#if !defined(SYMBIAN)
        /*
         * shutdown reads, after the last read
         */
        if (j == num_tcp_mesgs_per_connection - 1)
            if (PR_Shutdown(sockfd, PR_SHUTDOWN_RCV) < 0) {
                fprintf(stderr,"prsocket_test: ERROR - PR_Shutdown\n");
            }
#endif
        DPRINTF(("Serve_Client [0x%lx]: inbuf[0] = 0x%lx\n",PR_GetCurrentThread(),
            (*((int *) in_buf->data))));
        if (writen(sockfd, in_buf->data, bytes) < bytes) {
            fprintf(stderr,"prsocket_test: ERROR - Serve_Client:writen\n");
            failed_already=1;
            goto exit;
        }
    }
    /*
     * shutdown reads and writes
     */
    if (PR_Shutdown(sockfd, PR_SHUTDOWN_BOTH) < 0) {
        fprintf(stderr,"prsocket_test: ERROR - PR_Shutdown\n");
        failed_already=1;
    }

exit:
    PR_Close(sockfd);
    if (in_buf) {
        PR_DELETE(in_buf);
    }
}

PRThread* create_new_thread(PRThreadType type,
							void (*start)(void *arg),
							void *arg,
							PRThreadPriority priority,
							PRThreadScope scope,
							PRThreadState state,
							PRUint32 stackSize, PRInt32 index)
{
PRInt32 native_thread = 0;

	PR_ASSERT(state == PR_UNJOINABLE_THREAD);
#if (defined(_PR_PTHREADS) && !defined(_PR_DCETHREADS)) || defined(WIN32)
	switch(index %  4) {
		case 0:
			scope = (PR_LOCAL_THREAD);
			break;
		case 1:
			scope = (PR_GLOBAL_THREAD);
			break;
		case 2:
			scope = (PR_GLOBAL_BOUND_THREAD);
			break;
		case 3:
			native_thread = 1;
			break;
		default:
			PR_ASSERT(!"Invalid scope");
			break;
	}
	if (native_thread) {
#if defined(_PR_PTHREADS) && !defined(_PR_DCETHREADS)
		pthread_t tid;
		if (!pthread_create(&tid, NULL, (void * (*)(void *)) start, arg))
			return((PRThread *) tid);
		else
			return (NULL);
#else
		HANDLE thandle;
		unsigned tid;
		
		thandle = (HANDLE) _beginthreadex(
						NULL,
						stackSize,
						(unsigned (__stdcall *)(void *))start,
						arg,
						STACK_SIZE_PARAM_IS_A_RESERVATION,
						&tid);		
		return((PRThread *) thandle);
#endif
	} else {
		return(PR_CreateThread(type,start,arg,priority,scope,state,stackSize));
	}
#else
	return(PR_CreateThread(type,start,arg,priority,scope,state,stackSize));
#endif
}

/*
 * TCP Server
 *    Server Thread
 *    Bind an address to a socket and listen for incoming connections
 *    Start a Serve_Client thread for each incoming connection.
 */
static void PR_CALLBACK
TCP_Server(void *arg)
{
    PRThread *t;
    Server_Param *sp = (Server_Param *) arg;
    Serve_Client_Param *scp;
    PRFileDesc *sockfd, *newsockfd;
    PRNetAddr netaddr;
    PRInt32 i;
    /*
     * Create a tcp socket
     */
	if ((sockfd = PR_OpenTCPSocket(server_domain)) == NULL) {
        fprintf(stderr,"prsocket_test: PR_NewTCPSocket failed\n");
        goto exit;
    }
    memset(&netaddr, 0 , sizeof(netaddr));
	
	if (PR_SetNetAddr(PR_IpAddrAny, server_domain, TCP_SERVER_PORT,
									&netaddr) == PR_FAILURE) {
        fprintf(stderr,"prsocket_test: PR_SetNetAddr failed\n");
        goto exit;
	}
    /*
     * try a few times to bind server's address, if addresses are in
     * use
     */
    i = 0;
	
    while (PR_Bind(sockfd, &netaddr) < 0) {
        if (PR_GetError() == PR_ADDRESS_IN_USE_ERROR) {
            netaddr.inet.port += 2;
            if (i++ < SERVER_MAX_BIND_COUNT)
                continue;
        }
        fprintf(stderr,"prsocket_test: ERROR - PR_Bind failed\n");
        perror("PR_Bind");
        failed_already=1;
        goto exit;
    }

    if (PR_Listen(sockfd, 32) < 0) {
        fprintf(stderr,"prsocket_test: ERROR - PR_Listen failed\n");
        failed_already=1;
        goto exit;
    }

    if (PR_GetSockName(sockfd, &netaddr) < 0) {
        fprintf(stderr,"prsocket_test: ERROR - PR_GetSockName failed\n");
        failed_already=1;
        goto exit;
    }

    DPRINTF(("TCP_Server: PR_BIND netaddr.inet.ip = 0x%lx, netaddr.inet.port = %d\n",
        netaddr.inet.ip, netaddr.inet.port));
	if (PR_SetNetAddr(PR_IpAddrLoopback, client_domain,
									PR_ntohs(PR_NetAddrInetPort(&netaddr)),
									&tcp_server_addr) == PR_FAILURE) {
        fprintf(stderr,"prsocket_test: PR_SetNetAddr failed\n");
        goto exit;
	}
	if ((client_domain == PR_AF_INET6) && (server_domain == PR_AF_INET))
		PR_ConvertIPv4AddrToIPv6(PR_htonl(INADDR_LOOPBACK),
								&tcp_server_addr.ipv6.ip);

    /*
     * Wake up parent thread because server address is bound and made
     * available in the global variable 'tcp_server_addr'
     */
    PR_PostSem(sp->addr_sem);

    for (i = 0; i < (num_tcp_clients * num_tcp_connections_per_client); i++) {
        /* test both null and non-null 'addr' argument to PR_Accept */
        PRNetAddr *addrp = (i%2 ? &netaddr: NULL);

    DPRINTF(("TCP_Server: Accepting connection\n"));
        if ((newsockfd = PR_Accept(sockfd, addrp,
            PR_INTERVAL_NO_TIMEOUT)) == NULL) {
            fprintf(stderr,"prsocket_test: ERROR - PR_Accept failed\n");
            goto exit;
        }
    DPRINTF(("TCP_Server: Accepted connection\n"));
        scp = PR_NEW(Serve_Client_Param);
        if (scp == NULL) {
            fprintf(stderr,"prsocket_test: PR_NEW failed\n");
            goto exit;
        }

        /*
         * Start a Serve_Client thread for each incoming connection
         */
        scp->sockfd = newsockfd;
        scp->datalen = sp->datalen;

        t = create_new_thread(PR_USER_THREAD,
            Serve_Client, (void *)scp, 
            PR_PRIORITY_NORMAL,
            PR_LOCAL_THREAD,
            PR_UNJOINABLE_THREAD,
            0, i);
        if (t == NULL) {
            fprintf(stderr,"prsocket_test: PR_CreateThread failed\n");
            failed_already=1;
            goto exit;
        }
        DPRINTF(("TCP_Server: Created Serve_Client = 0x%lx\n", t));
    }

exit:
    if (sockfd) {
        PR_Close(sockfd);
    }

    /*
     * Decrement exit_counter and notify parent thread
     */

    PR_EnterMonitor(sp->exit_mon);
    --(*sp->exit_counter);
    PR_Notify(sp->exit_mon);
    PR_ExitMonitor(sp->exit_mon);
    DPRINTF(("TCP_Server [0x%lx] exiting\n", PR_GetCurrentThread()));
}

/*
 * UDP Server
 *    Server Thread
 *    Bind an address to a socket, read data from clients and send data
 *    back to clients
 */
static void PR_CALLBACK
UDP_Server(void *arg)
{
    Server_Param *sp = (Server_Param *) arg;
    PRFileDesc *sockfd;
    buffer *in_buf;
    PRNetAddr netaddr;
    PRInt32 bytes, i, rv = 0;


    bytes = sp->datalen;
    /*
     * Create a udp socket
     */
	if ((sockfd = PR_OpenUDPSocket(server_domain)) == NULL) {
        fprintf(stderr,"prsocket_test: PR_NewUDPSocket failed\n");
        failed_already=1;
        return;
    }
    memset(&netaddr, 0 , sizeof(netaddr));
	if (PR_SetNetAddr(PR_IpAddrAny, server_domain, UDP_SERVER_PORT,
									&netaddr) == PR_FAILURE) {
        fprintf(stderr,"prsocket_test: PR_SetNetAddr failed\n");
        failed_already=1;
        return;
	}
    /*
     * try a few times to bind server's address, if addresses are in
     * use
     */
    i = 0;
    while (PR_Bind(sockfd, &netaddr) < 0) {
        if (PR_GetError() == PR_ADDRESS_IN_USE_ERROR) {
            netaddr.inet.port += 2;
            if (i++ < SERVER_MAX_BIND_COUNT)
                continue;
        }
        fprintf(stderr,"prsocket_test: ERROR - PR_Bind failed\n");
        perror("PR_Bind");
        failed_already=1;
        return;
    }

    if (PR_GetSockName(sockfd, &netaddr) < 0) {
        fprintf(stderr,"prsocket_test: ERROR - PR_GetSockName failed\n");
        failed_already=1;
        return;
    }

    DPRINTF(("PR_Bind: UDP Server netaddr.inet.ip = 0x%lx, netaddr.inet.port = %d\n",
        netaddr.inet.ip, netaddr.inet.port));
    /*
     * We can't use the IP address returned by PR_GetSockName in
     * netaddr.inet.ip because netaddr.inet.ip is returned
     * as 0 (= PR_INADDR_ANY).
     */

	if (PR_SetNetAddr(PR_IpAddrLoopback, client_domain,
									PR_ntohs(PR_NetAddrInetPort(&netaddr)),
									&udp_server_addr) == PR_FAILURE) {
        fprintf(stderr,"prsocket_test: PR_SetNetAddr failed\n");
        failed_already=1;
        return;
	}
	if ((client_domain == PR_AF_INET6) && (server_domain == PR_AF_INET))
		PR_ConvertIPv4AddrToIPv6(PR_htonl(INADDR_LOOPBACK),
								&udp_server_addr.ipv6.ip);
		
    /*
     * Wake up parent thread because server address is bound and made
     * available in the global variable 'udp_server_addr'
     */
    PR_PostSem(sp->addr_sem);

    bytes = sp->datalen;
    in_buf = PR_NEW(buffer);
    if (in_buf == NULL) {
        fprintf(stderr,"prsocket_test: failed to alloc buffer struct\n");
        failed_already=1;
        return;
    }
    /*
     * Receive datagrams from clients and send them back, unmodified, to the
     * clients
     */
    memset(&netaddr, 0 , sizeof(netaddr));
    for (i = 0; i < (num_udp_clients * num_udp_datagrams_per_client); i++) {
        DPRINTF(("UDP_Server: calling PR_RecvFrom client  - ip = 0x%lx, port = %d bytes = %d inbuf = 0x%lx, inbuf[0] = 0x%lx\n",
            netaddr.inet.ip, netaddr.inet.port, bytes, in_buf->data,
            in_buf->data[0]));

        rv = PR_RecvFrom(sockfd, in_buf->data, bytes, 0, &netaddr,
            PR_INTERVAL_NO_TIMEOUT);
        DPRINTF(("UDP_Server: PR_RecvFrom client  - ip = 0x%lx, port = %d bytes = %d inbuf = 0x%lx, inbuf[0] = 0x%lx\n",
            netaddr.inet.ip, netaddr.inet.port, rv, in_buf->data,
            in_buf->data[0]));
        if (rv != bytes) {
            return;
        }
        rv = PR_SendTo(sockfd, in_buf->data, bytes, 0, &netaddr,
            PR_INTERVAL_NO_TIMEOUT);
        if (rv != bytes) {
            return;
        }
    }

    PR_DELETE(in_buf);
    PR_Close(sockfd);

    /*
     * Decrement exit_counter and notify parent thread
     */
    PR_EnterMonitor(sp->exit_mon);
    --(*sp->exit_counter);
    PR_Notify(sp->exit_mon);
    PR_ExitMonitor(sp->exit_mon);
    DPRINTF(("UDP_Server [0x%x] exiting\n", PR_GetCurrentThread()));
}

/*
 * TCP_Client
 *    Client Thread
 *    Connect to the server at the address specified in the argument.
 *    Fill in a buffer, write data to server, read it back and check
 *    for data corruption.
 *    Close the socket for server connection
 */
static void PR_CALLBACK
TCP_Client(void *arg)
{
    Client_Param *cp = (Client_Param *) arg;
    PRFileDesc *sockfd;
    buffer *in_buf, *out_buf;
    union PRNetAddr netaddr;
    PRInt32 bytes, i, j;


    bytes = cp->datalen;
    out_buf = PR_NEW(buffer);
    if (out_buf == NULL) {
        fprintf(stderr,"prsocket_test: failed to alloc buffer struct\n");
        failed_already=1;
        return;
    }
    in_buf = PR_NEW(buffer);
    if (in_buf == NULL) {
        fprintf(stderr,"prsocket_test: failed to alloc buffer struct\n");
        failed_already=1;
        return;
    }
    netaddr = cp->server_addr;

    for (i = 0; i < num_tcp_connections_per_client; i++) {
        if ((sockfd = PR_OpenTCPSocket(client_domain)) == NULL) {
            fprintf(stderr,"prsocket_test: PR_OpenTCPSocket failed\n");
            failed_already=1;
            return;
        }
        if (PR_Connect(sockfd, &netaddr,PR_INTERVAL_NO_TIMEOUT) < 0){
        	fprintf(stderr, "PR_Connect failed: (%ld, %ld)\n",
            		PR_GetError(), PR_GetOSError());
            failed_already=1;
            return;
        }
        for (j = 0; j < num_tcp_mesgs_per_connection; j++) {
            /*
             * fill in random data
             */
            memset(out_buf->data, ((PRInt32) (&netaddr)) + i + j, bytes);
            /*
             * write to server
             */
#ifdef WINNT
			if (test_cancelio && (j == 0))
				PR_Sleep(PR_SecondsToInterval(12));
#endif
            if (writen(sockfd, out_buf->data, bytes) < bytes) {
                fprintf(stderr,"prsocket_test: ERROR - TCP_Client:writen\n");
                failed_already=1;
                return;
            }
            DPRINTF(("TCP Client [0x%lx]: out_buf = 0x%lx out_buf[0] = 0x%lx\n",
                PR_GetCurrentThread(), out_buf, (*((int *) out_buf->data))));
            if (readn(sockfd, in_buf->data, bytes) < bytes) {
                fprintf(stderr,"prsocket_test: ERROR - TCP_Client:readn\n");
                failed_already=1;
                return;
            }
            /*
             * verify the data read
             */
            if (memcmp(in_buf->data, out_buf->data, bytes) != 0) {
                fprintf(stderr,"prsocket_test: ERROR - data corruption\n");
                failed_already=1;
                return;
            }
        }
        /*
         * shutdown reads and writes
         */
        if (PR_Shutdown(sockfd, PR_SHUTDOWN_BOTH) < 0) {
            fprintf(stderr,"prsocket_test: ERROR - PR_Shutdown\n");
#if defined(SYMBIAN)
            if (EPIPE != errno)
#endif
            failed_already=1;
        }
        PR_Close(sockfd);
    }

    PR_DELETE(out_buf);
    PR_DELETE(in_buf);

    /*
     * Decrement exit_counter and notify parent thread
     */

    PR_EnterMonitor(cp->exit_mon);
    --(*cp->exit_counter);
    PR_Notify(cp->exit_mon);
    PR_ExitMonitor(cp->exit_mon);
    DPRINTF(("TCP_Client [0x%x] exiting\n", PR_GetCurrentThread()));
}

/*
 * UDP_Client
 *    Client Thread
 *    Create a socket and bind an address 
 *    Communicate with the server at the address specified in the argument.
 *    Fill in a buffer, write data to server, read it back and check
 *    for data corruption.
 *    Close the socket
 */
static void PR_CALLBACK
UDP_Client(void *arg)
{
    Client_Param *cp = (Client_Param *) arg;
    PRFileDesc *sockfd;
    buffer *in_buf, *out_buf;
    union PRNetAddr netaddr;
    PRInt32 bytes, i, rv;


    bytes = cp->datalen;
    out_buf = PR_NEW(buffer);
    if (out_buf == NULL) {
        fprintf(stderr,"prsocket_test: failed to alloc buffer struct\n");
        failed_already=1;
        return;
    }
    in_buf = PR_NEW(buffer);
    if (in_buf == NULL) {
        fprintf(stderr,"prsocket_test: failed to alloc buffer struct\n");
        failed_already=1;
        return;
    }
    if ((sockfd = PR_OpenUDPSocket(client_domain)) == NULL) {
        fprintf(stderr,"prsocket_test: PR_OpenUDPSocket failed\n");
        failed_already=1;
        return;
    }

    /*
     * bind an address for the client, let the system chose the port
     * number
     */
    memset(&netaddr, 0 , sizeof(netaddr));
	if (PR_SetNetAddr(PR_IpAddrAny, client_domain, 0,
									&netaddr) == PR_FAILURE) {
        fprintf(stderr,"prsocket_test: PR_SetNetAddr failed\n");
        failed_already=1;
        return;
	}
    if (PR_Bind(sockfd, &netaddr) < 0) {
        fprintf(stderr,"prsocket_test: ERROR - PR_Bind failed\n");
        perror("PR_Bind");
        return;
    }

    if (PR_GetSockName(sockfd, &netaddr) < 0) {
        fprintf(stderr,"prsocket_test: ERROR - PR_GetSockName failed\n");
        failed_already=1;
        return;
    }

    DPRINTF(("PR_Bind: UDP Client netaddr.inet.ip = 0x%lx, netaddr.inet.port = %d\n",
        netaddr.inet.ip, netaddr.inet.port));

    netaddr = cp->server_addr;

    if (cp->udp_connect) {
        if (PR_Connect(sockfd, &netaddr,PR_INTERVAL_NO_TIMEOUT) < 0){
            fprintf(stderr,"prsocket_test: PR_Connect failed\n");
            failed_already=1;
            return;
        }
    }

    for (i = 0; i < num_udp_datagrams_per_client; i++) {
        /*
         * fill in random data
         */
        DPRINTF(("UDP_Client [0x%lx]: out_buf = 0x%lx bytes = 0x%lx\n",
            PR_GetCurrentThread(), out_buf->data, bytes));
        memset(out_buf->data, ((PRInt32) (&netaddr)) + i, bytes);
        /*
         * write to server
         */
        if (cp->udp_connect)
            rv = PR_Send(sockfd, out_buf->data, bytes, 0,
                PR_INTERVAL_NO_TIMEOUT);
        else
            rv = PR_SendTo(sockfd, out_buf->data, bytes, 0, &netaddr,
                PR_INTERVAL_NO_TIMEOUT);
        if (rv != bytes) {
            return;
        }
        DPRINTF(("UDP_Client [0x%lx]: out_buf = 0x%lx out_buf[0] = 0x%lx\n",
            PR_GetCurrentThread(), out_buf, (*((int *) out_buf->data))));
        if (cp->udp_connect)
            rv = PR_Recv(sockfd, in_buf->data, bytes, 0,
                PR_INTERVAL_NO_TIMEOUT);
        else
            rv = PR_RecvFrom(sockfd, in_buf->data, bytes, 0, &netaddr,
                PR_INTERVAL_NO_TIMEOUT);
        if (rv != bytes) {
            return;
        }
        DPRINTF(("UDP_Client [0x%lx]: in_buf = 0x%lx in_buf[0] = 0x%lx\n",
            PR_GetCurrentThread(), in_buf, (*((int *) in_buf->data))));
        /*
         * verify the data read
         */
        if (memcmp(in_buf->data, out_buf->data, bytes) != 0) {
            fprintf(stderr,"prsocket_test: ERROR - UDP data corruption\n");
            failed_already=1;
            return;
        }
    }
    PR_Close(sockfd);

    PR_DELETE(in_buf);
    PR_DELETE(out_buf);

    /*
     * Decrement exit_counter and notify parent thread
     */

    PR_EnterMonitor(cp->exit_mon);
    --(*cp->exit_counter);
    PR_Notify(cp->exit_mon);
    PR_ExitMonitor(cp->exit_mon);
    PR_DELETE(cp);
    DPRINTF(("UDP_Client [0x%x] exiting\n", PR_GetCurrentThread()));
}

/*
 * TCP_Socket_Client_Server_Test    - concurrent server test
 *    
 *    One server and several clients are started
 *    Each client connects to the server and sends a chunk of data
 *    For each connection, server starts another thread to read the data
 *    from the client and send it back to the client, unmodified.
 *    Each client checks that data received from server is same as the
 *    data it sent to the server.
 *
 */

static PRInt32
TCP_Socket_Client_Server_Test(void)
{
    int i;
    PRThread *t;
    PRSemaphore *server_sem;
    Server_Param *sparamp;
    Client_Param *cparamp;
    PRMonitor *mon2;
    PRInt32    datalen;


    datalen = tcp_mesg_size;
    thread_count = 0;
    /*
     * start the server thread
     */
    sparamp = PR_NEW(Server_Param);
    if (sparamp == NULL) {
        fprintf(stderr,"prsocket_test: PR_NEW failed\n");
        failed_already=1;
        return -1;
    }
    server_sem = PR_NewSem(0);
    if (server_sem == NULL) {
        fprintf(stderr,"prsocket_test: PR_NewSem failed\n");
        failed_already=1;
        return -1;
    }
    mon2 = PR_NewMonitor();
    if (mon2 == NULL) {
        fprintf(stderr,"prsocket_test: PR_NewMonitor failed\n");
        failed_already=1;
        return -1;
    }
    PR_EnterMonitor(mon2);

    sparamp->addr_sem = server_sem;
    sparamp->exit_mon = mon2;
    sparamp->exit_counter = &thread_count;
    sparamp->datalen = datalen;
    t = PR_CreateThread(PR_USER_THREAD,
        TCP_Server, (void *)sparamp, 
        PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD,
        PR_UNJOINABLE_THREAD,
        0);
    if (t == NULL) {
        fprintf(stderr,"prsocket_test: PR_CreateThread failed\n");
        failed_already=1;
        return -1;
    }
    DPRINTF(("Created TCP server = 0x%lx\n", t));
    thread_count++;

    /*
     * wait till the server address is setup
     */
    PR_WaitSem(server_sem);

    /*
     * Now start a bunch of client threads
     */

    cparamp = PR_NEW(Client_Param);
    if (cparamp == NULL) {
        fprintf(stderr,"prsocket_test: PR_NEW failed\n");
        failed_already=1;
        return -1;
    }
    cparamp->server_addr = tcp_server_addr;
    cparamp->exit_mon = mon2;
    cparamp->exit_counter = &thread_count;
    cparamp->datalen = datalen;
    for (i = 0; i < num_tcp_clients; i++) {
        t = create_new_thread(PR_USER_THREAD,
            TCP_Client, (void *) cparamp,
            PR_PRIORITY_NORMAL,
            PR_LOCAL_THREAD,
            PR_UNJOINABLE_THREAD,
            0, i);
        if (t == NULL) {
            fprintf(stderr,"prsocket_test: PR_CreateThread failed\n");
            failed_already=1;
            return -1;
        }
        DPRINTF(("Created TCP client = 0x%lx\n", t));
        thread_count++;
    }
    /* Wait for server and client threads to exit */
    while (thread_count) {
        PR_Wait(mon2, PR_INTERVAL_NO_TIMEOUT);
        DPRINTF(("TCP Server - thread_count  = %d\n", thread_count));
    }
    PR_ExitMonitor(mon2);
    printf("%30s","TCP_Socket_Client_Server_Test:");
    printf("%2ld Server %2ld Clients %2ld connections_per_client\n",1l,
        num_tcp_clients, num_tcp_connections_per_client);
    printf("%30s %2ld messages_per_connection %4ld bytes_per_message\n",":",
        num_tcp_mesgs_per_connection, tcp_mesg_size);

    return 0;
}

/*
 * UDP_Socket_Client_Server_Test    - iterative server test
 *    
 *    One server and several clients are started
 *    Each client connects to the server and sends a chunk of data
 *    For each connection, server starts another thread to read the data
 *    from the client and send it back to the client, unmodified.
 *    Each client checks that data received from server is same as the
 *    data it sent to the server.
 *
 */

static PRInt32
UDP_Socket_Client_Server_Test(void)
{
    int i;
    PRThread *t;
    PRSemaphore *server_sem;
    Server_Param *sparamp;
    Client_Param *cparamp;
    PRMonitor *mon2;
    PRInt32    datalen;
    PRInt32    udp_connect = 1;


    datalen = udp_datagram_size;
    thread_count = 0;
    /*
     * start the server thread
     */
    sparamp = PR_NEW(Server_Param);
    if (sparamp == NULL) {
        fprintf(stderr,"prsocket_test: PR_NEW failed\n");
        failed_already=1;
        return -1;
    }
    server_sem = PR_NewSem(0);
    if (server_sem == NULL) {
        fprintf(stderr,"prsocket_test: PR_NewSem failed\n");
        failed_already=1;
        return -1;
    }
    mon2 = PR_NewMonitor();
    if (mon2 == NULL) {
        fprintf(stderr,"prsocket_test: PR_NewMonitor failed\n");
        failed_already=1;
        return -1;
    }
    PR_EnterMonitor(mon2);

    sparamp->addr_sem = server_sem;
    sparamp->exit_mon = mon2;
    sparamp->exit_counter = &thread_count;
    sparamp->datalen = datalen;
    DPRINTF(("Creating UDP server"));
    t = PR_CreateThread(PR_USER_THREAD,
        UDP_Server, (void *)sparamp, 
        PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD,
        PR_UNJOINABLE_THREAD,
        0);
    if (t == NULL) {
        fprintf(stderr,"prsocket_test: PR_CreateThread failed\n");
        failed_already=1;
        return -1;
    }
    thread_count++;

    /*
     * wait till the server address is setup
     */
    PR_WaitSem(server_sem);

    /*
     * Now start a bunch of client threads
     */

    for (i = 0; i < num_udp_clients; i++) {
        cparamp = PR_NEW(Client_Param);
        if (cparamp == NULL) {
            fprintf(stderr,"prsocket_test: PR_NEW failed\n");
            failed_already=1;
            return -1;
        }
        cparamp->server_addr = udp_server_addr;
        cparamp->exit_mon = mon2;
        cparamp->exit_counter = &thread_count;
        cparamp->datalen = datalen;
        /*
         * Cause every other client thread to connect udp sockets
         */
        cparamp->udp_connect = udp_connect;
        if (udp_connect)
            udp_connect = 0;
        else
            udp_connect = 1;
        DPRINTF(("Creating UDP client %d\n", i));
        t = PR_CreateThread(PR_USER_THREAD,
            UDP_Client, (void *) cparamp,
            PR_PRIORITY_NORMAL,
            PR_LOCAL_THREAD,
            PR_UNJOINABLE_THREAD,
            0);
        if (t == NULL) {
            fprintf(stderr,"prsocket_test: PR_CreateThread failed\n");
            failed_already=1;
            return -1;
        }
        thread_count++;
    }
    /* Wait for server and client threads to exit */
    while (thread_count) {
        PR_Wait(mon2, PR_INTERVAL_NO_TIMEOUT);
        DPRINTF(("UDP Server - thread_count  = %d\n", thread_count));
    }
    PR_ExitMonitor(mon2);
    printf("%30s","UDP_Socket_Client_Server_Test: ");
    printf("%2ld Server %2ld Clients\n",1l, num_udp_clients);
    printf("%30s %2ld datagrams_per_client %4ld bytes_per_datagram\n",":",
        num_udp_datagrams_per_client, udp_datagram_size);

    return 0;
}

static PRFileDesc *small_file_fd, *large_file_fd;
static void *small_file_addr, *small_file_header, *large_file_addr;
static void *small_file_trailer, *large_file_header, *large_file_trailer;
/*
 * TransmitFile_Client
 *    Client Thread
 */
static void
TransmitFile_Client(void *arg)
{
    PRFileDesc *sockfd;
    union PRNetAddr netaddr;
    char *small_buf, *large_buf;
    Client_Param *cp = (Client_Param *) arg;
	PRInt32 rlen;

    small_buf = (char*)PR_Malloc(SMALL_FILE_SIZE + SMALL_FILE_HEADER_SIZE +
										SMALL_FILE_TRAILER_SIZE);
    if (small_buf == NULL) {
        fprintf(stderr,"prsocket_test: failed to alloc buffer\n");
        failed_already=1;
        return;
    }
    large_buf = (char*)PR_Malloc(LARGE_FILE_SIZE + LARGE_FILE_HEADER_SIZE +
												LARGE_FILE_TRAILER_SIZE);
    if (large_buf == NULL) {
        fprintf(stderr,"prsocket_test: failed to alloc buffer\n");
        failed_already=1;
        return;
    }
    netaddr.inet.family = cp->server_addr.inet.family;
    netaddr.inet.port = cp->server_addr.inet.port;
    netaddr.inet.ip = cp->server_addr.inet.ip;

    if ((sockfd = PR_NewTCPSocket()) == NULL) {
        fprintf(stderr,"prsocket_test: PR_NewTCPSocket failed\n");
        failed_already=1;
        return;
    }

    if (PR_Connect(sockfd, &netaddr,PR_INTERVAL_NO_TIMEOUT) < 0){
        fprintf(stderr,"prsocket_test: PR_Connect failed\n");
        failed_already=1;
        return;
    }
    /*
     * read the small file and verify the data
     */
    if (readn(sockfd, small_buf, SMALL_FILE_SIZE + SMALL_FILE_HEADER_SIZE)
        != (SMALL_FILE_SIZE + SMALL_FILE_HEADER_SIZE)) {
        fprintf(stderr,
            "prsocket_test: TransmitFile_Client failed to receive file\n");
        failed_already=1;
        return;
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    /* File transmission test can not be done because of large file's size */
    if (memcmp(small_file_header, small_buf, SMALL_FILE_HEADER_SIZE) != 0){
        fprintf(stderr,
            "prsocket_test: TransmitFile_Client ERROR - small file header data corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp(small_file_addr, small_buf + SMALL_FILE_HEADER_SIZE,
        SMALL_FILE_SIZE) != 0) {
        fprintf(stderr,
            "prsocket_test: TransmitFile_Client ERROR - small file data corruption\n");
        failed_already=1;
        return;
    }
#endif
    /*
     * read the large file and verify the data
     */
    if (readn(sockfd, large_buf, LARGE_FILE_SIZE) != LARGE_FILE_SIZE) {
        fprintf(stderr,
            "prsocket_test: TransmitFile_Client failed to receive file\n");
        failed_already=1;
        return;
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    if (memcmp(large_file_addr, large_buf, LARGE_FILE_SIZE) != 0) {
        fprintf(stderr,
            "prsocket_test: TransmitFile_Client ERROR - large file data corruption\n");
        failed_already=1;
    }
#endif


	/*
	 * receive data from PR_SendFile
	 */
	/*
	 * case 1: small file with header and trailer
	 */
    rlen = SMALL_FILE_SIZE + SMALL_FILE_HEADER_SIZE +
									SMALL_FILE_TRAILER_SIZE;
    if (readn(sockfd, small_buf, rlen) != rlen) {
        fprintf(stderr,
            "prsocket_test: SendFile_Client failed to receive file\n");
        failed_already=1;
        return;
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    if (memcmp(small_file_header, small_buf, SMALL_FILE_HEADER_SIZE) != 0){
        fprintf(stderr,
            "SendFile 1. ERROR - small file header corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp(small_file_addr, small_buf + SMALL_FILE_HEADER_SIZE,
        								SMALL_FILE_SIZE) != 0) {
        fprintf(stderr,
            "SendFile 1. ERROR - small file data corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp(small_file_trailer,
				small_buf + SMALL_FILE_HEADER_SIZE + SMALL_FILE_SIZE,
        				SMALL_FILE_TRAILER_SIZE) != 0) {
        fprintf(stderr,
            "SendFile 1. ERROR - small file trailer corruption\n");
        failed_already=1;
        return;
    }
#endif
	/*
	 * case 2: partial large file at zero offset, file with header and trailer
	 */
    rlen = LARGE_FILE_LEN_1 + LARGE_FILE_HEADER_SIZE +
									LARGE_FILE_TRAILER_SIZE;
    if (readn(sockfd, large_buf, rlen) != rlen) {
        fprintf(stderr,
            "prsocket_test: SendFile_Client failed to receive file\n");
        failed_already=1;
        return;
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    if (memcmp(large_file_header, large_buf, LARGE_FILE_HEADER_SIZE) != 0){
        fprintf(stderr,
            "SendFile 2. ERROR - large file header corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp(large_file_addr, large_buf + LARGE_FILE_HEADER_SIZE,
        								LARGE_FILE_LEN_1) != 0) {
        fprintf(stderr,
            "SendFile 2. ERROR - large file data corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp(large_file_trailer,
				large_buf + LARGE_FILE_HEADER_SIZE + LARGE_FILE_LEN_1,
        				LARGE_FILE_TRAILER_SIZE) != 0) {
        fprintf(stderr,
            "SendFile 2. ERROR - large file trailer corruption\n");
        failed_already=1;
        return;
    }
#endif
	/*
	 * case 3: partial small file at non-zero offset, with header
	 */
    rlen = SMALL_FILE_LEN_1 + SMALL_FILE_HEADER_SIZE;
    if (readn(sockfd, small_buf, rlen) != rlen) {
        fprintf(stderr,
            "prsocket_test: SendFile_Client failed to receive file\n");
        failed_already=1;
        return;
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    if (memcmp(small_file_header, small_buf, SMALL_FILE_HEADER_SIZE) != 0){
        fprintf(stderr,
            "SendFile 3. ERROR - small file header corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp((char *) small_file_addr + SMALL_FILE_OFFSET_1,
				small_buf + SMALL_FILE_HEADER_SIZE, SMALL_FILE_LEN_1) != 0) {
        fprintf(stderr,
            "SendFile 3. ERROR - small file data corruption\n");
        failed_already=1;
        return;
    }
#endif
	/*
	 * case 4: partial small file at non-zero offset, with trailer
	 */
    rlen = SMALL_FILE_LEN_2 + SMALL_FILE_TRAILER_SIZE;
    if (readn(sockfd, small_buf, rlen) != rlen) {
        fprintf(stderr,
            "prsocket_test: SendFile_Client failed to receive file\n");
        failed_already=1;
        return;
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    if (memcmp((char *) small_file_addr + SMALL_FILE_OFFSET_2, small_buf,
        								SMALL_FILE_LEN_2) != 0) {
        fprintf(stderr,
            "SendFile 4. ERROR - small file data corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp(small_file_trailer, small_buf + SMALL_FILE_LEN_2,
        				SMALL_FILE_TRAILER_SIZE) != 0) {
        fprintf(stderr,
            "SendFile 4. ERROR - small file trailer corruption\n");
        failed_already=1;
        return;
    }
#endif
	/*
	 * case 5: partial large file at non-zero offset, file with header
	 */
    rlen = LARGE_FILE_LEN_2 + LARGE_FILE_HEADER_SIZE;
    if (readn(sockfd, large_buf, rlen) != rlen) {
        fprintf(stderr,
            "prsocket_test: SendFile_Client failed to receive file\n");
        failed_already=1;
        return;
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    if (memcmp(large_file_header, large_buf, LARGE_FILE_HEADER_SIZE) != 0){
        fprintf(stderr,
            "SendFile 5. ERROR - large file header corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp((char *)large_file_addr + LARGE_FILE_OFFSET_2,
					large_buf + LARGE_FILE_HEADER_SIZE,
        								LARGE_FILE_LEN_2) != 0) {
        fprintf(stderr,
            "SendFile 5. ERROR - large file data corruption\n");
        failed_already=1;
        return;
    }
#endif
	/*
	 * case 6: partial small file at non-zero offset, with header
	 */
    rlen = SMALL_FILE_LEN_3 + SMALL_FILE_HEADER_SIZE;
    if (readn(sockfd, small_buf, rlen) != rlen) {
        fprintf(stderr,
            "prsocket_test: SendFile_Client failed to receive file\n");
        failed_already=1;
        return;
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    if (memcmp(small_file_header, small_buf, SMALL_FILE_HEADER_SIZE) != 0){
        fprintf(stderr,
            "SendFile 6. ERROR - small file header corruption\n");
        return;
    }
    if (memcmp((char *) small_file_addr + SMALL_FILE_OFFSET_3,
				small_buf + SMALL_FILE_HEADER_SIZE, SMALL_FILE_LEN_3) != 0) {
#if 0
		char *i, *j;
		int k;

		i = (char *) small_file_addr + SMALL_FILE_OFFSET_3;
		j = small_buf + SMALL_FILE_HEADER_SIZE;
		k = SMALL_FILE_LEN_3;
		while (k-- > 0) {
			if (*i++ != *j++)
			printf("i = %d j = %d\n",
				(int) (i - ((char *) small_file_addr + SMALL_FILE_OFFSET_3)),
				(int) (j - (small_buf + SMALL_FILE_HEADER_SIZE)));
		}
#endif
        fprintf(stderr,
            "SendFile 6. ERROR - small file data corruption\n");
        failed_already=1;
        return;
    }
#endif
	/*
	 * case 7: partial large file at non-zero offset, with header
	 */
    rlen = LARGE_FILE_LEN_3 + LARGE_FILE_HEADER_SIZE;
    if (readn(sockfd, large_buf, rlen) != rlen) {
        fprintf(stderr,
            "prsocket_test: SendFile_Client failed to receive file\n");
        failed_already=1;
        return;
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    if (memcmp(large_file_header, large_buf, LARGE_FILE_HEADER_SIZE) != 0){
        fprintf(stderr,
            "SendFile 7. ERROR - large file header corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp((char *)large_file_addr + LARGE_FILE_OFFSET_3,
					large_buf + LARGE_FILE_HEADER_SIZE,
        								LARGE_FILE_LEN_3) != 0) {
        fprintf(stderr,
            "SendFile 7. ERROR - large file data corruption\n");
        failed_already=1;
        return;
    }
#endif
	/*
	 * case 8: partial large file at non-zero, page-aligned offset, with
	 * header and trailer
	 */
    rlen = LARGE_FILE_LEN_4 + LARGE_FILE_HEADER_SIZE +
									LARGE_FILE_TRAILER_SIZE;
    if (readn(sockfd, large_buf, rlen) != rlen) {
        fprintf(stderr,
            "prsocket_test: SendFile_Client failed to receive file\n");
        failed_already=1;
        return;
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    if (memcmp(large_file_header, large_buf, LARGE_FILE_HEADER_SIZE) != 0){
        fprintf(stderr,
            "SendFile 2. ERROR - large file header corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp((char *)large_file_addr + LARGE_FILE_OFFSET_4,
				large_buf + LARGE_FILE_HEADER_SIZE,
        								LARGE_FILE_LEN_4) != 0) {
        fprintf(stderr,
            "SendFile 2. ERROR - large file data corruption\n");
        failed_already=1;
        return;
    }
    if (memcmp(large_file_trailer,
				large_buf + LARGE_FILE_HEADER_SIZE + LARGE_FILE_LEN_4,
        				LARGE_FILE_TRAILER_SIZE) != 0) {
        fprintf(stderr,
            "SendFile 2. ERROR - large file trailer corruption\n");
        failed_already=1;
        return;
    }
#endif
    PR_DELETE(small_buf);
    PR_DELETE(large_buf);
    PR_Close(sockfd);


    /*
     * Decrement exit_counter and notify parent thread
     */

    PR_EnterMonitor(cp->exit_mon);
    --(*cp->exit_counter);
    PR_Notify(cp->exit_mon);
    PR_ExitMonitor(cp->exit_mon);
    DPRINTF(("TransmitFile_Client [0x%lx] exiting\n", PR_GetCurrentThread()));
}

/*
 * Serve_TransmitFile_Client
 *    Thread, started by the server, for serving a client connection.
 *    Trasmits a small file, with a header, and a large file, without
 *    a header
 */
static void
Serve_TransmitFile_Client(void *arg)
{
    Serve_Client_Param *scp = (Serve_Client_Param *) arg;
    PRFileDesc *sockfd;
    PRInt32 bytes;
    PRFileDesc *local_small_file_fd=NULL;
    PRFileDesc *local_large_file_fd=NULL;
	PRSendFileData sfd;
	PRInt32 slen;

    sockfd = scp->sockfd;
    local_small_file_fd = PR_Open(SMALL_FILE_NAME, PR_RDONLY,0);

    if (local_small_file_fd == NULL) {
        fprintf(stderr,"prsocket_test failed to open file for transmitting %s\n",
            SMALL_FILE_NAME);
        failed_already=1;
        goto done;
    }
    local_large_file_fd = PR_Open(LARGE_FILE_NAME, PR_RDONLY,0);

    if (local_large_file_fd == NULL) {
        fprintf(stderr,"prsocket_test failed to open file for transmitting %s\n",
            LARGE_FILE_NAME);
        failed_already=1;
        goto done;
    }
    bytes = PR_TransmitFile(sockfd, local_small_file_fd, small_file_header,
        SMALL_FILE_HEADER_SIZE, PR_TRANSMITFILE_KEEP_OPEN,
        PR_INTERVAL_NO_TIMEOUT);
    if (bytes != (SMALL_FILE_SIZE+ SMALL_FILE_HEADER_SIZE)) {
        fprintf(stderr,
            "prsocet_test: PR_TransmitFile failed: (%ld, %ld)\n",
            PR_GetError(), PR_GetOSError());
        failed_already=1;
    }
    bytes = PR_TransmitFile(sockfd, local_large_file_fd, NULL, 0,
        PR_TRANSMITFILE_KEEP_OPEN, PR_INTERVAL_NO_TIMEOUT);
    if (bytes != LARGE_FILE_SIZE) {
        fprintf(stderr,
            "prsocket_test: PR_TransmitFile failed: (%ld, %ld)\n",
            PR_GetError(), PR_GetOSError());
        failed_already=1;
    }

	/*
	 * PR_SendFile test cases
	 */

	/*
	 * case 1: small file with header and trailer
	 */
	sfd.fd = local_small_file_fd;
	sfd.file_offset = 0;
	sfd.file_nbytes = 0;
	sfd.header = small_file_header;
	sfd.hlen = SMALL_FILE_HEADER_SIZE;
	sfd.trailer = small_file_trailer;
	sfd.tlen = SMALL_FILE_TRAILER_SIZE;
    bytes = PR_SendFile(sockfd, &sfd, PR_TRANSMITFILE_KEEP_OPEN,
        				PR_INTERVAL_NO_TIMEOUT);
    slen = SMALL_FILE_SIZE+ SMALL_FILE_HEADER_SIZE +
						SMALL_FILE_TRAILER_SIZE;
    if (bytes != slen) {
        fprintf(stderr,
			"socket: Error - 1. PR_SendFile  send_size = %d, bytes sent = %d\n",
									slen, bytes);
        fprintf(stderr,
            "prsocket_test: PR_SendFile failed: (%ld, %ld)\n",
            PR_GetError(), PR_GetOSError());
        failed_already=1;
    }

	/*
	 * case 2: partial large file at zero offset, file with header and trailer
	 */
	sfd.fd = local_large_file_fd;
	sfd.file_offset = 0;
	sfd.file_nbytes = LARGE_FILE_LEN_1;
	sfd.header = large_file_header;
	sfd.hlen = LARGE_FILE_HEADER_SIZE;
	sfd.trailer = large_file_trailer;
	sfd.tlen = LARGE_FILE_TRAILER_SIZE;
    bytes = PR_SendFile(sockfd, &sfd, PR_TRANSMITFILE_KEEP_OPEN,
        				PR_INTERVAL_NO_TIMEOUT);
    slen = LARGE_FILE_LEN_1 + LARGE_FILE_HEADER_SIZE +
						LARGE_FILE_TRAILER_SIZE;
    if (bytes != slen) {
        fprintf(stderr,
			"socket: Error - 2. PR_SendFile send_size = %d, bytes sent = %d\n",
									slen, bytes);
        fprintf(stderr,
            "prsocket_test: PR_SendFile failed: (%ld, %ld)\n",
            PR_GetError(), PR_GetOSError());
        failed_already=1;
    }
	/*
	 * case 3: partial small file at non-zero offset, with header
	 */
	sfd.fd = local_small_file_fd;
	sfd.file_offset = SMALL_FILE_OFFSET_1;
	sfd.file_nbytes = SMALL_FILE_LEN_1;
	sfd.header = small_file_header;
	sfd.hlen = SMALL_FILE_HEADER_SIZE;
	sfd.trailer = NULL;
	sfd.tlen = 0;
    bytes = PR_SendFile(sockfd, &sfd, PR_TRANSMITFILE_KEEP_OPEN,
        				PR_INTERVAL_NO_TIMEOUT);
    slen = SMALL_FILE_LEN_1 + SMALL_FILE_HEADER_SIZE;
    if (bytes != slen) {
        fprintf(stderr,
			"socket: Error - 3. PR_SendFile send_size = %d, bytes sent = %d\n",
									slen, bytes);
        fprintf(stderr,
            "prsocket_test: PR_SendFile failed: (%ld, %ld)\n",
            PR_GetError(), PR_GetOSError());
        failed_already=1;
    }
	/*
	 * case 4: partial small file at non-zero offset, with trailer
	 */
	sfd.fd = local_small_file_fd;
	sfd.file_offset = SMALL_FILE_OFFSET_2;
	sfd.file_nbytes = SMALL_FILE_LEN_2;
	sfd.header = NULL;
	sfd.hlen = 0;
	sfd.trailer = small_file_trailer;
	sfd.tlen = SMALL_FILE_TRAILER_SIZE;
    bytes = PR_SendFile(sockfd, &sfd, PR_TRANSMITFILE_KEEP_OPEN,
        				PR_INTERVAL_NO_TIMEOUT);
    slen = SMALL_FILE_LEN_2 + SMALL_FILE_TRAILER_SIZE;
    if (bytes != slen) {
        fprintf(stderr,
			"socket: Error - 4. PR_SendFile send_size = %d, bytes sent = %d\n",
									slen, bytes);
        fprintf(stderr,
            "prsocket_test: PR_SendFile failed: (%ld, %ld)\n",
            PR_GetError(), PR_GetOSError());
        failed_already=1;
    }
	/*
	 * case 5: partial large file at non-zero offset, file with header
	 */
	sfd.fd = local_large_file_fd;
	sfd.file_offset = LARGE_FILE_OFFSET_2;
	sfd.file_nbytes = LARGE_FILE_LEN_2;
	sfd.header = large_file_header;
	sfd.hlen = LARGE_FILE_HEADER_SIZE;
	sfd.trailer = NULL;
	sfd.tlen = 0;
    bytes = PR_SendFile(sockfd, &sfd, PR_TRANSMITFILE_KEEP_OPEN,
        				PR_INTERVAL_NO_TIMEOUT);
    slen = LARGE_FILE_LEN_2 + LARGE_FILE_HEADER_SIZE;
    if (bytes != slen) {
        fprintf(stderr,
			"socket: Error - 5. PR_SendFile send_size = %d, bytes sent = %d\n",
									slen, bytes);
        fprintf(stderr,
            "prsocket_test: PR_SendFile failed: (%ld, %ld)\n",
            PR_GetError(), PR_GetOSError());
        failed_already=1;
    }
	/*
	 * case 6: partial small file from non-zero offset till end of file, with header
	 */
	sfd.fd = local_small_file_fd;
	sfd.file_offset = SMALL_FILE_OFFSET_3;
	sfd.file_nbytes = 0;				/* data from offset to end-of-file */
	sfd.header = small_file_header;
	sfd.hlen = SMALL_FILE_HEADER_SIZE;
	sfd.trailer = NULL;
	sfd.tlen = 0;
    bytes = PR_SendFile(sockfd, &sfd, PR_TRANSMITFILE_KEEP_OPEN,
        				PR_INTERVAL_NO_TIMEOUT);
    slen = SMALL_FILE_LEN_3 + SMALL_FILE_HEADER_SIZE;
    if (bytes != slen) {
        fprintf(stderr,
			"socket: Error - 6. PR_SendFile send_size = %d, bytes sent = %d\n",
									slen, bytes);
        fprintf(stderr,
            "prsocket_test: PR_SendFile failed: (%ld, %ld)\n",
            PR_GetError(), PR_GetOSError());
        failed_already=1;
    }
	/*
	 * case 7: partial large file at non-zero offset till end-of-file, with header
	 */
	sfd.fd = local_large_file_fd;
	sfd.file_offset = LARGE_FILE_OFFSET_3;
	sfd.file_nbytes = 0;				/* data until end-of-file */
	sfd.header = large_file_header;
	sfd.hlen = LARGE_FILE_HEADER_SIZE;
	sfd.trailer = NULL;
	sfd.tlen = 0;
    bytes = PR_SendFile(sockfd, &sfd, PR_TRANSMITFILE_KEEP_OPEN,
        									PR_INTERVAL_NO_TIMEOUT);
    slen = LARGE_FILE_LEN_3 + LARGE_FILE_HEADER_SIZE;
    if (bytes != slen) {
        fprintf(stderr,
			"socket: Error - 7. PR_SendFile send_size = %d, bytes sent = %d\n",
									slen, bytes);
        fprintf(stderr,
            "prsocket_test: PR_SendFile failed: (%ld, %ld)\n",
            PR_GetError(), PR_GetOSError());
        failed_already=1;
    }
	/*
	 * case 8: partial large file at non-zero page-aligned offset,
	 * with header and trailer
	 */
	sfd.fd = local_large_file_fd;
	sfd.file_offset = LARGE_FILE_OFFSET_4;
	sfd.file_nbytes = LARGE_FILE_LEN_4;
	sfd.header = large_file_header;
	sfd.hlen = LARGE_FILE_HEADER_SIZE;
	sfd.trailer = large_file_trailer;
	sfd.tlen = LARGE_FILE_TRAILER_SIZE;
    bytes = PR_SendFile(sockfd, &sfd, PR_TRANSMITFILE_CLOSE_SOCKET,
        				PR_INTERVAL_NO_TIMEOUT);
    slen = LARGE_FILE_LEN_4 + LARGE_FILE_HEADER_SIZE +
						LARGE_FILE_TRAILER_SIZE;
    if (bytes != slen) {
        fprintf(stderr,
			"socket: Error - 2. PR_SendFile send_size = %d, bytes sent = %d\n",
									slen, bytes);
        fprintf(stderr,
            "prsocket_test: PR_SendFile failed: (%ld, %ld)\n",
            PR_GetError(), PR_GetOSError());
        failed_already=1;
    }
done:
    if (local_small_file_fd != NULL)
        PR_Close(local_small_file_fd);
    if (local_large_file_fd != NULL)
        PR_Close(local_large_file_fd);
}

/*
 * TransmitFile Server
 *    Server Thread
 *    Bind an address to a socket and listen for incoming connections
 *    Create worker threads to service clients
 */
static void
TransmitFile_Server(void *arg)
{
    PRThread **t = NULL;  /* an array of PRThread pointers */
    Server_Param *sp = (Server_Param *) arg;
    Serve_Client_Param *scp;
    PRFileDesc *sockfd = NULL, *newsockfd;
    PRNetAddr netaddr;
    PRInt32 i;

    t = (PRThread**)PR_MALLOC(num_transmitfile_clients * sizeof(PRThread *));
    if (t == NULL) {
        fprintf(stderr, "prsocket_test: run out of memory\n");
        failed_already=1;
        goto exit;
    }
    /*
     * Create a tcp socket
     */
    if ((sockfd = PR_OpenTCPSocket(PR_AF_INET)) == NULL) {
        fprintf(stderr,"prsocket_test: PR_OpenTCPSocket failed\n");
        failed_already=1;
        goto exit;
    }
    memset(&netaddr, 0 , sizeof(netaddr));
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
            if (i++ < SERVER_MAX_BIND_COUNT)
                continue;
        }
        fprintf(stderr,"prsocket_test: ERROR - PR_Bind failed\n");
        failed_already=1;
        perror("PR_Bind");
        goto exit;
    }

    if (PR_Listen(sockfd, 32) < 0) {
        fprintf(stderr,"prsocket_test: ERROR - PR_Listen failed\n");
        failed_already=1;
        goto exit;
    }

    if (PR_GetSockName(sockfd, &netaddr) < 0) {
        fprintf(stderr,
            "prsocket_test: ERROR - PR_GetSockName failed\n");
        failed_already=1;
        goto exit;
    }

    DPRINTF(("TCP_Server: PR_BIND netaddr.inet.ip = 0x%lx, netaddr.inet.port = %d\n",
        netaddr.inet.ip, netaddr.inet.port));
    tcp_server_addr.inet.family = netaddr.inet.family;
    tcp_server_addr.inet.port = netaddr.inet.port;
    tcp_server_addr.inet.ip = netaddr.inet.ip;

    /*
     * Wake up parent thread because server address is bound and made
     * available in the global variable 'tcp_server_addr'
     */
    PR_PostSem(sp->addr_sem);

    for (i = 0; i < num_transmitfile_clients ; i++) {
        /* test both null and non-null 'addr' argument to PR_Accept */
        PRNetAddr *addrp = (i%2 ? &netaddr: NULL);

        if ((newsockfd = PR_Accept(sockfd, addrp,
            PR_INTERVAL_NO_TIMEOUT)) == NULL) {
            fprintf(stderr,
                "prsocket_test: ERROR - PR_Accept failed\n");
            failed_already=1;
            goto exit;
        }
        /* test both regular and emulated PR_SendFile */
        if (i%2) {
            PRFileDesc *layer = PR_CreateIOLayerStub(
                emuSendFileIdentity, &emuSendFileMethods);
            if (layer == NULL) {
                fprintf(stderr,
                    "prsocket_test: ERROR - PR_CreateIOLayerStub failed\n");
                failed_already=1;
                goto exit;
            }
            if (PR_PushIOLayer(newsockfd, PR_TOP_IO_LAYER, layer)
                    == PR_FAILURE) {
                fprintf(stderr,
                    "prsocket_test: ERROR - PR_PushIOLayer failed\n");
                failed_already=1;
                goto exit;
            }
        }
        scp = PR_NEW(Serve_Client_Param);
        if (scp == NULL) {
            fprintf(stderr,"prsocket_test: PR_NEW failed\n");
            failed_already=1;
            goto exit;
        }

        /*
         * Start a Serve_Client thread for each incoming connection
         */
        scp->sockfd = newsockfd;
        scp->datalen = sp->datalen;

        t[i] = PR_CreateThread(PR_USER_THREAD,
            Serve_TransmitFile_Client, (void *)scp, 
            PR_PRIORITY_NORMAL,
            PR_LOCAL_THREAD,
            PR_JOINABLE_THREAD,
            0);
        if (t[i] == NULL) {
            fprintf(stderr,
                "prsocket_test: PR_CreateThread failed\n");
            failed_already=1;
            goto exit;
        }
        DPRINTF(("TransmitFile_Server: Created Serve_TransmitFile_Client = 0x%lx\n", t));
    }

    /*
     * Wait for all the worker threads to end, so that we know
     * they are no longer using the small and large file fd's.
     */

    for (i = 0; i < num_transmitfile_clients; i++) {
        PR_JoinThread(t[i]);
    }

exit:
    if (t) {
        PR_DELETE(t);
    }
    if (sockfd) {
        PR_Close(sockfd);
    }

    /*
     * Decrement exit_counter and notify parent thread
     */

    PR_EnterMonitor(sp->exit_mon);
    --(*sp->exit_counter);
    PR_Notify(sp->exit_mon);
    PR_ExitMonitor(sp->exit_mon);
    DPRINTF(("TransmitFile_Server [0x%lx] exiting\n", PR_GetCurrentThread()));
}

/*
 * Socket_Misc_Test    - test miscellaneous functions 
 *    
 */
static PRInt32
Socket_Misc_Test(void)
{
    PRIntn i, rv = 0, bytes, count, len;
    PRThread *t;
    PRSemaphore *server_sem;
    Server_Param *sparamp;
    Client_Param *cparamp;
    PRMonitor *mon2;
    PRInt32    datalen;

    /*
 * We deliberately pick a buffer size that is not a nice multiple
 * of 1024.
 */
#define TRANSMITFILE_BUF_SIZE    (4 * 1024 - 11)

    typedef struct {
        char    data[TRANSMITFILE_BUF_SIZE];
    } file_buf;
    file_buf *buf = NULL;

    /*
     * create file(s) to be transmitted
     */
    if ((PR_MkDir(TEST_DIR, 0777)) < 0) {
        printf("prsocket_test failed to create dir %s\n",TEST_DIR);
        failed_already=1;
        return -1;
    }

    small_file_fd = PR_Open(SMALL_FILE_NAME, PR_RDWR | PR_CREATE_FILE,0777);

    if (small_file_fd == NULL) {
        fprintf(stderr,"prsocket_test failed to create/open file %s\n",
            SMALL_FILE_NAME);
        failed_already=1;
        rv = -1;
        goto done;
    }
    buf = PR_NEW(file_buf);
    if (buf == NULL) {
        fprintf(stderr,"prsocket_test failed to allocate buffer\n");
        failed_already=1;
        rv = -1;
        goto done;
    }
    /*
     * fill in random data
     */
    for (i = 0; i < TRANSMITFILE_BUF_SIZE; i++) {
        buf->data[i] = i;
    }
    count = 0;
    do {
        len = (SMALL_FILE_SIZE - count) > TRANSMITFILE_BUF_SIZE ?
            TRANSMITFILE_BUF_SIZE : (SMALL_FILE_SIZE - count);
        bytes = PR_Write(small_file_fd, buf->data, len);
        if (bytes <= 0) {
            fprintf(stderr,
                "prsocket_test failed to write to file %s\n",
                SMALL_FILE_NAME);
            failed_already=1;
            rv = -1;
            goto done;
        }
        count += bytes;
    } while (count < SMALL_FILE_SIZE);
#ifdef XP_UNIX
    /*
     * map the small file; used in checking for data corruption
     */
    small_file_addr = mmap(0, SMALL_FILE_SIZE, PROT_READ,
        MAP_SHARED, small_file_fd->secret->md.osfd, 0);
    if (small_file_addr == (void *) -1) {
        fprintf(stderr,"prsocket_test failed to mmap file %s\n",
            SMALL_FILE_NAME);
        failed_already=1;
        rv = -1;
        goto done;
    }
#endif
    /*
     * header for small file
     */
    small_file_header = PR_MALLOC(SMALL_FILE_HEADER_SIZE);
    if (small_file_header == NULL) {
        fprintf(stderr,"prsocket_test failed to malloc header file\n");
        failed_already=1;
        rv = -1;
        goto done;
    }
    memset(small_file_header, (int) PR_IntervalNow(),
        SMALL_FILE_HEADER_SIZE);
    /*
     * trailer for small file
     */
    small_file_trailer = PR_MALLOC(SMALL_FILE_TRAILER_SIZE);
    if (small_file_trailer == NULL) {
        fprintf(stderr,"prsocket_test failed to malloc header trailer\n");
        failed_already=1;
        rv = -1;
        goto done;
    }
    memset(small_file_trailer, (int) PR_IntervalNow(),
        SMALL_FILE_TRAILER_SIZE);
    /*
     * setup large file
     */
    large_file_fd = PR_Open(LARGE_FILE_NAME, PR_RDWR | PR_CREATE_FILE,0777);

    if (large_file_fd == NULL) {
        fprintf(stderr,"prsocket_test failed to create/open file %s\n",
            LARGE_FILE_NAME);
        failed_already=1;
        rv = -1;
        goto done;
    }
    /*
     * fill in random data
     */
    for (i = 0; i < TRANSMITFILE_BUF_SIZE; i++) {
        buf->data[i] = i;
    }
    count = 0;
    do {
        len = (LARGE_FILE_SIZE - count) > TRANSMITFILE_BUF_SIZE ?
            TRANSMITFILE_BUF_SIZE : (LARGE_FILE_SIZE - count);
        bytes = PR_Write(large_file_fd, buf->data, len);
        if (bytes <= 0) {
            fprintf(stderr,
                "prsocket_test failed to write to file %s: (%ld, %ld)\n",
                LARGE_FILE_NAME,
                PR_GetError(), PR_GetOSError());
            failed_already=1;
            rv = -1;
            goto done;
        }
        count += bytes;
    } while (count < LARGE_FILE_SIZE);
#if defined(XP_UNIX) && !defined(SYMBIAN)
    /*
     * map the large file; used in checking for data corruption
     */
    large_file_addr = mmap(0, LARGE_FILE_SIZE, PROT_READ,
        MAP_SHARED, large_file_fd->secret->md.osfd, 0);
    if (large_file_addr == (void *) -1) {
        fprintf(stderr,"prsocket_test failed to mmap file %s\n",
            LARGE_FILE_NAME);
        failed_already=1;
        rv = -1;
        goto done;
    }
#endif
    /*
     * header for large file
     */
    large_file_header = PR_MALLOC(LARGE_FILE_HEADER_SIZE);
    if (large_file_header == NULL) {
        fprintf(stderr,"prsocket_test failed to malloc header file\n");
        failed_already=1;
        rv = -1;
        goto done;
    }
    memset(large_file_header, (int) PR_IntervalNow(),
        LARGE_FILE_HEADER_SIZE);
    /*
     * trailer for large file
     */
    large_file_trailer = PR_MALLOC(LARGE_FILE_TRAILER_SIZE);
    if (large_file_trailer == NULL) {
        fprintf(stderr,"prsocket_test failed to malloc header trailer\n");
        failed_already=1;
        rv = -1;
        goto done;
    }
    memset(large_file_trailer, (int) PR_IntervalNow(),
        LARGE_FILE_TRAILER_SIZE);

    datalen = tcp_mesg_size;
    thread_count = 0;
    /*
     * start the server thread
     */
    sparamp = PR_NEW(Server_Param);
    if (sparamp == NULL) {
        fprintf(stderr,"prsocket_test: PR_NEW failed\n");
        failed_already=1;
        rv = -1;
        goto done;
    }
    server_sem = PR_NewSem(0);
    if (server_sem == NULL) {
        fprintf(stderr,"prsocket_test: PR_NewSem failed\n");
        failed_already=1;
        rv = -1;
        goto done;
    }
    mon2 = PR_NewMonitor();
    if (mon2 == NULL) {
        fprintf(stderr,"prsocket_test: PR_NewMonitor failed\n");
        failed_already=1;
        rv = -1;
        goto done;
    }
    PR_EnterMonitor(mon2);

    sparamp->addr_sem = server_sem;
    sparamp->exit_mon = mon2;
    sparamp->exit_counter = &thread_count;
    sparamp->datalen = datalen;
    t = PR_CreateThread(PR_USER_THREAD,
        TransmitFile_Server, (void *)sparamp, 
        PR_PRIORITY_NORMAL,
        PR_LOCAL_THREAD,
        PR_UNJOINABLE_THREAD,
        0);
    if (t == NULL) {
        fprintf(stderr,"prsocket_test: PR_CreateThread failed\n");
        failed_already=1;
        rv = -1;
        goto done;
    }
    DPRINTF(("Created TCP server = 0x%x\n", t));
    thread_count++;

    /*
     * wait till the server address is setup
     */
    PR_WaitSem(server_sem);

    /*
     * Now start a bunch of client threads
     */

    cparamp = PR_NEW(Client_Param);
    if (cparamp == NULL) {
        fprintf(stderr,"prsocket_test: PR_NEW failed\n");
        failed_already=1;
        rv = -1;
        goto done;
    }
    cparamp->server_addr = tcp_server_addr;
    cparamp->server_addr.inet.ip = PR_htonl(PR_INADDR_LOOPBACK);
    cparamp->exit_mon = mon2;
    cparamp->exit_counter = &thread_count;
    cparamp->datalen = datalen;
    for (i = 0; i < num_transmitfile_clients; i++) {
        t = create_new_thread(PR_USER_THREAD,
            TransmitFile_Client, (void *) cparamp,
            PR_PRIORITY_NORMAL,
            PR_LOCAL_THREAD,
            PR_UNJOINABLE_THREAD,
            0, i);
        if (t == NULL) {
            fprintf(stderr,"prsocket_test: PR_CreateThread failed\n");
            rv = -1;
            failed_already=1;
            goto done;
        }
        DPRINTF(("Created TransmitFile client = 0x%lx\n", t));
        thread_count++;
    }
    /* Wait for server and client threads to exit */
    while (thread_count) {
        PR_Wait(mon2, PR_INTERVAL_NO_TIMEOUT);
        DPRINTF(("Socket_Misc_Test - thread_count  = %d\n", thread_count));
    }
    PR_ExitMonitor(mon2);
done:
    if (buf) {
        PR_DELETE(buf);
    }
#if defined(XP_UNIX) && !defined(SYMBIAN)
    munmap((char*)small_file_addr, SMALL_FILE_SIZE);
    munmap((char*)large_file_addr, LARGE_FILE_SIZE);
#endif
    PR_Close(small_file_fd);
    PR_Close(large_file_fd);
    if ((PR_Delete(SMALL_FILE_NAME)) == PR_FAILURE) {
        fprintf(stderr,"prsocket_test: failed to unlink file %s\n",
            SMALL_FILE_NAME);
        failed_already=1;
    }
    if ((PR_Delete(LARGE_FILE_NAME)) == PR_FAILURE) {
        fprintf(stderr,"prsocket_test: failed to unlink file %s\n",
            LARGE_FILE_NAME);
        failed_already=1;
    }
    if ((PR_RmDir(TEST_DIR)) == PR_FAILURE) {
        fprintf(stderr,"prsocket_test failed to rmdir %s: (%ld, %ld)\n",
            TEST_DIR, PR_GetError(), PR_GetOSError());
        failed_already=1;
    }

    printf("%-29s%s","Socket_Misc_Test",":");
    printf("%2d Server %2d Clients\n",1, num_transmitfile_clients);
    printf("%30s Sizes of Transmitted Files  - %4d KB, %2d MB \n",":",
        SMALL_FILE_SIZE/1024, LARGE_FILE_SIZE/(1024 * 1024));


    return rv;
}
/************************************************************************/

/*
 * Test Socket NSPR APIs
 */

int main(int argc, char **argv)
{
    /*
     * -d           debug mode
     */

    PLOptStatus os;
    PLOptState *opt = PL_CreateOptState(argc, argv, "d");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
            _debug_on = 1;
            break;
        default:
            break;
        }
    }
    PL_DestroyOptState(opt);

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

    PR_SetConcurrency(4);

    emuSendFileIdentity = PR_GetUniqueIdentity("Emulated SendFile");
    emuSendFileMethods = *PR_GetDefaultIOMethods();
    emuSendFileMethods.transmitfile = emu_TransmitFile;
    emuSendFileMethods.sendfile = emu_SendFile;

    /*
     * run client-server test with TCP, Ipv4-Ipv4
     */
	printf("TCP Client/Server Test - IPv4/Ipv4\n");
    if (TCP_Socket_Client_Server_Test() < 0) {
        printf("TCP_Socket_Client_Server_Test failed\n");
        goto done;
    } else
        printf("TCP_Socket_Client_Server_Test Passed\n");
    /*
     * client-server test, Ipv6-Ipv4
     */
	client_domain = PR_AF_INET6;
	printf("TCP Client/Server Test - IPv6/Ipv4\n");
    if (TCP_Socket_Client_Server_Test() < 0) {
        printf("TCP_Socket_Client_Server_Test failed\n");
        goto done;
    } else
        printf("TCP_Socket_Client_Server_Test Passed\n");
    /*
     * client-server test, Ipv4-Ipv6
     */
	client_domain = PR_AF_INET;
	server_domain = PR_AF_INET6;
	printf("TCP Client/Server Test - IPv4/Ipv6\n");
    if (TCP_Socket_Client_Server_Test() < 0) {
        printf("TCP_Socket_Client_Server_Test failed\n");
        goto done;
    } else
        printf("TCP_Socket_Client_Server_Test Passed\n");
    /*
     * client-server test, Ipv6-Ipv6
     */
	client_domain = PR_AF_INET6;
	server_domain = PR_AF_INET6;
	printf("TCP Client/Server Test - IPv6/Ipv6\n");
    if (TCP_Socket_Client_Server_Test() < 0) {
        printf("TCP_Socket_Client_Server_Test failed\n");
        goto done;
    } else
        printf("TCP_Socket_Client_Server_Test Passed\n");
	test_cancelio = 0;
	
#if defined(SYMBIAN) && !defined(__WINSCW__)
	/* UDP tests only run on Symbian devices but not emulator */
    /*
     * run client-server test with UDP, IPv4/IPv4
     */
	printf("UDP Client/Server Test - IPv4/Ipv4\n");
	client_domain = PR_AF_INET;
	server_domain = PR_AF_INET;
    if (UDP_Socket_Client_Server_Test() < 0) {
        printf("UDP_Socket_Client_Server_Test failed\n");
        goto done;
    } else
        printf("UDP_Socket_Client_Server_Test Passed\n");
    /*
     * run client-server test with UDP, IPv6/IPv4
     */
	printf("UDP Client/Server Test - IPv6/Ipv4\n");
	client_domain = PR_AF_INET6;
	server_domain = PR_AF_INET;
    if (UDP_Socket_Client_Server_Test() < 0) {
        printf("UDP_Socket_Client_Server_Test failed\n");
        goto done;
    } else
        printf("UDP_Socket_Client_Server_Test Passed\n");
    /*
     * run client-server test with UDP,IPv4-IPv6
     */
	printf("UDP Client/Server Test - IPv4/Ipv6\n");
	client_domain = PR_AF_INET;
	server_domain = PR_AF_INET6;
    if (UDP_Socket_Client_Server_Test() < 0) {
        printf("UDP_Socket_Client_Server_Test failed\n");
        goto done;
    } else
        printf("UDP_Socket_Client_Server_Test Passed\n");
    /*
     * run client-server test with UDP,IPv6-IPv6
     */
	printf("UDP Client/Server Test - IPv6/Ipv6\n");
	client_domain = PR_AF_INET6;
	server_domain = PR_AF_INET6;
    if (UDP_Socket_Client_Server_Test() < 0) {
        printf("UDP_Socket_Client_Server_Test failed\n");
        goto done;
    } else
        printf("UDP_Socket_Client_Server_Test Passed\n");
#endif
    
    /*
     * Misc socket tests - including transmitfile, etc.
     */

    /* File transmission test can not be done in Symbian OS because of 
     * large file's size and the incomplete mmap() implementation. */
#if !defined(WIN16) && !defined(SYMBIAN)
    /*
** The 'transmit file' test does not run because
** transmit file is not implemented in NSPR yet.
**
*/
    if (Socket_Misc_Test() < 0) {
        printf("Socket_Misc_Test failed\n");
        failed_already=1;
        goto done;
    } else
        printf("Socket_Misc_Test passed\n");

    /*
     * run client-server test with TCP again to test
     * recycling used sockets from PR_TransmitFile().
     */
    if (TCP_Socket_Client_Server_Test() < 0) {
        printf("TCP_Socket_Client_Server_Test failed\n");
        goto done;
    } else
        printf("TCP_Socket_Client_Server_Test Passed\n");
#endif

done:
    PR_Cleanup();
    if (failed_already) return 1;
    else return 0;
}
