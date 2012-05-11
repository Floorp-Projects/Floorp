/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: thrpool_client.c
**
** Description: Test threadpool functionality.
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
static int server_port = -1;
static char *program_name = NULL;

#include "obsolete/prsem.h"

#ifdef XP_PC
#define mode_t int
#endif

#define DPRINTF(arg) if (_debug_on) printf arg

#define    BUF_DATA_SIZE    (2 * 1024)
#define TCP_MESG_SIZE    1024
#define NUM_TCP_CLIENTS            10	/* for a listen queue depth of 5 */

#define NUM_TCP_CONNECTIONS_PER_CLIENT    10
#define NUM_TCP_MESGS_PER_CONNECTION    10
#define TCP_SERVER_PORT            10000

static PRInt32 num_tcp_clients = NUM_TCP_CLIENTS;
static PRInt32 num_tcp_connections_per_client = NUM_TCP_CONNECTIONS_PER_CLIENT;
static PRInt32 tcp_mesg_size = TCP_MESG_SIZE;
static PRInt32 num_tcp_mesgs_per_connection = NUM_TCP_MESGS_PER_CONNECTION;

int failed_already=0;

typedef struct buffer {
    char    data[BUF_DATA_SIZE];
} buffer;

PRNetAddr tcp_server_addr, udp_server_addr;

typedef struct Client_Param {
    PRNetAddr server_addr;
    PRMonitor *exit_mon;    /* monitor to signal on exit */
    PRInt32 *exit_counter;    /* counter to decrement, before exit */
    PRInt32    datalen;
} Client_Param;

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
	PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT;

    for (rem=len; rem; offset += bytes, rem -= bytes) {
        DPRINTF(("thread = 0x%lx: calling PR_Recv, bytes = %d\n",
            PR_GetCurrentThread(), rem));
        bytes = PR_Recv(sockfd, buf + offset, rem, 0,
            	timeout);
        DPRINTF(("thread = 0x%lx: returning from PR_Recv, bytes = %d\n",
            PR_GetCurrentThread(), bytes));
        if (bytes < 0) {
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
 * TCP_Client
 *    Client job
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


    DPRINTF(("TCP client started\n"));
    bytes = cp->datalen;
    out_buf = PR_NEW(buffer);
    if (out_buf == NULL) {
        fprintf(stderr,"%s: failed to alloc buffer struct\n", program_name);
        failed_already=1;
        return;
    }
    in_buf = PR_NEW(buffer);
    if (in_buf == NULL) {
        fprintf(stderr,"%s: failed to alloc buffer struct\n", program_name);
        failed_already=1;
        return;
    }
    netaddr.inet.family = cp->server_addr.inet.family;
    netaddr.inet.port = cp->server_addr.inet.port;
    netaddr.inet.ip = cp->server_addr.inet.ip;

    for (i = 0; i < num_tcp_connections_per_client; i++) {
        if ((sockfd = PR_OpenTCPSocket(PR_AF_INET)) == NULL) {
            fprintf(stderr,"%s: PR_OpenTCPSocket failed\n", program_name);
            failed_already=1;
            return;
        }

        DPRINTF(("TCP client connecting to server:%d\n", server_port));
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
            if (writen(sockfd, out_buf->data, bytes) < bytes) {
                fprintf(stderr,"%s: ERROR - TCP_Client:writen\n", program_name);
                failed_already=1;
                return;
            }
			/*
            DPRINTF(("TCP Client [0x%lx]: out_buf = 0x%lx out_buf[0] = 0x%lx\n",
                PR_GetCurrentThread(), out_buf, (*((int *) out_buf->data))));
			*/
            if (readn(sockfd, in_buf->data, bytes) < bytes) {
                fprintf(stderr,"%s: ERROR - TCP_Client:readn\n", program_name);
                failed_already=1;
                return;
            }
            /*
             * verify the data read
             */
            if (memcmp(in_buf->data, out_buf->data, bytes) != 0) {
                fprintf(stderr,"%s: ERROR - data corruption\n", program_name);
                failed_already=1;
                return;
            }
        }
        /*
         * shutdown reads and writes
         */
        if (PR_Shutdown(sockfd, PR_SHUTDOWN_BOTH) < 0) {
            fprintf(stderr,"%s: ERROR - PR_Shutdown\n", program_name);
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
    DPRINTF(("TCP_Client exiting\n"));
}

/*
 * TCP_Socket_Client_Server_Test    - concurrent server test
 *    
 *    Each client connects to the server and sends a chunk of data
 *    For each connection, server reads the data
 *    from the client and sends it back to the client, unmodified.
 *    Each client checks that data received from server is same as the
 *    data it sent to the server.
 *
 */

static PRInt32
TCP_Socket_Client_Server_Test(void)
{
    int i;
    Client_Param *cparamp;
    PRMonitor *mon2;
    PRInt32    datalen;
    PRInt32    connections = 0;
	PRThread *thr;

    datalen = tcp_mesg_size;
    connections = 0;

    mon2 = PR_NewMonitor();
    if (mon2 == NULL) {
        fprintf(stderr,"%s: PR_NewMonitor failed\n", program_name);
        failed_already=1;
        return -1;
    }

    /*
     * Start client jobs
     */
    cparamp = PR_NEW(Client_Param);
    if (cparamp == NULL) {
        fprintf(stderr,"%s: PR_NEW failed\n", program_name);
        failed_already=1;
        return -1;
    }
    cparamp->server_addr.inet.family = PR_AF_INET;
    cparamp->server_addr.inet.port = PR_htons(server_port);
    cparamp->server_addr.inet.ip = PR_htonl(PR_INADDR_LOOPBACK);
    cparamp->exit_mon = mon2;
    cparamp->exit_counter = &connections;
    cparamp->datalen = datalen;
    for (i = 0; i < num_tcp_clients; i++) {
		thr = PR_CreateThread(PR_USER_THREAD, TCP_Client, (void *)cparamp,
        		PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_UNJOINABLE_THREAD, 0);
        if (NULL == thr) {
            fprintf(stderr,"%s: PR_CreateThread failed\n", program_name);
            failed_already=1;
            return -1;
        }
    	PR_EnterMonitor(mon2);
        connections++;
    	PR_ExitMonitor(mon2);
        DPRINTF(("Created TCP client = 0x%lx\n", thr));
    }
    /* Wait for client jobs to exit */
    PR_EnterMonitor(mon2);
    while (0 != connections) {
        PR_Wait(mon2, PR_INTERVAL_NO_TIMEOUT);
        DPRINTF(("Client job count = %d\n", connections));
    }
    PR_ExitMonitor(mon2);
    printf("%30s","TCP_Socket_Client_Server_Test:");
    printf("%2ld Server %2ld Clients %2ld connections_per_client\n",1l,
        num_tcp_clients, num_tcp_connections_per_client);
    printf("%30s %2ld messages_per_connection %4ld bytes_per_message\n",":",
        num_tcp_mesgs_per_connection, tcp_mesg_size);

    PR_DELETE(cparamp);
    return 0;
}

/************************************************************************/

int main(int argc, char **argv)
{
    /*
     * -d           debug mode
     */
    PLOptStatus os;
    PLOptState *opt;
	program_name = argv[0];

    opt = PL_CreateOptState(argc, argv, "dp:");
    while (PL_OPT_EOL != (os = PL_GetNextOpt(opt)))
    {
        if (PL_OPT_BAD == os) continue;
        switch (opt->option)
        {
        case 'd':  /* debug mode */
            _debug_on = 1;
            break;
        case 'p':
            server_port = atoi(opt->value);
            break;
        default:
            break;
        }
    }
    PL_DestroyOptState(opt);

    PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

    PR_SetConcurrency(4);

	TCP_Socket_Client_Server_Test();

    PR_Cleanup();
    if (failed_already)
		return 1;
    else
		return 0;
}
