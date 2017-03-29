// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_PTR_UTIL_H_
#define BASE_MEMORY_PTR_UTIL_H_

#include <memory>
#include <utility>

namespace base {

// Helper to transfer ownership of a raw pointer to a std::unique_ptr<T>.
// Note that std::unique_ptr<T> has very different semantics from
// std::unique_ptr<T[]>: do not use this helper for array allocations.
template <typename T>
std::unique_ptr<T> WrapUnique(T* ptr) {
  return std::unique_ptr<T>(ptr);
}

namespace internal {

template <typename T>
struct MakeUniqueResult {
  using Scalar = std::unique_ptr<T>;
};

template <typename T>
struct MakeUniqueResult<T[]> {
  using Array = std::unique_ptr<T[]>;
};

template <typename T, size_t N>
struct MakeUniqueResult<T[N]> {
  using Invalid = void;
};

}  // namespace internal

// Helper to construct an object wrapped in a std::unique_ptr. This is an
// implementation of C++14's std::make_unique that can be used in Chrome.
//
// MakeUnique<T>(args) should be preferred over WrapUnique(new T(args)): bare
// calls to `new` should be treated with scrutiny.
//
// Usage:
//   // ptr is a std::unique_ptr<std::string>
//   auto ptr = MakeUnique<std::string>("hello world!");
//
//   // arr is a std::unique_ptr<int[]>
//   auto arr = MakeUnique<int[]>(5);

// Overload for non-array types. Arguments are forwarded to T's constructor.
template <typename T, typename... Args>
typename internal::MakeUniqueResult<T>::Scalar MakeUnique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// Overload for array types of unknown bound, e.g. T[]. The array is allocated
// with `new T[n]()` and value-initialized: note that this is distinct from
// `new T[n]`, which default-initializes.
template <typename T>
typename internal::MakeUniqueResult<T>::Array MakeUnique(size_t size) {
  return std::unique_ptr<T>(new typename std::remove_extent<T>::type[size]());
}

// Overload to reject array types of known bound, e.g. T[n].
template <typename T, typename... Args>
typename internal::MakeUniqueResult<T>::Invalid MakeUnique(Args&&... args) =
    delete;

}  // namespace base

#endif  // BASE_MEMORY_PTR_UTIL_H_
