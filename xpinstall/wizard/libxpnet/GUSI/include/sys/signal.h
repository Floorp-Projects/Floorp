/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * [¤3 Deleted as of 22Jul99, see
 *     ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change
 *	   for details]
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)signal.h	8.2 (Berkeley) 1/21/94
 */

/* Adapted for GUSI by Matthias Neeracher <neeri@iis.ee.ethz.ch> */

#ifndef	_SYS_SIGNAL_H_
#define	_SYS_SIGNAL_H_

#include <sys/cdefs.h>

#define NSIG	32		/* counting 0; could be 33 (mask is 1-32) */

#ifndef _ANSI_SOURCE
#include <machine/signal.h>	/* sigcontext; codes for SIGILL, SIGFPE */
#endif

__BEGIN_DECLS
typedef void (*__sig_handler)(int);
__END_DECLS

/* 1 is SIGABRT for MPW and CW */
#define	SIGINT	2	/* interrupt */
#define	SIGQUIT	3	/* quit */
/* 4 is SIGINT for CW */
#define	SIGABRT	6	/* abort() */
#define	SIGFPE	8	/* floating point exception */
#define	SIGKILL	9	/* kill (cannot be caught or ignored) */
#define	SIGSEGV	11	/* segmentation violation */
#define	SIGPIPE	13	/* write on a pipe with no one to read it */
#define	SIGALRM	14	/* alarm clock */
#define	SIGTERM	15	/* software termination signal from kill */
#ifndef _POSIX_SOURCE
#define	SIGURG	16	/* urgent condition on IO channel */
#endif
#define	SIGSTOP	17	/* sendable stop signal not from tty */
#define	SIGTSTP	18	/* stop signal from tty */
#define	SIGCONT	19	/* continue a stopped process */
#define	SIGCHLD	20	/* to parent on child stop or exit */
#define	SIGTTIN	21	/* to readers pgrp upon background tty read */
#define	SIGTTOU	22	/* like TTIN for output if (tp->t_local&LTOSTOP) */
#define	SIGHUP	24	/* hangup */
#define	SIGILL	25	/* illegal instruction (not reset when caught) */
#ifndef _POSIX_SOURCE
#define	SIGIO	23	/* input/output possible signal */
#define	SIGPROF	27	/* profiling time alarm */
#define SIGWINCH 28	/* window size changes */
#endif
#define SIGUSR1 30	/* user defined signal 1 */
#define SIGUSR2 31	/* user defined signal 2 */

#define	SIG_DFL		(__sig_handler)0
#define	SIG_IGN		(__sig_handler)1
#define	SIG_ERR		(__sig_handler)-1

#ifndef _ANSI_SOURCE
typedef unsigned int sigset_t;

/*
 * Signal vector "template" used in sigaction call.
 */
struct	sigaction {
	__sig_handler 	sa_handler;	/* signal handler */
	sigset_t 		sa_mask;		/* signal mask to apply */
	int				sa_flags;		/* see signal options below */
};
#define SA_NOCLDSTOP	0x0008	/* do not generate SIGCHLD on child stop 	*/
#define SA_RESETHAND	0x0001	/* emulate ANSI signals 					*/
#define SA_RESTART		0x0002	/* restart slow system calls 				*/
#define SA_NODEFER		0x0004	/* don't block current signal 				*/

/*
 * Flags for sigprocmask:
 */
#define	SIG_BLOCK	1	/* block specified signal set */
#define	SIG_UNBLOCK	2	/* unblock specified signal set */
#define	SIG_SETMASK	3	/* set specified signal set */

#endif	/* !_ANSI_SOURCE */

/*
 * For historical reasons; programs expect signal's return value to be
 * defined by <sys/signal.h>.
 */
__BEGIN_DECLS
__sig_handler signal __P((int, __sig_handler));
__END_DECLS
#endif	/* !_SYS_SIGNAL_H_ */
