/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"

#ifndef _MSC_VER  // Not supported by clang-cl yet

// When running with ThreadSanitizer, we need to explicitly set some
// options specific to our codebase to prevent errors during runtime.
// To override these, set the TSAN_OPTIONS environment variable.
//
// Currently, these are:
//
//   abort_on_error=1 - Causes TSan to abort instead of using exit().
//
//   report_signal_unsafe=0 - Required to avoid TSan deadlocks when
//   receiving external signals (e.g. SIGINT manually on console).
//
extern "C" const char* __tsan_default_options() {
  return "abort_on_error=1:report_signal_unsafe=0";
}

// When running with ThreadSanitizer, we sometimes need to suppress existing
// races. However, in any case, it should be either because
//
//    1) a bug is on file. In this case, the bug number should always be
//       included with the suppression.
//
// or 2) this is an intentional race. Please be very careful with judging
//       races as intentional and benign. Races in C++ are undefined behavior
//       and compilers increasingly rely on exploiting this for optimizations.
//       Hence, many seemingly benign races cause harmful or unexpected
//       side-effects.
//
//       See also:
//       https://software.intel.com/en-us/blogs/2013/01/06/benign-data-races-what-could-possibly-go-wrong
//
extern "C" const char* __tsan_default_suppressions() {
  return "# Add your suppressions below\n"

         // External uninstrumented libraries
         "called_from_lib:libatk-1\n"
         "called_from_lib:libgdk-3\n"
         "called_from_lib:libgdk_pixbuf\n"
         "called_from_lib:libgio-2\n"
         "called_from_lib:libglib\n"
         "called_from_lib:libgobject\n"
         "called_from_lib:libgtk-3\n"
         "called_from_lib:libgvfscommon\n"
         "called_from_lib:libgvfsdbus\n"
         "called_from_lib:libpangocairo\n"

         // libFuzzer internals
         "race:tools/fuzzing/libfuzzer/FuzzerTracePC.cpp\n"
         "race:.L__sancov_gen_\n"

         // TSan internals
         "race:__tsan::ProcessPendingSignals\n"
         "race:__tsan::CallUserSignalHandler\n"

         // Bug 1367344
         "race:TelemetryImpl::sTelemetry\n"

         // Bug 1506812
         "race:BeginBackgroundRead\n"

         // Bug 1506910
         "race:gMozillaPoisonValue\n"

         // Bug 1587510
         "race:SystemGroupImpl::sSingleton\n"

         // Bug 1587513
         "race:std::sync::mutex::Mutex\n"

      // End of suppressions.
      ;  // Please keep this semicolon.
}
#endif  // _MSC_VER
