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

/* NOTE: Macros that claim to abort no longer abort, see bug 486574.
 */

// See Mozilla bug 163260.
// This file can only be included in an Objective-C context.

void nsObjCExceptionLog(NSException* aException);

// For wrapping blocks of Obj-C calls. Does not actually terminate.
#define NS_OBJC_BEGIN_TRY_ABORT_BLOCK @try {
#define NS_OBJC_END_TRY_ABORT_BLOCK \
  }                                 \
  @catch (NSException * _exn) {     \
    nsObjCExceptionLog(_exn);       \
  }

// Same as above ABORT_BLOCK but returns a value after the try/catch block to
// suppress compiler warnings. This allows us to avoid having to refactor code
// to get scoping right when wrapping an entire method.

#define NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL @try {
#define NS_OBJC_END_TRY_ABORT_BLOCK_NIL \
  }                                     \
  @catch (NSException * _exn) {         \
    nsObjCExceptionLog(_exn);           \
  }                                     \
  return nil;

#define NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSNULL @try {
#define NS_OBJC_END_TRY_ABORT_BLOCK_NSNULL \
  }                                        \
  @catch (NSException * _exn) {            \
    nsObjCExceptionLog(_exn);              \
  }                                        \
  return nullptr;

#define NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT @try {
#define NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT \
  }                                          \
  @catch (NSException * _exn) {              \
    nsObjCExceptionLog(_exn);                \
  }                                          \
  return NS_ERROR_FAILURE;

#define NS_OBJC_BEGIN_TRY_BLOCK_RETURN @try {
#define NS_OBJC_END_TRY_BLOCK_RETURN(_rv) \
  }                                       \
  @catch (NSException * _exn) {           \
    nsObjCExceptionLog(_exn);             \
  }                                       \
  return _rv;

#endif  // nsObjCExceptions_h_
