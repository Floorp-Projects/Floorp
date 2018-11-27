/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if defined(_WIN32)

#include <errno.h>
#include <process.h>
#include <stdlib.h>
#include <windows.h>

#include "config.h"
#include "src/thread.h"

typedef struct dav1d_win32_thread_t {
    HANDLE h;
    void* param;
    void*(*proc)(void*);
    void* res;
} dav1d_win32_thread_t;

static unsigned __stdcall dav1d_thread_entrypoint(void* data) {
    dav1d_win32_thread_t* t = data;
    t->res = t->proc(t->param);
    return 0;
}

int dav1d_pthread_create(pthread_t* thread, const pthread_attr_t* attr,
                         void*(*proc)(void*), void* param)
{
    dav1d_win32_thread_t* th = *thread = malloc(sizeof(*th));
    (void)attr;
    if (th == NULL)
        return ENOMEM;
    th->proc = proc;
    th->param = param;
    uintptr_t h = _beginthreadex(NULL, 0, dav1d_thread_entrypoint, th, 0, NULL);
    if ( h == 0 ) {
        int err = errno;
        free(th);
        *thread = NULL;
        return err;
    }
    th->h = (HANDLE)h;
    return 0;
}

void dav1d_pthread_join(pthread_t thread, void** res) {
    dav1d_win32_thread_t* th = thread;
    WaitForSingleObject(th->h, INFINITE);

    if (res != NULL)
        *res = th->res;
    CloseHandle(th->h);
    free(th);
}

int dav1d_pthread_once(pthread_once_t *once_control,
                       void (*init_routine)(void))
{
    BOOL fPending = FALSE;
    BOOL fStatus;

    fStatus = InitOnceBeginInitialize(once_control, 0, &fPending, NULL);
    if (fStatus != TRUE)
        return EINVAL;

    if (fPending == TRUE)
        init_routine();

    fStatus = InitOnceComplete(once_control, 0, NULL);
    if (!fStatus)
        return EINVAL;

    return 0;
}

#endif
