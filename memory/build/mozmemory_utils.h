/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozjemalloc_utils_h
#define mozjemalloc_utils_h

#include <optional>
#include <type_traits>

#if defined(MOZ_MEMORY) && defined(XP_WIN)
#  include "mozmemory_wrap.h"
#endif

namespace mozilla {

namespace detail {
// Helper for StallAndRetry error messages.
template <typename T>
constexpr bool is_std_optional = false;
template <typename T>
constexpr bool is_std_optional<std::optional<T>> = true;
}  // namespace detail

struct StallSpecs {
  // Maximum number of retry-attempts before giving up.
  size_t maxAttempts;
  // Delay time between successive events.
  size_t delayMs;

  // Retry a fallible operation until it succeeds or until we've run out of
  // retries.
  //
  // Note that this invokes `aDelayFunc` immediately upon being called! It's
  // intended for use in the unhappy path, after an initial attempt has failed.
  //
  // The function type here may be read:
  // ```
  // fn StallAndRetry<R>(
  //     delay_func: impl Fn(usize) -> (),
  //     operation: impl Fn() -> Option<R>,
  // ) -> Option<R>;
  // ```
  //
  template <typename DelayFunc, typename OpFunc>
  auto StallAndRetry(DelayFunc&& aDelayFunc, OpFunc&& aOperation) const
      -> decltype(aOperation()) {
    {
      // Explicit typecheck for OpFunc, to provide an explicit error message.
      using detail::is_std_optional;
      static_assert(is_std_optional<decltype(aOperation())>,
                    "aOperation() must return std::optional");

      // (clang's existing error messages suffice for aDelayFunc.)
    }

    for (size_t i = 0; i < maxAttempts; ++i) {
      aDelayFunc(delayMs);
      if (const auto opt = aOperation()) {
        return opt;
      }
    }
    return std::nullopt;
  }
};

#if defined(MOZ_MEMORY) && defined(XP_WIN)
MOZ_JEMALLOC_API StallSpecs GetAllocatorStallSpecs();
#endif

}  // namespace mozilla

#endif  // mozjemalloc_utils_h
