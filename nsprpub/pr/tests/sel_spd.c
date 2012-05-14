/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test the speed of select within NSPR
 *
 */

#include "nspr.h"
#include "prpriv.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#ifdef SYMBIAN
#include <getopt.h>
#endif

#define PORT_BASE 19000

typedef struct timer_slot_t {
	unsigned long d_connect;
	unsigned long d_cl_data;
	unsigned long d_sv_data;
	unsigned long d_close;
	unsigned long d_total;
	unsigned long requests;
} timer_slot_t;

static long _iterations = 5;
static long _client_data = 8192;

#ifdef SYMBIAN
/*
 * Symbian OS does not scale well specially the requirement for thread stack
 * space and buffer allocation space.  It is easy to get into a fragmented
 * memory and not be able to allocate thread stack or client/server data
 * buffer.
 */
static long _server_data = (8*1024);
static long _threads_max = 10, _threads = 10;
#else
static long _server_data = (128*1024);
static long _threads_max = 10, _threads = 10;
#endif

static int verbose=0;
static PRMonitor *exit_cv;
static long _thread_exit_count;
static timer_slot_t *timer_data;
static PRThreadScope scope1, scope2;

void tally_results(int);

/* return the diff in microseconds */
unsigned long _delta(PRIntervalTime *start, PRIntervalTime *stop)
{
	/*
	 * Will C do the right thing with unsigned arithemtic?
	 */
	return PR_IntervalToMicroseconds(*stop - *start);
}

int _readn(PRFileDesc *sock, char *buf, int len)
{
	int rem;
	int bytes;

	for (rem=len; rem; rem -= bytes) {
		bytes = PR_Recv(sock, buf+len-rem, rem, 0, PR_INTERVAL_NO_TIMEOUT);
		if (bytes <= 0)
            return -1;
	}
	return len;
}

void
_thread_exit(int id)
{
	PR_EnterMonitor(exit_cv);
#ifdef DEBUG
	fprintf(stdout, "Thread %d EXIT\n", id);
#endif

	_thread_exit_count--;
	if (_thread_exit_count == 0) {
#ifdef DEBUG
	fprintf(stdout, "Thread %d EXIT triggered notify\n", id);
#endif
		PR_Notify(exit_cv);
	}
	PR_ExitMonitor(exit_cv);
}

void
_server_thread(void *arg_id)
{
	void _client_thread(void *);
	PRThread *thread;
	int *id =  (int *)arg_id;
	PRFileDesc *sock;
	PRSocketOptionData sockopt;
	PRNetAddr sa;
	PRFileDesc * newsock;
	char *data_buffer = NULL;
	int data_buffer_size;
	int index;
	PRIntervalTime start,
	      connect_done,
	      read_done,
	      write_done,
	      close_done;
	

#ifdef DEBUG
	fprintf(stdout, "server thread %d alive\n", *id);
#endif

	data_buffer_size = (_client_data>_server_data?_client_data:_server_data);

	if ( (data_buffer = (char *)PR_Malloc(data_buffer_size * sizeof(char))) == NULL ) {
		fprintf(stderr, "Error creating buffer in server thread %d\n", *id);
		goto done;
	}


	if ( (sock = PR_NewTCPSocket()) == NULL) {
		fprintf(stderr, "Error creating socket in server thread %d\n", *id);
		goto done;
	}

	sockopt.option = PR_SockOpt_Reuseaddr;
	sockopt.value.reuse_addr = PR_TRUE;
	if ( PR_SetSocketOption(sock, &sockopt) == PR_FAILURE) {
		fprintf(stderr, "Error setting socket option in server thread %d\n", *id);
		goto done;
	}

	memset(&sa, 0 , sizeof(sa));
	sa.inet.family = PR_AF_INET;
	sa.inet.port = PR_htons(PORT_BASE + *id);
	sa.inet.ip = PR_htonl(PR_INADDR_ANY);

	if ( PR_Bind(sock, &sa) < 0) {
		fprintf(stderr, "Error binding socket in server thread %d errno = %d\n", *id, errno);
		goto done;
	}

	if ( PR_Listen(sock, 32) < 0 ) {
		fprintf(stderr, "Error listening to socket in server thread %d\n", *id);
		goto done;
	}

	/* Tell the client to start */
	if ( (thread = PR_CreateThread(PR_USER_THREAD, 
                                      _client_thread, 
                                      id, 
                                      PR_PRIORITY_NORMAL, 
                                      scope2, 
                                      PR_UNJOINABLE_THREAD, 
                                      0)) == NULL)
		fprintf(stderr, "Error creating client thread %d\n", *id);

	for (index = 0; index< _iterations; index++) {

#ifdef DEBUG
	fprintf(stdout, "server thread %d loop %d\n", *id, index);
#endif

		start = PR_IntervalNow();

		if ( (newsock = PR_Accept(sock, &sa,
					PR_INTERVAL_NO_TIMEOUT)) == NULL) {
			fprintf(stderr, "Error accepting connection %d in server thread %d\n",
				index, *id);
			goto done;
		}
#ifdef DEBUG
	fprintf(stdout, "server thread %d got connection %d\n", *id, newsock);
#endif


		connect_done = PR_IntervalNow();
		
		if ( _readn(newsock, data_buffer, _client_data) < _client_data) {
			fprintf(stderr, "Error reading client data for iteration %d in server thread %d\n", index, *id );
			goto done;
		}

#ifdef DEBUG
	fprintf(stdout, "server thread %d read %d bytes\n", *id, _client_data);
#endif
		read_done = PR_IntervalNow();

		if ( PR_Send(newsock, data_buffer, _server_data, 0,
				PR_INTERVAL_NO_TIMEOUT) < _server_data) {
			fprintf(stderr, "Error sending client data for iteration %d in server thread %d\n", index, *id );
			goto done;
		}

#ifdef DEBUG
	fprintf(stdout, "server thread %d write %d bytes\n", *id, _server_data);
#endif

		write_done = PR_IntervalNow();

		PR_Close(newsock);

		close_done = PR_IntervalNow();

		timer_data[2*(*id)].d_connect += _delta(&start, &connect_done);
		timer_data[2*(*id)].d_cl_data += _delta(&connect_done, &read_done);
		timer_data[2*(*id)].d_sv_data += _delta(&read_done, &write_done);
		timer_data[2*(*id)].d_close += _delta(&write_done, &close_done);
		timer_data[2*(*id)].d_total += _delta(&start, &close_done);
		timer_data[2*(*id)].requests++;


#ifdef DEBUG
		fprintf(stdout, "server: %d %d %d %d %d\n",
			 _delta(&start, &connect_done), _delta(&connect_done, &read_done),
			 _delta(&read_done, &write_done), _delta(&write_done, &close_done),
			_delta(&start, &close_done));
#endif
	}

done:
	if (data_buffer != NULL) PR_Free (data_buffer);
	if (sock) PR_Close(sock);
	_thread_exit(*id);
	return;
}

void
_client_thread(void *arg_id)
{
	int *id =  (int *)arg_id;
	int index;
	PRNetAddr sa;
	PRFileDesc *sock_h;
	char *data_buffer = NULL;
	int data_buffer_size;
	int bytes;
	PRIntervalTime start,
	      connect_done,
	      read_done,
	      write_done,
	      close_done;
	PRStatus rv;

#ifdef DEBUG
	fprintf(stdout, "client thread %d alive\n", *id);
#endif

	data_buffer_size = (_client_data>_server_data?_client_data:_server_data);

	if ( (data_buffer = (char *)PR_Malloc(data_buffer_size * sizeof(char))) == NULL) {
		fprintf(stderr, "Error creating buffer in server thread %d\n", *id);
		goto done;
	}

	memset(&sa, 0 , sizeof(sa));
	rv = PR_InitializeNetAddr(PR_IpAddrLoopback, PORT_BASE + *id, &sa);
	PR_ASSERT(PR_SUCCESS == rv);
	
	for (index = 0; index< _iterations; index++) {

#ifdef DEBUG
	fprintf(stdout, "client thread %d loop %d\n", *id, index);
#endif

		start = PR_IntervalNow();
		if ( (sock_h = PR_NewTCPSocket()) == NULL) {
			fprintf(stderr, "Error creating socket %d in client thread %d\n",
				index, *id);
			goto done;
		}

#ifdef DEBUG
	fprintf(stdout, "client thread %d socket created %d\n", *id, sock_h);
#endif

		if ( PR_Connect(sock_h, &sa,
				PR_INTERVAL_NO_TIMEOUT) < 0) {
			fprintf(stderr, "Error accepting connection %d in client thread %d\n",
				index, *id);
			goto done;
		}

#ifdef DEBUG
	fprintf(stdout, "client thread %d socket connected %d\n", *id, sock_h);
#endif

		connect_done = PR_IntervalNow();
		if ( PR_Send(sock_h, data_buffer, _client_data, 0,
				PR_INTERVAL_NO_TIMEOUT) < _client_data) {
			fprintf(stderr, "Error sending client data for iteration %d in client thread %d\n", index, *id );
			goto done;
		}

#ifdef DEBUG
	fprintf(stdout, "client thread %d socket wrote %d\n", *id, _client_data);
#endif

		write_done = PR_IntervalNow();
		if ( (bytes = _readn(sock_h, data_buffer, _server_data)) < _server_data) {
			fprintf(stderr, "Error reading server data for iteration %d in client thread %d (read %d bytes)\n", index, *id, bytes );
			goto done;
		}

#ifdef DEBUG
	fprintf(stdout, "client thread %d socket read %d\n", *id, _server_data);
#endif

		read_done = PR_IntervalNow();
		PR_Close(sock_h);
		close_done = PR_IntervalNow();

		timer_data[2*(*id)+1].d_connect += _delta(&start, &connect_done);
		timer_data[2*(*id)+1].d_cl_data += _delta(&connect_done, &write_done);
		timer_data[2*(*id)+1].d_sv_data += _delta(&write_done, &read_done);
		timer_data[2*(*id)+1].d_close += _delta(&read_done, &close_done);
		timer_data[2*(*id)+1].d_total += _delta(&start, &close_done);
		timer_data[2*(*id)+1].requests++;
	}
done:
	if (data_buffer != NULL) PR_Free (data_buffer);
	_thread_exit(*id);

	return;
}

static
void do_work(void)
{
    int index;

	_thread_exit_count = _threads * 2;
	for (index=0; index<_threads; index++) {
		PRThread *thread;
		int *id = (int *)PR_Malloc(sizeof(int));

		*id = index;

		if ( (thread = PR_CreateThread(PR_USER_THREAD, 
                                       _server_thread, 
                                       id, 
                                       PR_PRIORITY_NORMAL, 
                                       scope1, 
                                       PR_UNJOINABLE_THREAD, 
                                       0)) == NULL)
			fprintf(stderr, "Error creating server thread %d\n", index);
	}
	
	PR_EnterMonitor(exit_cv);
	while (_thread_exit_count > 0)
		PR_Wait(exit_cv, PR_INTERVAL_NO_TIMEOUT);
	PR_ExitMonitor(exit_cv);

	fprintf(stdout, "TEST COMPLETE!\n");

	tally_results(verbose);

}

static void do_workUU(void)
{
    scope1 = PR_LOCAL_THREAD;
    scope2 = PR_LOCAL_THREAD;
    do_work();
}

static void do_workUK(void)
{
    scope1 = PR_LOCAL_THREAD;
    scope2 = PR_GLOBAL_THREAD;
    do_work();
}

static void do_workKU(void)
{
    scope1 = PR_GLOBAL_THREAD;
    scope2 = PR_LOCAL_THREAD;
    do_work();
}

static void do_workKK(void)
{
    scope1 = PR_GLOBAL_THREAD;
    scope2 = PR_GLOBAL_THREAD;
    do_work();
}



static void Measure(void (*func)(void), const char *msg)
{
    PRIntervalTime start, stop;
    double d;

    start = PR_IntervalNow();
    (*func)();
    stop = PR_IntervalNow();

    d = (double)PR_IntervalToMicroseconds(stop - start);

    printf("%40s: %6.2f usec\n", msg, d / _iterations);
}


int main(int argc, char **argv)
{
#if defined(XP_UNIX) || defined(XP_OS2)
	int opt;
	PR_IMPORT_DATA(char *) optarg;
#endif

#if defined(XP_UNIX) || defined(XP_OS2)
	while ( (opt = getopt(argc, argv, "c:s:i:t:v")) != EOF) {
		switch(opt) {
			case 'i':
				_iterations = atoi(optarg);
				break;
			case 't':
				_threads_max = _threads = atoi(optarg);
				break;
			case 'c':
				_client_data = atoi(optarg);
				break;
			case 's':
				_server_data = atoi(optarg);
				break;
			case 'v':
				verbose = 1;
				break;
			default: 
				break;
		}
	}
#endif

	PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    PR_STDIO_INIT();

	fprintf(stdout, "Running test for %d iterations with %d simultaneous threads.\n", 
		_iterations, _threads);
	fprintf(stdout, "\tWill send %d bytes of client data and %d bytes of server data\n", 
		_client_data, _server_data);

	if ( (exit_cv = PR_NewMonitor()) == NULL) 
		fprintf(stderr, "Error creating monitor for exit cv\n");
	if ( (timer_data = (timer_slot_t *)PR_Malloc(2*_threads * sizeof(timer_slot_t))) == NULL) 
		fprintf(stderr, "error allocating thread time results array\n");
	memset(timer_data, 0 , 2*_threads*sizeof(timer_slot_t));

    Measure(do_workUU, "select loop user/user");
    Measure(do_workUK, "select loop user/kernel");
    Measure(do_workKU, "select loop kernel/user");
    Measure(do_workKK, "select loop kernel/kernel");


	return 0;
}

void
tally_results(int verbose)
{
	int index;
	unsigned long tot_connect = 0;
	unsigned long tot_cl_data = 0;
	unsigned long tot_sv_data = 0;
	unsigned long tot_close = 0;
	unsigned long tot_all = 0;
	unsigned long tot_requests = 0;

	fprintf(stdout, "Server results:\n\n");
	for (index=0; index<_threads_max*2; index+=2) {

		if (verbose)
			fprintf(stdout, "server thread %u\t%u\t%u\t%u\t%u\t%u\t%u\n",
				index, timer_data[index].requests, timer_data[index].d_connect,
				timer_data[index].d_cl_data, timer_data[index].d_sv_data,
				timer_data[index].d_close, timer_data[index].d_total);

		tot_connect += timer_data[index].d_connect / _threads;
		tot_cl_data += timer_data[index].d_cl_data / _threads;
		tot_sv_data += timer_data[index].d_sv_data / _threads;
		tot_close += timer_data[index].d_close / _threads;
		tot_all += timer_data[index].d_total / _threads;
		tot_requests += timer_data[index].requests / _threads;
	}
	fprintf(stdout, "----------\n");
	fprintf(stdout, "server per thread totals %u\t%u\t%u\t%u\t%u\n",
		tot_requests, tot_connect, tot_cl_data, tot_sv_data, tot_close);
	fprintf(stdout, "server per thread elapsed time %u\n", tot_all);
	fprintf(stdout, "----------\n");

	tot_connect = tot_cl_data = tot_sv_data = tot_close = tot_all = tot_requests = 0;
	fprintf(stdout, "Client results:\n\n");
	for (index=1; index<_threads_max*2; index+=2) {

		if (verbose)
			fprintf(stdout, "client thread %u\t%u\t%u\t%u\t%u\t%u\t%u\n",
				index, timer_data[index].requests, timer_data[index].d_connect,
				timer_data[index].d_cl_data, timer_data[index].d_sv_data,
				timer_data[index].d_close, timer_data[index].d_total);

		tot_connect += timer_data[index].d_connect / _threads;
		tot_cl_data += timer_data[index].d_cl_data / _threads;
		tot_sv_data += timer_data[index].d_sv_data / _threads;
		tot_close += timer_data[index].d_close / _threads;
		tot_all += timer_data[index].d_total / _threads;
		tot_requests += timer_data[index].requests / _threads;
	}
	fprintf(stdout, "----------\n");
	fprintf(stdout, "client per thread totals %u\t%u\t%u\t%u\t%u\n",
		tot_requests, tot_connect, tot_cl_data, tot_sv_data, tot_close);
	fprintf(stdout, "client per thread elapsed time %u\n", tot_all);
}

