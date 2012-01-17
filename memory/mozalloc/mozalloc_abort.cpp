/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 */
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
 * The Original Code is Mozilla Code
 *
 * The Initial Developer of the Original Code is
 * The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
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

