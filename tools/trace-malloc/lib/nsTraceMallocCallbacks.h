/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is the Windows port of trace-malloc.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Judge (original author)
 *   L. David Baron <dbaron@dbaron.org>, Mozilla Corporation
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* declarations needed by both nsTraceMalloc.c and nsWinTraceMalloc.cpp */

#ifndef NSTRACEMALLOCCALLBACKS_H
#define NSTRACEMALLOCCALLBACKS_H

#include <stdlib.h>

PR_BEGIN_EXTERN_C

/* Used by backtrace. */
typedef struct stack_buffer_info {
    void **buffer;
    size_t size;
    size_t entries;
} stack_buffer_info;

typedef struct tm_thread tm_thread;
struct tm_thread {
    /*
     * This counter suppresses tracing, in case any tracing code needs
     * to malloc.
     */
    uint32 suppress_tracing;

    /* buffer for backtrace, below */
    stack_buffer_info backtrace_buf;
};

/* implemented in nsTraceMalloc.c */
tm_thread * tm_get_thread(void);

/* implemented in nsTraceMalloc.c */
PR_EXTERN(void) MallocCallback(void *aPtr, size_t aSize, PRUint32 start, PRUint32 end, tm_thread *t);
PR_EXTERN(void) CallocCallback(void *aPtr, size_t aCount, size_t aSize, PRUint32 start, PRUint32 end, tm_thread *t);
PR_EXTERN(void) ReallocCallback(void *aPin, void* aPout, size_t aSize, PRUint32 start, PRUint32 end, tm_thread *t);
PR_EXTERN(void) FreeCallback(void *aPtr, PRUint32 start, PRUint32 end, tm_thread *t);

#ifdef XP_WIN32
/* implemented in nsTraceMalloc.c */
PR_EXTERN(void) StartupHooker();
PR_EXTERN(void) ShutdownHooker();

/* implemented in nsWinTraceMalloc.cpp */
void* dhw_orig_malloc(size_t);
void* dhw_orig_calloc(size_t, size_t);
void* dhw_orig_realloc(void*, size_t);
void dhw_orig_free(void*);

#endif /* defined(XP_WIN32) */

PR_END_EXTERN_C

#endif /* !defined(NSTRACEMALLOCCALLBACKS_H) */
