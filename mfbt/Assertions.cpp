/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2012
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Hommey <mh@glandium.org>
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

#include "mozilla/Attributes.h"
#include "mozilla/Types.h"

#include <cstdio>
#include <cstdlib>
#ifndef WIN32
#include <signal.h>
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

/* Implementations of runtime and static assertion macros for C and C++. */

extern "C" {

MOZ_EXPORT_API(void)
MOZ_Crash()
{
  /*
   * We write 123 here so that the machine code for this function is
   * unique. Otherwise the linker, trying to be smart, might use the
   * same code for MOZ_Crash and for some other function. That
   * messes up the signature in minidumps.
   */

#if defined(WIN32)
  /*
   * We used to call DebugBreak() on Windows, but amazingly, it causes
   * the MSVS 2010 debugger not to be able to recover a call stack.
   */
  *((volatile int *) NULL) = 123;
  exit(3);
#elif defined(ANDROID)
  /*
   * On Android, raise(SIGABRT) is handled asynchronously. Seg fault now
   * so we crash immediately and capture the current call stack.
   */
  *((volatile int *) NULL) = 123;
  abort();
#elif defined(__APPLE__)
  /*
   * On Mac OS X, Breakpad ignores signals. Only real Mach exceptions are
   * trapped.
   */
  *((volatile int *) NULL) = 123;  /* To continue from here in GDB: "return" then "continue". */
  raise(SIGABRT);  /* In case above statement gets nixed by the optimizer. */
#else
  raise(SIGABRT);  /* To continue from here in GDB: "signal 0". */
#endif
}

MOZ_EXPORT_API(void)
MOZ_Assert(const char* s, const char* file, int ln)
{
#ifdef ANDROID
  __android_log_print(ANDROID_LOG_FATAL, "MOZ_Assert",
                      "Assertion failure: %s, at %s:%d\n", s, file, ln);
#else
  fprintf(stderr, "Assertion failure: %s, at %s:%d\n", s, file, ln);
  fflush(stderr);
#endif
  MOZ_Crash();
}

}
