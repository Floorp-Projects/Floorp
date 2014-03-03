/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** File:   ptio.c
** Descritpion:  Implemenation of I/O methods for pthreads
*/

#if defined(_PR_PTHREADS)

#if defined(_PR_POLL_WITH_SELECT)
#if !(defined(HPUX) && defined(_USE_BIG_FDS))
/* set fd limit for select(), before including system header files */
#define FD_SETSIZE (16 * 1024)
#endif
#endif

#include <pthread.h>
#include <string.h>  /* for memset() */
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#if defined(DARWIN)
#include <sys/utsname.h> /* for uname */
#endif
#if defined(SOLARIS) || defined(UNIXWARE)
#include <sys/filio.h>  /* to pick up FIONREAD */
#endif
#ifdef _PR_POLL_AVAILABLE
#include <poll.h>
#endif
#ifdef AIX
/* To pick up sysconf() */
#include <unistd.h>
#include <dlfcn.h>  /* for dlopen */
#else
/* To pick up getrlimit() etc. */
#include <sys/time.h>
#include <sys/resource.h>
#endif

#ifdef SOLARIS
/*
 * Define HAVE_SENDFILEV if the system has the sendfilev() system call.
 * Code built this way won't run on a system without sendfilev().
 * We can define HAVE_SENDFILEV by default when the minimum release
 * of Solaris that NSPR supports has sendfilev().
 */
#ifdef HAVE_SENDFILEV

#include <sys/sendfile.h>

#define SOLARIS_SENDFILEV(a, b, c, d) sendfilev((a), (b), (c), (d))

#else

#include <dlfcn.h>  /* for dlopen */

/*
 * Match the definitions in <sys/sendfile.h>.
 */
typedef struct sendfilevec {
    int sfv_fd;       /* input fd */
    uint_t sfv_flag;  /* flags */
    off_t sfv_off;    /* offset to start reading from */
    size_t sfv_len;   /* amount of data */
} sendfilevec_t;

#define SFV_FD_SELF (-2)

/*
 * extern ssize_t sendfilev(int, const struct sendfilevec *, int, size_t *);
 */
static ssize_t (*pt_solaris_sendfilev_fptr)() = NULL;

#define SOLARIS_SENDFILEV(a, b, c, d) \
        (*pt_solaris_sendfilev_fptr)((a), (b), (c), (d))

#endif /* HAVE_SENDFILEV */
#endif /* SOLARIS */

/*
 * The send_file() system call is available in AIX 4.3.2 or later.
 * If this file is compiled on an older AIX system, it attempts to
 * look up the send_file symbol at run time to determine whether
 * we can use the faster PR_SendFile/PR_TransmitFile implementation based on
 * send_file().  On AIX 4.3.2 or later, we can safely skip this
 * runtime function dispatching and just use the send_file based
 * implementation.
 */
#ifdef AIX
#ifdef SF_CLOSE
#define HAVE_SEND_FILE
#endif

#ifdef HAVE_SEND_FILE

#define AIX_SEND_FILE(a, b, c) send_file(a, b, c)

#else /* HAVE_SEND_FILE */

/*
 * The following definitions match those in <sys/socket.h>
 * on AIX 4.3.2.
 */

/*
 * Structure for the send_file() system call
 */
struct sf_parms {
    /* --------- header parms ---------- */
    void      *header_data;         /* Input/Output. Points to header buf */
    uint_t    header_length;        /* Input/Output. Length of the header */
    /* --------- file parms ------------ */
    int       file_descriptor;      /* Input. File descriptor of the file */
    unsigned long long file_size;   /* Output. Size of the file */
    unsigned long long file_offset; /* Input/Output. Starting offset */
    long long file_bytes;           /* Input/Output. no. of bytes to send */
    /* --------- trailer parms --------- */
    void      *trailer_data;        /* Input/Output. Points to trailer buf */
    uint_t    trailer_length;       /* Input/Output. Length of the trailer */
    /* --------- return info ----------- */
    unsigned long long bytes_sent;  /* Output. no. of bytes sent */
};

/*
 * Flags for the send_file() system call
 */
#define SF_CLOSE        0x00000001      /* close the socket after completion */
#define SF_REUSE        0x00000002      /* reuse socket. not supported */
#define SF_DONT_CACHE   0x00000004      /* don't apply network buffer cache */
#define SF_SYNC_CACHE   0x00000008      /* sync/update network buffer cache */

/*
 * prototype: size_t send_file(int *, struct sf_parms *, uint_t);
 */
static ssize_t (*pt_aix_sendfile_fptr)() = NULL;

#define AIX_SEND_FILE(a, b, c) (*pt_aix_sendfile_fptr)(a, b, c)

#endif /* HAVE_SEND_FILE */
#endif /* AIX */

#ifdef LINUX
#include <sys/sendfile.h>
#endif

#include "primpl.h"

#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>  /* TCP_NODELAY, TCP_MAXSEG */
#endif

#ifdef LINUX
/* TCP_CORK is not defined in <netinet/tcp.h> on Red Hat Linux 6.0 */
#ifndef TCP_CORK
#define TCP_CORK 3
#endif
#endif

#ifdef _PR_IPV6_V6ONLY_PROBE
static PRBool _pr_ipv6_v6only_on_by_default;
#endif

#if (defined(HPUX) && !defined(HPUX10_30) && !defined(HPUX11))
#define _PRSelectFdSetArg_t int *
#elif defined(AIX4_1)
#define _PRSelectFdSetArg_t void *
#elif defined(IRIX) || (defined(AIX) && !defined(AIX4_1)) \
    || defined(OSF1) || defined(SOLARIS) \
    || defined(HPUX10_30) || defined(HPUX11) \
    || defined(LINUX) || defined(__GNU__) || defined(__GLIBC__) \
    || defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) \
    || defined(BSDI) || defined(NTO) || defined(DARWIN) \
    || defined(UNIXWARE) || defined(RISCOS) || defined(SYMBIAN)
#define _PRSelectFdSetArg_t fd_set *
#else
#error "Cannot determine architecture"
#endif

#if defined(SOLARIS)            
#ifndef PROTO_SDP
/* on solaris, SDP is a new type of protocol */
#define PROTO_SDP   257
#endif 
#define _PR_HAVE_SDP
#elif defined(LINUX)
#ifndef AF_INET_SDP
/* on linux, SDP is a new type of address family */
#define AF_INET_SDP 27
#endif
#define _PR_HAVE_SDP
#endif /* LINUX */

static PRFileDesc *pt_SetMethods(
    PRIntn osfd, PRDescType type, PRBool isAcceptedSocket, PRBool imported);

static PRLock *_pr_flock_lock;  /* For PR_LockFile() etc. */
static PRCondVar *_pr_flock_cv;  /* For PR_LockFile() etc. */
static PRLock *_pr_rename_lock;  /* For PR_Rename() */

/**************************************************************************/

/* These two functions are only used in assertions. */
#if defined(DEBUG)

PRBool IsValidNetAddr(const PRNetAddr *addr)
{
    if ((addr != NULL)
            && (addr->raw.family != AF_UNIX)
            && (addr->raw.family != PR_AF_INET6)
            && (addr->raw.family != AF_INET)) {
        return PR_FALSE;
    }
    return PR_TRUE;
}

static PRBool IsValidNetAddrLen(const PRNetAddr *addr, PRInt32 addr_len)
{
    /*
     * The definition of the length of a Unix domain socket address
     * is not uniform, so we don't check it.
     */
    if ((addr != NULL)
            && (addr->raw.family != AF_UNIX)
            && (PR_NETADDR_SIZE(addr) != addr_len)) {
#if defined(LINUX) && __GLIBC__ == 2 && __GLIBC_MINOR__ == 1
        /*
         * In glibc 2.1, struct sockaddr_in6 is 24 bytes.  In glibc 2.2
         * and in the 2.4 kernel, struct sockaddr_in6 has the scope_id
         * field and is 28 bytes.  It is possible for socket functions
         * to return an addr_len greater than sizeof(struct sockaddr_in6).
         * We need to allow that.  (Bugzilla bug #77264)
         */
        if ((PR_AF_INET6 == addr->raw.family)
                && (sizeof(addr->ipv6) == addr_len)) {
            return PR_TRUE;
        }
#endif
        return PR_FALSE;
    }
    return PR_TRUE;
}

#endif /* DEBUG */

/*****************************************************************************/
/************************* I/O Continuation machinery ************************/
/*****************************************************************************/

/*
 * The polling interval defines the maximum amount of time that a thread
 * might hang up before an interrupt is noticed.
 */
#define PT_DEFAULT_POLL_MSEC 5000
#if defined(_PR_POLL_WITH_SELECT)
#define PT_DEFAULT_SELECT_SEC (PT_DEFAULT_POLL_MSEC/PR_MSEC_PER_SEC)
#define PT_DEFAULT_SELECT_USEC							\
		((PT_DEFAULT_POLL_MSEC % PR_MSEC_PER_SEC) * PR_USEC_PER_MSEC)
#endif

/*
 * pt_SockLen is the type for the length of a socket address
 * structure, used in the address length argument to bind,
 * connect, accept, getsockname, getpeername, etc.  Posix.1g
 * defines this type as socklen_t.  It is size_t or int on
 * most current systems.
 */
#if defined(HAVE_SOCKLEN_T) \
    || (defined(__GLIBC__) && __GLIBC__ >= 2)
typedef socklen_t pt_SockLen;
#elif (defined(AIX) && !defined(AIX4_1)) 
typedef PRSize pt_SockLen;
#else
typedef PRIntn pt_SockLen;
#endif

typedef struct pt_Continuation pt_Continuation;
typedef PRBool (*ContinuationFn)(pt_Continuation *op, PRInt16 revents);

typedef enum pr_ContuationStatus
{
    pt_continuation_pending,
    pt_continuation_done
} pr_ContuationStatus;

struct pt_Continuation
{
    /* The building of the continuation operation */
    ContinuationFn function;                /* what function to continue */
    union { PRIntn osfd; } arg1;            /* #1 - the op's fd */
    union { void* buffer; } arg2;           /* #2 - primary transfer buffer */
    union {
        PRSize amount;                      /* #3 - size of 'buffer', or */
        pt_SockLen *addr_len;                  /*    - length of address */
#ifdef HPUX11
        /*
         * For sendfile()
         */
		struct file_spec {		
        	off_t offset;                       /* offset in file to send */
        	size_t nbytes;                      /* length of file data to send */
        	size_t st_size;                     /* file size */
		} file_spec;
#endif
    } arg3;
    union { PRIntn flags; } arg4;           /* #4 - read/write flags */
    union { PRNetAddr *addr; } arg5;        /* #5 - send/recv address */

#ifdef HPUX11
    /*
     * For sendfile()
     */
    int filedesc;                           /* descriptor of file to send */
    int nbytes_to_send;                     /* size of header and file */
#endif  /* HPUX11 */
    
#ifdef SOLARIS
    /*
     * For sendfilev()
     */
    int nbytes_to_send;                     /* size of header and file */
#endif  /* SOLARIS */

#ifdef LINUX
    /*
     * For sendfile()
     */
    int in_fd;                              /* descriptor of file to send */
    off_t offset;
    size_t count;
#endif  /* LINUX */
 
    PRIntervalTime timeout;                 /* client (relative) timeout */

    PRInt16 event;                           /* flags for poll()'s events */

    /*
    ** The representation and notification of the results of the operation.
    ** These function can either return an int return code or a pointer to
    ** some object.
    */
    union { PRSize code; void *object; } result;

    PRIntn syserrno;                        /* in case it failed, why (errno) */
    pr_ContuationStatus status;             /* the status of the operation */
};

#if defined(DEBUG)

PTDebug pt_debug;  /* this is shared between several modules */

PR_IMPLEMENT(void) PT_FPrintStats(PRFileDesc *debug_out, const char *msg)
{
    PTDebug stats;
    char buffer[100];
    PRExplodedTime tod;
    PRInt64 elapsed, aMil;
    stats = pt_debug;  /* a copy */
    PR_ExplodeTime(stats.timeStarted, PR_LocalTimeParameters, &tod);
    (void)PR_FormatTime(buffer, sizeof(buffer), "%T", &tod);

    LL_SUB(elapsed, PR_Now(), stats.timeStarted);
    LL_I2L(aMil, 1000000);
    LL_DIV(elapsed, elapsed, aMil);
    
    if (NULL != msg) PR_fprintf(debug_out, "%s", msg);
    PR_fprintf(
        debug_out, "\tstarted: %s[%lld]\n", buffer, elapsed);
    PR_fprintf(
        debug_out, "\tlocks [created: %u, destroyed: %u]\n",
        stats.locks_created, stats.locks_destroyed);
    PR_fprintf(
        debug_out, "\tlocks [acquired: %u, released: %u]\n",
        stats.locks_acquired, stats.locks_released);
    PR_fprintf(
        debug_out, "\tcvars [created: %u, destroyed: %u]\n",
        stats.cvars_created, stats.cvars_destroyed);
    PR_fprintf(
        debug_out, "\tcvars [notified: %u, delayed_delete: %u]\n",
        stats.cvars_notified, stats.delayed_cv_deletes);
}  /* PT_FPrintStats */

#else

PR_IMPLEMENT(void) PT_FPrintStats(PRFileDesc *debug_out, const char *msg)
{
    /* do nothing */
}  /* PT_FPrintStats */

#endif  /* DEBUG */

#if defined(_PR_POLL_WITH_SELECT)
/*
 * OSF1 and HPUX report the POLLHUP event for a socket when the
 * shutdown(SHUT_WR) operation is called for the remote end, even though
 * the socket is still writeable. Use select(), instead of poll(), to
 * workaround this problem.
 */
static void pt_poll_now_with_select(pt_Continuation *op)
{
    PRInt32 msecs;
	fd_set rd, wr, *rdp, *wrp;
	struct timeval tv;
	PRIntervalTime epoch, now, elapsed, remaining;
	PRBool wait_for_remaining;
    PRThread *self = PR_GetCurrentThread();
    
	PR_ASSERT(PR_INTERVAL_NO_WAIT != op->timeout);
	PR_ASSERT(op->arg1.osfd < FD_SETSIZE);

    switch (op->timeout) {
        case PR_INTERVAL_NO_TIMEOUT:
			tv.tv_sec = PT_DEFAULT_SELECT_SEC;
			tv.tv_usec = PT_DEFAULT_SELECT_USEC;
			do
			{
				PRIntn rv;

				if (op->event & POLLIN) {
					FD_ZERO(&rd);
					FD_SET(op->arg1.osfd, &rd);
					rdp = &rd;
				} else
					rdp = NULL;
				if (op->event & POLLOUT) {
					FD_ZERO(&wr);
					FD_SET(op->arg1.osfd, &wr);
					wrp = &wr;
				} else
					wrp = NULL;

				rv = select(op->arg1.osfd + 1, rdp, wrp, NULL, &tv);

				if (_PT_THREAD_INTERRUPTED(self))
				{
					self->state &= ~PT_THREAD_ABORTED;
					op->result.code = -1;
					op->syserrno = EINTR;
					op->status = pt_continuation_done;
					return;
				}

				if ((-1 == rv) && ((errno == EINTR) || (errno == EAGAIN)))
					continue; /* go around the loop again */

				if (rv > 0)
				{
					PRInt16 revents = 0;

					if ((op->event & POLLIN) && FD_ISSET(op->arg1.osfd, &rd))
						revents |= POLLIN;
					if ((op->event & POLLOUT) && FD_ISSET(op->arg1.osfd, &wr))
						revents |= POLLOUT;
						
					if (op->function(op, revents))
						op->status = pt_continuation_done;
				} else if (rv == -1) {
					op->result.code = -1;
					op->syserrno = errno;
					op->status = pt_continuation_done;
				}
				/* else, select timed out */
			} while (pt_continuation_done != op->status);
			break;
        default:
            now = epoch = PR_IntervalNow();
            remaining = op->timeout;
			do
			{
				PRIntn rv;

				if (op->event & POLLIN) {
					FD_ZERO(&rd);
					FD_SET(op->arg1.osfd, &rd);
					rdp = &rd;
				} else
					rdp = NULL;
				if (op->event & POLLOUT) {
					FD_ZERO(&wr);
					FD_SET(op->arg1.osfd, &wr);
					wrp = &wr;
				} else
					wrp = NULL;

    			wait_for_remaining = PR_TRUE;
    			msecs = (PRInt32)PR_IntervalToMilliseconds(remaining);
				if (msecs > PT_DEFAULT_POLL_MSEC) {
					wait_for_remaining = PR_FALSE;
					msecs = PT_DEFAULT_POLL_MSEC;
				}
				tv.tv_sec = msecs/PR_MSEC_PER_SEC;
				tv.tv_usec = (msecs % PR_MSEC_PER_SEC) * PR_USEC_PER_MSEC;
				rv = select(op->arg1.osfd + 1, rdp, wrp, NULL, &tv);

				if (_PT_THREAD_INTERRUPTED(self))
				{
					self->state &= ~PT_THREAD_ABORTED;
					op->result.code = -1;
					op->syserrno = EINTR;
					op->status = pt_continuation_done;
					return;
				}

				if (rv > 0) {
					PRInt16 revents = 0;

					if ((op->event & POLLIN) && FD_ISSET(op->arg1.osfd, &rd))
						revents |= POLLIN;
					if ((op->event & POLLOUT) && FD_ISSET(op->arg1.osfd, &wr))
						revents |= POLLOUT;
						
					if (op->function(op, revents))
						op->status = pt_continuation_done;

				} else if ((rv == 0) ||
						((errno == EINTR) || (errno == EAGAIN))) {
					if (rv == 0) {	/* select timed out */
						if (wait_for_remaining)
							now += remaining;
						else
							now += PR_MillisecondsToInterval(msecs);
					} else
						now = PR_IntervalNow();
					elapsed = (PRIntervalTime) (now - epoch);
					if (elapsed >= op->timeout) {
						op->result.code = -1;
						op->syserrno = ETIMEDOUT;
						op->status = pt_continuation_done;
					} else
						remaining = op->timeout - elapsed;
				} else {
					op->result.code = -1;
					op->syserrno = errno;
					op->status = pt_continuation_done;
				}
			} while (pt_continuation_done != op->status);
            break;
    }

}  /* pt_poll_now_with_select */

#endif	/* _PR_POLL_WITH_SELECT */

static void pt_poll_now(pt_Continuation *op)
{
    PRInt32 msecs;
	PRIntervalTime epoch, now, elapsed, remaining;
	PRBool wait_for_remaining;
    PRThread *self = PR_GetCurrentThread();
    
	PR_ASSERT(PR_INTERVAL_NO_WAIT != op->timeout);
#if defined (_PR_POLL_WITH_SELECT)
	/*
 	 * If the fd is small enough call the select-based poll operation
	 */
	if (op->arg1.osfd < FD_SETSIZE) {
		pt_poll_now_with_select(op);
		return;
	}
#endif

    switch (op->timeout) {
        case PR_INTERVAL_NO_TIMEOUT:
			msecs = PT_DEFAULT_POLL_MSEC;
			do
			{
				PRIntn rv;
				struct pollfd tmp_pfd;

				tmp_pfd.revents = 0;
				tmp_pfd.fd = op->arg1.osfd;
				tmp_pfd.events = op->event;

				rv = poll(&tmp_pfd, 1, msecs);
				
				if (_PT_THREAD_INTERRUPTED(self))
				{
					self->state &= ~PT_THREAD_ABORTED;
					op->result.code = -1;
					op->syserrno = EINTR;
					op->status = pt_continuation_done;
					return;
				}

				if ((-1 == rv) && ((errno == EINTR) || (errno == EAGAIN)))
					continue; /* go around the loop again */

				if (rv > 0)
				{
					PRInt16 events = tmp_pfd.events;
					PRInt16 revents = tmp_pfd.revents;

					if ((revents & POLLNVAL)  /* busted in all cases */
					|| ((events & POLLOUT) && (revents & POLLHUP)))
						/* write op & hup */
					{
						op->result.code = -1;
						if (POLLNVAL & revents) op->syserrno = EBADF;
						else if (POLLHUP & revents) op->syserrno = EPIPE;
						op->status = pt_continuation_done;
					} else {
						if (op->function(op, revents))
							op->status = pt_continuation_done;
					}
				} else if (rv == -1) {
					op->result.code = -1;
					op->syserrno = errno;
					op->status = pt_continuation_done;
				}
				/* else, poll timed out */
			} while (pt_continuation_done != op->status);
			break;
        default:
            now = epoch = PR_IntervalNow();
            remaining = op->timeout;
			do
			{
				PRIntn rv;
				struct pollfd tmp_pfd;

				tmp_pfd.revents = 0;
				tmp_pfd.fd = op->arg1.osfd;
				tmp_pfd.events = op->event;

    			wait_for_remaining = PR_TRUE;
    			msecs = (PRInt32)PR_IntervalToMilliseconds(remaining);
				if (msecs > PT_DEFAULT_POLL_MSEC)
				{
					wait_for_remaining = PR_FALSE;
					msecs = PT_DEFAULT_POLL_MSEC;
				}
				rv = poll(&tmp_pfd, 1, msecs);
				
				if (_PT_THREAD_INTERRUPTED(self))
				{
					self->state &= ~PT_THREAD_ABORTED;
					op->result.code = -1;
					op->syserrno = EINTR;
					op->status = pt_continuation_done;
					return;
				}

				if (rv > 0)
				{
					PRInt16 events = tmp_pfd.events;
					PRInt16 revents = tmp_pfd.revents;

					if ((revents & POLLNVAL)  /* busted in all cases */
						|| ((events & POLLOUT) && (revents & POLLHUP))) 
											/* write op & hup */
					{
						op->result.code = -1;
						if (POLLNVAL & revents) op->syserrno = EBADF;
						else if (POLLHUP & revents) op->syserrno = EPIPE;
						op->status = pt_continuation_done;
					} else {
						if (op->function(op, revents))
						{
							op->status = pt_continuation_done;
						}
					}
				} else if ((rv == 0) ||
						((errno == EINTR) || (errno == EAGAIN))) {
					if (rv == 0)	/* poll timed out */
					{
						if (wait_for_remaining)
							now += remaining;
						else
							now += PR_MillisecondsToInterval(msecs);
					}
					else
						now = PR_IntervalNow();
					elapsed = (PRIntervalTime) (now - epoch);
					if (elapsed >= op->timeout) {
						op->result.code = -1;
						op->syserrno = ETIMEDOUT;
						op->status = pt_continuation_done;
					} else
						remaining = op->timeout - elapsed;
				} else {
					op->result.code = -1;
					op->syserrno = errno;
					op->status = pt_continuation_done;
				}
			} while (pt_continuation_done != op->status);
            break;
    }

}  /* pt_poll_now */

static PRIntn pt_Continue(pt_Continuation *op)
{
    op->status = pt_continuation_pending;  /* set default value */
	/*
	 * let each thread call poll directly
	 */
	pt_poll_now(op);
	PR_ASSERT(pt_continuation_done == op->status);
    return op->result.code;
}  /* pt_Continue */

/*****************************************************************************/
/*********************** specific continuation functions *********************/
/*****************************************************************************/
static PRBool pt_connect_cont(pt_Continuation *op, PRInt16 revents)
{
    op->syserrno = _MD_unix_get_nonblocking_connect_error(op->arg1.osfd);
    if (op->syserrno != 0) {
        op->result.code = -1;
    } else {
        op->result.code = 0;
    }
    return PR_TRUE; /* this one is cooked */
}  /* pt_connect_cont */

static PRBool pt_accept_cont(pt_Continuation *op, PRInt16 revents)
{
    op->syserrno = 0;
    op->result.code = accept(
        op->arg1.osfd, op->arg2.buffer, op->arg3.addr_len);
    if (-1 == op->result.code)
    {
        op->syserrno = errno;
        if (EWOULDBLOCK == errno || EAGAIN == errno || ECONNABORTED == errno)
            return PR_FALSE;  /* do nothing - this one ain't finished */
    }
    return PR_TRUE;
}  /* pt_accept_cont */

static PRBool pt_read_cont(pt_Continuation *op, PRInt16 revents)
{
    /*
     * Any number of bytes will complete the operation. It need
     * not (and probably will not) satisfy the request. The only
     * error we continue is EWOULDBLOCK|EAGAIN.
     */
    op->result.code = read(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
    op->syserrno = errno;
    return ((-1 == op->result.code) && 
            (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
        PR_FALSE : PR_TRUE;
}  /* pt_read_cont */

static PRBool pt_recv_cont(pt_Continuation *op, PRInt16 revents)
{
    /*
     * Any number of bytes will complete the operation. It need
     * not (and probably will not) satisfy the request. The only
     * error we continue is EWOULDBLOCK|EAGAIN.
     */
#if defined(SOLARIS)
    if (0 == op->arg4.flags)
        op->result.code = read(
            op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
    else
        op->result.code = recv(
            op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags);
#else
    op->result.code = recv(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags);
#endif
    op->syserrno = errno;
    return ((-1 == op->result.code) && 
            (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
        PR_FALSE : PR_TRUE;
}  /* pt_recv_cont */

static PRBool pt_send_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
#if defined(SOLARIS)
    PRInt32 tmp_amount = op->arg3.amount;
#endif
    /*
     * We want to write the entire amount out, no matter how many
     * tries it takes. Keep advancing the buffer and the decrementing
     * the amount until the amount goes away. Return the total bytes
     * (which should be the original amount) when finished (or an
     * error).
     */
#if defined(SOLARIS)
retry:
    bytes = write(op->arg1.osfd, op->arg2.buffer, tmp_amount);
#else
    bytes = send(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags);
#endif
    op->syserrno = errno;

#if defined(SOLARIS)
    /*
     * The write system call has been reported to return the ERANGE error
     * on occasion. Try to write in smaller chunks to workaround this bug.
     */
    if ((bytes == -1) && (op->syserrno == ERANGE))
    {
        if (tmp_amount > 1)
        {
            tmp_amount = tmp_amount/2;  /* half the bytes */
            goto retry;
        }
    }
#endif

    if (bytes >= 0)  /* this is progress */
    {
        char *bp = (char*)op->arg2.buffer;
        bp += bytes;  /* adjust the buffer pointer */
        op->arg2.buffer = bp;
        op->result.code += bytes;  /* accumulate the number sent */
        op->arg3.amount -= bytes;  /* and reduce the required count */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else if ((EWOULDBLOCK != op->syserrno) && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_send_cont */

static PRBool pt_write_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
    /*
     * We want to write the entire amount out, no matter how many
     * tries it takes. Keep advancing the buffer and the decrementing
     * the amount until the amount goes away. Return the total bytes
     * (which should be the original amount) when finished (or an
     * error).
     */
    bytes = write(op->arg1.osfd, op->arg2.buffer, op->arg3.amount);
    op->syserrno = errno;
    if (bytes >= 0)  /* this is progress */
    {
        char *bp = (char*)op->arg2.buffer;
        bp += bytes;  /* adjust the buffer pointer */
        op->arg2.buffer = bp;
        op->result.code += bytes;  /* accumulate the number sent */
        op->arg3.amount -= bytes;  /* and reduce the required count */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else if ((EWOULDBLOCK != op->syserrno) && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_write_cont */

static PRBool pt_writev_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes;
    struct iovec *iov = (struct iovec*)op->arg2.buffer;
    /*
     * Same rules as write, but continuing seems to be a bit more
     * complicated. As the number of bytes sent grows, we have to
     * redefine the vector we're pointing at. We might have to
     * modify an individual vector parms or we might have to eliminate
     * a pair altogether.
     */
    bytes = writev(op->arg1.osfd, iov, op->arg3.amount);
    op->syserrno = errno;
    if (bytes >= 0)  /* this is progress */
    {
        PRIntn iov_index;
        op->result.code += bytes;  /* accumulate the number sent */
        for (iov_index = 0; iov_index < op->arg3.amount; ++iov_index)
        {
            /* how much progress did we make in the i/o vector? */
            if (bytes < iov[iov_index].iov_len)
            {
                /* this element's not done yet */
                char **bp = (char**)&(iov[iov_index].iov_base);
                iov[iov_index].iov_len -= bytes;  /* there's that much left */
                *bp += bytes;  /* starting there */
                break;  /* go off and do that */
            }
            bytes -= iov[iov_index].iov_len;  /* that element's consumed */
        }
        op->arg2.buffer = &iov[iov_index];  /* new start of array */
        op->arg3.amount -= iov_index;  /* and array length */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else if ((EWOULDBLOCK != op->syserrno) && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_writev_cont */

static PRBool pt_sendto_cont(pt_Continuation *op, PRInt16 revents)
{
    PRIntn bytes = sendto(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount, op->arg4.flags,
        (struct sockaddr*)op->arg5.addr, PR_NETADDR_SIZE(op->arg5.addr));
    op->syserrno = errno;
    if (bytes >= 0)  /* this is progress */
    {
        char *bp = (char*)op->arg2.buffer;
        bp += bytes;  /* adjust the buffer pointer */
        op->arg2.buffer = bp;
        op->result.code += bytes;  /* accumulate the number sent */
        op->arg3.amount -= bytes;  /* and reduce the required count */
        return (0 == op->arg3.amount) ? PR_TRUE : PR_FALSE;
    }
    else if ((EWOULDBLOCK != op->syserrno) && (EAGAIN != op->syserrno))
    {
        op->result.code = -1;
        return PR_TRUE;
    }
    else return PR_FALSE;
}  /* pt_sendto_cont */

static PRBool pt_recvfrom_cont(pt_Continuation *op, PRInt16 revents)
{
    pt_SockLen addr_len = sizeof(PRNetAddr);
    op->result.code = recvfrom(
        op->arg1.osfd, op->arg2.buffer, op->arg3.amount,
        op->arg4.flags, (struct sockaddr*)op->arg5.addr, &addr_len);
    op->syserrno = errno;
    return ((-1 == op->result.code) && 
            (EWOULDBLOCK == op->syserrno || EAGAIN == op->syserrno)) ?
        PR_FALSE : PR_TRUE;
}  /* pt_recvfrom_cont */

#ifdef AIX
static PRBool pt_aix_sendfile_cont(pt_Continuation *op, PRInt16 revents)
{
    struct sf_parms *sf_struct = (struct sf_parms *) op->arg2.buffer;
    ssize_t rv;
	unsigned long long saved_file_offset;
	long long saved_file_bytes;

	saved_file_offset = sf_struct->file_offset;
	saved_file_bytes = sf_struct->file_bytes;
	sf_struct->bytes_sent = 0;

	if ((sf_struct->file_bytes > 0) && (sf_struct->file_size > 0))
	PR_ASSERT((sf_struct->file_bytes + sf_struct->file_offset) <=
									sf_struct->file_size);
    rv = AIX_SEND_FILE(&op->arg1.osfd, sf_struct, op->arg4.flags);
    op->syserrno = errno;

    if (rv != -1) {
        op->result.code += sf_struct->bytes_sent;
		/*
		 * A bug in AIX 4.3.2 prevents the 'file_bytes' field from
		 * being updated. So, 'file_bytes' is maintained by NSPR to
		 * avoid conflict when this bug is fixed in AIX, in the future.
		 */
		if (saved_file_bytes != -1)
			saved_file_bytes -= (sf_struct->file_offset - saved_file_offset);
		sf_struct->file_bytes = saved_file_bytes;
    } else if (op->syserrno != EWOULDBLOCK && op->syserrno != EAGAIN) {
        op->result.code = -1;
    } else {
        return PR_FALSE;
    }

    if (rv == 1) {    /* more data to send */
        return PR_FALSE;
    }

    return PR_TRUE;
}
#endif  /* AIX */

#ifdef HPUX11
static PRBool pt_hpux_sendfile_cont(pt_Continuation *op, PRInt16 revents)
{
    struct iovec *hdtrl = (struct iovec *) op->arg2.buffer;
    int count;

    count = sendfile(op->arg1.osfd, op->filedesc, op->arg3.file_spec.offset,
			op->arg3.file_spec.nbytes, hdtrl, op->arg4.flags);
    PR_ASSERT(count <= op->nbytes_to_send);
    op->syserrno = errno;

    if (count != -1) {
        op->result.code += count;
    } else if (op->syserrno != EWOULDBLOCK && op->syserrno != EAGAIN) {
        op->result.code = -1;
    } else {
        return PR_FALSE;
    }
    if (count != -1 && count < op->nbytes_to_send) {
        if (count < hdtrl[0].iov_len) {
			/* header not sent */

            hdtrl[0].iov_base = ((char *) hdtrl[0].iov_base) + count;
            hdtrl[0].iov_len -= count;

        } else if (count < (hdtrl[0].iov_len + op->arg3.file_spec.nbytes)) {
			/* header sent, file not sent */
            PRUint32 file_nbytes_sent = count - hdtrl[0].iov_len;

            hdtrl[0].iov_base = NULL;
            hdtrl[0].iov_len = 0;

            op->arg3.file_spec.offset += file_nbytes_sent;
            op->arg3.file_spec.nbytes -= file_nbytes_sent;
        } else if (count < (hdtrl[0].iov_len + op->arg3.file_spec.nbytes +
											hdtrl[1].iov_len)) {
            PRUint32 trailer_nbytes_sent = count - (hdtrl[0].iov_len +
                                         op->arg3.file_spec.nbytes);

			/* header sent, file sent, trailer not sent */

            hdtrl[0].iov_base = NULL;
            hdtrl[0].iov_len = 0;
			/*
			 * set file offset and len so that no more file data is
			 * sent
			 */
            op->arg3.file_spec.offset = op->arg3.file_spec.st_size;
            op->arg3.file_spec.nbytes = 0;

            hdtrl[1].iov_base =((char *) hdtrl[1].iov_base)+ trailer_nbytes_sent;
            hdtrl[1].iov_len -= trailer_nbytes_sent;
		}
        op->nbytes_to_send -= count;
        return PR_FALSE;
    }

    return PR_TRUE;
}
#endif  /* HPUX11 */

#ifdef SOLARIS  
static PRBool pt_solaris_sendfile_cont(pt_Continuation *op, PRInt16 revents)
{
    struct sendfilevec *vec = (struct sendfilevec *) op->arg2.buffer;
    size_t xferred;
    ssize_t count;

    count = SOLARIS_SENDFILEV(op->arg1.osfd, vec, op->arg3.amount, &xferred);
    op->syserrno = errno;
    PR_ASSERT((count == -1) || (count == xferred));

    if (count == -1) {
        if (op->syserrno != EWOULDBLOCK && op->syserrno != EAGAIN
                && op->syserrno != EINTR) {
            op->result.code = -1;
            return PR_TRUE;
        }
        count = xferred;
    } else if (count == 0) {
        /* 
         * We are now at EOF. The file was truncated. Solaris sendfile is
         * supposed to return 0 and no error in this case, though some versions
         * may return -1 and EINVAL .
         */
        op->result.code = -1;
        op->syserrno = 0; /* will be treated as EOF */
        return PR_TRUE;
    }
    PR_ASSERT(count <= op->nbytes_to_send);
    
    op->result.code += count;
    if (count < op->nbytes_to_send) {
        op->nbytes_to_send -= count;

        while (count >= vec->sfv_len) {
            count -= vec->sfv_len;
            vec++;
            op->arg3.amount--;
        }
        PR_ASSERT(op->arg3.amount > 0);

        vec->sfv_off += count;
        vec->sfv_len -= count;
        PR_ASSERT(vec->sfv_len > 0);
        op->arg2.buffer = vec;

        return PR_FALSE;
    }

    return PR_TRUE;
}
#endif  /* SOLARIS */

#ifdef LINUX 
static PRBool pt_linux_sendfile_cont(pt_Continuation *op, PRInt16 revents)
{
    ssize_t rv;
    off_t oldoffset;

    oldoffset = op->offset;
    rv = sendfile(op->arg1.osfd, op->in_fd, &op->offset, op->count);
    op->syserrno = errno;

    if (rv == -1) {
        if (op->syserrno != EWOULDBLOCK && op->syserrno != EAGAIN) {
            op->result.code = -1;
            return PR_TRUE;
        }
        rv = 0;
    }
    PR_ASSERT(rv == op->offset - oldoffset);
    op->result.code += rv;
    if (rv < op->count) {
        op->count -= rv;
        return PR_FALSE;
    }
    return PR_TRUE;
}
#endif  /* LINUX */

void _PR_InitIO(void)
{
#if defined(DEBUG)
    memset(&pt_debug, 0, sizeof(PTDebug));
    pt_debug.timeStarted = PR_Now();
#endif

    _pr_flock_lock = PR_NewLock();
    PR_ASSERT(NULL != _pr_flock_lock);
    _pr_flock_cv = PR_NewCondVar(_pr_flock_lock);
    PR_ASSERT(NULL != _pr_flock_cv);
    _pr_rename_lock = PR_NewLock();
    PR_ASSERT(NULL != _pr_rename_lock); 

    _PR_InitFdCache();  /* do that */   

    _pr_stdin = pt_SetMethods(0, PR_DESC_FILE, PR_FALSE, PR_TRUE);
    _pr_stdout = pt_SetMethods(1, PR_DESC_FILE, PR_FALSE, PR_TRUE);
    _pr_stderr = pt_SetMethods(2, PR_DESC_FILE, PR_FALSE, PR_TRUE);
    PR_ASSERT(_pr_stdin && _pr_stdout && _pr_stderr);

#ifdef _PR_IPV6_V6ONLY_PROBE
    /* In Mac OS X v10.3 Panther Beta the IPV6_V6ONLY socket option
     * is turned on by default, contrary to what RFC 3493, Section
     * 5.3 says.  So we have to turn it off.  Find out whether we
     * are running on such a system.
     */
    {
        int osfd;
        osfd = socket(AF_INET6, SOCK_STREAM, 0);
        if (osfd != -1) {
            int on;
            socklen_t optlen = sizeof(on);
            if (getsockopt(osfd, IPPROTO_IPV6, IPV6_V6ONLY,
                    &on, &optlen) == 0) {
                _pr_ipv6_v6only_on_by_default = on;
            }
            close(osfd);
        }
    }
#endif
}  /* _PR_InitIO */

void _PR_CleanupIO(void)
{
    _PR_Putfd(_pr_stdin);
    _pr_stdin = NULL;
    _PR_Putfd(_pr_stdout);
    _pr_stdout = NULL;
    _PR_Putfd(_pr_stderr); 
    _pr_stderr = NULL;

    _PR_CleanupFdCache();
    
    if (_pr_flock_cv)
    {
        PR_DestroyCondVar(_pr_flock_cv);
        _pr_flock_cv = NULL;
    }
    if (_pr_flock_lock)
    {
        PR_DestroyLock(_pr_flock_lock);
        _pr_flock_lock = NULL;
    }
    if (_pr_rename_lock)
    {
        PR_DestroyLock(_pr_rename_lock);
        _pr_rename_lock = NULL;
    }
}  /* _PR_CleanupIO */

PR_IMPLEMENT(PRFileDesc*) PR_GetSpecialFD(PRSpecialFD osfd)
{
    PRFileDesc *result = NULL;
    PR_ASSERT(osfd >= PR_StandardInput && osfd <= PR_StandardError);

    if (!_pr_initialized) _PR_ImplicitInitialization();
    
    switch (osfd)
    {
        case PR_StandardInput: result = _pr_stdin; break;
        case PR_StandardOutput: result = _pr_stdout; break;
        case PR_StandardError: result = _pr_stderr; break;
        default:
            (void)PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    }
    return result;
}  /* PR_GetSpecialFD */

/*****************************************************************************/
/***************************** I/O private methods ***************************/
/*****************************************************************************/

static PRBool pt_TestAbort(void)
{
    PRThread *me = PR_GetCurrentThread();
    if(_PT_THREAD_INTERRUPTED(me))
    {
        PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        me->state &= ~PT_THREAD_ABORTED;
        return PR_TRUE;
    }
    return PR_FALSE;
}  /* pt_TestAbort */

static void pt_MapError(void (*mapper)(PRIntn), PRIntn syserrno)
{
    switch (syserrno)
    {
        case EINTR:
            PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0); break;
        case ETIMEDOUT:
            PR_SetError(PR_IO_TIMEOUT_ERROR, 0); break;
        default:
            mapper(syserrno);
    }
}  /* pt_MapError */

static PRStatus pt_Close(PRFileDesc *fd)
{
    if ((NULL == fd) || (NULL == fd->secret)
        || ((_PR_FILEDESC_OPEN != fd->secret->state)
        && (_PR_FILEDESC_CLOSED != fd->secret->state)))
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if (pt_TestAbort()) return PR_FAILURE;

    if (_PR_FILEDESC_OPEN == fd->secret->state)
    {
        if (-1 == close(fd->secret->md.osfd))
        {
#ifdef OSF1
            /*
             * Bug 86941: On Tru64 UNIX V5.0A and V5.1, the close()
             * system call, when called to close a TCP socket, may
             * return -1 with errno set to EINVAL but the system call
             * does close the socket successfully.  An application
             * may safely ignore the EINVAL error.  This bug is fixed
             * on Tru64 UNIX V5.1A and later.  The defect tracking
             * number is QAR 81431.
             */
            if (PR_DESC_SOCKET_TCP != fd->methods->file_type
            || EINVAL != errno)
            {
                pt_MapError(_PR_MD_MAP_CLOSE_ERROR, errno);
                return PR_FAILURE;
            }
#else
            pt_MapError(_PR_MD_MAP_CLOSE_ERROR, errno);
            return PR_FAILURE;
#endif
        }
        fd->secret->state = _PR_FILEDESC_CLOSED;
    }
    _PR_Putfd(fd);
    return PR_SUCCESS;
}  /* pt_Close */

static PRInt32 pt_Read(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    PRInt32 syserrno, bytes = -1;

    if (pt_TestAbort()) return bytes;

    bytes = read(fd->secret->md.osfd, buf, amount);
    syserrno = errno;

    if ((bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking))
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = buf;
        op.arg3.amount = amount;
        op.timeout = PR_INTERVAL_NO_TIMEOUT;
        op.function = pt_read_cont;
        op.event = POLLIN | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_READ_ERROR, syserrno);
    return bytes;
}  /* pt_Read */

static PRInt32 pt_Write(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    PRInt32 syserrno, bytes = -1;
    PRBool fNeedContinue = PR_FALSE;

    if (pt_TestAbort()) return bytes;

    bytes = write(fd->secret->md.osfd, buf, amount);
    syserrno = errno;

    if ( (bytes >= 0) && (bytes < amount) && (!fd->secret->nonblocking) )
    {
        buf = (char *) buf + bytes;
        amount -= bytes;
        fNeedContinue = PR_TRUE;
    }
    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        bytes = 0;
        fNeedContinue = PR_TRUE;
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)buf;
        op.arg3.amount = amount;
        op.timeout = PR_INTERVAL_NO_TIMEOUT;
        op.result.code = bytes;  /* initialize the number sent */
        op.function = pt_write_cont;
        op.event = POLLOUT | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes == -1)
        pt_MapError(_PR_MD_MAP_WRITE_ERROR, syserrno);
    return bytes;
}  /* pt_Write */

static PRInt32 pt_Writev(
    PRFileDesc *fd, const PRIOVec *iov, PRInt32 iov_len, PRIntervalTime timeout)
{
    PRIntn iov_index;
    PRBool fNeedContinue = PR_FALSE;
    PRInt32 syserrno, bytes, rv = -1;
    struct iovec osiov_local[PR_MAX_IOVECTOR_SIZE], *osiov;
    int osiov_len;

    if (pt_TestAbort()) return rv;

    /* Ensured by PR_Writev */
    PR_ASSERT(iov_len <= PR_MAX_IOVECTOR_SIZE);

    /*
     * We can't pass iov to writev because PRIOVec and struct iovec
     * may not be binary compatible.  Make osiov a copy of iov and
     * pass osiov to writev.  We can modify osiov if we need to
     * continue the operation.
     */
    osiov = osiov_local;
    osiov_len = iov_len;
    for (iov_index = 0; iov_index < osiov_len; iov_index++)
    {
        osiov[iov_index].iov_base = iov[iov_index].iov_base;
        osiov[iov_index].iov_len = iov[iov_index].iov_len;
    }

    rv = bytes = writev(fd->secret->md.osfd, osiov, osiov_len);
    syserrno = errno;

    if (!fd->secret->nonblocking)
    {
        if (bytes >= 0)
        {
            /*
             * If we moved some bytes, how does that implicate the
             * i/o vector list?  In other words, exactly where are
             * we within that array?  What are the parameters for
             * resumption?  Maybe we're done!
             */
            for ( ;osiov_len > 0; osiov++, osiov_len--)
            {
                if (bytes < osiov->iov_len)
                {
                    /* this one's not done yet */
                    osiov->iov_base = (char*)osiov->iov_base + bytes;
                    osiov->iov_len -= bytes;
                    break;  /* go off and do that */
                }
                bytes -= osiov->iov_len;  /* this one's done cooked */
            }
            PR_ASSERT(osiov_len > 0 || bytes == 0);
            if (osiov_len > 0)
            {
                if (PR_INTERVAL_NO_WAIT == timeout)
                {
                    rv = -1;
                    syserrno = ETIMEDOUT;
                }
                else fNeedContinue = PR_TRUE;
            }
        }
        else if (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        {
            if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
            else
            {
                rv = 0;
                fNeedContinue = PR_TRUE;
            }
        }
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;

        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)osiov;
        op.arg3.amount = osiov_len;
        op.timeout = timeout;
        op.result.code = rv;
        op.function = pt_writev_cont;
        op.event = POLLOUT | POLLPRI;
        rv = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (rv == -1) pt_MapError(_PR_MD_MAP_WRITEV_ERROR, syserrno);
    return rv;
}  /* pt_Writev */

static PRInt32 pt_Seek(PRFileDesc *fd, PRInt32 offset, PRSeekWhence whence)
{
    return _PR_MD_LSEEK(fd, offset, whence);
}  /* pt_Seek */

static PRInt64 pt_Seek64(PRFileDesc *fd, PRInt64 offset, PRSeekWhence whence)
{
    return _PR_MD_LSEEK64(fd, offset, whence);
}  /* pt_Seek64 */

static PRInt32 pt_Available_f(PRFileDesc *fd)
{
    PRInt32 result, cur, end;

    cur = _PR_MD_LSEEK(fd, 0, PR_SEEK_CUR);

    if (cur >= 0)
        end = _PR_MD_LSEEK(fd, 0, PR_SEEK_END);

    if ((cur < 0) || (end < 0)) {
        return -1;
    }

    result = end - cur;
    _PR_MD_LSEEK(fd, cur, PR_SEEK_SET);

    return result;
}  /* pt_Available_f */

static PRInt64 pt_Available64_f(PRFileDesc *fd)
{
    PRInt64 result, cur, end;
    PRInt64 minus_one;

    LL_I2L(minus_one, -1);
    cur = _PR_MD_LSEEK64(fd, LL_ZERO, PR_SEEK_CUR);

    if (LL_GE_ZERO(cur))
        end = _PR_MD_LSEEK64(fd, LL_ZERO, PR_SEEK_END);

    if (!LL_GE_ZERO(cur) || !LL_GE_ZERO(end)) return minus_one;

    LL_SUB(result, end, cur);
    (void)_PR_MD_LSEEK64(fd, cur, PR_SEEK_SET);

    return result;
}  /* pt_Available64_f */

static PRInt32 pt_Available_s(PRFileDesc *fd)
{
    PRInt32 rv, bytes = -1;
    if (pt_TestAbort()) return bytes;

    rv = ioctl(fd->secret->md.osfd, FIONREAD, &bytes);

    if (rv == -1)
        pt_MapError(_PR_MD_MAP_SOCKETAVAILABLE_ERROR, errno);
    return bytes;
}  /* pt_Available_s */

static PRInt64 pt_Available64_s(PRFileDesc *fd)
{
    PRInt64 rv;
    LL_I2L(rv, pt_Available_s(fd));
    return rv;
}  /* pt_Available64_s */

static PRStatus pt_FileInfo(PRFileDesc *fd, PRFileInfo *info)
{
    PRInt32 rv = _PR_MD_GETOPENFILEINFO(fd, info);
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_FileInfo */

static PRStatus pt_FileInfo64(PRFileDesc *fd, PRFileInfo64 *info)
{
    PRInt32 rv = _PR_MD_GETOPENFILEINFO64(fd, info);
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_FileInfo64 */

static PRStatus pt_Synch(PRFileDesc *fd)
{
    return (NULL == fd) ? PR_FAILURE : PR_SUCCESS;
} /* pt_Synch */

static PRStatus pt_Fsync(PRFileDesc *fd)
{
    PRIntn rv = -1;
    if (pt_TestAbort()) return PR_FAILURE;

    rv = fsync(fd->secret->md.osfd);
    if (rv < 0) {
        pt_MapError(_PR_MD_MAP_FSYNC_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Fsync */

static PRStatus pt_Connect(
    PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
    PRIntn rv = -1, syserrno;
    pt_SockLen addr_len;
	const PRNetAddr *addrp = addr;
#if defined(_PR_HAVE_SOCKADDR_LEN) || defined(_PR_INET6)
	PRUint16 md_af = addr->raw.family;
    PRNetAddr addrCopy;
#endif

    if (pt_TestAbort()) return PR_FAILURE;

    PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
    addr_len = PR_NETADDR_SIZE(addr);
#if defined(_PR_INET6)
	if (addr->raw.family == PR_AF_INET6) {
		md_af = AF_INET6;
#ifndef _PR_HAVE_SOCKADDR_LEN
		addrCopy = *addr;
		addrCopy.raw.family = AF_INET6;
		addrp = &addrCopy;
#endif
	}
#endif

#ifdef _PR_HAVE_SOCKADDR_LEN
    addrCopy = *addr;
    ((struct sockaddr*)&addrCopy)->sa_len = addr_len;
    ((struct sockaddr*)&addrCopy)->sa_family = md_af;
    addrp = &addrCopy;
#endif
    rv = connect(fd->secret->md.osfd, (struct sockaddr*)addrp, addr_len);
    syserrno = errno;
    if ((-1 == rv) && (EINPROGRESS == syserrno) && (!fd->secret->nonblocking))
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            pt_Continuation op;
            op.arg1.osfd = fd->secret->md.osfd;
            op.arg2.buffer = (void*)addrp;
            op.arg3.amount = addr_len;
            op.timeout = timeout;
            op.function = pt_connect_cont;
            op.event = POLLOUT | POLLPRI;
            rv = pt_Continue(&op);
            syserrno = op.syserrno;
        }
    }
    if (-1 == rv) {
        pt_MapError(_PR_MD_MAP_CONNECT_ERROR, syserrno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Connect */

static PRStatus pt_ConnectContinue(
    PRFileDesc *fd, PRInt16 out_flags)
{
    int err;
    PRInt32 osfd;

    if (out_flags & PR_POLL_NVAL)
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    if ((out_flags & (PR_POLL_WRITE | PR_POLL_EXCEPT | PR_POLL_ERR
        | PR_POLL_HUP)) == 0)
    {
        PR_ASSERT(out_flags == 0);
        PR_SetError(PR_IN_PROGRESS_ERROR, 0);
        return PR_FAILURE;
    }

    osfd = fd->secret->md.osfd;

    err = _MD_unix_get_nonblocking_connect_error(osfd);
    if (err != 0)
    {
        _PR_MD_MAP_CONNECT_ERROR(err);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_ConnectContinue */

PR_IMPLEMENT(PRStatus) PR_GetConnectStatus(const PRPollDesc *pd)
{
    /* Find the NSPR layer and invoke its connectcontinue method */
    PRFileDesc *bottom = PR_GetIdentitiesLayer(pd->fd, PR_NSPR_IO_LAYER);

    if (NULL == bottom)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    return pt_ConnectContinue(bottom, pd->out_flags);
}  /* PR_GetConnectStatus */

static PRFileDesc* pt_Accept(
    PRFileDesc *fd, PRNetAddr *addr, PRIntervalTime timeout)
{
    PRFileDesc *newfd = NULL;
    PRIntn syserrno, osfd = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);
#ifdef SYMBIAN
    PRNetAddr dummy_addr;
#endif

    if (pt_TestAbort()) return newfd;

#ifdef SYMBIAN
    /* On Symbian OS, accept crashes if addr is NULL. */
    if (!addr)
        addr = &dummy_addr;
#endif

#ifdef _PR_STRICT_ADDR_LEN
    if (addr)
    {
        /*
         * Set addr->raw.family just so that we can use the
         * PR_NETADDR_SIZE macro.
         */
        addr->raw.family = fd->secret->af;
        addr_len = PR_NETADDR_SIZE(addr);
    }
#endif

    osfd = accept(fd->secret->md.osfd, (struct sockaddr*)addr, &addr_len);
    syserrno = errno;

    if (osfd == -1)
    {
        if (fd->secret->nonblocking) goto failed;

        if (EWOULDBLOCK != syserrno && EAGAIN != syserrno
        && ECONNABORTED != syserrno)
            goto failed;
        else
        {
            if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
            else
            {
                pt_Continuation op;
                op.arg1.osfd = fd->secret->md.osfd;
                op.arg2.buffer = addr;
                op.arg3.addr_len = &addr_len;
                op.timeout = timeout;
                op.function = pt_accept_cont;
                op.event = POLLIN | POLLPRI;
                osfd = pt_Continue(&op);
                syserrno = op.syserrno;
            }
            if (osfd < 0) goto failed;
        }
    }
#ifdef _PR_HAVE_SOCKADDR_LEN
    /* ignore the sa_len field of struct sockaddr */
    if (addr)
    {
        addr->raw.family = ((struct sockaddr*)addr)->sa_family;
    }
#endif /* _PR_HAVE_SOCKADDR_LEN */
#ifdef _PR_INET6
	if (addr && (AF_INET6 == addr->raw.family))
        addr->raw.family = PR_AF_INET6;
#endif
    newfd = pt_SetMethods(osfd, PR_DESC_SOCKET_TCP, PR_TRUE, PR_FALSE);
    if (newfd == NULL) close(osfd);  /* $$$ whoops! this doesn't work $$$ */
    else
    {
        PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
        PR_ASSERT(IsValidNetAddrLen(addr, addr_len) == PR_TRUE);
#ifdef LINUX
        /*
         * On Linux, experiments showed that the accepted sockets
         * inherit the TCP_NODELAY socket option of the listening
         * socket.
         */
        newfd->secret->md.tcp_nodelay = fd->secret->md.tcp_nodelay;
#endif
    }
    return newfd;

failed:
    pt_MapError(_PR_MD_MAP_ACCEPT_ERROR, syserrno);
    return NULL;
}  /* pt_Accept */

static PRStatus pt_Bind(PRFileDesc *fd, const PRNetAddr *addr)
{
    PRIntn rv;
    pt_SockLen addr_len;
	const PRNetAddr *addrp = addr;
#if defined(_PR_HAVE_SOCKADDR_LEN) || defined(_PR_INET6)
	PRUint16 md_af = addr->raw.family;
    PRNetAddr addrCopy;
#endif

    if (pt_TestAbort()) return PR_FAILURE;

    PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
    if (addr->raw.family == AF_UNIX)
    {
        /* Disallow relative pathnames */
        if (addr->local.path[0] != '/')
        {
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return PR_FAILURE;
        }
    }

#if defined(_PR_INET6)
	if (addr->raw.family == PR_AF_INET6) {
		md_af = AF_INET6;
#ifndef _PR_HAVE_SOCKADDR_LEN
		addrCopy = *addr;
		addrCopy.raw.family = AF_INET6;
		addrp = &addrCopy;
#endif
	}
#endif

    addr_len = PR_NETADDR_SIZE(addr);
#ifdef _PR_HAVE_SOCKADDR_LEN
    addrCopy = *addr;
    ((struct sockaddr*)&addrCopy)->sa_len = addr_len;
    ((struct sockaddr*)&addrCopy)->sa_family = md_af;
    addrp = &addrCopy;
#endif
    rv = bind(fd->secret->md.osfd, (struct sockaddr*)addrp, addr_len);

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_BIND_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Bind */

static PRStatus pt_Listen(PRFileDesc *fd, PRIntn backlog)
{
    PRIntn rv;

    if (pt_TestAbort()) return PR_FAILURE;

    rv = listen(fd->secret->md.osfd, backlog);
    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_LISTEN_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Listen */

static PRStatus pt_Shutdown(PRFileDesc *fd, PRIntn how)
{
    PRIntn rv = -1;
    if (pt_TestAbort()) return PR_FAILURE;

    rv = shutdown(fd->secret->md.osfd, how);

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_SHUTDOWN_ERROR, errno);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}  /* pt_Shutdown */

static PRInt16 pt_Poll(PRFileDesc *fd, PRInt16 in_flags, PRInt16 *out_flags)
{
    *out_flags = 0;
    return in_flags;
}  /* pt_Poll */

static PRInt32 pt_Recv(
    PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    PRInt32 syserrno, bytes = -1;
    PRIntn osflags;

    if (0 == flags)
        osflags = 0;
    else if (PR_MSG_PEEK == flags)
    {
#ifdef SYMBIAN
        /* MSG_PEEK doesn't work as expected. */
        PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
        return bytes;
#else
        osflags = MSG_PEEK;
#endif
    }
    else
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return bytes;
    }

    if (pt_TestAbort()) return bytes;

    /* recv() is a much slower call on pre-2.6 Solaris than read(). */
#if defined(SOLARIS)
    if (0 == osflags)
        bytes = read(fd->secret->md.osfd, buf, amount);
    else
        bytes = recv(fd->secret->md.osfd, buf, amount, osflags);
#else
    bytes = recv(fd->secret->md.osfd, buf, amount, osflags);
#endif
    syserrno = errno;

    if ((bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking))
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            pt_Continuation op;
            op.arg1.osfd = fd->secret->md.osfd;
            op.arg2.buffer = buf;
            op.arg3.amount = amount;
            op.arg4.flags = osflags;
            op.timeout = timeout;
            op.function = pt_recv_cont;
            op.event = POLLIN | POLLPRI;
            bytes = pt_Continue(&op);
            syserrno = op.syserrno;
        }
    }
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_RECV_ERROR, syserrno);
    return bytes;
}  /* pt_Recv */

static PRInt32 pt_SocketRead(PRFileDesc *fd, void *buf, PRInt32 amount)
{
    return pt_Recv(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}  /* pt_SocketRead */

static PRInt32 pt_Send(
    PRFileDesc *fd, const void *buf, PRInt32 amount,
    PRIntn flags, PRIntervalTime timeout)
{
    PRInt32 syserrno, bytes = -1;
    PRBool fNeedContinue = PR_FALSE;
#if defined(SOLARIS)
	PRInt32 tmp_amount = amount;
#endif

    /*
     * Under HP-UX DCE threads, pthread.h includes dce/cma_ux.h,
     * which has the following:
     *     #  define send        cma_send
     *     extern int  cma_send (int , void *, int, int );
     * So we need to cast away the 'const' of argument #2 for send().
     */
#if defined (HPUX) && defined(_PR_DCETHREADS)
#define PT_SENDBUF_CAST (void *)
#else
#define PT_SENDBUF_CAST
#endif

    if (pt_TestAbort()) return bytes;

    /*
     * On pre-2.6 Solaris, send() is much slower than write().
     * On 2.6 and beyond, with in-kernel sockets, send() and
     * write() are fairly equivalent in performance.
     */
#if defined(SOLARIS)
    PR_ASSERT(0 == flags);
retry:
    bytes = write(fd->secret->md.osfd, PT_SENDBUF_CAST buf, tmp_amount);
#else
    bytes = send(fd->secret->md.osfd, PT_SENDBUF_CAST buf, amount, flags);
#endif
    syserrno = errno;

#if defined(SOLARIS)
    /*
     * The write system call has been reported to return the ERANGE error
     * on occasion. Try to write in smaller chunks to workaround this bug.
     */
    if ((bytes == -1) && (syserrno == ERANGE))
    {
        if (tmp_amount > 1)
        {
            tmp_amount = tmp_amount/2;  /* half the bytes */
            goto retry;
        }
    }
#endif

    if ( (bytes >= 0) && (bytes < amount) && (!fd->secret->nonblocking) )
    {
        if (PR_INTERVAL_NO_WAIT == timeout)
        {
            bytes = -1;
            syserrno = ETIMEDOUT;
        }
        else
        {
            buf = (char *) buf + bytes;
            amount -= bytes;
            fNeedContinue = PR_TRUE;
        }
    }
    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else
        {
            bytes = 0;
            fNeedContinue = PR_TRUE;
        }
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)buf;
        op.arg3.amount = amount;
        op.arg4.flags = flags;
        op.timeout = timeout;
        op.result.code = bytes;  /* initialize the number sent */
        op.function = pt_send_cont;
        op.event = POLLOUT | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes == -1)
        pt_MapError(_PR_MD_MAP_SEND_ERROR, syserrno);
    return bytes;
}  /* pt_Send */

static PRInt32 pt_SocketWrite(PRFileDesc *fd, const void *buf, PRInt32 amount)
{
    return pt_Send(fd, buf, amount, 0, PR_INTERVAL_NO_TIMEOUT);
}  /* pt_SocketWrite */

static PRInt32 pt_SendTo(
    PRFileDesc *fd, const void *buf,
    PRInt32 amount, PRIntn flags, const PRNetAddr *addr,
    PRIntervalTime timeout)
{
    PRInt32 syserrno, bytes = -1;
    PRBool fNeedContinue = PR_FALSE;
    pt_SockLen addr_len;
	const PRNetAddr *addrp = addr;
#if defined(_PR_HAVE_SOCKADDR_LEN) || defined(_PR_INET6)
	PRUint16 md_af = addr->raw.family;
    PRNetAddr addrCopy;
#endif

    if (pt_TestAbort()) return bytes;

    PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
#if defined(_PR_INET6)
	if (addr->raw.family == PR_AF_INET6) {
		md_af = AF_INET6;
#ifndef _PR_HAVE_SOCKADDR_LEN
		addrCopy = *addr;
		addrCopy.raw.family = AF_INET6;
		addrp = &addrCopy;
#endif
	}
#endif

    addr_len = PR_NETADDR_SIZE(addr);
#ifdef _PR_HAVE_SOCKADDR_LEN
    addrCopy = *addr;
    ((struct sockaddr*)&addrCopy)->sa_len = addr_len;
    ((struct sockaddr*)&addrCopy)->sa_family = md_af;
    addrp = &addrCopy;
#endif
    bytes = sendto(
        fd->secret->md.osfd, buf, amount, flags,
        (struct sockaddr*)addrp, addr_len);
    syserrno = errno;
    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else fNeedContinue = PR_TRUE;
    }
    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = (void*)buf;
        op.arg3.amount = amount;
        op.arg4.flags = flags;
        op.arg5.addr = (PRNetAddr*)addrp;
        op.timeout = timeout;
        op.result.code = 0;  /* initialize the number sent */
        op.function = pt_sendto_cont;
        op.event = POLLOUT | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes < 0)
        pt_MapError(_PR_MD_MAP_SENDTO_ERROR, syserrno);
    return bytes;
}  /* pt_SendTo */

static PRInt32 pt_RecvFrom(PRFileDesc *fd, void *buf, PRInt32 amount,
    PRIntn flags, PRNetAddr *addr, PRIntervalTime timeout)
{
    PRBool fNeedContinue = PR_FALSE;
    PRInt32 syserrno, bytes = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);

    if (pt_TestAbort()) return bytes;

    bytes = recvfrom(
        fd->secret->md.osfd, buf, amount, flags,
        (struct sockaddr*)addr, &addr_len);
    syserrno = errno;

    if ( (bytes == -1) && (syserrno == EWOULDBLOCK || syserrno == EAGAIN)
        && (!fd->secret->nonblocking) )
    {
        if (PR_INTERVAL_NO_WAIT == timeout) syserrno = ETIMEDOUT;
        else fNeedContinue = PR_TRUE;
    }

    if (fNeedContinue == PR_TRUE)
    {
        pt_Continuation op;
        op.arg1.osfd = fd->secret->md.osfd;
        op.arg2.buffer = buf;
        op.arg3.amount = amount;
        op.arg4.flags = flags;
        op.arg5.addr = addr;
        op.timeout = timeout;
        op.function = pt_recvfrom_cont;
        op.event = POLLIN | POLLPRI;
        bytes = pt_Continue(&op);
        syserrno = op.syserrno;
    }
    if (bytes >= 0)
    {
#ifdef _PR_HAVE_SOCKADDR_LEN
        /* ignore the sa_len field of struct sockaddr */
        if (addr)
        {
            addr->raw.family = ((struct sockaddr*)addr)->sa_family;
        }
#endif /* _PR_HAVE_SOCKADDR_LEN */
#ifdef _PR_INET6
        if (addr && (AF_INET6 == addr->raw.family))
            addr->raw.family = PR_AF_INET6;
#endif
    }
    else
        pt_MapError(_PR_MD_MAP_RECVFROM_ERROR, syserrno);
    return bytes;
}  /* pt_RecvFrom */

#ifdef AIX
#ifndef HAVE_SEND_FILE
static pthread_once_t pt_aix_sendfile_once_block = PTHREAD_ONCE_INIT;

static void pt_aix_sendfile_init_routine(void)
{
    void *handle = dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);
    pt_aix_sendfile_fptr = (ssize_t (*)()) dlsym(handle, "send_file");
    dlclose(handle);
}

/* 
 * pt_AIXDispatchSendFile
 */
static PRInt32 pt_AIXDispatchSendFile(PRFileDesc *sd, PRSendFileData *sfd,
	  PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    int rv;

    rv = pthread_once(&pt_aix_sendfile_once_block,
            pt_aix_sendfile_init_routine);
    PR_ASSERT(0 == rv);
    if (pt_aix_sendfile_fptr) {
        return pt_AIXSendFile(sd, sfd, flags, timeout);
    } else {
        return PR_EmulateSendFile(sd, sfd, flags, timeout);
    }
}
#endif /* !HAVE_SEND_FILE */


/*
 * pt_AIXSendFile
 *
 *    Send file sfd->fd across socket sd. If specified, header and trailer
 *    buffers are sent before and after the file, respectively. 
 *
 *    PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *    
 *    return number of bytes sent or -1 on error
 *
 *      This implementation takes advantage of the send_file() system
 *      call available in AIX 4.3.2.
 */

static PRInt32 pt_AIXSendFile(PRFileDesc *sd, PRSendFileData *sfd, 
		PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    struct sf_parms sf_struct;
    uint_t send_flags;
    ssize_t rv;
    int syserrno;
    PRInt32 count;
	unsigned long long saved_file_offset;
	long long saved_file_bytes;

    sf_struct.header_data = (void *) sfd->header;  /* cast away the 'const' */
    sf_struct.header_length = sfd->hlen;
    sf_struct.file_descriptor = sfd->fd->secret->md.osfd;
    sf_struct.file_size = 0;
    sf_struct.file_offset = sfd->file_offset;
    if (sfd->file_nbytes == 0)
    	sf_struct.file_bytes = -1;
	else
    	sf_struct.file_bytes = sfd->file_nbytes;
    sf_struct.trailer_data = (void *) sfd->trailer;
    sf_struct.trailer_length = sfd->tlen;
    sf_struct.bytes_sent = 0;

	saved_file_offset = sf_struct.file_offset;
    saved_file_bytes = sf_struct.file_bytes;

    send_flags = 0;			/* flags processed at the end */

    /* The first argument to send_file() is int*. */
    PR_ASSERT(sizeof(int) == sizeof(sd->secret->md.osfd));
    do {
        rv = AIX_SEND_FILE(&sd->secret->md.osfd, &sf_struct, send_flags);
    } while (rv == -1 && (syserrno = errno) == EINTR);

    if (rv == -1) {
        if (syserrno == EAGAIN || syserrno == EWOULDBLOCK) {
            count = 0; /* Not a real error.  Need to continue. */
        } else {
            count = -1;
        }
    } else {
        count = sf_struct.bytes_sent;
		/*
		 * A bug in AIX 4.3.2 prevents the 'file_bytes' field from
		 * being updated. So, 'file_bytes' is maintained by NSPR to
		 * avoid conflict when this bug is fixed in AIX, in the future.
		 */
		if (saved_file_bytes != -1)
			saved_file_bytes -= (sf_struct.file_offset - saved_file_offset);
		sf_struct.file_bytes = saved_file_bytes;
    }

    if ((rv == 1) || ((rv == -1) && (count == 0))) {
        pt_Continuation op;

        op.arg1.osfd = sd->secret->md.osfd;
        op.arg2.buffer = &sf_struct;
        op.arg4.flags = send_flags;
        op.result.code = count;
        op.timeout = timeout;
        op.function = pt_aix_sendfile_cont;
        op.event = POLLOUT | POLLPRI;
        count = pt_Continue(&op);
        syserrno = op.syserrno;
    }

    if (count == -1) {
        pt_MapError(_MD_aix_map_sendfile_error, syserrno);
        return -1;
    }
    if (flags & PR_TRANSMITFILE_CLOSE_SOCKET) {
        PR_Close(sd);
    }
	PR_ASSERT(count == (sfd->hlen + sfd->tlen +
						((sfd->file_nbytes ==  0) ?
						sf_struct.file_size - sfd->file_offset :
						sfd->file_nbytes)));
    return count;
}
#endif /* AIX */

#ifdef HPUX11
/*
 * pt_HPUXSendFile
 *
 *    Send file sfd->fd across socket sd. If specified, header and trailer
 *    buffers are sent before and after the file, respectively.
 *
 *    PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *    
 *    return number of bytes sent or -1 on error
 *
 *      This implementation takes advantage of the sendfile() system
 *      call available in HP-UX B.11.00.
 */

static PRInt32 pt_HPUXSendFile(PRFileDesc *sd, PRSendFileData *sfd, 
		PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    struct stat statbuf;
    size_t nbytes_to_send, file_nbytes_to_send;
    struct iovec hdtrl[2];  /* optional header and trailer buffers */
    int send_flags;
    PRInt32 count;
    int syserrno;

    if (sfd->file_nbytes == 0) {
        /* Get file size */
        if (fstat(sfd->fd->secret->md.osfd, &statbuf) == -1) {
            _PR_MD_MAP_FSTAT_ERROR(errno);
            return -1;
        } 		
        file_nbytes_to_send = statbuf.st_size - sfd->file_offset;
    } else {
        file_nbytes_to_send = sfd->file_nbytes;
    }
    nbytes_to_send = sfd->hlen + sfd->tlen + file_nbytes_to_send;

    hdtrl[0].iov_base = (void *) sfd->header;  /* cast away the 'const' */
    hdtrl[0].iov_len = sfd->hlen;
    hdtrl[1].iov_base = (void *) sfd->trailer;
    hdtrl[1].iov_len = sfd->tlen;
    /*
     * SF_DISCONNECT seems to close the socket even if sendfile()
     * only does a partial send on a nonblocking socket.  This
     * would prevent the subsequent sendfile() calls on that socket
     * from working.  So we don't use the SD_DISCONNECT flag.
     */
    send_flags = 0;

    do {
        count = sendfile(sd->secret->md.osfd, sfd->fd->secret->md.osfd,
                sfd->file_offset, file_nbytes_to_send, hdtrl, send_flags);
    } while (count == -1 && (syserrno = errno) == EINTR);

    if (count == -1 && (syserrno == EAGAIN || syserrno == EWOULDBLOCK)) {
        count = 0;
    }
    if (count != -1 && count < nbytes_to_send) {
        pt_Continuation op;

        if (count < sfd->hlen) {
			/* header not sent */

            hdtrl[0].iov_base = ((char *) sfd->header) + count;
            hdtrl[0].iov_len = sfd->hlen - count;
            op.arg3.file_spec.offset = sfd->file_offset;
            op.arg3.file_spec.nbytes = file_nbytes_to_send;
        } else if (count < (sfd->hlen + file_nbytes_to_send)) {
			/* header sent, file not sent */

            hdtrl[0].iov_base = NULL;
            hdtrl[0].iov_len = 0;

            op.arg3.file_spec.offset = sfd->file_offset + count - sfd->hlen;
            op.arg3.file_spec.nbytes = file_nbytes_to_send - (count - sfd->hlen);
        } else if (count < (sfd->hlen + file_nbytes_to_send + sfd->tlen)) {
			PRUint32 trailer_nbytes_sent;

			/* header sent, file sent, trailer not sent */

            hdtrl[0].iov_base = NULL;
            hdtrl[0].iov_len = 0;
			/*
			 * set file offset and len so that no more file data is
			 * sent
			 */
            op.arg3.file_spec.offset = statbuf.st_size;
            op.arg3.file_spec.nbytes = 0;

			trailer_nbytes_sent = count - sfd->hlen - file_nbytes_to_send;
            hdtrl[1].iov_base = ((char *) sfd->trailer) + trailer_nbytes_sent;
            hdtrl[1].iov_len = sfd->tlen - trailer_nbytes_sent;
		}

        op.arg1.osfd = sd->secret->md.osfd;
        op.filedesc = sfd->fd->secret->md.osfd;
        op.arg2.buffer = hdtrl;
        op.arg3.file_spec.st_size = statbuf.st_size;
        op.arg4.flags = send_flags;
        op.nbytes_to_send = nbytes_to_send - count;
        op.result.code = count;
        op.timeout = timeout;
        op.function = pt_hpux_sendfile_cont;
        op.event = POLLOUT | POLLPRI;
        count = pt_Continue(&op);
        syserrno = op.syserrno;
    }

    if (count == -1) {
        pt_MapError(_MD_hpux_map_sendfile_error, syserrno);
        return -1;
    }
    if (flags & PR_TRANSMITFILE_CLOSE_SOCKET) {
        PR_Close(sd);
    }
    PR_ASSERT(count == nbytes_to_send);
    return count;
}

#endif  /* HPUX11 */

#ifdef SOLARIS 

/*
 *    pt_SolarisSendFile
 *
 *    Send file sfd->fd across socket sd. If specified, header and trailer
 *    buffers are sent before and after the file, respectively.
 *
 *    PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *
 *    return number of bytes sent or -1 on error
 *
 *    This implementation takes advantage of the sendfilev() system
 *    call available in Solaris 8.
 */

static PRInt32 pt_SolarisSendFile(PRFileDesc *sd, PRSendFileData *sfd,
                PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    struct stat statbuf;
    size_t nbytes_to_send, file_nbytes_to_send;	
    struct sendfilevec sfv_struct[3];  
    int sfvcnt = 0;	
    size_t xferred;
    PRInt32 count;
    int syserrno;

    if (sfd->file_nbytes == 0) {
        /* Get file size */
        if (fstat(sfd->fd->secret->md.osfd, &statbuf) == -1) {
            _PR_MD_MAP_FSTAT_ERROR(errno);
            return -1;
        } 		
        file_nbytes_to_send = statbuf.st_size - sfd->file_offset;
    } else {
        file_nbytes_to_send = sfd->file_nbytes;
    }

    nbytes_to_send = sfd->hlen + sfd->tlen + file_nbytes_to_send;

    if (sfd->hlen != 0) {
        sfv_struct[sfvcnt].sfv_fd = SFV_FD_SELF;
        sfv_struct[sfvcnt].sfv_flag = 0;
        sfv_struct[sfvcnt].sfv_off = (off_t) sfd->header; 
        sfv_struct[sfvcnt].sfv_len = sfd->hlen;
        sfvcnt++;
    }

    if (file_nbytes_to_send != 0) {
        sfv_struct[sfvcnt].sfv_fd = sfd->fd->secret->md.osfd;
        sfv_struct[sfvcnt].sfv_flag = 0;
        sfv_struct[sfvcnt].sfv_off = sfd->file_offset;
        sfv_struct[sfvcnt].sfv_len = file_nbytes_to_send;
        sfvcnt++;
    }

    if (sfd->tlen != 0) {
        sfv_struct[sfvcnt].sfv_fd = SFV_FD_SELF;
        sfv_struct[sfvcnt].sfv_flag = 0;
        sfv_struct[sfvcnt].sfv_off = (off_t) sfd->trailer; 
        sfv_struct[sfvcnt].sfv_len = sfd->tlen;
        sfvcnt++;
    }

    if (0 == sfvcnt) {
        count = 0;
        goto done;
    }
   	   
    /*
     * Strictly speaking, we may have sent some bytes when the
     * sendfilev() is interrupted and we should retry it from an
     * updated offset.  We are not doing that here.
     */
    count = SOLARIS_SENDFILEV(sd->secret->md.osfd, sfv_struct,
            sfvcnt, &xferred);

    PR_ASSERT((count == -1) || (count == xferred));

    if (count == -1) {
        syserrno = errno;
        if (syserrno == EINTR
                || syserrno == EAGAIN || syserrno == EWOULDBLOCK) {
            count = xferred;
        }
    } else if (count == 0) {
        /*
         * We are now at EOF. The file was truncated. Solaris sendfile is
         * supposed to return 0 and no error in this case, though some versions
         * may return -1 and EINVAL .
         */
        count = -1;
        syserrno = 0;  /* will be treated as EOF */
    }

    if (count != -1 && count < nbytes_to_send) {
        pt_Continuation op;
        struct sendfilevec *vec = sfv_struct;
        PRInt32 rem = count;

        while (rem >= vec->sfv_len) {
            rem -= vec->sfv_len;
            vec++;
            sfvcnt--;
        }
        PR_ASSERT(sfvcnt > 0);

        vec->sfv_off += rem;
        vec->sfv_len -= rem;
        PR_ASSERT(vec->sfv_len > 0);

        op.arg1.osfd = sd->secret->md.osfd;
        op.arg2.buffer = vec;
        op.arg3.amount = sfvcnt;
        op.arg4.flags = 0;
        op.nbytes_to_send = nbytes_to_send - count;
        op.result.code = count;
        op.timeout = timeout;
        op.function = pt_solaris_sendfile_cont;
        op.event = POLLOUT | POLLPRI;
        count = pt_Continue(&op);
        syserrno = op.syserrno;
    }

done:
    if (count == -1) {
        pt_MapError(_MD_solaris_map_sendfile_error, syserrno);
        return -1;
    }
    if (flags & PR_TRANSMITFILE_CLOSE_SOCKET) {
        PR_Close(sd);
    }
    PR_ASSERT(count == nbytes_to_send);
    return count;
}

#ifndef HAVE_SENDFILEV
static pthread_once_t pt_solaris_sendfilev_once_block = PTHREAD_ONCE_INIT;

static void pt_solaris_sendfilev_init_routine(void)
{
    void *handle;
    PRBool close_it = PR_FALSE;
 
    /*
     * We do not want to unload libsendfile.so.  This handle is leaked
     * intentionally.
     */
    handle = dlopen("libsendfile.so", RTLD_LAZY | RTLD_GLOBAL);
    PR_LOG(_pr_io_lm, PR_LOG_DEBUG,
        ("dlopen(libsendfile.so) returns %p", handle));

    if (NULL == handle) {
        /*
         * The dlopen(0, mode) call is to allow for the possibility that
         * sendfilev() may become part of a standard system library in a
         * future Solaris release.
         */
        handle = dlopen(0, RTLD_LAZY | RTLD_GLOBAL);
        PR_LOG(_pr_io_lm, PR_LOG_DEBUG,
            ("dlopen(0) returns %p", handle));
        close_it = PR_TRUE;
    }
    pt_solaris_sendfilev_fptr = (ssize_t (*)()) dlsym(handle, "sendfilev");
    PR_LOG(_pr_io_lm, PR_LOG_DEBUG,
        ("dlsym(sendfilev) returns %p", pt_solaris_sendfilev_fptr));
    
    if (close_it) {
        dlclose(handle);
    }
}

/* 
 * pt_SolarisDispatchSendFile
 */
static PRInt32 pt_SolarisDispatchSendFile(PRFileDesc *sd, PRSendFileData *sfd,
	  PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    int rv;

    rv = pthread_once(&pt_solaris_sendfilev_once_block,
            pt_solaris_sendfilev_init_routine);
    PR_ASSERT(0 == rv);
    if (pt_solaris_sendfilev_fptr) {
        return pt_SolarisSendFile(sd, sfd, flags, timeout);
    } else {
        return PR_EmulateSendFile(sd, sfd, flags, timeout);
    }
}
#endif /* !HAVE_SENDFILEV */

#endif  /* SOLARIS */

#ifdef LINUX
/*
 * pt_LinuxSendFile
 *
 *    Send file sfd->fd across socket sd. If specified, header and trailer
 *    buffers are sent before and after the file, respectively.
 *
 *    PR_TRANSMITFILE_CLOSE_SOCKET flag - close socket after sending file
 *    
 *    return number of bytes sent or -1 on error
 *
 *      This implementation takes advantage of the sendfile() system
 *      call available in Linux kernel 2.2 or higher.
 */

static PRInt32 pt_LinuxSendFile(PRFileDesc *sd, PRSendFileData *sfd,
                PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    struct stat statbuf;
    size_t file_nbytes_to_send;	
    PRInt32 count = 0;
    ssize_t rv;
    int syserrno;
    off_t offset;
    PRBool tcp_cork_enabled = PR_FALSE;
    int tcp_cork;

    if (sfd->file_nbytes == 0) {
        /* Get file size */
        if (fstat(sfd->fd->secret->md.osfd, &statbuf) == -1) {
            _PR_MD_MAP_FSTAT_ERROR(errno);
            return -1;
        } 		
        file_nbytes_to_send = statbuf.st_size - sfd->file_offset;
    } else {
        file_nbytes_to_send = sfd->file_nbytes;
    }

    if ((sfd->hlen != 0 || sfd->tlen != 0)
            && sd->secret->md.tcp_nodelay == 0) {
        tcp_cork = 1;
        if (setsockopt(sd->secret->md.osfd, SOL_TCP, TCP_CORK,
                &tcp_cork, sizeof tcp_cork) == 0) {
            tcp_cork_enabled = PR_TRUE;
        } else {
            syserrno = errno;
            if (syserrno != EINVAL) {
                _PR_MD_MAP_SETSOCKOPT_ERROR(syserrno);
                return -1;
            }
            /*
             * The most likely reason for the EINVAL error is that
             * TCP_NODELAY is set (with a function other than
             * PR_SetSocketOption).  This is not fatal, so we keep
             * on going.
             */
            PR_LOG(_pr_io_lm, PR_LOG_WARNING,
                ("pt_LinuxSendFile: "
                "setsockopt(TCP_CORK) failed with EINVAL\n"));
        }
    }

    if (sfd->hlen != 0) {
        count = PR_Send(sd, sfd->header, sfd->hlen, 0, timeout);
        if (count == -1) {
            goto failed;
        }
    }

    if (file_nbytes_to_send != 0) {
        offset = sfd->file_offset;
        do {
            rv = sendfile(sd->secret->md.osfd, sfd->fd->secret->md.osfd,
                &offset, file_nbytes_to_send);
        } while (rv == -1 && (syserrno = errno) == EINTR);
        if (rv == -1) {
            if (syserrno != EAGAIN && syserrno != EWOULDBLOCK) {
                _MD_linux_map_sendfile_error(syserrno);
                count = -1;
                goto failed;
            }
            rv = 0;
        }
        PR_ASSERT(rv == offset - sfd->file_offset);
        count += rv;

        if (rv < file_nbytes_to_send) {
            pt_Continuation op;

            op.arg1.osfd = sd->secret->md.osfd;
            op.in_fd = sfd->fd->secret->md.osfd;
            op.offset = offset;
            op.count = file_nbytes_to_send - rv;
            op.result.code = count;
            op.timeout = timeout;
            op.function = pt_linux_sendfile_cont;
            op.event = POLLOUT | POLLPRI;
            count = pt_Continue(&op);
            syserrno = op.syserrno;
            if (count == -1) {
                pt_MapError(_MD_linux_map_sendfile_error, syserrno);
                goto failed;
            }
        }
    }

    if (sfd->tlen != 0) {
        rv = PR_Send(sd, sfd->trailer, sfd->tlen, 0, timeout);
        if (rv == -1) {
            count = -1;
            goto failed;
        }
        count += rv;
    }

failed:
    if (tcp_cork_enabled) {
        tcp_cork = 0;
        if (setsockopt(sd->secret->md.osfd, SOL_TCP, TCP_CORK,
                &tcp_cork, sizeof tcp_cork) == -1 && count != -1) {
            _PR_MD_MAP_SETSOCKOPT_ERROR(errno);
            count = -1;
        }
    }
    if (count != -1) {
        if (flags & PR_TRANSMITFILE_CLOSE_SOCKET) {
            PR_Close(sd);
        }
        PR_ASSERT(count == sfd->hlen + sfd->tlen + file_nbytes_to_send);
    }
    return count;
}
#endif  /* LINUX */

#ifdef AIX
extern	int _pr_aix_send_file_use_disabled;
#endif

static PRInt32 pt_SendFile(
    PRFileDesc *sd, PRSendFileData *sfd,
    PRTransmitFileFlags flags, PRIntervalTime timeout)
{
    if (pt_TestAbort()) return -1;
    /* The socket must be in blocking mode. */
    if (sd->secret->nonblocking)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return -1;
    }
#ifdef HPUX11
    return(pt_HPUXSendFile(sd, sfd, flags, timeout));
#elif defined(AIX)
#ifdef HAVE_SEND_FILE
	/*
	 * A bug in AIX 4.3.2 results in corruption of data transferred by
	 * send_file(); AIX patch PTF U463956 contains the fix.  A user can
	 * disable the use of send_file function in NSPR, when this patch is
	 * not installed on the system, by setting the envionment variable
	 * NSPR_AIX_SEND_FILE_USE_DISABLED to 1.
	 */
	if (_pr_aix_send_file_use_disabled)
		return(PR_EmulateSendFile(sd, sfd, flags, timeout));
	else
    	return(pt_AIXSendFile(sd, sfd, flags, timeout));
#else
	return(PR_EmulateSendFile(sd, sfd, flags, timeout));
    /* return(pt_AIXDispatchSendFile(sd, sfd, flags, timeout));*/
#endif /* HAVE_SEND_FILE */
#elif defined(SOLARIS)
#ifdef HAVE_SENDFILEV
    	return(pt_SolarisSendFile(sd, sfd, flags, timeout));
#else
	return(pt_SolarisDispatchSendFile(sd, sfd, flags, timeout));
#endif /* HAVE_SENDFILEV */
#elif defined(LINUX)
    	return(pt_LinuxSendFile(sd, sfd, flags, timeout));
#else
	return(PR_EmulateSendFile(sd, sfd, flags, timeout));
#endif
}

static PRInt32 pt_TransmitFile(
    PRFileDesc *sd, PRFileDesc *fd, const void *headers,
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

	return(pt_SendFile(sd, &sfd, flags, timeout));
}  /* pt_TransmitFile */

static PRInt32 pt_AcceptRead(
    PRFileDesc *sd, PRFileDesc **nd, PRNetAddr **raddr,
    void *buf, PRInt32 amount, PRIntervalTime timeout)
{
    PRInt32 rv = -1;

    if (pt_TestAbort()) return rv;
    /* The socket must be in blocking mode. */
    if (sd->secret->nonblocking)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return rv;
    }

    rv = PR_EmulateAcceptRead(sd, nd, raddr, buf, amount, timeout);
    return rv;
}  /* pt_AcceptRead */

static PRStatus pt_GetSockName(PRFileDesc *fd, PRNetAddr *addr)
{
    PRIntn rv = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);

    if (pt_TestAbort()) return PR_FAILURE;

    rv = getsockname(
        fd->secret->md.osfd, (struct sockaddr*)addr, &addr_len);
    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_GETSOCKNAME_ERROR, errno);
        return PR_FAILURE;
    } else {
#ifdef _PR_HAVE_SOCKADDR_LEN
        /* ignore the sa_len field of struct sockaddr */
        if (addr)
        {
            addr->raw.family = ((struct sockaddr*)addr)->sa_family;
        }
#endif /* _PR_HAVE_SOCKADDR_LEN */
#ifdef _PR_INET6
		if (AF_INET6 == addr->raw.family)
			addr->raw.family = PR_AF_INET6;
#endif
        PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
        PR_ASSERT(IsValidNetAddrLen(addr, addr_len) == PR_TRUE);
        return PR_SUCCESS;
    }
}  /* pt_GetSockName */

static PRStatus pt_GetPeerName(PRFileDesc *fd, PRNetAddr *addr)
{
    PRIntn rv = -1;
    pt_SockLen addr_len = sizeof(PRNetAddr);

    if (pt_TestAbort()) return PR_FAILURE;

    rv = getpeername(
        fd->secret->md.osfd, (struct sockaddr*)addr, &addr_len);

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_GETPEERNAME_ERROR, errno);
        return PR_FAILURE;
    } else {
#ifdef _PR_HAVE_SOCKADDR_LEN
        /* ignore the sa_len field of struct sockaddr */
        if (addr)
        {
            addr->raw.family = ((struct sockaddr*)addr)->sa_family;
        }
#endif /* _PR_HAVE_SOCKADDR_LEN */
#ifdef _PR_INET6
		if (AF_INET6 == addr->raw.family)
        	addr->raw.family = PR_AF_INET6;
#endif
        PR_ASSERT(IsValidNetAddr(addr) == PR_TRUE);
        PR_ASSERT(IsValidNetAddrLen(addr, addr_len) == PR_TRUE);
        return PR_SUCCESS;
    }
}  /* pt_GetPeerName */

static PRStatus pt_GetSocketOption(PRFileDesc *fd, PRSocketOptionData *data)
{
    PRIntn rv;
    pt_SockLen length;
    PRInt32 level, name;

    /*
     * PR_SockOpt_Nonblocking is a special case that does not
     * translate to a getsockopt() call
     */
    if (PR_SockOpt_Nonblocking == data->option)
    {
        data->value.non_blocking = fd->secret->nonblocking;
        return PR_SUCCESS;
    }

    rv = _PR_MapOptionName(data->option, &level, &name);
    if (PR_SUCCESS == rv)
    {
        switch (data->option)
        {
            case PR_SockOpt_Linger:
            {
                struct linger linger;
                length = sizeof(linger);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char *) &linger, &length);
                PR_ASSERT((-1 == rv) || (sizeof(linger) == length));
                data->value.linger.polarity =
                    (linger.l_onoff) ? PR_TRUE : PR_FALSE;
                data->value.linger.linger =
                    PR_SecondsToInterval(linger.l_linger);
                break;
            }
            case PR_SockOpt_Reuseaddr:
            case PR_SockOpt_Keepalive:
            case PR_SockOpt_NoDelay:
            case PR_SockOpt_Broadcast:
            {
                PRIntn value;
                length = sizeof(PRIntn);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char*)&value, &length);
                PR_ASSERT((-1 == rv) || (sizeof(PRIntn) == length));
                data->value.reuse_addr = (0 == value) ? PR_FALSE : PR_TRUE;
                break;
            }
            case PR_SockOpt_McastLoopback:
            {
                PRUint8 xbool;
                length = sizeof(xbool);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&xbool, &length);
                PR_ASSERT((-1 == rv) || (sizeof(xbool) == length));
                data->value.mcast_loopback = (0 == xbool) ? PR_FALSE : PR_TRUE;
                break;
            }
            case PR_SockOpt_RecvBufferSize:
            case PR_SockOpt_SendBufferSize:
            case PR_SockOpt_MaxSegment:
            {
                PRIntn value;
                length = sizeof(PRIntn);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char*)&value, &length);
                PR_ASSERT((-1 == rv) || (sizeof(PRIntn) == length));
                data->value.recv_buffer_size = value;
                break;
            }
            case PR_SockOpt_IpTimeToLive:
            case PR_SockOpt_IpTypeOfService:
            {
                length = sizeof(PRUintn);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.ip_ttl, &length);
                PR_ASSERT((-1 == rv) || (sizeof(PRIntn) == length));
                break;
            }
            case PR_SockOpt_McastTimeToLive:
            {
                PRUint8 ttl;
                length = sizeof(ttl);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&ttl, &length);
                PR_ASSERT((-1 == rv) || (sizeof(ttl) == length));
                data->value.mcast_ttl = ttl;
                break;
            }
            case PR_SockOpt_AddMember:
            case PR_SockOpt_DropMember:
            {
                struct ip_mreq mreq;
                length = sizeof(mreq);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name, (char*)&mreq, &length);
                PR_ASSERT((-1 == rv) || (sizeof(mreq) == length));
                data->value.add_member.mcaddr.inet.ip =
                    mreq.imr_multiaddr.s_addr;
                data->value.add_member.ifaddr.inet.ip =
                    mreq.imr_interface.s_addr;
                break;
            }
            case PR_SockOpt_McastInterface:
            {
                length = sizeof(data->value.mcast_if.inet.ip);
                rv = getsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.mcast_if.inet.ip, &length);
                PR_ASSERT((-1 == rv)
                    || (sizeof(data->value.mcast_if.inet.ip) == length));
                break;
            }
            default:
                PR_NOT_REACHED("Unknown socket option");
                break;
        }
        if (-1 == rv) _PR_MD_MAP_GETSOCKOPT_ERROR(errno);
    }
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_GetSocketOption */

static PRStatus pt_SetSocketOption(PRFileDesc *fd, const PRSocketOptionData *data)
{
    PRIntn rv;
    PRInt32 level, name;

    /*
     * PR_SockOpt_Nonblocking is a special case that does not
     * translate to a setsockopt call.
     */
    if (PR_SockOpt_Nonblocking == data->option)
    {
        fd->secret->nonblocking = data->value.non_blocking;
        return PR_SUCCESS;
    }

    rv = _PR_MapOptionName(data->option, &level, &name);
    if (PR_SUCCESS == rv)
    {
        switch (data->option)
        {
            case PR_SockOpt_Linger:
            {
                struct linger linger;
                linger.l_onoff = data->value.linger.polarity;
                linger.l_linger = PR_IntervalToSeconds(data->value.linger.linger);
                rv = setsockopt(
                    fd->secret->md.osfd, level, name, (char*)&linger, sizeof(linger));
                break;
            }
            case PR_SockOpt_Reuseaddr:
            case PR_SockOpt_Keepalive:
            case PR_SockOpt_NoDelay:
            case PR_SockOpt_Broadcast:
            {
                PRIntn value = (data->value.reuse_addr) ? 1 : 0;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&value, sizeof(PRIntn));
#ifdef LINUX
                /* for pt_LinuxSendFile */
                if (name == TCP_NODELAY && rv == 0) {
                    fd->secret->md.tcp_nodelay = value;
                }
#endif
                break;
            }
            case PR_SockOpt_McastLoopback:
            {
                PRUint8 xbool = data->value.mcast_loopback ? 1 : 0;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&xbool, sizeof(xbool));
                break;
            }
            case PR_SockOpt_RecvBufferSize:
            case PR_SockOpt_SendBufferSize:
            case PR_SockOpt_MaxSegment:
            {
                PRIntn value = data->value.recv_buffer_size;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&value, sizeof(PRIntn));
                break;
            }
            case PR_SockOpt_IpTimeToLive:
            case PR_SockOpt_IpTypeOfService:
            {
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.ip_ttl, sizeof(PRUintn));
                break;
            }
            case PR_SockOpt_McastTimeToLive:
            {
                PRUint8 ttl = data->value.mcast_ttl;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&ttl, sizeof(ttl));
                break;
            }
            case PR_SockOpt_AddMember:
            case PR_SockOpt_DropMember:
            {
                struct ip_mreq mreq;
                mreq.imr_multiaddr.s_addr =
                    data->value.add_member.mcaddr.inet.ip;
                mreq.imr_interface.s_addr =
                    data->value.add_member.ifaddr.inet.ip;
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&mreq, sizeof(mreq));
                break;
            }
            case PR_SockOpt_McastInterface:
            {
                rv = setsockopt(
                    fd->secret->md.osfd, level, name,
                    (char*)&data->value.mcast_if.inet.ip,
                    sizeof(data->value.mcast_if.inet.ip));
                break;
            }
            default:
                PR_NOT_REACHED("Unknown socket option");
                break;
        }
        if (-1 == rv) _PR_MD_MAP_SETSOCKOPT_ERROR(errno);
    }
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* pt_SetSocketOption */

/*****************************************************************************/
/****************************** I/O method objects ***************************/
/*****************************************************************************/

static PRIOMethods _pr_file_methods = {
    PR_DESC_FILE,
    pt_Close,
    pt_Read,
    pt_Write,
    pt_Available_f,
    pt_Available64_f,
    pt_Fsync,
    pt_Seek,
    pt_Seek64,
    pt_FileInfo,
    pt_FileInfo64,
    (PRWritevFN)_PR_InvalidInt,        
    (PRConnectFN)_PR_InvalidStatus,        
    (PRAcceptFN)_PR_InvalidDesc,        
    (PRBindFN)_PR_InvalidStatus,        
    (PRListenFN)_PR_InvalidStatus,        
    (PRShutdownFN)_PR_InvalidStatus,    
    (PRRecvFN)_PR_InvalidInt,        
    (PRSendFN)_PR_InvalidInt,        
    (PRRecvfromFN)_PR_InvalidInt,    
    (PRSendtoFN)_PR_InvalidInt,        
    pt_Poll,
    (PRAcceptreadFN)_PR_InvalidInt,   
    (PRTransmitfileFN)_PR_InvalidInt, 
    (PRGetsocknameFN)_PR_InvalidStatus,    
    (PRGetpeernameFN)_PR_InvalidStatus,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRSendfileFN)_PR_InvalidInt, 
    (PRConnectcontinueFN)_PR_InvalidStatus, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt
};

static PRIOMethods _pr_pipe_methods = {
    PR_DESC_PIPE,
    pt_Close,
    pt_Read,
    pt_Write,
    pt_Available_s,
    pt_Available64_s,
    pt_Synch,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    (PRWritevFN)_PR_InvalidInt,        
    (PRConnectFN)_PR_InvalidStatus,        
    (PRAcceptFN)_PR_InvalidDesc,        
    (PRBindFN)_PR_InvalidStatus,        
    (PRListenFN)_PR_InvalidStatus,        
    (PRShutdownFN)_PR_InvalidStatus,    
    (PRRecvFN)_PR_InvalidInt,        
    (PRSendFN)_PR_InvalidInt,        
    (PRRecvfromFN)_PR_InvalidInt,    
    (PRSendtoFN)_PR_InvalidInt,        
    pt_Poll,
    (PRAcceptreadFN)_PR_InvalidInt,   
    (PRTransmitfileFN)_PR_InvalidInt, 
    (PRGetsocknameFN)_PR_InvalidStatus,    
    (PRGetpeernameFN)_PR_InvalidStatus,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRSendfileFN)_PR_InvalidInt, 
    (PRConnectcontinueFN)_PR_InvalidStatus, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt
};

static PRIOMethods _pr_tcp_methods = {
    PR_DESC_SOCKET_TCP,
    pt_Close,
    pt_SocketRead,
    pt_SocketWrite,
    pt_Available_s,
    pt_Available64_s,
    pt_Synch,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    pt_Writev,
    pt_Connect,
    pt_Accept,
    pt_Bind,
    pt_Listen,
    pt_Shutdown,
    pt_Recv,
    pt_Send,
    (PRRecvfromFN)_PR_InvalidInt,
    (PRSendtoFN)_PR_InvalidInt,
    pt_Poll,
    pt_AcceptRead,
    pt_TransmitFile,
    pt_GetSockName,
    pt_GetPeerName,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    pt_GetSocketOption,
    pt_SetSocketOption,
    pt_SendFile, 
    pt_ConnectContinue,
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt
};

static PRIOMethods _pr_udp_methods = {
    PR_DESC_SOCKET_UDP,
    pt_Close,
    pt_SocketRead,
    pt_SocketWrite,
    pt_Available_s,
    pt_Available64_s,
    pt_Synch,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    pt_Writev,
    pt_Connect,
    (PRAcceptFN)_PR_InvalidDesc,
    pt_Bind,
    pt_Listen,
    pt_Shutdown,
    pt_Recv,
    pt_Send,
    pt_RecvFrom,
    pt_SendTo,
    pt_Poll,
    (PRAcceptreadFN)_PR_InvalidInt,
    (PRTransmitfileFN)_PR_InvalidInt,
    pt_GetSockName,
    pt_GetPeerName,
    (PRReservedFN)_PR_InvalidInt,
    (PRReservedFN)_PR_InvalidInt,
    pt_GetSocketOption,
    pt_SetSocketOption,
    (PRSendfileFN)_PR_InvalidInt, 
    (PRConnectcontinueFN)_PR_InvalidStatus, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt
};

static PRIOMethods _pr_socketpollfd_methods = {
    (PRDescType) 0,
    (PRCloseFN)_PR_InvalidStatus,
    (PRReadFN)_PR_InvalidInt,
    (PRWriteFN)_PR_InvalidInt,
    (PRAvailableFN)_PR_InvalidInt,
    (PRAvailable64FN)_PR_InvalidInt64,
    (PRFsyncFN)_PR_InvalidStatus,
    (PRSeekFN)_PR_InvalidInt,
    (PRSeek64FN)_PR_InvalidInt64,
    (PRFileInfoFN)_PR_InvalidStatus,
    (PRFileInfo64FN)_PR_InvalidStatus,
    (PRWritevFN)_PR_InvalidInt,        
    (PRConnectFN)_PR_InvalidStatus,        
    (PRAcceptFN)_PR_InvalidDesc,        
    (PRBindFN)_PR_InvalidStatus,        
    (PRListenFN)_PR_InvalidStatus,        
    (PRShutdownFN)_PR_InvalidStatus,    
    (PRRecvFN)_PR_InvalidInt,        
    (PRSendFN)_PR_InvalidInt,        
    (PRRecvfromFN)_PR_InvalidInt,    
    (PRSendtoFN)_PR_InvalidInt,        
	pt_Poll,
    (PRAcceptreadFN)_PR_InvalidInt,   
    (PRTransmitfileFN)_PR_InvalidInt, 
    (PRGetsocknameFN)_PR_InvalidStatus,    
    (PRGetpeernameFN)_PR_InvalidStatus,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRReservedFN)_PR_InvalidInt,    
    (PRGetsocketoptionFN)_PR_InvalidStatus,
    (PRSetsocketoptionFN)_PR_InvalidStatus,
    (PRSendfileFN)_PR_InvalidInt, 
    (PRConnectcontinueFN)_PR_InvalidStatus, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt, 
    (PRReservedFN)_PR_InvalidInt
};

#if defined(HPUX) || defined(OSF1) || defined(SOLARIS) || defined (IRIX) \
    || defined(LINUX) || defined(__GNU__) || defined(__GLIBC__) \
    || defined(AIX) || defined(FREEBSD) || defined(NETBSD) \
    || defined(OPENBSD) || defined(BSDI) || defined(NTO) \
    || defined(DARWIN) || defined(UNIXWARE) || defined(RISCOS) \
    || defined(SYMBIAN)
#define _PR_FCNTL_FLAGS O_NONBLOCK
#else
#error "Can't determine architecture"
#endif

/*
 * Put a Unix file descriptor in non-blocking mode.
 */
static void pt_MakeFdNonblock(PRIntn osfd)
{
    PRIntn flags;
    flags = fcntl(osfd, F_GETFL, 0);
    flags |= _PR_FCNTL_FLAGS;
    (void)fcntl(osfd, F_SETFL, flags);
}

/*
 * Put a Unix socket fd in non-blocking mode that can
 * ideally be inherited by an accepted socket.
 *
 * Why doesn't pt_MakeFdNonblock do?  This is to deal with
 * the special case of HP-UX.  HP-UX has three kinds of
 * non-blocking modes for sockets: the fcntl() O_NONBLOCK
 * and O_NDELAY flags and ioctl() FIOSNBIO request.  Only
 * the ioctl() FIOSNBIO form of non-blocking mode is
 * inherited by an accepted socket.
 *
 * Other platforms just use the generic pt_MakeFdNonblock
 * to put a socket in non-blocking mode.
 */
#ifdef HPUX
static void pt_MakeSocketNonblock(PRIntn osfd)
{
    PRIntn one = 1;
    (void)ioctl(osfd, FIOSNBIO, &one);
}
#else
#define pt_MakeSocketNonblock pt_MakeFdNonblock
#endif

static PRFileDesc *pt_SetMethods(
    PRIntn osfd, PRDescType type, PRBool isAcceptedSocket, PRBool imported)
{
    PRFileDesc *fd = _PR_Getfd();
    
    if (fd == NULL) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        fd->secret->md.osfd = osfd;
        fd->secret->state = _PR_FILEDESC_OPEN;
        if (imported) fd->secret->inheritable = _PR_TRI_UNKNOWN;
        else
        {
            /* By default, a Unix fd is not closed on exec. */
#ifdef DEBUG
            PRIntn flags;
            flags = fcntl(osfd, F_GETFD, 0);
            PR_ASSERT(0 == flags);
#endif
            fd->secret->inheritable = _PR_TRI_TRUE;
        }
        switch (type)
        {
            case PR_DESC_FILE:
                fd->methods = PR_GetFileMethods();
                break;
            case PR_DESC_SOCKET_TCP:
                fd->methods = PR_GetTCPMethods();
#ifdef _PR_ACCEPT_INHERIT_NONBLOCK
                if (!isAcceptedSocket) pt_MakeSocketNonblock(osfd);
#else
                pt_MakeSocketNonblock(osfd);
#endif
                break;
            case PR_DESC_SOCKET_UDP:
                fd->methods = PR_GetUDPMethods();
                pt_MakeFdNonblock(osfd);
                break;
            case PR_DESC_PIPE:
                fd->methods = PR_GetPipeMethods();
                pt_MakeFdNonblock(osfd);
                break;
            default:
                break;
        }
    }
    return fd;
}  /* pt_SetMethods */

PR_IMPLEMENT(const PRIOMethods*) PR_GetFileMethods(void)
{
    return &_pr_file_methods;
}  /* PR_GetFileMethods */

PR_IMPLEMENT(const PRIOMethods*) PR_GetPipeMethods(void)
{
    return &_pr_pipe_methods;
}  /* PR_GetPipeMethods */

PR_IMPLEMENT(const PRIOMethods*) PR_GetTCPMethods(void)
{
    return &_pr_tcp_methods;
}  /* PR_GetTCPMethods */

PR_IMPLEMENT(const PRIOMethods*) PR_GetUDPMethods(void)
{
    return &_pr_udp_methods;
}  /* PR_GetUDPMethods */

static const PRIOMethods* PR_GetSocketPollFdMethods(void)
{
    return &_pr_socketpollfd_methods;
}  /* PR_GetSocketPollFdMethods */

PR_IMPLEMENT(PRFileDesc*) PR_AllocFileDesc(
    PRInt32 osfd, const PRIOMethods *methods)
{
    PRFileDesc *fd = _PR_Getfd();

    if (NULL == fd) goto failed;

    fd->methods = methods;
    fd->secret->md.osfd = osfd;
    /* Make fd non-blocking */
    if (osfd > 2)
    {
        /* Don't mess around with stdin, stdout or stderr */
        if (&_pr_tcp_methods == methods) pt_MakeSocketNonblock(osfd);
        else pt_MakeFdNonblock(osfd);
    }
    fd->secret->state = _PR_FILEDESC_OPEN;
    fd->secret->inheritable = _PR_TRI_UNKNOWN;
    return fd;
    
failed:
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    return fd;
}  /* PR_AllocFileDesc */

#if !defined(_PR_INET6) || defined(_PR_INET6_PROBE)
PR_EXTERN(PRStatus) _pr_push_ipv6toipv4_layer(PRFileDesc *fd);
#if defined(_PR_INET6_PROBE)
extern PRBool _pr_ipv6_is_present(void);
PR_IMPLEMENT(PRBool) _pr_test_ipv6_socket()
{
    int osfd;

#if defined(DARWIN)
    /*
     * Disable IPv6 if Darwin version is less than 7.0.0 (OS X 10.3).  IPv6 on
     * lesser versions is not ready for general use (see bug 222031).
     */
    {
        struct utsname u;
        if (uname(&u) != 0 || atoi(u.release) < 7)
            return PR_FALSE;
    }
#endif

    /*
     * HP-UX only: HP-UX IPv6 Porting Guide (dated February 2001)
     * suggests that we call open("/dev/ip6", O_RDWR) to determine
     * whether IPv6 APIs and the IPv6 stack are on the system.
     * Our portable test below seems to work fine, so I am using it.
     */
    osfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (osfd != -1) {
        close(osfd);
        return PR_TRUE;
    }
    return PR_FALSE;
}
#endif	/* _PR_INET6_PROBE */
#endif

PR_IMPLEMENT(PRFileDesc*) PR_Socket(PRInt32 domain, PRInt32 type, PRInt32 proto)
{
    PRIntn osfd;
    PRDescType ftype;
    PRFileDesc *fd = NULL;
#if defined(_PR_INET6_PROBE) || !defined(_PR_INET6)
    PRInt32 tmp_domain = domain;
#endif

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return NULL;

    if (PF_INET != domain
        && PR_AF_INET6 != domain
#if defined(_PR_HAVE_SDP)
        && PR_AF_INET_SDP != domain
#if defined(SOLARIS)
        && PR_AF_INET6_SDP != domain
#endif /* SOLARIS */
#endif /* _PR_HAVE_SDP */
        && PF_UNIX != domain)
    {
        PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, 0);
        return fd;
    }
	if (type == SOCK_STREAM) ftype = PR_DESC_SOCKET_TCP;
	else if (type == SOCK_DGRAM) ftype = PR_DESC_SOCKET_UDP;
	else
	{
		(void)PR_SetError(PR_ADDRESS_NOT_SUPPORTED_ERROR, 0);
		return fd;
	}
#if defined(_PR_HAVE_SDP)
#if defined(LINUX)
    if (PR_AF_INET_SDP == domain)
        domain = AF_INET_SDP;
#elif defined(SOLARIS)
    if (PR_AF_INET_SDP == domain) {
        domain = AF_INET;
        proto = PROTO_SDP;
    } else if(PR_AF_INET6_SDP == domain) {
        domain = AF_INET6;
        proto = PROTO_SDP;
    }
#endif /* SOLARIS */
#endif /* _PR_HAVE_SDP */
#if defined(_PR_INET6_PROBE)
	if (PR_AF_INET6 == domain)
		domain = _pr_ipv6_is_present() ? AF_INET6 : AF_INET;
#elif defined(_PR_INET6) 
	if (PR_AF_INET6 == domain)
		domain = AF_INET6;
#else
	if (PR_AF_INET6 == domain)
		domain = AF_INET;
#endif

    osfd = socket(domain, type, proto);
    if (osfd == -1) pt_MapError(_PR_MD_MAP_SOCKET_ERROR, errno);
    else
    {
#ifdef _PR_IPV6_V6ONLY_PROBE
        if ((domain == AF_INET6) && _pr_ipv6_v6only_on_by_default)
        {
            int on = 0;
            (void)setsockopt(osfd, IPPROTO_IPV6, IPV6_V6ONLY,
                    &on, sizeof(on));
        }
#endif
        fd = pt_SetMethods(osfd, ftype, PR_FALSE, PR_FALSE);
        if (fd == NULL) close(osfd);
    }
#ifdef _PR_NEED_SECRET_AF
    if (fd != NULL) fd->secret->af = domain;
#endif
#if defined(_PR_INET6_PROBE) || !defined(_PR_INET6)
	if (fd != NULL) {
		/*
		 * For platforms with no support for IPv6 
		 * create layered socket for IPv4-mapped IPv6 addresses
		 */
		if (PR_AF_INET6 == tmp_domain && PR_AF_INET == domain) {
			if (PR_FAILURE == _pr_push_ipv6toipv4_layer(fd)) {
				PR_Close(fd);
				fd = NULL;
			}
		}
	}
#endif
    return fd;
}  /* PR_Socket */

/*****************************************************************************/
/****************************** I/O public methods ***************************/
/*****************************************************************************/

PR_IMPLEMENT(PRFileDesc*) PR_OpenFile(
    const char *name, PRIntn flags, PRIntn mode)
{
    PRFileDesc *fd = NULL;
    PRIntn syserrno, osfd = -1, osflags = 0;;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return NULL;

    if (flags & PR_RDONLY) osflags |= O_RDONLY;
    if (flags & PR_WRONLY) osflags |= O_WRONLY;
    if (flags & PR_RDWR) osflags |= O_RDWR;
    if (flags & PR_APPEND) osflags |= O_APPEND;
    if (flags & PR_TRUNCATE) osflags |= O_TRUNC;
    if (flags & PR_EXCL) osflags |= O_EXCL;
    if (flags & PR_SYNC)
    {
#if defined(O_SYNC)
        osflags |= O_SYNC;
#elif defined(O_FSYNC)
        osflags |= O_FSYNC;
#else
#error "Neither O_SYNC nor O_FSYNC is defined on this platform"
#endif
    }

    /*
    ** We have to hold the lock across the creation in order to
    ** enforce the sematics of PR_Rename(). (see the latter for
    ** more details)
    */
    if (flags & PR_CREATE_FILE)
    {
        osflags |= O_CREAT;
        if (NULL !=_pr_rename_lock)
            PR_Lock(_pr_rename_lock);
    }

    osfd = _md_iovector._open64(name, osflags, mode);
    syserrno = errno;

    if ((flags & PR_CREATE_FILE) && (NULL !=_pr_rename_lock))
        PR_Unlock(_pr_rename_lock);

    if (osfd == -1)
        pt_MapError(_PR_MD_MAP_OPEN_ERROR, syserrno);
    else
    {
        fd = pt_SetMethods(osfd, PR_DESC_FILE, PR_FALSE, PR_FALSE);
        if (fd == NULL) close(osfd);  /* $$$ whoops! this is bad $$$ */
    }
    return fd;
}  /* PR_OpenFile */

PR_IMPLEMENT(PRFileDesc*) PR_Open(const char *name, PRIntn flags, PRIntn mode)
{
    return PR_OpenFile(name, flags, mode);
}  /* PR_Open */

PR_IMPLEMENT(PRStatus) PR_Delete(const char *name)
{
    PRIntn rv = -1;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    if (pt_TestAbort()) return PR_FAILURE;

    rv = unlink(name);

    if (rv == -1) {
        pt_MapError(_PR_MD_MAP_UNLINK_ERROR, errno);
        return PR_FAILURE;
    } else
        return PR_SUCCESS;
}  /* PR_Delete */

PR_IMPLEMENT(PRStatus) PR_Access(const char *name, PRAccessHow how)
{
    PRIntn rv;

    if (pt_TestAbort()) return PR_FAILURE;

    switch (how)
    {
    case PR_ACCESS_READ_OK:
        rv =  access(name, R_OK);
        break;
    case PR_ACCESS_WRITE_OK:
        rv = access(name, W_OK);
        break;
    case PR_ACCESS_EXISTS:
    default:
        rv = access(name, F_OK);
    }
    if (0 == rv) return PR_SUCCESS;
    pt_MapError(_PR_MD_MAP_ACCESS_ERROR, errno);
    return PR_FAILURE;
    
}  /* PR_Access */

PR_IMPLEMENT(PRStatus) PR_GetFileInfo(const char *fn, PRFileInfo *info)
{
    PRInt32 rv = _PR_MD_GETFILEINFO(fn, info);
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_GetFileInfo */

PR_IMPLEMENT(PRStatus) PR_GetFileInfo64(const char *fn, PRFileInfo64 *info)
{
    PRInt32 rv;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    rv = _PR_MD_GETFILEINFO64(fn, info);
    return (0 == rv) ? PR_SUCCESS : PR_FAILURE;
}  /* PR_GetFileInfo64 */

PR_IMPLEMENT(PRStatus) PR_Rename(const char *from, const char *to)
{
    PRIntn rv = -1;

    if (pt_TestAbort()) return PR_FAILURE;

    /*
    ** We have to acquire a lock here to stiffle anybody trying to create
    ** a new file at the same time. And we have to hold that lock while we
    ** test to see if the file exists and do the rename. The other place
    ** where the lock is held is in PR_Open() when possibly creating a 
    ** new file.
    */

    PR_Lock(_pr_rename_lock);
    rv = access(to, F_OK);
    if (0 == rv)
    {
        PR_SetError(PR_FILE_EXISTS_ERROR, 0);
        rv = -1;
    }
    else
    {
        rv = rename(from, to);
        if (rv == -1)
            pt_MapError(_PR_MD_MAP_RENAME_ERROR, errno);
    }
    PR_Unlock(_pr_rename_lock);
    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* PR_Rename */

PR_IMPLEMENT(PRStatus) PR_CloseDir(PRDir *dir)
{
    if (pt_TestAbort()) return PR_FAILURE;

    if (NULL != dir->md.d)
    {
        if (closedir(dir->md.d) == -1)
        {
            _PR_MD_MAP_CLOSEDIR_ERROR(errno);
            return PR_FAILURE;
        }
        dir->md.d = NULL;
        PR_DELETE(dir);
    }
    return PR_SUCCESS;
}  /* PR_CloseDir */

PR_IMPLEMENT(PRStatus) PR_MakeDir(const char *name, PRIntn mode)
{
    PRInt32 rv = -1;

    if (pt_TestAbort()) return PR_FAILURE;

    /*
    ** This lock is used to enforce rename semantics as described
    ** in PR_Rename.
    */
    if (NULL !=_pr_rename_lock)
        PR_Lock(_pr_rename_lock);
    rv = mkdir(name, mode);
    if (-1 == rv)
        pt_MapError(_PR_MD_MAP_MKDIR_ERROR, errno);
    if (NULL !=_pr_rename_lock)
        PR_Unlock(_pr_rename_lock);

    return (-1 == rv) ? PR_FAILURE : PR_SUCCESS;
}  /* PR_Makedir */

PR_IMPLEMENT(PRStatus) PR_MkDir(const char *name, PRIntn mode)
{
    return PR_MakeDir(name, mode);
}  /* PR_Mkdir */

PR_IMPLEMENT(PRStatus) PR_RmDir(const char *name)
{
    PRInt32 rv;

    if (pt_TestAbort()) return PR_FAILURE;

    rv = rmdir(name);
    if (0 == rv) {
    return PR_SUCCESS;
    } else {
    pt_MapError(_PR_MD_MAP_RMDIR_ERROR, errno);
    return PR_FAILURE;
    }
}  /* PR_Rmdir */


PR_IMPLEMENT(PRDir*) PR_OpenDir(const char *name)
{
    DIR *osdir;
    PRDir *dir = NULL;

    if (pt_TestAbort()) return dir;

    osdir = opendir(name);
    if (osdir == NULL)
        pt_MapError(_PR_MD_MAP_OPENDIR_ERROR, errno);
    else
    {
        dir = PR_NEWZAP(PRDir);
        if (dir)
            dir->md.d = osdir;
        else
            (void)closedir(osdir);
    }
    return dir;
}  /* PR_OpenDir */

static PRInt32 _pr_poll_with_poll(
    PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRInt32 ready = 0;
    /*
     * For restarting poll() if it is interrupted by a signal.
     * We use these variables to figure out how much time has
     * elapsed and how much of the timeout still remains.
     */
    PRIntervalTime start, elapsed, remaining;

    if (pt_TestAbort()) return -1;

    if (0 == npds) PR_Sleep(timeout);
    else
    {
#define STACK_POLL_DESC_COUNT 64
        struct pollfd stack_syspoll[STACK_POLL_DESC_COUNT];
        struct pollfd *syspoll;
        PRIntn index, msecs;

        if (npds <= STACK_POLL_DESC_COUNT)
        {
            syspoll = stack_syspoll;
        }
        else
        {
            PRThread *me = PR_GetCurrentThread();
            if (npds > me->syspoll_count)
            {
                PR_Free(me->syspoll_list);
                me->syspoll_list =
                    (struct pollfd*)PR_MALLOC(npds * sizeof(struct pollfd));
                if (NULL == me->syspoll_list)
                {
                    me->syspoll_count = 0;
                    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
                    return -1;
                }
                me->syspoll_count = npds;
            }
            syspoll = me->syspoll_list;
        }

        for (index = 0; index < npds; ++index)
        {
            PRInt16 in_flags_read = 0, in_flags_write = 0;
            PRInt16 out_flags_read = 0, out_flags_write = 0;

            if ((NULL != pds[index].fd) && (0 != pds[index].in_flags))
            {
                if (pds[index].in_flags & PR_POLL_READ)
                {
                    in_flags_read = (pds[index].fd->methods->poll)(
                        pds[index].fd,
                        pds[index].in_flags & ~PR_POLL_WRITE,
                        &out_flags_read);
                }
                if (pds[index].in_flags & PR_POLL_WRITE)
                {
                    in_flags_write = (pds[index].fd->methods->poll)(
                        pds[index].fd,
                        pds[index].in_flags & ~PR_POLL_READ,
                        &out_flags_write);
                }
                if ((0 != (in_flags_read & out_flags_read))
                || (0 != (in_flags_write & out_flags_write)))
                {
                    /* this one is ready right now */
                    if (0 == ready)
                    {
                        /*
                         * We will return without calling the system
                         * poll function.  So zero the out_flags
                         * fields of all the poll descriptors before
                         * this one.
                         */
                        int i;
                        for (i = 0; i < index; i++)
                        {
                            pds[i].out_flags = 0;
                        }
                    }
                    ready += 1;
                    pds[index].out_flags = out_flags_read | out_flags_write;
                }
                else
                {
                    /* now locate the NSPR layer at the bottom of the stack */
                    PRFileDesc *bottom = PR_GetIdentitiesLayer(
                        pds[index].fd, PR_NSPR_IO_LAYER);
                    PR_ASSERT(NULL != bottom);  /* what to do about that? */
                    pds[index].out_flags = 0;  /* pre-condition */
                    if ((NULL != bottom)
                    && (_PR_FILEDESC_OPEN == bottom->secret->state))
                    {
                        if (0 == ready)
                        {
                            syspoll[index].fd = bottom->secret->md.osfd;
                            syspoll[index].events = 0;
                            if (in_flags_read & PR_POLL_READ)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_READ_SYS_READ;
                                syspoll[index].events |= POLLIN;
                            }
                            if (in_flags_read & PR_POLL_WRITE)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_READ_SYS_WRITE;
                                syspoll[index].events |= POLLOUT;
                            }
                            if (in_flags_write & PR_POLL_READ)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_WRITE_SYS_READ;
                                syspoll[index].events |= POLLIN;
                            }
                            if (in_flags_write & PR_POLL_WRITE)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_WRITE_SYS_WRITE;
                                syspoll[index].events |= POLLOUT;
                            }
                            if (pds[index].in_flags & PR_POLL_EXCEPT)
                                syspoll[index].events |= POLLPRI;
                        }
                    }
                    else
                    {
                        if (0 == ready)
                        {
                            int i;
                            for (i = 0; i < index; i++)
                            {
                                pds[i].out_flags = 0;
                            }
                        }
                        ready += 1;  /* this will cause an abrupt return */
                        pds[index].out_flags = PR_POLL_NVAL;  /* bogii */
                    }
                }
            }
            else
            {
                /* make poll() ignore this entry */
                syspoll[index].fd = -1;
                syspoll[index].events = 0;
                pds[index].out_flags = 0;
            }
        }
        if (0 == ready)
        {
            switch (timeout)
            {
            case PR_INTERVAL_NO_WAIT: msecs = 0; break;
            case PR_INTERVAL_NO_TIMEOUT: msecs = -1; break;
            default:
                msecs = PR_IntervalToMilliseconds(timeout);
                start = PR_IntervalNow();
            }

retry:
            ready = poll(syspoll, npds, msecs);
            if (-1 == ready)
            {
                PRIntn oserror = errno;

                if (EINTR == oserror)
                {
                    if (timeout == PR_INTERVAL_NO_TIMEOUT)
                        goto retry;
                    else if (timeout == PR_INTERVAL_NO_WAIT)
                        ready = 0;  /* don't retry, just time out */
                    else
                    {
                        elapsed = (PRIntervalTime) (PR_IntervalNow()
                                - start);
                        if (elapsed > timeout)
                            ready = 0;  /* timed out */
                        else
                        {
                            remaining = timeout - elapsed;
                            msecs = PR_IntervalToMilliseconds(remaining);
                            goto retry;
                        }
                    }
                }
                else
                {
                    _PR_MD_MAP_POLL_ERROR(oserror);
                }
            }
            else if (ready > 0)
            {
                for (index = 0; index < npds; ++index)
                {
                    PRInt16 out_flags = 0;
                    if ((NULL != pds[index].fd) && (0 != pds[index].in_flags))
                    {
                        if (0 != syspoll[index].revents)
                        {
                            if (syspoll[index].revents & POLLIN)
                            {
                                if (pds[index].out_flags
                                & _PR_POLL_READ_SYS_READ)
                                {
                                    out_flags |= PR_POLL_READ;
                                }
                                if (pds[index].out_flags
                                & _PR_POLL_WRITE_SYS_READ)
                                {
                                    out_flags |= PR_POLL_WRITE;
                                }
                            }
                            if (syspoll[index].revents & POLLOUT)
                            {
                                if (pds[index].out_flags
                                & _PR_POLL_READ_SYS_WRITE)
                                {
                                    out_flags |= PR_POLL_READ;
                                }
                                if (pds[index].out_flags
                                & _PR_POLL_WRITE_SYS_WRITE)
                                {
                                    out_flags |= PR_POLL_WRITE;
                                }
                            }
                            if (syspoll[index].revents & POLLPRI)
                                out_flags |= PR_POLL_EXCEPT;
                            if (syspoll[index].revents & POLLERR)
                                out_flags |= PR_POLL_ERR;
                            if (syspoll[index].revents & POLLNVAL)
                                out_flags |= PR_POLL_NVAL;
                            if (syspoll[index].revents & POLLHUP)
                                out_flags |= PR_POLL_HUP;
                        }
                    }
                    pds[index].out_flags = out_flags;
                }
            }
        }
    }
    return ready;

} /* _pr_poll_with_poll */

#if defined(_PR_POLL_WITH_SELECT)
/*
 * OSF1 and HPUX report the POLLHUP event for a socket when the
 * shutdown(SHUT_WR) operation is called for the remote end, even though
 * the socket is still writeable. Use select(), instead of poll(), to
 * workaround this problem.
 */
static PRInt32 _pr_poll_with_select(
    PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
    PRInt32 ready = 0;
    /*
     * For restarting select() if it is interrupted by a signal.
     * We use these variables to figure out how much time has
     * elapsed and how much of the timeout still remains.
     */
    PRIntervalTime start, elapsed, remaining;

    if (pt_TestAbort()) return -1;

    if (0 == npds) PR_Sleep(timeout);
    else
    {
#define STACK_POLL_DESC_COUNT 64
        int stack_selectfd[STACK_POLL_DESC_COUNT];
        int *selectfd;
		fd_set rd, wr, ex, *rdp = NULL, *wrp = NULL, *exp = NULL;
		struct timeval tv, *tvp;
        PRIntn index, msecs, maxfd = 0;

        if (npds <= STACK_POLL_DESC_COUNT)
        {
            selectfd = stack_selectfd;
        }
        else
        {
            PRThread *me = PR_GetCurrentThread();
            if (npds > me->selectfd_count)
            {
                PR_Free(me->selectfd_list);
                me->selectfd_list = (int *)PR_MALLOC(npds * sizeof(int));
                if (NULL == me->selectfd_list)
                {
                    me->selectfd_count = 0;
                    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
                    return -1;
                }
                me->selectfd_count = npds;
            }
            selectfd = me->selectfd_list;
        }
		FD_ZERO(&rd);
		FD_ZERO(&wr);
		FD_ZERO(&ex);

        for (index = 0; index < npds; ++index)
        {
            PRInt16 in_flags_read = 0, in_flags_write = 0;
            PRInt16 out_flags_read = 0, out_flags_write = 0;

            if ((NULL != pds[index].fd) && (0 != pds[index].in_flags))
            {
                if (pds[index].in_flags & PR_POLL_READ)
                {
                    in_flags_read = (pds[index].fd->methods->poll)(
                        pds[index].fd,
                        pds[index].in_flags & ~PR_POLL_WRITE,
                        &out_flags_read);
                }
                if (pds[index].in_flags & PR_POLL_WRITE)
                {
                    in_flags_write = (pds[index].fd->methods->poll)(
                        pds[index].fd,
                        pds[index].in_flags & ~PR_POLL_READ,
                        &out_flags_write);
                }
                if ((0 != (in_flags_read & out_flags_read))
                || (0 != (in_flags_write & out_flags_write)))
                {
                    /* this one is ready right now */
                    if (0 == ready)
                    {
                        /*
                         * We will return without calling the system
                         * poll function.  So zero the out_flags
                         * fields of all the poll descriptors before
                         * this one.
                         */
                        int i;
                        for (i = 0; i < index; i++)
                        {
                            pds[i].out_flags = 0;
                        }
                    }
                    ready += 1;
                    pds[index].out_flags = out_flags_read | out_flags_write;
                }
                else
                {
                    /* now locate the NSPR layer at the bottom of the stack */
                    PRFileDesc *bottom = PR_GetIdentitiesLayer(
                        pds[index].fd, PR_NSPR_IO_LAYER);
                    PR_ASSERT(NULL != bottom);  /* what to do about that? */
                    pds[index].out_flags = 0;  /* pre-condition */
                    if ((NULL != bottom)
                    && (_PR_FILEDESC_OPEN == bottom->secret->state))
                    {
                        if (0 == ready)
                        {
                            PRBool add_to_rd = PR_FALSE;
                            PRBool add_to_wr = PR_FALSE;
                            PRBool add_to_ex = PR_FALSE;

                            selectfd[index] = bottom->secret->md.osfd;
                            if (in_flags_read & PR_POLL_READ)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_READ_SYS_READ;
                                add_to_rd = PR_TRUE;
                            }
                            if (in_flags_read & PR_POLL_WRITE)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_READ_SYS_WRITE;
                                add_to_wr = PR_TRUE;
                            }
                            if (in_flags_write & PR_POLL_READ)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_WRITE_SYS_READ;
                                add_to_rd = PR_TRUE;
                            }
                            if (in_flags_write & PR_POLL_WRITE)
                            {
                                pds[index].out_flags |=
                                    _PR_POLL_WRITE_SYS_WRITE;
                                add_to_wr = PR_TRUE;
                            }
                            if (pds[index].in_flags & PR_POLL_EXCEPT)
                            {
                                add_to_ex = PR_TRUE;
                            }
                            if ((selectfd[index] > maxfd) &&
                                    (add_to_rd || add_to_wr || add_to_ex))
                            {
                                maxfd = selectfd[index];
                                /*
                                 * If maxfd is too large to be used with
                                 * select, fall back to calling poll.
                                 */
                                if (maxfd >= FD_SETSIZE)
                                    break;
                            }
                            if (add_to_rd)
                            {
                                FD_SET(bottom->secret->md.osfd, &rd);
                                rdp = &rd;
                            }
                            if (add_to_wr)
                            {
                                FD_SET(bottom->secret->md.osfd, &wr);
                                wrp = &wr;
                            }
                            if (add_to_ex)
                            {
                                FD_SET(bottom->secret->md.osfd, &ex);
                                exp = &ex;
                            }
                        }
                    }
                    else
                    {
                        if (0 == ready)
                        {
                            int i;
                            for (i = 0; i < index; i++)
                            {
                                pds[i].out_flags = 0;
                            }
                        }
                        ready += 1;  /* this will cause an abrupt return */
                        pds[index].out_flags = PR_POLL_NVAL;  /* bogii */
                    }
                }
            }
            else
            {
                pds[index].out_flags = 0;
            }
        }
        if (0 == ready)
        {
			if (maxfd >= FD_SETSIZE)
			{
				/*
				 * maxfd too large to be used with select, fall back to
				 * calling poll
				 */
				return(_pr_poll_with_poll(pds, npds, timeout));
			}
            switch (timeout)
            {
            case PR_INTERVAL_NO_WAIT:
				tv.tv_sec = 0;
				tv.tv_usec = 0;
				tvp = &tv;
				break;
            case PR_INTERVAL_NO_TIMEOUT:
				tvp = NULL;
				break;
            default:
                msecs = PR_IntervalToMilliseconds(timeout);
				tv.tv_sec = msecs/PR_MSEC_PER_SEC;
				tv.tv_usec = (msecs % PR_MSEC_PER_SEC) * PR_USEC_PER_MSEC;
				tvp = &tv;
                start = PR_IntervalNow();
            }

retry:
            ready = select(maxfd + 1, rdp, wrp, exp, tvp);
            if (-1 == ready)
            {
                PRIntn oserror = errno;

                if ((EINTR == oserror) || (EAGAIN == oserror))
                {
                    if (timeout == PR_INTERVAL_NO_TIMEOUT)
                        goto retry;
                    else if (timeout == PR_INTERVAL_NO_WAIT)
                        ready = 0;  /* don't retry, just time out */
                    else
                    {
                        elapsed = (PRIntervalTime) (PR_IntervalNow()
                                - start);
                        if (elapsed > timeout)
                            ready = 0;  /* timed out */
                        else
                        {
                            remaining = timeout - elapsed;
                            msecs = PR_IntervalToMilliseconds(remaining);
							tv.tv_sec = msecs/PR_MSEC_PER_SEC;
							tv.tv_usec = (msecs % PR_MSEC_PER_SEC) *
													PR_USEC_PER_MSEC;
                            goto retry;
                        }
                    }
                } else if (EBADF == oserror)
                {
					/* find all the bad fds */
					ready = 0;
                	for (index = 0; index < npds; ++index)
					{
                    	pds[index].out_flags = 0;
            			if ((NULL != pds[index].fd) &&
											(0 != pds[index].in_flags))
						{
							if (fcntl(selectfd[index], F_GETFL, 0) == -1)
							{
                    			pds[index].out_flags = PR_POLL_NVAL;
								ready++;
							}
						}
					}
                } else 
                    _PR_MD_MAP_SELECT_ERROR(oserror);
            }
            else if (ready > 0)
            {
                for (index = 0; index < npds; ++index)
                {
                    PRInt16 out_flags = 0;
                    if ((NULL != pds[index].fd) && (0 != pds[index].in_flags))
                    {
						if (FD_ISSET(selectfd[index], &rd))
						{
							if (pds[index].out_flags
							& _PR_POLL_READ_SYS_READ)
							{
								out_flags |= PR_POLL_READ;
							}
							if (pds[index].out_flags
							& _PR_POLL_WRITE_SYS_READ)
							{
								out_flags |= PR_POLL_WRITE;
							}
						}
						if (FD_ISSET(selectfd[index], &wr))
						{
							if (pds[index].out_flags
							& _PR_POLL_READ_SYS_WRITE)
							{
								out_flags |= PR_POLL_READ;
							}
							if (pds[index].out_flags
							& _PR_POLL_WRITE_SYS_WRITE)
							{
								out_flags |= PR_POLL_WRITE;
							}
						}
						if (FD_ISSET(selectfd[index], &ex))
							out_flags |= PR_POLL_EXCEPT;
                    }
                    pds[index].out_flags = out_flags;
                }
            }
        }
    }
    return ready;

} /* _pr_poll_with_select */
#endif	/* _PR_POLL_WITH_SELECT */

PR_IMPLEMENT(PRInt32) PR_Poll(
    PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
#if defined(_PR_POLL_WITH_SELECT)
	return(_pr_poll_with_select(pds, npds, timeout));
#else
	return(_pr_poll_with_poll(pds, npds, timeout));
#endif
}

PR_IMPLEMENT(PRDirEntry*) PR_ReadDir(PRDir *dir, PRDirFlags flags)
{
    struct dirent *dp;

    if (pt_TestAbort()) return NULL;

    for (;;)
    {
        errno = 0;
        dp = readdir(dir->md.d);
        if (NULL == dp)
        {
            pt_MapError(_PR_MD_MAP_READDIR_ERROR, errno);
            return NULL;
        }
        if ((flags & PR_SKIP_DOT)
            && ('.' == dp->d_name[0])
            && (0 == dp->d_name[1])) continue;
        if ((flags & PR_SKIP_DOT_DOT)
            && ('.' == dp->d_name[0])
            && ('.' == dp->d_name[1])
            && (0 == dp->d_name[2])) continue;
        if ((flags & PR_SKIP_HIDDEN) && ('.' == dp->d_name[0]))
            continue;
        break;
    }
    dir->d.name = dp->d_name;
    return &dir->d;
}  /* PR_ReadDir */

PR_IMPLEMENT(PRFileDesc*) PR_NewUDPSocket(void)
{
    PRIntn domain = PF_INET;

    return PR_Socket(domain, SOCK_DGRAM, 0);
}  /* PR_NewUDPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_NewTCPSocket(void)
{
    PRIntn domain = PF_INET;

    return PR_Socket(domain, SOCK_STREAM, 0);
}  /* PR_NewTCPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_OpenUDPSocket(PRIntn af)
{
    return PR_Socket(af, SOCK_DGRAM, 0);
}  /* PR_NewUDPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_OpenTCPSocket(PRIntn af)
{
    return PR_Socket(af, SOCK_STREAM, 0);
}  /* PR_NewTCPSocket */

PR_IMPLEMENT(PRStatus) PR_NewTCPSocketPair(PRFileDesc *fds[2])
{
#ifdef SYMBIAN
    /*
     * For the platforms that don't have socketpair.
     *
     * Copied from prsocket.c, with the parameter f[] renamed fds[] and the
     * _PR_CONNECT_DOES_NOT_BIND code removed.
     */
    PRFileDesc *listenSock;
    PRNetAddr selfAddr, peerAddr;
    PRUint16 port;

    fds[0] = fds[1] = NULL;
    listenSock = PR_NewTCPSocket();
    if (listenSock == NULL) {
        goto failed;
    }
    PR_InitializeNetAddr(PR_IpAddrLoopback, 0, &selfAddr); /* BugZilla: 35408 */
    if (PR_Bind(listenSock, &selfAddr) == PR_FAILURE) {
        goto failed;
    }
    if (PR_GetSockName(listenSock, &selfAddr) == PR_FAILURE) {
        goto failed;
    }
    port = ntohs(selfAddr.inet.port);
    if (PR_Listen(listenSock, 5) == PR_FAILURE) {
        goto failed;
    }
    fds[0] = PR_NewTCPSocket();
    if (fds[0] == NULL) {
        goto failed;
    }
    PR_InitializeNetAddr(PR_IpAddrLoopback, port, &selfAddr);

    /*
     * Only a thread is used to do the connect and accept.
     * I am relying on the fact that PR_Connect returns
     * successfully as soon as the connect request is put
     * into the listen queue (but before PR_Accept is called).
     * This is the behavior of the BSD socket code.  If
     * connect does not return until accept is called, we
     * will need to create another thread to call connect.
     */
    if (PR_Connect(fds[0], &selfAddr, PR_INTERVAL_NO_TIMEOUT)
            == PR_FAILURE) {
        goto failed;
    }
    /*
     * A malicious local process may connect to the listening
     * socket, so we need to verify that the accepted connection
     * is made from our own socket fds[0].
     */
    if (PR_GetSockName(fds[0], &selfAddr) == PR_FAILURE) {
        goto failed;
    }
    fds[1] = PR_Accept(listenSock, &peerAddr, PR_INTERVAL_NO_TIMEOUT);
    if (fds[1] == NULL) {
        goto failed;
    }
    if (peerAddr.inet.port != selfAddr.inet.port) {
        /* the connection we accepted is not from fds[0] */
        PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, 0);
        goto failed;
    }
    PR_Close(listenSock);
    return PR_SUCCESS;

failed:
    if (listenSock) {
        PR_Close(listenSock);
    }
    if (fds[0]) {
        PR_Close(fds[0]);
    }
    if (fds[1]) {
        PR_Close(fds[1]);
    }
    return PR_FAILURE;
#else
    PRInt32 osfd[2];

    if (pt_TestAbort()) return PR_FAILURE;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, osfd) == -1) {
        pt_MapError(_PR_MD_MAP_SOCKETPAIR_ERROR, errno);
        return PR_FAILURE;
    }

    fds[0] = pt_SetMethods(osfd[0], PR_DESC_SOCKET_TCP, PR_FALSE, PR_FALSE);
    if (fds[0] == NULL) {
        close(osfd[0]);
        close(osfd[1]);
        return PR_FAILURE;
    }
    fds[1] = pt_SetMethods(osfd[1], PR_DESC_SOCKET_TCP, PR_FALSE, PR_FALSE);
    if (fds[1] == NULL) {
        PR_Close(fds[0]);
        close(osfd[1]);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
#endif
}  /* PR_NewTCPSocketPair */

PR_IMPLEMENT(PRStatus) PR_CreatePipe(
    PRFileDesc **readPipe,
    PRFileDesc **writePipe
)
{
    int pipefd[2];

    if (pt_TestAbort()) return PR_FAILURE;

    if (pipe(pipefd) == -1)
    {
    /* XXX map pipe error */
        PR_SetError(PR_UNKNOWN_ERROR, errno);
        return PR_FAILURE;
    }
    *readPipe = pt_SetMethods(pipefd[0], PR_DESC_PIPE, PR_FALSE, PR_FALSE);
    if (NULL == *readPipe)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return PR_FAILURE;
    }
    *writePipe = pt_SetMethods(pipefd[1], PR_DESC_PIPE, PR_FALSE, PR_FALSE);
    if (NULL == *writePipe)
    {
        PR_Close(*readPipe);
        close(pipefd[1]);
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}

/*
** Set the inheritance attribute of a file descriptor.
*/
PR_IMPLEMENT(PRStatus) PR_SetFDInheritable(
    PRFileDesc *fd,
    PRBool inheritable)
{
    /*
     * Only a non-layered, NSPR file descriptor can be inherited
     * by a child process.
     */
    if (fd->identity != PR_NSPR_IO_LAYER)
    {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    if (fd->secret->inheritable != inheritable)
    {
        if (fcntl(fd->secret->md.osfd, F_SETFD,
        inheritable ? 0 : FD_CLOEXEC) == -1)
        {
            _PR_MD_MAP_DEFAULT_ERROR(errno);
            return PR_FAILURE;
        }
        fd->secret->inheritable = (_PRTriStateBool) inheritable;
    }
    return PR_SUCCESS;
}

/*****************************************************************************/
/***************************** I/O friends methods ***************************/
/*****************************************************************************/

PR_IMPLEMENT(PRFileDesc*) PR_ImportFile(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_FILE, PR_FALSE, PR_TRUE);
    if (NULL == fd) close(osfd);
    return fd;
}  /* PR_ImportFile */

PR_IMPLEMENT(PRFileDesc*) PR_ImportPipe(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_PIPE, PR_FALSE, PR_TRUE);
    if (NULL == fd) close(osfd);
    return fd;
}  /* PR_ImportPipe */

PR_IMPLEMENT(PRFileDesc*) PR_ImportTCPSocket(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_SOCKET_TCP, PR_FALSE, PR_TRUE);
    if (NULL == fd) close(osfd);
#ifdef _PR_NEED_SECRET_AF
    if (NULL != fd) fd->secret->af = PF_INET;
#endif
    return fd;
}  /* PR_ImportTCPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_ImportUDPSocket(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();
    fd = pt_SetMethods(osfd, PR_DESC_SOCKET_UDP, PR_FALSE, PR_TRUE);
    if (NULL == fd) close(osfd);
    return fd;
}  /* PR_ImportUDPSocket */

PR_IMPLEMENT(PRFileDesc*) PR_CreateSocketPollFd(PRInt32 osfd)
{
    PRFileDesc *fd;

    if (!_pr_initialized) _PR_ImplicitInitialization();

    fd = _PR_Getfd();

    if (fd == NULL) PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    else
    {
        fd->secret->md.osfd = osfd;
        fd->secret->inheritable = _PR_TRI_FALSE;
    	fd->secret->state = _PR_FILEDESC_OPEN;
        fd->methods = PR_GetSocketPollFdMethods();
    }

    return fd;
}  /* PR_CreateSocketPollFD */

PR_IMPLEMENT(PRStatus) PR_DestroySocketPollFd(PRFileDesc *fd)
{
    if (NULL == fd)
    {
        PR_SetError(PR_BAD_DESCRIPTOR_ERROR, 0);
        return PR_FAILURE;
    }
    fd->secret->state = _PR_FILEDESC_CLOSED;
    _PR_Putfd(fd);
    return PR_SUCCESS;
}  /* PR_DestroySocketPollFd */

PR_IMPLEMENT(PRInt32) PR_FileDesc2NativeHandle(PRFileDesc *bottom)
{
    PRInt32 osfd = -1;
    bottom = (NULL == bottom) ?
        NULL : PR_GetIdentitiesLayer(bottom, PR_NSPR_IO_LAYER);
    if (NULL == bottom) PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
    else osfd = bottom->secret->md.osfd;
    return osfd;
}  /* PR_FileDesc2NativeHandle */

PR_IMPLEMENT(void) PR_ChangeFileDescNativeHandle(PRFileDesc *fd,
    PRInt32 handle)
{
    if (fd) fd->secret->md.osfd = handle;
}  /*  PR_ChangeFileDescNativeHandle*/

PR_IMPLEMENT(PRStatus) PR_LockFile(PRFileDesc *fd)
{
    PRStatus status = PR_SUCCESS;

    if (pt_TestAbort()) return PR_FAILURE;

    PR_Lock(_pr_flock_lock);
    while (-1 == fd->secret->lockCount)
        PR_WaitCondVar(_pr_flock_cv, PR_INTERVAL_NO_TIMEOUT);
    if (0 == fd->secret->lockCount)
    {
        fd->secret->lockCount = -1;
        PR_Unlock(_pr_flock_lock);
        status = _PR_MD_LOCKFILE(fd->secret->md.osfd);
        PR_Lock(_pr_flock_lock);
        fd->secret->lockCount = (PR_SUCCESS == status) ? 1 : 0;
        PR_NotifyAllCondVar(_pr_flock_cv);
    }
    else
    {
        fd->secret->lockCount += 1;
    }
    PR_Unlock(_pr_flock_lock);
 
    return status;
}  /* PR_LockFile */

PR_IMPLEMENT(PRStatus) PR_TLockFile(PRFileDesc *fd)
{
    PRStatus status = PR_SUCCESS;

    if (pt_TestAbort()) return PR_FAILURE;

    PR_Lock(_pr_flock_lock);
    if (0 == fd->secret->lockCount)
    {
        status = _PR_MD_TLOCKFILE(fd->secret->md.osfd);
        if (PR_SUCCESS == status) fd->secret->lockCount = 1;
    }
    else fd->secret->lockCount += 1;
    PR_Unlock(_pr_flock_lock);
 
    return status;
}  /* PR_TLockFile */

PR_IMPLEMENT(PRStatus) PR_UnlockFile(PRFileDesc *fd)
{
    PRStatus status = PR_SUCCESS;

    if (pt_TestAbort()) return PR_FAILURE;

    PR_Lock(_pr_flock_lock);
    if (fd->secret->lockCount == 1)
    {
        status = _PR_MD_UNLOCKFILE(fd->secret->md.osfd);
        if (PR_SUCCESS == status) fd->secret->lockCount = 0;
    }
    else fd->secret->lockCount -= 1;
    PR_Unlock(_pr_flock_lock);

    return status;
}

/*
 * The next two entry points should not be in the API, but they are
 * defined here for historical (or hysterical) reasons.
 */

PR_IMPLEMENT(PRInt32) PR_GetSysfdTableMax(void)
{
#if defined(AIX) || defined(SYMBIAN)
    return sysconf(_SC_OPEN_MAX);
#else
    struct rlimit rlim;

    if ( getrlimit(RLIMIT_NOFILE, &rlim) < 0) 
       return -1;

    return rlim.rlim_max;
#endif
}

PR_IMPLEMENT(PRInt32) PR_SetSysfdTableSize(PRIntn table_size)
{
#if defined(AIX) || defined(SYMBIAN)
    return -1;
#else
    struct rlimit rlim;
    PRInt32 tableMax = PR_GetSysfdTableMax();

    if (tableMax < 0) return -1;
    rlim.rlim_max = tableMax;

    /* Grow as much as we can; even if too big */
    if ( rlim.rlim_max < table_size )
        rlim.rlim_cur = rlim.rlim_max;
    else
        rlim.rlim_cur = table_size;

    if ( setrlimit(RLIMIT_NOFILE, &rlim) < 0) 
        return -1;

    return rlim.rlim_cur;
#endif
}

/*
 * PR_Stat is supported for backward compatibility; some existing Java
 * code uses it.  New code should use PR_GetFileInfo.
 */

#ifndef NO_NSPR_10_SUPPORT
PR_IMPLEMENT(PRInt32) PR_Stat(const char *name, struct stat *buf)
{
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_Stat", "PR_GetFileInfo");

    if (pt_TestAbort()) return -1;

    if (-1 == stat(name, buf)) {
        pt_MapError(_PR_MD_MAP_STAT_ERROR, errno);
        return -1;
    } else {
        return 0;
    }
}
#endif /* ! NO_NSPR_10_SUPPORT */


PR_IMPLEMENT(void) PR_FD_ZERO(PR_fd_set *set)
{
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_ZERO (PR_Select)", "PR_Poll");
    memset(set, 0, sizeof(PR_fd_set));
}

PR_IMPLEMENT(void) PR_FD_SET(PRFileDesc *fh, PR_fd_set *set)
{
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_SET (PR_Select)", "PR_Poll");
    PR_ASSERT( set->hsize < PR_MAX_SELECT_DESC );

    set->harray[set->hsize++] = fh;
}

PR_IMPLEMENT(void) PR_FD_CLR(PRFileDesc *fh, PR_fd_set *set)
{
    PRUint32 index, index2;
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_CLR (PR_Select)", "PR_Poll");

    for (index = 0; index<set->hsize; index++)
       if (set->harray[index] == fh) {
           for (index2=index; index2 < (set->hsize-1); index2++) {
               set->harray[index2] = set->harray[index2+1];
           }
           set->hsize--;
           break;
       }
}

PR_IMPLEMENT(PRInt32) PR_FD_ISSET(PRFileDesc *fh, PR_fd_set *set)
{
    PRUint32 index;
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_ISSET (PR_Select)", "PR_Poll");
    for (index = 0; index<set->hsize; index++)
       if (set->harray[index] == fh) {
           return 1;
       }
    return 0;
}

PR_IMPLEMENT(void) PR_FD_NSET(PRInt32 fd, PR_fd_set *set)
{
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_NSET (PR_Select)", "PR_Poll");
    PR_ASSERT( set->nsize < PR_MAX_SELECT_DESC );

    set->narray[set->nsize++] = fd;
}

PR_IMPLEMENT(void) PR_FD_NCLR(PRInt32 fd, PR_fd_set *set)
{
    PRUint32 index, index2;
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_NCLR (PR_Select)", "PR_Poll");

    for (index = 0; index<set->nsize; index++)
       if (set->narray[index] == fd) {
           for (index2=index; index2 < (set->nsize-1); index2++) {
               set->narray[index2] = set->narray[index2+1];
           }
           set->nsize--;
           break;
       }
}

PR_IMPLEMENT(PRInt32) PR_FD_NISSET(PRInt32 fd, PR_fd_set *set)
{
    PRUint32 index;
    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete("PR_FD_NISSET (PR_Select)", "PR_Poll");
    for (index = 0; index<set->nsize; index++)
       if (set->narray[index] == fd) {
           return 1;
       }
    return 0;
}

#include <sys/types.h>
#include <sys/time.h>
#if !defined(HPUX) \
    && !defined(LINUX) && !defined(__GNU__) && !defined(__GLIBC__)
#include <sys/select.h>
#endif

static PRInt32
_PR_getset(PR_fd_set *pr_set, fd_set *set)
{
    PRUint32 index;
    PRInt32 max = 0;

    if (!pr_set)
        return 0;
   
    FD_ZERO(set);

    /* First set the pr file handle osfds */
    for (index=0; index<pr_set->hsize; index++) {
        FD_SET(pr_set->harray[index]->secret->md.osfd, set);
        if (pr_set->harray[index]->secret->md.osfd > max)
            max = pr_set->harray[index]->secret->md.osfd;
    }
    /* Second set the native osfds */
    for (index=0; index<pr_set->nsize; index++) {
        FD_SET(pr_set->narray[index], set);
        if (pr_set->narray[index] > max)
            max = pr_set->narray[index];
    }
    return max;
}

static void
_PR_setset(PR_fd_set *pr_set, fd_set *set)
{
    PRUint32 index, last_used;

    if (!pr_set)
        return;

    for (last_used=0, index=0; index<pr_set->hsize; index++) {
        if ( FD_ISSET(pr_set->harray[index]->secret->md.osfd, set) ) {
            pr_set->harray[last_used++] = pr_set->harray[index];
        }
    }
    pr_set->hsize = last_used;

    for (last_used=0, index=0; index<pr_set->nsize; index++) {
        if ( FD_ISSET(pr_set->narray[index], set) ) {
            pr_set->narray[last_used++] = pr_set->narray[index];
        }
    }
    pr_set->nsize = last_used;
}

PR_IMPLEMENT(PRInt32) PR_Select(
    PRInt32 unused, PR_fd_set *pr_rd, PR_fd_set *pr_wr, 
    PR_fd_set *pr_ex, PRIntervalTime timeout)
{
    fd_set rd, wr, ex;
    struct timeval tv, *tvp;
    PRInt32 max, max_fd;
    PRInt32 rv;
    /*
     * For restarting select() if it is interrupted by a Unix signal.
     * We use these variables to figure out how much time has elapsed
     * and how much of the timeout still remains.
     */
    PRIntervalTime start, elapsed, remaining;

    static PRBool unwarned = PR_TRUE;
    if (unwarned) unwarned = _PR_Obsolete( "PR_Select", "PR_Poll");

    FD_ZERO(&rd);
    FD_ZERO(&wr);
    FD_ZERO(&ex);

    max_fd = _PR_getset(pr_rd, &rd);
    max_fd = (max = _PR_getset(pr_wr, &wr))>max_fd?max:max_fd;
    max_fd = (max = _PR_getset(pr_ex, &ex))>max_fd?max:max_fd;

    if (timeout == PR_INTERVAL_NO_TIMEOUT) {
        tvp = NULL;
    } else {
        tv.tv_sec = (PRInt32)PR_IntervalToSeconds(timeout);
        tv.tv_usec = (PRInt32)PR_IntervalToMicroseconds(
                timeout - PR_SecondsToInterval(tv.tv_sec));
        tvp = &tv;
        start = PR_IntervalNow();
    }

retry:
    rv = select(max_fd + 1, (_PRSelectFdSetArg_t) &rd,
        (_PRSelectFdSetArg_t) &wr, (_PRSelectFdSetArg_t) &ex, tvp);

    if (rv == -1 && errno == EINTR) {
        if (timeout == PR_INTERVAL_NO_TIMEOUT) {
            goto retry;
        } else {
            elapsed = (PRIntervalTime) (PR_IntervalNow() - start);
            if (elapsed > timeout) {
                rv = 0;  /* timed out */
            } else {
                remaining = timeout - elapsed;
                tv.tv_sec = (PRInt32)PR_IntervalToSeconds(remaining);
                tv.tv_usec = (PRInt32)PR_IntervalToMicroseconds(
                        remaining - PR_SecondsToInterval(tv.tv_sec));
                goto retry;
            }
        }
    }

    if (rv > 0) {
        _PR_setset(pr_rd, &rd);
        _PR_setset(pr_wr, &wr);
        _PR_setset(pr_ex, &ex);
    } else if (rv == -1) {
        pt_MapError(_PR_MD_MAP_SELECT_ERROR, errno);
    }
    return rv;
}
#endif /* defined(_PR_PTHREADS) */

#ifdef MOZ_UNICODE 
/* ================ UTF16 Interfaces ================================ */
PR_IMPLEMENT(PRFileDesc*) PR_OpenFileUTF16(
    const PRUnichar *name, PRIntn flags, PRIntn mode)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}

PR_IMPLEMENT(PRStatus) PR_CloseDirUTF16(PRDir *dir)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}

PR_IMPLEMENT(PRDirUTF16*) PR_OpenDirUTF16(const PRUnichar *name)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}

PR_IMPLEMENT(PRDirEntryUTF16*) PR_ReadDirUTF16(PRDirUTF16 *dir, PRDirFlags flags)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return NULL;
}

PR_IMPLEMENT(PRStatus) PR_GetFileInfo64UTF16(const PRUnichar *fn, PRFileInfo64 *info)
{
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
}
/* ================ UTF16 Interfaces ================================ */
#endif /* MOZ_UNICODE */

/* ptio.c */
