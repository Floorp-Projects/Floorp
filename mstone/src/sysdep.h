/* -*- Mode: C; c-file-style: "stroustrup"; comment-column: 40 -*- */
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
 * The Original Code is the Netscape Mailstone utility, 
 * released March 17, 2000.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s):	Dan Christian <robodan@netscape.com>
 *			Marcel DePaolis <marcel@netcape.com>
 *			Mike Blakely
 *			David Shak
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License Version 2 or later (the "GPL"), in
 * which case the provisions of the GPL are applicable instead of
 * those above.  If you wish to allow use of your version of this file
 * only under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice and
 * other provisions required by the GPL.  If you do not delete the
 * provisions above, a recipient may use your version of this file
 * under either the NPL or the GPL.
 */
#ifndef __SYSDEP_H__ 
#define __SYSDEP_H__

/* include config.h, output from autoconf */
#ifdef HAVE_CONFIG_H
#ifndef __CONFIG_H__
#define __CONFIG_H__
/* borrow config.h from gnuplot.  Should build our own */
#include "gnuplot/config.h"
#endif
#else
/* Modern OSes have these */
#define HAVE_SNPRINTF 1
#define HAVE_STRERROR 1
#endif

#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#else
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#endif /* _WIN32 */

#ifdef __OSF1__
#include <sys/sysinfo.h> /* for setsysinfo() */
#endif

/* MAXHOSTNAMELEN is undefined on some systems */
#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif

/* SunOS doesn't define NULL */
#ifndef NULL
#define NULL 0
#endif

/* encapsulation of minor UNIX/WIN NT differences */
#ifdef _WIN32
#define NETREAD(sock, buf, len)  	recv(sock, buf, len, 0)
#define NETWRITE(sock, buf, len) 	send(sock, buf, len, 0)
#define NETCLOSE(sock)     		closesocket(sock)
#define OUTPUT_WRITE(sock, buf, len) 	write(sock, buf, len)
#define	BADSOCKET(sock)			((sock) == INVALID_SOCKET)
#define	BADSOCKET_ERRNO(sock)		BADSOCKET(sock)
#define	BADSOCKET_VALUE			INVALID_SOCKET
#define S_ADDR				S_un.S_addr
#define	GET_ERROR			WSAGetLastError()
#define	SET_ERROR(err)			WSASetLastError(err)

/* NT gettimeofday() doesn't support USE_TIMEZONE (yet) */
#include <time.h>
#define	GETTIMEOFDAY(timeval, tz)	gettimeofday(timeval)

typedef unsigned short			NETPORT;
#define SRANDOM				srand
#define RANDOM				rand
#define	PROGPATH			"c:\\mailstone\\mailclient"
#define FILENAME_SIZE			256
#define HAVE_VPRINTF			1

#define	SIGCHLD				0	/* dummy value */
#define	SIGALRM				0	/* dummy value */
typedef int				pid_t;
typedef	unsigned short			ushort;
#define MAXPATHLEN			512

extern void sock_cleanup(void);

#define THREAD_RET void
#define THREAD_ID	int

#else /* not _WIN32 */

#define strnicmp(s1,s2,n)	strncasecmp(s1,s2,n)

#define NETREAD(sock, buf, len)		read(sock, buf, len)
#define NETWRITE(sock, buf, len)	write(sock, buf, len)
#define	NETCLOSE(sock)			close(sock)
#define OUTPUT_WRITE(sock, buf, len)	write(sock, buf, len)
#define BADSOCKET(sock)			((sock) < 0)
#define BADSOCKET_ERRNO(sock)		(BADSOCKET(sock) || errno)
#define	BADSOCKET_VALUE			(-1)
#define S_ADDR				s_addr
#define	GET_ERROR			errno
#define	SET_ERROR(err)			(errno = (err))

#define	GETTIMEOFDAY(timeval,tz)	gettimeofday(timeval, NULL)

typedef unsigned short			NETPORT;

#if defined (USE_LRAND48_R)
extern void osf_srand48_r(unsigned int seed);
extern long osf_lrand48_r(void);
#define	SRANDOM				osf_srand48_r
#define	RANDOM				osf_lrand48_r

#elif defined (USE_LRAND48)

#define	SRANDOM				srand48
#define	RANDOM				lrand48

#else /* !USE_LRAND48 */

#define	SRANDOM				srandom
#define	RANDOM				random

#endif /* USE_LRAND48_R */

#define PROGPATH			"/mailstone/mailclient"
#define	FILENAME_SIZE			1024
#define HAVE_VPRINTF			1

typedef int				SOCKET;
#define min(a,b)			(((a) < (b)) ? a : b)
#define max(a,b)			(((a) > (b)) ? a : b)

#define THREAD_RET void *
#ifdef USE_PTHREADS
#define THREAD_ID	pthread_t
#else
#define THREAD_ID	int
#endif

#endif /* _WIN32 */


typedef THREAD_RET (*thread_fn_t)(void *);

/* function prototypes */

extern void crank_limits(void);
extern int sysdep_thread_create(THREAD_ID *id, thread_fn_t tfn, void *arg);
extern int sysdep_thread_join(THREAD_ID id, int *pstatus);
extern void setup_signal_handlers (void);

#ifdef _WIN32
int	getopt(int argc, char ** argv, char *opts);
int	getpid(void);
int	gettimeofday(struct timeval *curTimeP);
int	random_number(int max);
SOCKET	rexec(const char **hostname, NETPORT port, char *username, char *password,
		char *command, SOCKET *sockerr);

#else
#ifdef NO_REXEC
extern int	rexec(char **, int, char *, char *, char *, int *);
#endif 
#endif /* _WIN32 */


#ifndef HAVE_STRERROR
/* strerror() is not available on SunOS 4.x and others */
char *strerror(int errnum); 

#endif
/* strerror() */


#ifndef INADDR_NONE
#define INADDR_NONE -1
#endif

#endif /* !__SYSDEP_H__ */
