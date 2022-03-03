/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ThreadAnnotation_h
#define ThreadAnnotation_h

#include <functional>

#include "nsExceptionHandler.h"

// Thread annotation interfaces for the crash reporter.
namespace CrashReporter {

void InitThreadAnnotation();

void ShutdownThreadAnnotation();

void GetFlatThreadAnnotation(const std::function<void(const char*)>& aCallback,
                             bool aIsHandlingException = false);

}  // namespace CrashReporter

#endif
