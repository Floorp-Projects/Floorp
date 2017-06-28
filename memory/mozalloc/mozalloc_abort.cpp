/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mozalloc_abort.h"

#ifdef ANDROID
# include <android/log.h>
#endif
#ifdef MOZ_WIDGET_ANDROID
# include "APKOpen.h"
# include "dlfcn.h"
#endif
#include <stdio.h>

#include "mozilla/Assertions.h"

void
mozalloc_abort(const char* const msg)
{
#ifndef ANDROID
    fputs(msg, stderr);
    fputs("\n", stderr);
#else
    __android_log_print(ANDROID_LOG_ERROR, "Gecko", "mozalloc_abort: %s", msg);
#endif
#ifdef MOZ_WIDGET_ANDROID
    abortThroughJava(msg);
#endif
    MOZ_CRASH();
}

#ifdef MOZ_WIDGET_ANDROID
template <size_t N>
void fillAbortMessage(char (&msg)[N], uintptr_t retAddress) {
    /*
     * On Android, we often don't have reliable backtrace when crashing inside
     * abort(). Therefore, we try to find out who is calling abort() and add
     * that to the message.
     */
    Dl_info info = {};
    dladdr(reinterpret_cast<void*>(retAddress), &info);

    const char* const module = info.dli_fname ? info.dli_fname : "";
    const char* const base_module = strrchr(module, '/');
    const void* const module_offset =
        reinterpret_cast<void*>(retAddress - uintptr_t(info.dli_fbase));
    const char* const sym = info.dli_sname ? info.dli_sname : "";

    snprintf(msg, sizeof(msg), "abort() called from %s:%p (%s)",
             base_module ? base_module + 1 : module, module_offset, sym);
}
#endif

#if defined(XP_UNIX) && !defined(MOZ_ASAN)
// Define abort() here, so that it is used instead of the system abort(). This
// lets us control the behavior when aborting, in order to get better results
// on *NIX platforms. See mozalloc_abort for details.
//
// For AddressSanitizer, we must not redefine system abort because the ASan
// option "abort_on_error=1" calls abort() and therefore causes the following
// call chain with our redefined abort:
//
// ASan -> abort() -> moz_abort() -> MOZ_CRASH() -> Segmentation fault
//
// That segmentation fault will be interpreted as another bug by ASan and as a
// result, ASan will just exit(1) instead of aborting.
extern "C" void abort(void)
{
#ifdef MOZ_WIDGET_ANDROID
    char msg[64] = {};
    fillAbortMessage(msg, uintptr_t(__builtin_return_address(0)));
#else
    const char* const msg = "Redirecting call to abort() to mozalloc_abort\n";
#endif

    mozalloc_abort(msg);

    // We won't reach here because mozalloc_abort() is MOZ_NORETURN. But that
    // annotation isn't used on ARM (see mozalloc_abort.h for why) so we add a
    // redundant MOZ_CRASH() here to avoid a "'noreturn' function does return"
    // warning.
    MOZ_CRASH();
}
#endif

