/* -*- Mode: C++; tab-width: 8; c-basic-offset: 8 -*- */
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
#include <stdio.h>
#include <signal.h>

#define _PR_SIGNALED_EXITSTATUS 256

PRProcess*
_MD_create_process (const char *path, char *const *argv,
		    char *const *envp, const PRProcessAttr *attr)
{
	PRProcess *process;
	int nEnv, idx;
	char *const *childEnvp;
	char **newEnvp = NULL;
	int flags;

	process = PR_NEW(PRProcess);
	if (!process) {
		PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
		return NULL;
	}

	childEnvp = envp;
	if (attr && attr->fdInheritBuffer) {
		if (NULL == childEnvp) {
			childEnvp = environ;
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

	process->md.pid = fork();

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

		if (childEnvp) {
			(void)execve(path, argv, childEnvp);
		} else {
			/* Inherit the environment of the parent. */
			(void)execv(path, argv);
		}
		/* Whoops! It returned. That's a bad sign. */
		_exit(1);
	}

	if (newEnvp) {
		PR_DELETE(newEnvp);
	}

	return process;
}

PRStatus
_MD_detach_process (PRProcess *process)
{
	/* If we kept a process table like unix does,
	 * we'd remove the entry here.
	 * Since we dont', just delete the process variable
	 */
	PR_DELETE(process);
	return PR_SUCCESS;
}

PRStatus
_MD_wait_process (PRProcess *process, PRInt32 *exitCode)
{
	PRStatus retVal = PR_SUCCESS;
	int ret, status;
	
	/* Ignore interruptions */
	do {
		ret = waitpid(process->md.pid, &status, 0);
	} while (ret == -1 && errno == EINTR);

	/*
	 * waitpid() cannot return 0 because we did not invoke it
	 * with the WNOHANG option.
	 */ 
	PR_ASSERT(0 != ret);

	if (ret < 0) {
                PR_SetError(PR_UNKNOWN_ERROR, _MD_ERRNO());
		return PR_FAILURE;
	}

	/* If child process exited normally, return child exit code */
	if (WIFEXITED(status)) {
		*exitCode = WEXITSTATUS(status);
	} else {
		PR_ASSERT(WIFSIGNALED(status));
		*exitCode = _PR_SIGNALED_EXITSTATUS;
	}		

	PR_DELETE(process);
	return PR_SUCCESS;
}

PRStatus
_MD_kill_process (PRProcess *process)
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
}
