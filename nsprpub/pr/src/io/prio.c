/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "primpl.h"

#include <string.h> /* for memset() */


/************************************************************************/

PRLock *_pr_flock_lock;

PRFileDesc *_pr_filedesc_freelist;
PRLock *_pr_filedesc_freelist_lock;

void _PR_InitIO(void)
{
    const PRIOMethods *methods = PR_GetFileMethods();

    _pr_filedesc_freelist = NULL;
    _pr_filedesc_freelist_lock = PR_NewLock();
    _pr_flock_lock = PR_NewLock();

#ifdef WIN32
    _pr_stdin = PR_AllocFileDesc((PRInt32)GetStdHandle(STD_INPUT_HANDLE),
            methods);
    _pr_stdout = PR_AllocFileDesc((PRInt32)GetStdHandle(STD_OUTPUT_HANDLE),
            methods);
    _pr_stderr = PR_AllocFileDesc((PRInt32)GetStdHandle(STD_ERROR_HANDLE),
            methods);
#ifdef WINNT
    _pr_stdin->secret->md.nonoverlapped = PR_TRUE;
    _pr_stdout->secret->md.nonoverlapped = PR_TRUE;
    _pr_stderr->secret->md.nonoverlapped = PR_TRUE;
#endif
#else
    _pr_stdin = PR_AllocFileDesc(0, methods);
    _pr_stdout = PR_AllocFileDesc(1, methods);
    _pr_stderr = PR_AllocFileDesc(2, methods);
#endif

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
    if (_pr_filedesc_freelist) {
        PR_Lock(_pr_filedesc_freelist_lock);
        if (_pr_filedesc_freelist) {
            PRFilePrivate *secretSave;
            fd = _pr_filedesc_freelist;
            _pr_filedesc_freelist = _pr_filedesc_freelist->secret->next;
            PR_Unlock(_pr_filedesc_freelist_lock);
            secretSave = fd->secret;
            memset(fd, 0, sizeof(PRFileDesc));
            memset(secretSave, 0, sizeof(PRFilePrivate));
            fd->secret = secretSave;
        } else {
            PR_Unlock(_pr_filedesc_freelist_lock);
            fd = PR_NEWZAP(PRFileDesc);
            fd->secret = PR_NEWZAP(PRFilePrivate);
        }
    } else {
        fd = PR_NEWZAP(PRFileDesc);
        fd->secret = PR_NEWZAP(PRFilePrivate);
    }
    if (fd) {
        /* Initialize the members of PRFileDesc and PRFilePrivate */
        fd->methods = methods;
        fd->secret->state = _PR_FILEDESC_OPEN;
	fd->secret->md.osfd = osfd;
    } else {
	    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    }

    return fd;
}

PR_IMPLEMENT(void) PR_FreeFileDesc(PRFileDesc *fd)
{
    PR_ASSERT(fd);
    PR_ASSERT(fd->secret->state == _PR_FILEDESC_CLOSED);

    fd->secret->state = _PR_FILEDESC_FREED;

    PR_Lock(_pr_filedesc_freelist_lock);

    /* Add to head of list- this is a LIFO structure to avoid constantly
     * using different structs
     */
    fd->secret->next = _pr_filedesc_freelist;
    _pr_filedesc_freelist = fd;

    PR_Unlock(_pr_filedesc_freelist_lock);
}

#ifdef XP_UNIX
#include <fcntl.h>    /* to pick up F_GETFL */
#endif

/*
** Wait for some i/o to finish on one or more more poll descriptors.
*/
PR_IMPLEMENT(PRInt32) PR_Poll(PRPollDesc *pds, PRIntn npds,
	PRIntervalTime timeout)
{
    PRPollDesc *pd, *epd;
    PRInt32 ready;
    PRThread *me = _PR_MD_CURRENT_THREAD();

    if (_PR_PENDING_INTERRUPT(me)) {
        me->flags &= ~_PR_INTERRUPT;
		PR_SetError(PR_PENDING_INTERRUPT_ERROR, 0);
        return -1;
    }
    /*
    ** Before we do the poll operation, check each descriptor and see if
    ** it needs special poll handling. If it does, use the method poll
    ** proc to check for data before blocking.
    */
    pd = pds;
    ready = 0;
    for (pd = pds, epd = pd + npds; pd < epd; pd++) {
        PRFileDesc *fd = pd->fd;
        PRInt16 in_flags = pd->in_flags;
        if (NULL != fd)
        {
            if (in_flags && fd->methods->poll) {
                PRInt16 out_flags;
                in_flags = (*fd->methods->poll)(fd, in_flags, &out_flags);
                if (0 != (in_flags & out_flags)) {
                    pd->out_flags = out_flags;  /* ready already */
                    ready++;
                }
            }
        }
    }
    if (ready != 0) {
        return ready;  /* don't need to block */
    }
	return(_PR_MD_PR_POLL(pds, npds, timeout));
}
