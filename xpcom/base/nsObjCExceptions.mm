/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsObjCExceptions.h"

#import <Foundation/Foundation.h>

#include <unistd.h>
#include <signal.h>

#include "nsICrashReporter.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"

#include "nsError.h"

void nsObjCExceptionLog(NSException* aException) {
  NSLog(@"Mozilla has caught an Obj-C exception [%@: %@]", [aException name],
        [aException reason]);

  // Attach exception info to the crash report.
  nsCOMPtr<nsICrashReporter> crashReporter =
      do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  if (crashReporter) {
    crashReporter->AppendObjCExceptionInfoToAppNotes(
        static_cast<void*>(aException));
  }

#ifdef DEBUG
  NSLog(@"Stack trace:\n%@", [aException callStackSymbols]);
#endif
}

namespace mozilla {

bool ShouldIgnoreObjCException(NSException* aException) {
  // Ignore known exceptions that we've seen in crash reports, which shouldn't
  // cause a MOZ_CRASH in Nightly.
  if (aException.name == NSInternalInconsistencyException) {
    if ([aException.reason containsString:@"Missing Touches."]) {
      // Seen in bug 1694000.
      return true;
    }
  }
  return false;
}

}  // namespace mozilla
