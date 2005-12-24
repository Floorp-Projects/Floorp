/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "primpl.h"

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#if defined(AIX)
#include <dlfcn.h>  /* For dlopen, dlsym, dlclose */
#endif

#if defined(DARWIN)
#include <crt_externs.h>
#else
PR_IMPORT_DATA(char **) environ;
#endif

/*
 * HP-UX 9 doesn't have the SA_RESTART flag.
 */
#ifndef SA_RESTART
#define SA_RESTART 0
#endif

#if defined(VMS)
static PRLock *_pr_vms_fork_lock = NULL;
#endif

/*
 **********************************************************************
 *
 * The Unix process routines
 *
 **********************************************************************
 */

#define _PR_SIGNALED_EXITSTATUS 256

typedef enum pr_PidState {
    _PR_PID_DETACHED,
    _PR_PID_REAPED,
    _PR_PID_WAITING
} pr_PidState;

typedef struct pr_PidRecord {
    pid_t pid;
    int exitStatus;
    pr_PidState state;
    PRCondVar *reapedCV;
    struct pr_PidRecord *next;
} pr_PidRecord;

/*
 * Irix sprocs and LinuxThreads are actually a kind of processes
 * that can share the virtual address space and file descriptors.
 */
#if (defined(IRIX) && !defined(_PR_PTHREADS)) \
        || ((defined(LINUX) || defined(__GNU__) || defined(__GLIBC__)) \
        && defined(_PR_PTHREADS))
#define _PR_SHARE_CLONES
#endif

/*
 * The macro _PR_NATIVE_THREADS indicates that we are
 * using native threads only, so waitpid() blocks just the
 * calling thread, not the process.  In this case, the waitpid
 * daemon thread can safely block in waitpid().  So we don't
 * need to catch SIGCHLD, and the pipe to unblock PR_Poll() is
 * also not necessary.
 */

#if defined(_PR_GLOBAL_THREADS_ONLY) \
	|| (defined(_PR_PTHREADS) \
	&& !defined(LINUX) && !defined(__GNU__) && !defined(__GLIBC__))
#define _PR_NATIVE_THREADS
#endif

/*
 * All the static variables used by the Unix process routines are
 * collected in this structure.
 */

static struct {
    PRCallOnceType once;
    PRThread *thread;
    PRLock *ml;
#if defined(_PR_NATIVE_THREADS)
    PRInt32 numProcs;
    PRCondVar *cv;
#else
    int pipefd[2];
#endif
    pr_PidRecord **pidTable;

#ifdef _PR_SHARE_CLONES
    struct pr_CreateProcOp *opHead, *opTail;
#endif

#ifdef AIX
    pid_t (*forkptr)(void);  /* Newer versions of AIX (starting in 4.3.2)
                              * have f_fork, which is faster than the
                              * regular fork in a multithreaded process
                              * because it skips calling the fork handlers.
                              * So we look up the f_fork symbol to see if
                              * it's available and fall back on fork.
                              */
#endif /* AIX */
} pr_wp;

#ifdef _PR_SHARE_CLONES
static int pr_waitpid_daemon_exit;

void
_MD_unix_terminate_waitpid_daemon(void)
{
    if (pr_wp.thread) {
        pr_waitpid_daemon_exit = 1;
        write(pr_wp.pipefd[1], "", 1);
        PR_JoinThread(pr_wp.thread);
    }
}
#endif

static PRStatus _MD_InitProcesses(void);
#if !defined(_PR_NATIVE_THREADS)
static void pr_InstallSigchldHandler(void);
#endif

static PRProcess *
ForkAndExec(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr)
{
    PRProcess *process;
    int nEnv, idx;
    char *const *childEnvp;
    char **newEnvp = NULL;
    int flags;
#ifdef VMS
    char VMScurdir[FILENAME_MAX+1] = { '\0' } ;
#endif	

    process = PR_NEW(PRProcess);
    if (!process) {
        PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
        return NULL;
    }

    childEnvp = envp;
    if (attr && attr->fdInheritBuffer) {
        if (NULL == childEnvp) {
#ifdef DARWIN
            childEnvp = *(_NSGetEnviron());
#else
            childEnvp = environ;
#endif
        }
        for (nEnv = 0; childEnvp[nEnv]; nEnv++) {
        }
        newEnvp = (char **) PR_MALLOC((nEnv + 2) * sizeof(char *));
        if (NULL == newEnvp) {
            PR_DELETE(process);
            PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
            return NULL;
        }
        for (idx = 0; idx < nEnv; idx++) {
            newEnvp[idx] = childEnvp[idx];
        }
        newEnvp[idx++] = attr->fdInheritBuffer;
        newEnvp[idx] = NULL;
        childEnvp = newEnvp;
    }

#ifdef VMS
/*
** Since vfork/exec is implemented VERY differently on OpenVMS, we have to
** handle the setting up of the standard streams very differently. And since
** none of this code can ever execute in the context of the child, we have
** to perform the chdir in the parent so the child is born into the correct
** directory (and then switch the parent back again).
*/
{
    int decc$set_child_standard_streams(int,int,int);
    int n, fd_stdin=0, fd_stdout=1, fd_stderr=2;

    /* Set up any standard streams we are given, assuming defaults */
    if (attr) {
       if (attr->stdinFd)
           fd_stdin = attr->stdinFd->secret->md.osfd;
       if (attr->stdoutFd)
           fd_stdout = attr->stdoutFd->secret->md.osfd;
       if (attr->stderrFd)
           fd_stderr = attr->stderrFd->secret->md.osfd;
    }

    /*
    ** Put a lock around anything that isn't going to be thread-safe.
    */
    PR_Lock(_pr_vms_fork_lock);

    /*
    ** Prepare the child's streams. We always do this in case a previous fork
    ** has left the stream assignments in some non-standard way.
    */
    n = decc$set_child_standard_streams(fd_stdin,fd_stdout,fd_stderr);
    if (n == -1) {
       PR_SetError(PR_BAD_DESCRIPTOR_ERROR, errno);
       PR_DELETE(process);
       if (newEnvp) {
           PR_DELETE(newEnvp);
       }
       PR_Unlock(_pr_vms_fork_lock);
       return NULL;
    }

    /* Switch directory if we have to */
    if (attr) {
       if (attr->currentDirectory) {
           if ( (getcwd(VMScurdir,sizeof(VMScurdir)) == NULL) ||
                (chdir(attr->currentDirectory) < 0) ) {
               PR_SetError(PR_DIRECTORY_OPEN_ERROR, errno);
               PR_DELETE(process);
               if (newEnvp) {
                   PR_DELETE(newEnvp);
               }
               PR_Unlock(_pr_vms_fork_lock);
               return NULL;
           }
       }
    }
}
#endif /* VMS */

#ifdef AIX
    process->md.pid = (*pr_wp.forkptr)();
#elif defined(NTO)
    /*
     * fork() & exec() does not work in a multithreaded process.
     * Use spawn() instead.
     */
    {
        int fd_map[3] = { 0, 1, 2 };

        if (attr) {
            if (attr->stdinFd && attr->stdinFd->secret->md.osfd != 0) {
                fd_map[0] = dup(attr->stdinFd->secret->md.osfd);
                flags = fcntl(fd_map[0], F_GETFL, 0);
                if (flags & O_NONBLOCK)
                    fcntl(fd_map[0], F_SETFL, flags & ~O_NONBLOCK);
            }
            if (attr->stdoutFd && attr->stdoutFd->secret->md.osfd != 1) {
                fd_map[1] = dup(attr->stdoutFd->secret->md.osfd);
                flags = fcntl(fd_map[1], F_GETFL, 0);
                if (flags & O_NONBLOCK)
                    fcntl(fd_map[1], F_SETFL, flags & ~O_NONBLOCK);
            }
            if (attr->stderrFd && attr->stderrFd->secret->md.osfd != 2) {
                fd_map[2] = dup(attr->stderrFd->secret->md.osfd);
                flags = fcntl(fd_map[2], F_GETFL, 0);
                if (flags & O_NONBLOCK)
                    fcntl(fd_map[2], F_SETFL, flags & ~O_NONBLOCK);
            }

            PR_ASSERT(attr->currentDirectory == NULL);  /* not implemented */
        }

        process->md.pid = spawn(path, 3, fd_map, NULL, argv, childEnvp);

        if (fd_map[0] != 0)
            close(fd_map[0]);
        if (fd_map[1] != 1)
            close(fd_map[1]);
        if (fd_map[2] != 2)
            close(fd_map[2]);
    }
#else
    process->md.pid = fork();
#endif
    if ((pid_t) -1 == process->md.pid) {
        PR_SetError(PR_INSUFFICIENT_RESOURCES_ERROR, errno);
        PR_DELETE(process);
        if (newEnvp) {
            PR_DELETE(newEnvp);
        }
        return NULL;
    } else if (0 == process->md.pid) {  /* the child process */
        /*
         * If the child process needs to exit, it must call _exit().
         * Do not call exit(), because exit() will flush and close
         * the standard I/O file descriptors, and hence corrupt
         * the parent process's standard I/O data structures.
         */

#if !defined(NTO)
#ifdef VMS
       /* OpenVMS has already handled all this above */
#else
        if (attr) {
            /* the osfd's to redirect stdin, stdout, and stderr to */
            int in_osfd = -1, out_osfd = -1, err_osfd = -1;

            if (attr->stdinFd
                    && attr->stdinFd->secret->md.osfd != 0) {
                in_osfd = attr->stdinFd->secret->md.osfd;
                if (dup2(in_osfd, 0) != 0) {
                    _exit(1);  /* failed */
                }
                flags = fcntl(0, F_GETFL, 0);
                if (flags & O_NONBLOCK) {
                    fcntl(0, F_SETFL, flags & ~O_NONBLOCK);
                }
            }
            if (attr->stdoutFd
                    && attr->stdoutFd->secret->md.osfd != 1) {
                out_osfd = attr->stdoutFd->secret->md.osfd;
                if (dup2(out_osfd, 1) != 1) {
                    _exit(1);  /* failed */
                }
                flags = fcntl(1, F_GETFL, 0);
                if (flags & O_NONBLOCK) {
                    fcntl(1, F_SETFL, flags & ~O_NONBLOCK);
                }
            }
            if (attr->stderrFd
                    && attr->stderrFd->secret->md.osfd != 2) {
                err_osfd = attr->stderrFd->secret->md.osfd;
                if (dup2(err_osfd, 2) != 2) {
                    _exit(1);  /* failed */
                }
                flags = fcntl(2, F_GETFL, 0);
                if (flags & O_NONBLOCK) {
                    fcntl(2, F_SETFL, flags & ~O_NONBLOCK);
                }
            }
            if (in_osfd != -1) {
                close(in_osfd);
            }
            if (out_osfd != -1 && out_osfd != in_osfd) {
                close(out_osfd);
            }
            if (err_osfd != -1 && err_osfd != in_osfd
                    && err_osfd != out_osfd) {
                close(err_osfd);
            }
            if (attr->currentDirectory) {
                if (chdir(attr->currentDirectory) < 0) {
                    _exit(1);  /* failed */
                }
            }
        }
#endif /* !VMS */

        if (childEnvp) {
            (void)execve(path, argv, childEnvp);
        } else {
            /* Inherit the environment of the parent. */
            (void)execv(path, argv);
        }
        /* Whoops! It returned. That's a bad sign. */
#ifdef VMS
       /*
       ** On OpenVMS we are still in the context of the parent, and so we
       ** can (and should!) perform normal error handling.
       */
       PR_SetError(PR_UNKNOWN_ERROR, errno);
       PR_DELETE(process);
       if (newEnvp) {
           PR_DELETE(newEnvp);
       }
       if (VMScurdir[0] != '\0')
           chdir(VMScurdir);
       PR_Unlock(_pr_vms_fork_lock);
       return NULL;
#else
        _exit(1);
#endif /* VMS */
#endif /* !NTO */
    }

    if (newEnvp) {
        PR_DELETE(newEnvp);
    }
#ifdef VMS
    /* If we switched directories, then remember to switch back */
    if (VMScurdir[0] != '\0') {
       chdir(VMScurdir); /* can't do much if it fails */
    }
    PR_Unlock(_pr_vms_fork_lock);
#endif /* VMS */

#if defined(_PR_NATIVE_THREADS)
    PR_Lock(pr_wp.ml);
    if (0 == pr_wp.numProcs++) {
        PR_NotifyCondVar(pr_wp.cv);
    }
    PR_Unlock(pr_wp.ml);
#endif
    return process;
}

#ifdef _PR_SHARE_CLONES

struct pr_CreateProcOp {
    const char *path;
    char *const *argv;
    char *const *envp;
    const PRProcessAttr *attr;
    PRProcess *process;
    PRErrorCode prerror;
    PRInt32 oserror;
    PRBool done;
    PRCondVar *doneCV;
    struct pr_CreateProcOp *next;
};

PRProcess *
_MD_CreateUnixProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr)
{
    struct pr_CreateProcOp *op;
    PRProcess *proc;
    int rv;

    if (PR_CallOnce(&pr_wp.once, _MD_InitProcesses) == PR_FAILURE) {
	return NULL;
    }

    op = PR_NEW(struct pr_CreateProcOp);
    if (NULL == op) {
	PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	return NULL;
    }
    op->path = path;
    op->argv = argv;
    op->envp = envp;
    op->attr = attr;
    op->done = PR_FALSE;
    op->doneCV = PR_NewCondVar(pr_wp.ml);
    if (NULL == op->doneCV) {
	PR_DELETE(op);
	return NULL;
    }
    PR_Lock(pr_wp.ml);

    /* add to the tail of op queue */
    op->next = NULL;
    if (pr_wp.opTail) {
	pr_wp.opTail->next = op;
	pr_wp.opTail = op;
    } else {
	PR_ASSERT(NULL == pr_wp.opHead);
	pr_wp.opHead = pr_wp.opTail = op;
    }

    /* wake up the daemon thread */
    do {
        rv = write(pr_wp.pipefd[1], "", 1);
    } while (-1 == rv && EINTR == errno);

    while (op->done == PR_FALSE) {
	PR_WaitCondVar(op->doneCV, PR_INTERVAL_NO_TIMEOUT);
    }
    PR_Unlock(pr_wp.ml);
    PR_DestroyCondVar(op->doneCV);
    proc = op->process;
    if (!proc) {
	PR_SetError(op->prerror, op->oserror);
    }
    PR_DELETE(op);
    return proc;
}

#else  /* ! _PR_SHARE_CLONES */

PRProcess *
_MD_CreateUnixProcess(
    const char *path,
    char *const *argv,
    char *const *envp,
    const PRProcessAttr *attr)
{
    if (PR_CallOnce(&pr_wp.once, _MD_InitProcesses) == PR_FAILURE) {
	return NULL;
    }
    return ForkAndExec(path, argv, envp, attr);
}  /* _MD_CreateUnixProcess */

#endif  /* _PR_SHARE_CLONES */

/*
 * The pid table is a hashtable.
 *
 * The number of buckets in the hashtable (NBUCKETS) must be a power of 2.
 */
#define NBUCKETS_LOG2 6
#define NBUCKETS (1 << NBUCKETS_LOG2)
#define PID_HASH_MASK ((pid_t) (NBUCKETS - 1))

static pr_PidRecord *
FindPidTable(pid_t pid)
{
    pr_PidRecord *pRec;
    int keyHash = (int) (pid & PID_HASH_MASK);

    pRec =  pr_wp.pidTable[keyHash];
    while (pRec) {
	if (pRec->pid == pid) {
	    break;
	}
	pRec = pRec->next;
    }
    return pRec;
}

static void
InsertPidTable(pr_PidRecord *pRec)
{
    int keyHash = (int) (pRec->pid & PID_HASH_MASK);

    pRec->next = pr_wp.pidTable[keyHash];
    pr_wp.pidTable[keyHash] = pRec;
}

static void
DeletePidTable(pr_PidRecord *pRec)
{
    int keyHash = (int) (pRec->pid & PID_HASH_MASK);

    if (pr_wp.pidTable[keyHash] == pRec) {
	pr_wp.pidTable[keyHash] = pRec->next;
    } else {
	pr_PidRecord *pred, *cur;  /* predecessor and current */

	pred = pr_wp.pidTable[keyHash];
	cur = pred->next;
	while (cur) {
	    if (cur == pRec) {
		pred->next = cur->next;
		break;
            }
	    pred = cur;
	    cur = cur->next;
        }
	PR_ASSERT(cur != NULL);
    }
}

static int
ExtractExitStatus(int rawExitStatus)
{
    /*
     * We did not specify the WCONTINUED and WUNTRACED options
     * for waitpid, so these two events should not be reported.
     */
    PR_ASSERT(!WIFSTOPPED(rawExitStatus));
#ifdef WIFCONTINUED
    PR_ASSERT(!WIFCONTINUED(rawExitStatus));
#endif
    if (WIFEXITED(rawExitStatus)) {
	return WEXITSTATUS(rawExitStatus);
    } else {
	PR_ASSERT(WIFSIGNALED(rawExitStatus));
	return _PR_SIGNALED_EXITSTATUS;
    }
}

static void
ProcessReapedChildInternal(pid_t pid, int status)
{
    pr_PidRecord *pRec;

    pRec = FindPidTable(pid);
    if (NULL == pRec) {
        pRec = PR_NEW(pr_PidRecord);
        pRec->pid = pid;
        pRec->state = _PR_PID_REAPED;
        pRec->exitStatus = ExtractExitStatus(status);
        pRec->reapedCV = NULL;
        InsertPidTable(pRec);
    } else {
        PR_ASSERT(pRec->state != _PR_PID_REAPED);
        if (_PR_PID_DETACHED == pRec->state) {
            PR_ASSERT(NULL == pRec->reapedCV);
            DeletePidTable(pRec);
            PR_DELETE(pRec);
        } else {
            PR_ASSERT(_PR_PID_WAITING == pRec->state);
            PR_ASSERT(NULL != pRec->reapedCV);
            pRec->exitStatus = ExtractExitStatus(status);
            pRec->state = _PR_PID_REAPED;
            PR_NotifyCondVar(pRec->reapedCV);
        }
    }
}

#if defined(_PR_NATIVE_THREADS)

/*
 * If all the threads are native threads, the daemon thread is
 * simpler.  We don't need to catch the SIGCHLD signal.  We can
 * just have the daemon thread block in waitpid().
 */

static void WaitPidDaemonThread(void *unused)
{
    pid_t pid;
    int status;

    while (1) {
        PR_Lock(pr_wp.ml);
        while (0 == pr_wp.numProcs) {
            PR_WaitCondVar(pr_wp.cv, PR_INTERVAL_NO_TIMEOUT);
        }
        PR_Unlock(pr_wp.ml);

	while (1) {
	    do {
	        pid = waitpid((pid_t) -1, &status, 0);
	    } while ((pid_t) -1 == pid && EINTR == errno);

            /*
             * waitpid() cannot return 0 because we did not invoke it
             * with the WNOHANG option.
             */ 
	    PR_ASSERT(0 != pid);

            /*
             * The only possible error code is ECHILD.  But if we do
             * our accounting correctly, we should only call waitpid()
             * when there is a child process to wait for.
             */
            PR_ASSERT((pid_t) -1 != pid);
	    if ((pid_t) -1 == pid) {
                break;
            }

	    PR_Lock(pr_wp.ml);
            ProcessReapedChildInternal(pid, status);
            pr_wp.numProcs--;
            while (0 == pr_wp.numProcs) {
                PR_WaitCondVar(pr_wp.cv, PR_INTERVAL_NO_TIMEOUT);
            }
	    PR_Unlock(pr_wp.ml);
	}
    }
}

#else /* _PR_NATIVE_THREADS */

static void WaitPidDaemonThread(void *unused)
{
    PRPollDesc pd;
    PRFileDesc *fd;
    int rv;
    char buf[128];
    pid_t pid;
    int status;
#ifdef _PR_SHARE_CLONES
    struct pr_CreateProcOp *op;
#endif

#ifdef _PR_SHARE_CLONES
    pr_InstallSigchldHandler();
#endif

    fd = PR_ImportFile(pr_wp.pipefd[0]);
    PR_ASSERT(NULL != fd);
    pd.fd = fd;
    pd.in_flags = PR_POLL_READ;

    while (1) {
        rv = PR_Poll(&pd, 1, PR_INTERVAL_NO_TIMEOUT);
        PR_ASSERT(1 == rv);

#ifdef _PR_SHARE_CLONES
        if (pr_waitpid_daemon_exit) {
            return;
        }
	PR_Lock(pr_wp.ml);
#endif
	    
        do {
            rv = read(pr_wp.pipefd[0], buf, sizeof(buf));
        } while (sizeof(buf) == rv || (-1 == rv && EINTR == errno));

#ifdef _PR_SHARE_CLONES
	PR_Unlock(pr_wp.ml);
	while ((op = pr_wp.opHead) != NULL) {
	    op->process = ForkAndExec(op->path, op->argv,
		    op->envp, op->attr);
	    if (NULL == op->process) {
		op->prerror = PR_GetError();
		op->oserror = PR_GetOSError();
	    }
	    PR_Lock(pr_wp.ml);
	    pr_wp.opHead = op->next;
	    if (NULL == pr_wp.opHead) {
		pr_wp.opTail = NULL;
	    }
	    op->done = PR_TRUE;
	    PR_NotifyCondVar(op->doneCV);
	    PR_Unlock(pr_wp.ml);
	}
#endif

	while (1) {
	    do {
	        pid = waitpid((pid_t) -1, &status, WNOHANG);
	    } while ((pid_t) -1 == pid && EINTR == errno);
	    if (0 == pid) break;
	    if ((pid_t) -1 == pid) {
		/* must be because we have no child processes */
		PR_ASSERT(ECHILD == errno);
		break;
            }

	    PR_Lock(pr_wp.ml);
            ProcessReapedChildInternal(pid, status);
	    PR_Unlock(pr_wp.ml);
	}
    }
}

static void pr_SigchldHandler(int sig)
{
    int errnoCopy;
    int rv;

    errnoCopy = errno;

    do {
        rv = write(pr_wp.pipefd[1], "", 1);
    } while (-1 == rv && EINTR == errno);

#ifdef DEBUG
    if (-1 == rv && EAGAIN != errno && EWOULDBLOCK != errno) {
        char *msg = "cannot write to pipe\n";
        write(2, msg, strlen(msg) + 1);
        _exit(1);
    }
#endif

    errno = errnoCopy;
}

static void pr_InstallSigchldHandler()
{
#if defined(HPUX) && defined(_PR_DCETHREADS)
#error "HP-UX DCE threads have their own SIGCHLD handler"
#endif

    struct sigaction act, oact;
    int rv;

    act.sa_handler = pr_SigchldHandler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_NOCLDSTOP | SA_RESTART;
    rv = sigaction(SIGCHLD, &act, &oact);
    PR_ASSERT(0 == rv);
    /* Make sure we are not overriding someone else's SIGCHLD handler */
#ifndef _PR_SHARE_CLONES
    PR_ASSERT(oact.sa_handler == SIG_DFL);
#endif
}

#endif  /* !defined(_PR_NATIVE_THREADS) */

static PRStatus _MD_InitProcesses(void)
{
#if !defined(_PR_NATIVE_THREADS)
    int rv;
    int flags;
#endif
#ifdef SUNOS4
#define _PR_NBIO_FLAG FNDELAY
#else
#define _PR_NBIO_FLAG O_NONBLOCK
#endif

#ifdef AIX
    {
        void *handle = dlopen(NULL, RTLD_NOW | RTLD_GLOBAL);
        pr_wp.forkptr = (pid_t (*)(void)) dlsym(handle, "f_fork");
        if (!pr_wp.forkptr) {
            pr_wp.forkptr = fork;
        }
        dlclose(handle);
    }
#endif /* AIX */

    pr_wp.ml = PR_NewLock();
    PR_ASSERT(NULL != pr_wp.ml);

#if defined(VMS)
    _pr_vms_fork_lock = PR_NewLock();
    PR_ASSERT(NULL != _pr_vms_fork_lock);
#endif

#if defined(_PR_NATIVE_THREADS)
    pr_wp.numProcs = 0;
    pr_wp.cv = PR_NewCondVar(pr_wp.ml);
    PR_ASSERT(NULL != pr_wp.cv);
#else
    rv = pipe(pr_wp.pipefd);
    PR_ASSERT(0 == rv);
    flags = fcntl(pr_wp.pipefd[0], F_GETFL, 0);
    fcntl(pr_wp.pipefd[0], F_SETFL, flags | _PR_NBIO_FLAG);
    flags = fcntl(pr_wp.pipefd[1], F_GETFL, 0);
    fcntl(pr_wp.pipefd[1], F_SETFL, flags | _PR_NBIO_FLAG);

#ifndef _PR_SHARE_CLONES
    pr_InstallSigchldHandler();
#endif
#endif  /* !_PR_NATIVE_THREADS */

    pr_wp.thread = PR_CreateThread(PR_SYSTEM_THREAD,
	    WaitPidDaemonThread, NULL, PR_PRIORITY_NORMAL,
#ifdef _PR_SHARE_CLONES
            PR_GLOBAL_THREAD,
#else
	    PR_LOCAL_THREAD,
#endif
	    PR_JOINABLE_THREAD, 0);
    PR_ASSERT(NULL != pr_wp.thread);

    pr_wp.pidTable = (pr_PidRecord**)PR_CALLOC(NBUCKETS * sizeof(pr_PidRecord *));
    PR_ASSERT(NULL != pr_wp.pidTable);
    return PR_SUCCESS;
}

PRStatus _MD_DetachUnixProcess(PRProcess *process)
{
    PRStatus retVal = PR_SUCCESS;
    pr_PidRecord *pRec;

    PR_Lock(pr_wp.ml);
    pRec = FindPidTable(process->md.pid);
    if (NULL == pRec) {
	pRec = PR_NEW(pr_PidRecord);
	if (NULL == pRec) {
	    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	    retVal = PR_FAILURE;
	    goto done;
	}
	pRec->pid = process->md.pid;
	pRec->state = _PR_PID_DETACHED;
	pRec->reapedCV = NULL;
	InsertPidTable(pRec);
    } else {
	PR_ASSERT(_PR_PID_REAPED == pRec->state);
	if (_PR_PID_REAPED != pRec->state) {
	    PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
	    retVal = PR_FAILURE;
	} else {
	    DeletePidTable(pRec);
	    PR_ASSERT(NULL == pRec->reapedCV);
	    PR_DELETE(pRec);
	}
    }
    PR_DELETE(process);

done:
    PR_Unlock(pr_wp.ml);
    return retVal;
}

PRStatus _MD_WaitUnixProcess(
    PRProcess *process,
    PRInt32 *exitCode)
{
    pr_PidRecord *pRec;
    PRStatus retVal = PR_SUCCESS;
    PRBool interrupted = PR_FALSE;

    PR_Lock(pr_wp.ml);
    pRec = FindPidTable(process->md.pid);
    if (NULL == pRec) {
	pRec = PR_NEW(pr_PidRecord);
	if (NULL == pRec) {
	    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
	    retVal = PR_FAILURE;
	    goto done;
	}
	pRec->pid = process->md.pid;
	pRec->state = _PR_PID_WAITING;
	pRec->reapedCV = PR_NewCondVar(pr_wp.ml);
	if (NULL == pRec->reapedCV) {
	    PR_DELETE(pRec);
	    retVal = PR_FAILURE;
	    goto done;
	}
	InsertPidTable(pRec);
	while (!interrupted && _PR_PID_REAPED != pRec->state) {
	    if (PR_WaitCondVar(pRec->reapedCV,
		    PR_INTERVAL_NO_TIMEOUT) == PR_FAILURE
		    && PR_GetError() == PR_PENDING_INTERRUPT_ERROR) {
		interrupted = PR_TRUE;
            }
	}
	if (_PR_PID_REAPED == pRec->state) {
            if (exitCode) {
                *exitCode = pRec->exitStatus;
            }
	} else {
	    PR_ASSERT(interrupted);
	    retVal = PR_FAILURE;
	}
	DeletePidTable(pRec);
	PR_DestroyCondVar(pRec->reapedCV);
	PR_DELETE(pRec);
    } else {
	PR_ASSERT(_PR_PID_REAPED == pRec->state);
	PR_ASSERT(NULL == pRec->reapedCV);
	DeletePidTable(pRec);
        if (exitCode) {
            *exitCode = pRec->exitStatus;
        }
	PR_DELETE(pRec);
    }
    PR_DELETE(process);

done:
    PR_Unlock(pr_wp.ml);
    return retVal;
}  /* _MD_WaitUnixProcess */

PRStatus _MD_KillUnixProcess(PRProcess *process)
{
    PRErrorCode prerror;
    PRInt32 oserror;

    if (kill(process->md.pid, SIGKILL) == 0) {
	return PR_SUCCESS;
    }
    oserror = errno;
    switch (oserror) {
        case EPERM:
	    prerror = PR_NO_ACCESS_RIGHTS_ERROR;
	    break;
        case ESRCH:
	    prerror = PR_INVALID_ARGUMENT_ERROR;
	    break;
        default:
	    prerror = PR_UNKNOWN_ERROR;
	    break;
    }
    PR_SetError(prerror, oserror);
    return PR_FAILURE;
}  /* _MD_KillUnixProcess */
