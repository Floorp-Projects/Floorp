/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsObjCExceptions_h_
#define nsObjCExceptions_h_

// Undo the damage that exception_defines.h does.
#undef try
#undef catch

@class NSException;

// See Mozilla bug 163260.
// This file can only be included in an Objective-C context.

void nsObjCExceptionLog(NSException* aException);

namespace mozilla {

// Check if this is an exception that's outside of our control.
// Called when an exception bubbles up to the native event loop.
bool ShouldIgnoreObjCException(NSException* aException);

}  // namespace mozilla

// For wrapping blocks of Obj-C calls which are not expected to throw exception.
// Causes a MOZ_CRASH if an Obj-C exception is encountered.
#define NS_OBJC_BEGIN_TRY_ABORT_BLOCK @try {
#define NS_OBJC_END_TRY_ABORT_BLOCK                            \
  }                                                            \
  @catch (NSException * _exn) {                                \
    nsObjCExceptionLog(_exn);                                  \
    MOZ_CRASH("Encountered unexpected Objective C exception"); \
  }

// For wrapping blocks of Obj-C calls. Logs the exception and moves on.
#define NS_OBJC_BEGIN_TRY_IGNORE_BLOCK @try {
#define NS_OBJC_END_TRY_IGNORE_BLOCK \
  }                                  \
  @catch (NSException * _exn) {      \
    nsObjCExceptionLog(_exn);        \
  }

// Same as above IGNORE_BLOCK but returns a value after the try/catch block.
#define NS_OBJC_BEGIN_TRY_BLOCK_RETURN @try {
#define NS_OBJC_END_TRY_BLOCK_RETURN(_rv) \
  }                                       \
  @catch (NSException * _exn) {           \
    nsObjCExceptionLog(_exn);             \
  }                                       \
  return _rv;

#endif  // nsObjCExceptions_h_
