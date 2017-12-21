/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/***********************************************************************
**
** Name: thrpool.c
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
#if defined(_PR_PTHREADS)
#include <pthread.h>
#endif

/* for getcwd */
#if defined(XP_UNIX) || defined (XP_OS2) || defined(XP_BEOS)
#include <unistd.h>
#elif defined(XP_PC)
#include <direct.h>
#endif

#ifdef WIN32
#include <process.h>
#endif

static int _debug_on = 0;
static char *program_name = NULL;
static void serve_client_write(void *arg);

#include "obsolete/prsem.h"

#ifdef XP_PC
#define mode_t int
#endif

#define DPRINTF(arg) if (_debug_on) printf arg


#define BUF_DATA_SIZE    (2 * 1024)
#define TCP_MESG_SIZE    1024
#define NUM_TCP_CLIENTS  10	/* for a listen queue depth of 5 */


#define NUM_TCP_CONNECTIONS_PER_CLIENT  10
#define NUM_TCP_MESGS_PER_CONNECTION    10
#define TCP_SERVER_PORT            		10000
#define SERVER_MAX_BIND_COUNT        	100

#ifdef WINCE
char *getcwd(char *buf, size_t size)
{
    wchar_t wpath[MAX_PATH];
    _wgetcwd(wpath, MAX_PATH);
    WideCharToMultiByte(CP_ACP, 0, wpath, -1, buf, size, 0, 0);
}
 
#define perror(s)
#endif

static PRInt32 num_tcp_clients = NUM_TCP_CLIENTS;
static PRInt32 num_tcp_connections_per_client = NUM_TCP_CONNECTIONS_PER_CLIENT;
static PRInt32 tcp_mesg_size = TCP_MESG_SIZE;
static PRInt32 num_tcp_mesgs_per_connection = NUM_TCP_MESGS_PER_CONNECTION;
static void TCP_Server_Accept(void *arg);


int failed_already=0;
typedef struct buffer {
    char    data[BUF_DATA_SIZE];
} buffer;


typedef struct Server_Param {
    PRJobIoDesc iod;    /* socket to read from/write to    */
    PRInt32    	datalen;    /* bytes of data transfered in each read/write */
    PRNetAddr	netaddr;
    PRMonitor	*exit_mon;    /* monitor to signal on exit            */
    PRInt32		*job_counterp;    /* counter to decrement, before exit        */
    PRInt32		conn_counter;    /* counter to decrement, before exit        */
	PRThreadPool *tp;
} Server_Param;

typedef struct Serve_Client_Param {
    PRJobIoDesc iod;    /* socket to read from/write to    */
    PRInt32    datalen;    /* bytes of data transfered in each read/write */
    PRMonitor *exit_mon;    /* monitor to signal on exit            */
    PRInt32 *job_counterp;    /* counter to decrement, before exit        */
	PRThreadPool *tp;
} Serve_Client_Param;

typedef struct Session {
    PRJobIoDesc iod;    /* socket to read from/write to    */
	buffer 	*in_buf;
	PRInt32 bytes;
	PRInt32 msg_num;
	PRInt32 bytes_read;
    PRMonitor *exit_mon;    /* monitor to signal on exit            */
    PRInt32 *job_counterp;    /* counter to decrement, before exit        */
	PRThreadPool *tp;
} Session;

static void
serve_client_read(void *arg)
{
	Session *sp = (Session *) arg;
    int rem;
    int bytes;
    int offset;
	PRFileDesc *sockfd;
	char *buf;
	PRJob *jobp;

	PRIntervalTime timeout = PR_INTERVAL_NO_TIMEOUT;

	sockfd = sp->iod.socket;
	buf = sp->in_buf->data;

    PR_ASSERT(sp->msg_num < num_tcp_mesgs_per_connection);
	PR_ASSERT(sp->bytes_read < sp->bytes);

	offset = sp->bytes_read;
	rem = sp->bytes - offset;
	bytes = PR_Recv(sockfd, buf + offset, rem, 0, timeout);
	if (bytes < 0) {
		return;
	}
	sp->bytes_read += bytes;
	sp->iod.timeout = PR_SecondsToInterval(60);
	if (sp->bytes_read <  sp->bytes) {
		jobp = PR_QueueJob_Read(sp->tp, &sp->iod, serve_client_read, sp,
							PR_FALSE);
		PR_ASSERT(NULL != jobp);
		return;
	}
	PR_ASSERT(sp->bytes_read == sp->bytes);
	DPRINTF(("serve_client: read complete, msg(%d) \n", sp->msg_num));

	sp->iod.timeout = PR_SecondsToInterval(60);
	jobp = PR_QueueJob_Write(sp->tp, &sp->iod, serve_client_write, sp,
							PR_FALSE);
	PR_ASSERT(NULL != jobp);

    return;
}

static void
serve_client_write(void *arg)
{
	Session *sp = (Session *) arg;
    int bytes;
	PRFileDesc *sockfd;
	char *buf;
	PRJob *jobp;

	sockfd = sp->iod.socket;
	buf = sp->in_buf->data;

    PR_ASSERT(sp->msg_num < num_tcp_mesgs_per_connection);

	bytes = PR_Send(sockfd, buf, sp->bytes, 0, PR_INTERVAL_NO_TIMEOUT);
	PR_ASSERT(bytes == sp->bytes);

	if (bytes < 0) {
		return;
	}
	DPRINTF(("serve_client: write complete, msg(%d) \n", sp->msg_num));
    sp->msg_num++;
    if (sp->msg_num < num_tcp_mesgs_per_connection) {
		sp->bytes_read = 0;
		sp->iod.timeout = PR_SecondsToInterval(60);
		jobp = PR_QueueJob_Read(sp->tp, &sp->iod, serve_client_read, sp,
							PR_FALSE);
		PR_ASSERT(NULL != jobp);
		return;
	}

	DPRINTF(("serve_client: read/write complete, msg(%d) \n", sp->msg_num));
    if (PR_Shutdown(sockfd, PR_SHUTDOWN_BOTH) < 0) {
        fprintf(stderr,"%s: ERROR - PR_Shutdown\n", program_name);
    }

    PR_Close(sockfd);
    PR_EnterMonitor(sp->exit_mon);
    --(*sp->job_counterp);
    PR_Notify(sp->exit_mon);
    PR_ExitMonitor(sp->exit_mon);

    PR_DELETE(sp->in_buf);
    PR_DELETE(sp);

    return;
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
    buffer *in_buf;
	Session *sp;
	PRJob *jobp;

	sp = PR_NEW(Session);
	sp->iod = scp->iod;

    in_buf = PR_NEW(buffer);
    if (in_buf == NULL) {
        fprintf(stderr,"%s: failed to alloc buffer struct\n",program_name);
        failed_already=1;
        return;
    }

	sp->in_buf = in_buf;
	sp->bytes = scp->datalen;
	sp->msg_num = 0;
	sp->bytes_read = 0;
	sp->tp = scp->tp;
   	sp->exit_mon = scp->exit_mon;
    sp->job_counterp = scp->job_counterp;

	sp->iod.timeout = PR_SecondsToInterval(60);
	jobp = PR_QueueJob_Read(sp->tp, &sp->iod, serve_client_read, sp,
							PR_FALSE);
	PR_ASSERT(NULL != jobp);
	PR_DELETE(scp);
}

static void
print_stats(void *arg)
{
    Server_Param *sp = (Server_Param *) arg;
    PRThreadPool *tp = sp->tp;
    PRInt32 counter;
	PRJob *jobp;

	PR_EnterMonitor(sp->exit_mon);
	counter = (*sp->job_counterp);
	PR_ExitMonitor(sp->exit_mon);

	printf("PRINT_STATS: #client connections = %d\n",counter);


	jobp = PR_QueueJob_Timer(tp, PR_MillisecondsToInterval(500),
						print_stats, sp, PR_FALSE);

	PR_ASSERT(NULL != jobp);
}

static int job_counter = 0;
/*
 * TCP Server
 *    Server binds an address to a socket, starts a client process and
 *	  listens for incoming connections.
 *    Each client connects to the server and sends a chunk of data
 *    Starts a Serve_Client job for each incoming connection, to read
 *    the data from the client and send it back to the client, unmodified.
 *    Each client checks that data received from server is same as the
 *    data it sent to the server.
 *	  Finally, the threadpool is shutdown
 */
static void PR_CALLBACK
TCP_Server(void *arg)
{
    PRThreadPool *tp = (PRThreadPool *) arg;
    Server_Param *sp;
    PRFileDesc *sockfd;
    PRNetAddr netaddr;
	PRMonitor *sc_mon;
	PRJob *jobp;
	int i;
	PRStatus rval;

    /*
     * Create a tcp socket
     */
    if ((sockfd = PR_NewTCPSocket()) == NULL) {
        fprintf(stderr,"%s: PR_NewTCPSocket failed\n", program_name);
        return;
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
        fprintf(stderr,"%s: ERROR - PR_Bind failed\n", program_name);
        perror("PR_Bind");
        failed_already=1;
        return;
    }

    if (PR_Listen(sockfd, 32) < 0) {
        fprintf(stderr,"%s: ERROR - PR_Listen failed\n", program_name);
        failed_already=1;
        return;
    }

    if (PR_GetSockName(sockfd, &netaddr) < 0) {
        fprintf(stderr,"%s: ERROR - PR_GetSockName failed\n", program_name);
        failed_already=1;
        return;
    }

    DPRINTF((
	"TCP_Server: PR_BIND netaddr.inet.ip = 0x%lx, netaddr.inet.port = %d\n",
        netaddr.inet.ip, netaddr.inet.port));

	sp = PR_NEW(Server_Param);
	if (sp == NULL) {
		fprintf(stderr,"%s: PR_NEW failed\n", program_name);
		failed_already=1;
		return;
	}
	sp->iod.socket = sockfd;
	sp->iod.timeout = PR_SecondsToInterval(60);
	sp->datalen = tcp_mesg_size;
	sp->exit_mon = sc_mon;
	sp->job_counterp = &job_counter;
	sp->conn_counter = 0;
	sp->tp = tp;
	sp->netaddr = netaddr;

	/* create and cancel an io job */
	jobp = PR_QueueJob_Accept(tp, &sp->iod, TCP_Server_Accept, sp,
							PR_FALSE);
	PR_ASSERT(NULL != jobp);
	rval = PR_CancelJob(jobp);
	PR_ASSERT(PR_SUCCESS == rval);

	/*
	 * create the client process
	 */
	{
#define MAX_ARGS 4
		char *argv[MAX_ARGS + 1];
		int index = 0;
		char port[32];
        char path[1024 + sizeof("/thrpool_client")];

        getcwd(path, sizeof(path));

        (void)strcat(path, "/thrpool_client");
#ifdef XP_PC
        (void)strcat(path, ".exe");
#endif
        argv[index++] = path;
		sprintf(port,"%d",PR_ntohs(netaddr.inet.port));
        if (_debug_on)
        {
            argv[index++] = "-d";
            argv[index++] = "-p";
            argv[index++] = port;
            argv[index++] = NULL;
        } else {
            argv[index++] = "-p";
            argv[index++] = port;
			argv[index++] = NULL;
		}
		PR_ASSERT(MAX_ARGS >= (index - 1));
        
        DPRINTF(("creating client process %s ...\n", path));
        if (PR_FAILURE == PR_CreateProcessDetached(path, argv, NULL, NULL)) {
        	fprintf(stderr,
				"thrpool_server: ERROR - PR_CreateProcessDetached failed\n");
        	failed_already=1;
        	return;
		}
	}

    sc_mon = PR_NewMonitor();
    if (sc_mon == NULL) {
        fprintf(stderr,"%s: PR_NewMonitor failed\n", program_name);
        failed_already=1;
        return;
    }

	sp->iod.socket = sockfd;
	sp->iod.timeout = PR_SecondsToInterval(60);
	sp->datalen = tcp_mesg_size;
	sp->exit_mon = sc_mon;
	sp->job_counterp = &job_counter;
	sp->conn_counter = 0;
	sp->tp = tp;
	sp->netaddr = netaddr;

	/* create and cancel a timer job */
	jobp = PR_QueueJob_Timer(tp, PR_MillisecondsToInterval(5000),
						print_stats, sp, PR_FALSE);
	PR_ASSERT(NULL != jobp);
	rval = PR_CancelJob(jobp);
	PR_ASSERT(PR_SUCCESS == rval);

    DPRINTF(("TCP_Server: Accepting connections \n"));

	jobp = PR_QueueJob_Accept(tp, &sp->iod, TCP_Server_Accept, sp,
							PR_FALSE);
	PR_ASSERT(NULL != jobp);
	return;
}

static void
TCP_Server_Accept(void *arg)
{
    Server_Param *sp = (Server_Param *) arg;
    PRThreadPool *tp = sp->tp;
    Serve_Client_Param *scp;
	PRFileDesc *newsockfd;
	PRJob *jobp;

	if ((newsockfd = PR_Accept(sp->iod.socket, &sp->netaddr,
		PR_INTERVAL_NO_TIMEOUT)) == NULL) {
		fprintf(stderr,"%s: ERROR - PR_Accept failed\n", program_name);
		failed_already=1;
		goto exit;
	}
	scp = PR_NEW(Serve_Client_Param);
	if (scp == NULL) {
		fprintf(stderr,"%s: PR_NEW failed\n", program_name);
		failed_already=1;
		goto exit;
	}

	/*
	 * Start a Serve_Client job for each incoming connection
	 */
	scp->iod.socket = newsockfd;
	scp->iod.timeout = PR_SecondsToInterval(60);
	scp->datalen = tcp_mesg_size;
	scp->exit_mon = sp->exit_mon;
	scp->job_counterp = sp->job_counterp;
	scp->tp = sp->tp;

	PR_EnterMonitor(sp->exit_mon);
	(*sp->job_counterp)++;
	PR_ExitMonitor(sp->exit_mon);
	jobp = PR_QueueJob(tp, Serve_Client, scp,
						PR_FALSE);

	PR_ASSERT(NULL != jobp);
	DPRINTF(("TCP_Server: Created Serve_Client = 0x%lx\n", jobp));

	/*
	 * single-threaded update; no lock needed
	 */
    sp->conn_counter++;
    if (sp->conn_counter <
			(num_tcp_clients * num_tcp_connections_per_client)) {
		jobp = PR_QueueJob_Accept(tp, &sp->iod, TCP_Server_Accept, sp,
								PR_FALSE);
		PR_ASSERT(NULL != jobp);
		return;
	}
	jobp = PR_QueueJob_Timer(tp, PR_MillisecondsToInterval(500),
						print_stats, sp, PR_FALSE);

	PR_ASSERT(NULL != jobp);
	DPRINTF(("TCP_Server: Created print_stats timer job = 0x%lx\n", jobp));

exit:
	PR_EnterMonitor(sp->exit_mon);
    /* Wait for server jobs to finish */
    while (0 != *sp->job_counterp) {
        PR_Wait(sp->exit_mon, PR_INTERVAL_NO_TIMEOUT);
        DPRINTF(("TCP_Server: conn_counter = %d\n",
												*sp->job_counterp));
    }

    PR_ExitMonitor(sp->exit_mon);
    if (sp->iod.socket) {
        PR_Close(sp->iod.socket);
    }
	PR_DestroyMonitor(sp->exit_mon);
    printf("%30s","TCP_Socket_Client_Server_Test:");
    printf("%2ld Server %2ld Clients %2ld connections_per_client\n",1l,
        num_tcp_clients, num_tcp_connections_per_client);
    printf("%30s %2ld messages_per_connection %4ld bytes_per_message\n",":",
        num_tcp_mesgs_per_connection, tcp_mesg_size);

	DPRINTF(("%s: calling PR_ShutdownThreadPool\n", program_name));
	PR_ShutdownThreadPool(sp->tp);
	PR_DELETE(sp);
}

/************************************************************************/

#define DEFAULT_INITIAL_THREADS		4
#define DEFAULT_MAX_THREADS			100
#define DEFAULT_STACKSIZE			(512 * 1024)

int main(int argc, char **argv)
{
	PRInt32 initial_threads = DEFAULT_INITIAL_THREADS;
	PRInt32 max_threads = DEFAULT_MAX_THREADS;
	PRInt32 stacksize = DEFAULT_STACKSIZE;
	PRThreadPool *tp = NULL;
	PRStatus rv;
	PRJob *jobp;

    /*
     * -d           debug mode
     */
    PLOptStatus os;
    PLOptState *opt;

	program_name = argv[0];
    opt = PL_CreateOptState(argc, argv, "d");
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

	tp = PR_CreateThreadPool(initial_threads, max_threads, stacksize);
    if (NULL == tp) {
        printf("PR_CreateThreadPool failed\n");
        failed_already=1;
        goto done;
	}
	jobp = PR_QueueJob(tp, TCP_Server, tp, PR_TRUE);
	rv = PR_JoinJob(jobp);		
	PR_ASSERT(PR_SUCCESS == rv);

	DPRINTF(("%s: calling PR_JoinThreadPool\n", program_name));
	rv = PR_JoinThreadPool(tp);
	PR_ASSERT(PR_SUCCESS == rv);
	DPRINTF(("%s: returning from PR_JoinThreadPool\n", program_name));

done:
    PR_Cleanup();
    if (failed_already) return 1;
    else return 0;
}
