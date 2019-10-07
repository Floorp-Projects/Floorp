/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prbit.h"
#include "prsystem.h"

#ifdef XP_UNIX
#include <unistd.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

PRInt32 _pr_pageShift;
PRInt32 _pr_pageSize;

/*
** Get system page size
*/
static void GetPageSize(void)
{
    PRInt32 pageSize;

    /* Get page size */
#ifdef XP_UNIX
#if defined BSDI || defined AIX \
        || defined LINUX || defined __GNU__ || defined __GLIBC__ \
        || defined FREEBSD || defined NETBSD || defined OPENBSD \
        || defined DARWIN
    _pr_pageSize = getpagesize();
#elif defined(HPUX)
    /* I have no idea. Don't get me started. --Rob */
    _pr_pageSize = sysconf(_SC_PAGE_SIZE);
#else
    _pr_pageSize = sysconf(_SC_PAGESIZE);
#endif
#endif /* XP_UNIX */

#ifdef XP_PC
#ifdef _WIN32
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    _pr_pageSize = info.dwPageSize;
#else
    _pr_pageSize = 4096;
#endif
#endif /* XP_PC */

    pageSize = _pr_pageSize;
    PR_CEILING_LOG2(_pr_pageShift, pageSize);
}

PR_IMPLEMENT(PRInt32) PR_GetPageShift(void)
{
    if (!_pr_pageSize) {
        GetPageSize();
    }
    return _pr_pageShift;
}

PR_IMPLEMENT(PRInt32) PR_GetPageSize(void)
{
    if (!_pr_pageSize) {
        GetPageSize();
    }
    return _pr_pageSize;
}
