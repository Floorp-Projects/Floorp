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

#if defined(WIN95)
/*
** Some local variables report warnings on Win95 because the code paths 
** using them are conditioned on HAVE_CUSTOME_USER_THREADS.
** The pragma suppresses the warning.
** 
*/
#pragma warning(disable : 4101)
#endif

/* XXX use unbuffered nspr stdio */

PRFileDesc *_pr_dumpOut;

PRUint32 _PR_DumpPrintf(PRFileDesc *fd, const char *fmt, ...)
{
    char buf[100];
    PRUint32 nb;
    va_list ap;

    va_start(ap, fmt);
    nb = PR_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    PR_Write(fd, buf, nb);

    return nb;
}

void _PR_DumpThread(PRFileDesc *fd, PRThread *thread)
{

#ifndef _PR_GLOBAL_THREADS_ONLY
    _PR_DumpPrintf(fd, "%05d[%08p] pri=%2d flags=0x%02x",
                   thread->id, thread, thread->priority, thread->flags);
    switch (thread->state) {
      case _PR_RUNNABLE:
      case _PR_RUNNING:
        break;
      case _PR_LOCK_WAIT:
        _PR_DumpPrintf(fd, " lock=%p", thread->wait.lock);
        break;
      case _PR_COND_WAIT:
        _PR_DumpPrintf(fd, " condvar=%p sleep=%lldms",
                       thread->wait.cvar, thread->sleep);
        break;
      case _PR_SUSPENDED:
        _PR_DumpPrintf(fd, " suspended");
        break;
    }
    PR_Write(fd, "\n", 1);
#endif

    /* Now call dump routine */
    if (thread->dump) {
	thread->dump(fd, thread, thread->dumpArg);
    }
}

static void DumpThreadQueue(PRFileDesc *fd, PRCList *list)
{
#ifndef _PR_GLOBAL_THREADS_ONLY
    PRCList *q;

    q = list->next;
    while (q != list) {
        PRThread *t = _PR_THREAD_PTR(q);
        _PR_DumpThread(fd, t);
        q = q->next;
    }
#endif
}

void _PR_DumpThreads(PRFileDesc *fd)
{
    PRThread *t;
    PRIntn i;

    _PR_DumpPrintf(fd, "Current Thread:\n");
    t = _PR_MD_CURRENT_THREAD();
    _PR_DumpThread(fd, t);

    _PR_DumpPrintf(fd, "Runnable Threads:\n");
    for (i = 0; i < 32; i++) {
        DumpThreadQueue(fd, &_PR_RUNQ(t->cpu)[i]);
    }

    _PR_DumpPrintf(fd, "CondVar timed wait Threads:\n");
    DumpThreadQueue(fd, &_PR_SLEEPQ(t->cpu));

    _PR_DumpPrintf(fd, "CondVar wait Threads:\n");
    DumpThreadQueue(fd, &_PR_PAUSEQ(t->cpu));

    _PR_DumpPrintf(fd, "Suspended Threads:\n");
    DumpThreadQueue(fd, &_PR_SUSPENDQ(t->cpu));
}

PR_IMPLEMENT(void) PR_ShowStatus(void)
{
    PRIntn is;

    if ( _PR_MD_CURRENT_THREAD()
    && !_PR_IS_NATIVE_THREAD(_PR_MD_CURRENT_THREAD())) _PR_INTSOFF(is);
    _pr_dumpOut = _pr_stderr;
    _PR_DumpThreads(_pr_dumpOut);
    if ( _PR_MD_CURRENT_THREAD()
    && !_PR_IS_NATIVE_THREAD(_PR_MD_CURRENT_THREAD())) _PR_FAST_INTSON(is);
}

PR_IMPLEMENT(void)
PR_SetThreadDumpProc(PRThread* thread, PRThreadDumpProc dump, void *arg)
{
    thread->dump = dump;
    thread->dumpArg = arg;
}
