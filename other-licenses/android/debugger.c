/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include <sys/mman.h>
#include <errno.h>

#include "linker.h"

#include <sys/socket.h>
#ifndef MOZ_LINKER
#include <cutils/sockets.h>
#endif

void notify_gdb_of_libraries();

#define  RETRY_ON_EINTR(ret,cond) \
    do { \
        ret = (cond); \
    } while (ret < 0 && errno == EINTR)

void debugger_signal_handler(int n)
{
    unsigned tid;
    int s;

    /* avoid picking up GC interrupts */
    signal(SIGUSR1, SIG_IGN);

    tid = gettid();
#ifndef MOZ_LINKER
    s = socket_local_client("android:debuggerd",
            ANDROID_SOCKET_NAMESPACE_ABSTRACT, SOCK_STREAM);
#else
    s = -1;
#endif

    if(s >= 0) {
        /* debugger knows our pid from the credentials on the
         * local socket but we need to tell it our tid.  It
         * is paranoid and will verify that we are giving a tid
         * that's actually in our process
         */
        int  ret;

        RETRY_ON_EINTR(ret, write(s, &tid, sizeof(unsigned)));
        if (ret == sizeof(unsigned)) {
            /* if the write failed, there is no point to read on
             * the file descriptor. */
            RETRY_ON_EINTR(ret, read(s, &tid, 1));
            notify_gdb_of_libraries();
        }
        close(s);
    }

    /* remove our net so we fault for real when we return */
    signal(n, SIG_IGN);
}

void debugger_init()
{
    signal(SIGILL, debugger_signal_handler);
    signal(SIGABRT, debugger_signal_handler);
    signal(SIGBUS, debugger_signal_handler);
    signal(SIGFPE, debugger_signal_handler);
    signal(SIGSEGV, debugger_signal_handler);
    signal(SIGSTKFLT, debugger_signal_handler);
    signal(SIGPIPE, debugger_signal_handler);
}
