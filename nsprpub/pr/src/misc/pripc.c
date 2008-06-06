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
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
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

/*
 * File: pripc.c
 *
 * Description: functions for IPC support
 */

#include "primpl.h"

#include <string.h>

/*
 * A POSIX IPC name must begin with a '/'.
 * A POSIX IPC name on Solaris cannot contain any '/' except
 * the required leading '/'.
 * A POSIX IPC name on HP-UX and OSF1 must be a valid pathname
 * in the file system.
 *
 * The ftok() function for System V IPC requires a valid pathname
 * in the file system.
 *
 * A Win32 IPC name cannot contain '\'.
 */

static void _pr_ConvertSemName(char *result)
{
#ifdef _PR_HAVE_POSIX_SEMAPHORES
#if defined(SOLARIS)
    char *p;

    /* Convert '/' to '_' except for the leading '/' */
    for (p = result+1; *p; p++) {
        if (*p == '/') {
            *p = '_';
        }
    }
    return;
#else
    return;
#endif
#elif defined(_PR_HAVE_SYSV_SEMAPHORES)
    return;
#elif defined(WIN32)
    return;
#endif
}

static void _pr_ConvertShmName(char *result)
{
#if defined(PR_HAVE_POSIX_NAMED_SHARED_MEMORY)
#if defined(SOLARIS)
    char *p;

    /* Convert '/' to '_' except for the leading '/' */
    for (p = result+1; *p; p++) {
        if (*p == '/') {
            *p = '_';
        }
    }
    return;
#else
    return;
#endif
#elif defined(PR_HAVE_SYSV_NAMED_SHARED_MEMORY)
    return;
#elif defined(WIN32)
    return;
#else
    return;
#endif
}

PRStatus _PR_MakeNativeIPCName(
    const char *name,
    char *result,
    PRIntn size,
    _PRIPCType type)
{
    if (strlen(name) >= (PRSize)size) {
        PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
        return PR_FAILURE;
    }
    strcpy(result, name);
    switch (type) {
        case _PRIPCSem:
            _pr_ConvertSemName(result);
            break;
        case _PRIPCShm:
            _pr_ConvertShmName(result);
            break;
        default:
            PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
            return PR_FAILURE;
    }
    return PR_SUCCESS;
}
