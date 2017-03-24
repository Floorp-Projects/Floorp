// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MOVE_H_
#define BASE_MOVE_H_

#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "build/build_config.h"

// TODO(crbug.com/566182): DEPRECATED!
// Use DISALLOW_COPY_AND_ASSIGN instead, or if your type will be used in
// Callbacks, use DISALLOW_COPY_AND_ASSIGN_WITH_MOVE_FOR_BIND instead.
#define MOVE_ONLY_TYPE_FOR_CPP_03(type) \
  DISALLOW_COPY_AND_ASSIGN_WITH_MOVE_FOR_BIND(type)

// A macro to disallow the copy constructor and copy assignment functions.
// This should be used in the private: declarations for a class.
//
// Use this macro instead of DISALLOW_COPY_AND_ASSIGN if you want to pass
// ownership of the type through a base::Callback without heap-allocating it
// into a scoped_ptr.  The class must define a move constructor and move
// assignment operator to make this work.
//
// This version of the macro adds a Pass() function and a cryptic
// MoveOnlyTypeForCPP03 typedef for the base::Callback implementation to use.
// See IsMoveOnlyType template and its usage in base/callback_internal.h
// for more details.
// TODO(crbug.com/566182): Remove this macro and use DISALLOW_COPY_AND_ASSIGN
// everywhere instead.
#if defined(OS_ANDROID) || defined(OS_LINUX) || defined(OS_MACOSX)
#define DISALLOW_COPY_AND_ASSIGN_WITH_MOVE_FOR_BIND(type)       \
 private:                                                       \
  type(const type&) = delete;                                   \
  void operator=(const type&) = delete;                         \
                                                                \
 public:                                                        \
  typedef void MoveOnlyTypeForCPP03;                            \
                                                                \
 private:
#else
#define DISALLOW_COPY_AND_ASSIGN_WITH_MOVE_FOR_BIND(type)       \
 private:                                                       \
  type(const type&) = delete;                                   \
  void operator=(const type&) = delete;                         \
                                                                \
 public:                                                        \
  type&& Pass() WARN_UNUSED_RESULT { return std::move(*this); } \
  typedef void MoveOnlyTypeForCPP03;                            \
                                                                \
 private:
#endif

#endif  // BASE_MOVE_H_
