/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XPCOM_THREADS_STATICSTRING_H_
#define XPCOM_THREADS_STATICSTRING_H_

#include <cstddef>
#include "mozilla/Attributes.h"

// from "nsStringFwd.h"
template <typename T>
class nsTLiteralString;
using nsLiteralCString = nsTLiteralString<char>;

namespace mozilla {
// class StaticString
//
// Wrapper type containing a C-style string which is guaranteed[*] to have been
// created (potentially indirectly) from static data. Primarily intended for
// text that may eventually be sent up via telemetry, to avoid the possibility
// of accidentally exfiltrating PII.
//
// `StaticString`, like `template <size_t N> const char (&str)[N]`, can be
// freely and implicitly converted to `const char *`, to simplify its use as a
// drop-in replacement when detemplatizing functions.
//
// ### Comparison/contrast with `nsLiteralCString`
//
// Concretely, `StaticString` is smaller than `nsLiteralCString`, as it does not
// store the string-length. It's also trivial to construct a `StaticString` from
// the variable `__func__` or the preprocessor token `__FILE__`, which would
// require additional work to be used with the latter's `_ns` constructor.
//
// Conventionally, the primary intended use case of `StaticString` is subtly
// different from that of `nsLiteralCString`:
//   * `StaticString` is intended for correctness (etc.) in contexts where the
//     consumer of the string requires that it be static.
//   * `nsLiteralCString` is more for efficiency in contexts where the source
//     string _happens to be_ static, but in which the consumer does not care
//     (and so accepts `nsACString const &` or similar).
//
// This is not a hard rule, however, and is notably bent in dom::Promise. (See
// comments below.)
//
// Both are trivially-copyable/-movable/-destructible, guaranteed non-null, and
// can only contain static data.
//
// #### Footnotes
//
// [*] ```
//     CHORUS:   "What, never?"
//     CAPTAIN:  "Well... hardly ever!"
//     CHORUS:   "He's hardly ever sick of C!"
//         -- Gilbert & Sullivan, _H.M.S. Pinafore_ (emended)
// ```
//
class StaticString {
  /* TODO(C++20): convert `constexpr` to `consteval` wherever possible. */
  const char* mStr;  // guaranteed nonnull

 public:
  template <size_t N>
  constexpr MOZ_IMPLICIT StaticString(const char (&str)[N]) : mStr(str) {}

  // `nsLiteralCString` has the same guarantees as `StaticString` (both in being
  // nonnull and containing only static data), so it's safe to construct either
  // from the other.
  //
  // At present we only support construction of a `StaticString` from an
  // `nsLiteralCString`, since this is zero-cost (the converse would not be),
  // and is probably the simplest way to support dom::Promise's interoperation
  // with MozPromise.
  //
  // (A more principled approach, in some sense, would be to create a third type
  // `StaticStringWithLength` (or whatever) acting as the lattice-join of the
  // two, which could then decay to either one as necessary. This is overkill
  // for our current goals... but might be worthwhile if, _e.g._, you really
  // need to get `__func__` into an `nsLiteralCString` rather than just an
  // `nsDependentCString` for some reason.)
  //
  constexpr explicit StaticString(nsLiteralCString const& str);

  constexpr StaticString(StaticString const&) = default;
  constexpr StaticString(StaticString&&) = default;
  ~StaticString() = default;

  constexpr MOZ_IMPLICIT operator const char*() const { return mStr; }

  // Not normally needed, but useful for variadic logging functions.
  constexpr const char* get() const { return mStr; }
};

// Under the covers, StaticString is as lightweight as a single pointer: it does
// not store the length of its deta.
static_assert(sizeof(StaticString) == sizeof(const char*));
static_assert(alignof(StaticString) == alignof(const char*));
}  // namespace mozilla

#endif
