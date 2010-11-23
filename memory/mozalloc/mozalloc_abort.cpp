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
#endif

#if defined(XP_WIN) || defined(XP_OS2)
#  define MOZALLOC_EXPORT __declspec(dllexport)
#endif

#include "mozilla/mozalloc_abort.h"

static int gDummyCounter;

static void
TouchBadMemory()
{
    // XXX this should use the frame poisoning code
    gDummyCounter += *((int *) 0);   // TODO annotation saying we know 
                                     // this is crazy
}

void
mozalloc_abort(const char* const msg)
{
    fputs(msg, stderr);
    fputs("\n", stderr);

#if defined(XP_UNIX) && !defined(XP_MACOSX)
    abort();
#elif defined(_MSC_VER)
    __debugbreak();
#elif defined(XP_WIN)
    DebugBreak();
#endif
    // abort() doesn't trigger breakpad on Mac, "fall through" to the
    // fail-safe code

    // Still haven't aborted?  Try dereferencing null.
    TouchBadMemory();

    // Still haven't aborted?  Try _exit().
    _exit(127);
}
