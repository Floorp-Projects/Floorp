/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape Portable Runtime (NSPR).
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

#include "primpl.h"

#include <string.h> /* for memset() */


/************************************************************************/

PRLock *_pr_flock_lock;
PRCondVar *_pr_flock_cv;

void _PR_InitIO(void)
{
    const PRIOMethods *methods = PR_GetFileMethods();

    _PR_InitFdCache();

    _pr_flock_lock = PR_NewLock();
    _pr_flock_cv = PR_NewCondVar(_pr_flock_lock);

#ifdef WIN32
    _pr_stdin = PR_AllocFileDesc((PRInt32)GetStdHandle(STD_INPUT_HANDLE),
            methods);
    _pr_stdout = PR_AllocFileDesc((PRInt32)GetStdHandle(STD_OUTPUT_HANDLE),
            methods);
    _pr_stderr = PR_AllocFileDesc((PRInt32)GetStdHandle(STD_ERROR_HANDLE),
            methods);
#ifdef WINNT
    _pr_stdin->secret->md.sync_file_io = PR_TRUE;
    _pr_stdout->secret->md.sync_file_io = PR_TRUE;
    _pr_stderr->secret->md.sync_file_io = PR_TRUE;
#endif
#else
    _pr_stdin = PR_AllocFileDesc(0, methods);
    _pr_stdout = PR_AllocFileDesc(1, methods);
    _pr_stderr = PR_AllocFileDesc(2, methods);
#endif
    _PR_MD_INIT_FD_INHERITABLE(_pr_stdin, PR_TRUE);
    _PR_MD_INIT_FD_INHERITABLE(_pr_stdout, PR_TRUE);
    _PR_MD_INIT_FD_INHERITABLE(_pr_stderr, PR_TRUE);

    _PR_MD_INIT_IO();
}

PR_IMPLEMENT(PRFileDesc*) PR_GetSpecialFD(PRSpecialFD osfd)
{
    PRFileDesc *result = NULL;
    PR_ASSERT((int) osfd >= PR_StandardInput && osfd <= PR_StandardError);

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
}

PR_IMPLEMENT(PRFileDesc*) PR_AllocFileDesc(
    PRInt32 osfd, const PRIOMethods *methods)
{
    PRFileDesc *fd;

#ifdef XP_UNIX
	/*
	 * Assert that the file descriptor is small enough to fit in the
	 * fd_set passed to select
	 */
	PR_ASSERT(osfd < FD_SETSIZE);
#endif
    fd = _PR_Getfd();
    if (fd) {
        /* Initialize the members of PRFileDesc and PRFilePrivate */
        fd->methods = methods;
        fd->secret->state = _PR_FILEDESC_OPEN;
	fd->secret->md.osfd = osfd;
        _PR_MD_INIT_FILEDESC(fd);
    } else {
	    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }

    return fd;
}

PR_IMPLEMENT(void) PR_FreeFileDesc(PRFileDesc *fd)
{
    PR_ASSERT(fd);
#ifdef XP_MAC
	_PR_MD_FREE_FILEDESC(fd);
#endif
    _PR_Putfd(fd);
}

/*
** Wait for some i/o to finish on one or more more poll descriptors.
*/
PR_IMPLEMENT(PRInt32) PR_Poll(PRPollDesc *pds, PRIntn npds, PRIntervalTime timeout)
{
	return(_PR_MD_PR_POLL(pds, npds, timeout));
}

/*
** Set the inheritance attribute of a file descriptor.
*/
PR_IMPLEMENT(PRStatus) PR_SetFDInheritable(
    PRFileDesc *fd,
    PRBool inheritable)
{
#if defined(XP_UNIX) || defined(WIN32) || defined(XP_OS2)
    /*
     * Only a non-layered, NSPR file descriptor can be inherited
     * by a child process.
     */
    if (fd->identity != PR_NSPR_IO_LAYER) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return PR_FAILURE;
    }
    if (fd->secret->inheritable != inheritable) {
        if (_PR_MD_SET_FD_INHERITABLE(fd, inheritable) == PR_FAILURE) {
            return PR_FAILURE;
        }
        fd->secret->inheritable = inheritable;
    }
    return PR_SUCCESS;
#else
#ifdef XP_MAC
#pragma unused (fd, inheritable)
#endif
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return PR_FAILURE;
#endif
}
