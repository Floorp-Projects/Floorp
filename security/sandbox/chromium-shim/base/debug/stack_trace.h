/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a dummy version of Chromium source file base/debug/stack_trace.h.
// To provide a dummy class StackTrace required in base/win/scoped_handle.cc.

#ifndef BASE_DEBUG_STACK_TRACE_H_
#define BASE_DEBUG_STACK_TRACE_H_

namespace base {
namespace debug {

class BASE_EXPORT StackTrace {
 public:
  StackTrace() {};
};

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_STACK_TRACE_H_
