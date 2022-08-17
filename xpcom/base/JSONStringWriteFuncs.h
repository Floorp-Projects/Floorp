/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef JSONSTRINGWRITEFUNCS_H
#define JSONSTRINGWRITEFUNCS_H

#include "mozilla/JSONWriter.h"
#include "nsString.h"

#include <type_traits>

namespace mozilla {

// JSONWriteFunc that writes to an owned string.
template <typename StringType>
class JSONStringWriteFunc final : public JSONWriteFunc {
  static_assert(
      !std::is_reference_v<StringType>,
      "Use JSONStringRefWriteFunc instead to write to a referenced string");

 public:
  JSONStringWriteFunc() = default;

  void Write(const Span<const char>& aStr) final { mString.Append(aStr); }

  const StringType& StringCRef() const { return mString; }

  StringType&& StringRRef() && { return std::move(mString); }

 private:
  StringType mString;
};

// JSONWriteFunc that writes to a given nsACString reference.
class JSONStringRefWriteFunc final : public JSONWriteFunc {
 public:
  MOZ_IMPLICIT JSONStringRefWriteFunc(nsACString& aString) : mString(aString) {}

  void Write(const Span<const char>& aStr) final { mString.Append(aStr); }

  const nsACString& StringCRef() const { return mString; }

 private:
  nsACString& mString;
};

}  // namespace mozilla

#endif  // JSONSTRINGWRITEFUNCS_H
