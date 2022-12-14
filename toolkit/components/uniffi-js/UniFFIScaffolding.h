/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_UniFFI_h
#define mozilla_dom_UniFFI_h

#include "mozilla/ErrorResult.h"
#include "mozilla/dom/RootedDictionary.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/dom/UniFFIBinding.h"

namespace mozilla::dom {

using ScaffoldingType = OwningDoubleOrArrayBufferOrUniFFIPointer;

// Handle functions defined in UniFFIScaffolding.webidl
class UniFFIScaffolding {
 public:
  static already_AddRefed<Promise> CallAsync(
      const GlobalObject& aUniFFIGlobal, uint64_t aId,
      const Sequence<ScaffoldingType>& aArgs, ErrorResult& aUniFFIErrorResult);

  static void CallSync(
      const GlobalObject& aUniFFIGlobal, uint64_t aId,
      const Sequence<ScaffoldingType>& aArgs,
      RootedDictionary<UniFFIScaffoldingCallResult>& aUniFFIReturnValue,
      ErrorResult& aUniFFIErrorResult);

  static already_AddRefed<UniFFIPointer> ReadPointer(
      const GlobalObject& aUniFFIGlobal, uint64_t aId,
      const ArrayBuffer& aArrayBuff, long aPosition, ErrorResult& aError);

  static void WritePointer(const GlobalObject& aUniFFIGlobal, uint64_t aId,
                           const UniFFIPointer& aPtr,
                           const ArrayBuffer& aArrayBuff, long aPosition,
                           ErrorResult& aError);
};

}  // namespace mozilla::dom

#endif /* mozilla_dom_UniFFI_h */
