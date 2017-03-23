// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_CALLBACK_FORWARD_H_
#define BASE_CALLBACK_FORWARD_H_

namespace base {
namespace internal {

// CopyMode is used to control the copyablity of a Callback.
// MoveOnly indicates the Callback is not copyable but movable, and Copyable
// indicates it is copyable and movable.
enum class CopyMode {
  MoveOnly,
  Copyable,
};

enum class RepeatMode {
  Once,
  Repeating,
};

}  // namespace internal

template <typename Signature,
          internal::CopyMode copy_mode = internal::CopyMode::Copyable,
          internal::RepeatMode repeat_mode = internal::RepeatMode::Repeating>
class Callback;

// Syntactic sugar to make Callback<void()> easier to declare since it
// will be used in a lot of APIs with delayed execution.
using Closure = Callback<void()>;

template <typename Signature>
using OnceCallback = Callback<Signature,
                              internal::CopyMode::MoveOnly,
                              internal::RepeatMode::Once>;
template <typename Signature>
using RepeatingCallback = Callback<Signature,
                                   internal::CopyMode::Copyable,
                                   internal::RepeatMode::Repeating>;
using OnceClosure = OnceCallback<void()>;
using RepeatingClosure = RepeatingCallback<void()>;

}  // namespace base

#endif  // BASE_CALLBACK_FORWARD_H_
