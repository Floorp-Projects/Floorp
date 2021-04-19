/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "linux/handler/exception_handler.h"

using google_breakpad::ExceptionHandler;

static ExceptionHandler* gExceptionHandler = nullptr;

bool gBreakpadInjectorEnabled = true;

bool TestEnabled(void* /* context */) { return gBreakpadInjectorEnabled; }

bool SetGlobalExceptionHandler(
    ExceptionHandler::FilterCallback filterCallback,
    ExceptionHandler::MinidumpCallback minidumpCallback) {
  const char* tempPath = getenv("TMPDIR");
  if (!tempPath) tempPath = "/tmp";

  google_breakpad::MinidumpDescriptor descriptor(tempPath);

  gExceptionHandler = new ExceptionHandler(descriptor, filterCallback,
                                           minidumpCallback, nullptr, true, -1);
  if (!gExceptionHandler) return false;

  return true;
}

// Called when loading the DLL (eg via LD_PRELOAD, or the JS shell --dll
// option).
void __attribute__((constructor)) SetBreakpadExceptionHandler() {
  if (gExceptionHandler) abort();

  if (!SetGlobalExceptionHandler(TestEnabled, nullptr)) abort();

  if (!gExceptionHandler) abort();
}
