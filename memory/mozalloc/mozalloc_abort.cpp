/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>             // for abort()

#if defined(_MSC_VER)           // MSVC
#  include <intrin.h>           // for __debugbreak()
#elif defined(XP_WIN)           // mingw
#  include <windows.h>          // for DebugBreak
#elif defined(XP_UNIX)
#  include <unistd.h>           // for _exit
#  include <signal.h>
#endif

#if defined(XP_WIN) || defined(XP_OS2)
#  define MOZALLOC_EXPORT __declspec(dllexport)
#endif

#include "mozilla/mozalloc_abort.h"

static int gDummyCounter;

// Not inlining this function avoids the compiler making optimizations
// that end up corrupting stack traces.
MOZ_NEVER_INLINE static void
TouchBadMemory()
{
    // XXX this should use the frame poisoning code
    volatile int *p = 0;
    gDummyCounter += *p;   // TODO annotation saying we know 
                           // this is crazy
}

void
mozalloc_abort(const char* const msg)
{
    fputs(msg, stderr);
    fputs("\n", stderr);

#if defined(_MSC_VER)
    __debugbreak();
#elif defined(XP_WIN)
    DebugBreak();
#endif

    // On *NIX platforms the prefered way to abort is by touching bad memory,
    // since this generates a stack trace inside our own code (avoiding
    // problems with starting the trace inside libc, where we might not have
    // symbols and can get lost).

    TouchBadMemory();

    // If we haven't aborted yet, we can try to raise SIGABRT which might work
    // on some *NIXs, but not OS X (it doesn't trigger breakpad there).
    // Note that we don't call abort(), since raise is likelier to give us
    // useful stack data, and also since abort() is redirected to call this
    // function (see below).
#if defined(XP_UNIX) && !defined(XP_MACOSX)
    raise(SIGABRT);
#endif

    // Still haven't aborted?  Try _exit().
    _exit(127);
}

#if defined(XP_UNIX)
// Define abort() here, so that it is used instead of the system abort(). This
// lets us control the behavior when aborting, in order to get better results
// on *NIX platfrorms. See mozalloc_abort for details.
void abort(void)
{
  mozalloc_abort("Redirecting call to abort() to mozalloc_abort\n");
}
#endif

