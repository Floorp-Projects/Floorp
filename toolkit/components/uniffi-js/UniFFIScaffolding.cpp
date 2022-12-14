/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <inttypes.h>
#include "nsError.h"
#include "nsString.h"
#include "nsPrintfCString.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/UniFFIScaffolding.h"

// This file implements the UniFFI WebIDL interface by leveraging the generate
// code in UniFFIScaffolding.cpp and UniFFIFixtureScaffolding.cpp.  It's main
// purpose is to check if MOZ_UNIFFI_FIXTURES is set and only try calling the
// scaffolding code if it is.

using mozilla::dom::ArrayBuffer;
using mozilla::dom::GlobalObject;
using mozilla::dom::Promise;
using mozilla::dom::RootedDictionary;
using mozilla::dom::ScaffoldingType;
using mozilla::dom::Sequence;
using mozilla::dom::UniFFIPointer;
using mozilla::dom::UniFFIScaffoldingCallResult;

namespace mozilla::uniffi {

// Prototypes for the generated functions
Maybe<already_AddRefed<Promise>> UniFFICallAsync(
    const GlobalObject& aGlobal, uint64_t aId,
    const Sequence<ScaffoldingType>& aArgs, ErrorResult& aError);
bool UniFFICallSync(const GlobalObject& aGlobal, uint64_t aId,
                    const Sequence<ScaffoldingType>& aArgs,
                    RootedDictionary<UniFFIScaffoldingCallResult>& aReturnValue,
                    ErrorResult& aError);
Maybe<already_AddRefed<UniFFIPointer>> UniFFIReadPointer(
    const GlobalObject& aGlobal, uint64_t aId, const ArrayBuffer& aArrayBuff,
    long aPosition, ErrorResult& aError);
bool UniFFIWritePointer(const GlobalObject& aGlobal, uint64_t aId,
                        const UniFFIPointer& aPtr,
                        const ArrayBuffer& aArrayBuff, long aPosition,
                        ErrorResult& aError);

#ifdef MOZ_UNIFFI_FIXTURES
Maybe<already_AddRefed<Promise>> UniFFIFixturesCallAsync(
    const GlobalObject& aGlobal, uint64_t aId,
    const Sequence<ScaffoldingType>& aArgs, ErrorResult& aError);
bool UniFFIFixturesCallSync(
    const GlobalObject& aGlobal, uint64_t aId,
    const Sequence<ScaffoldingType>& aArgs,
    RootedDictionary<UniFFIScaffoldingCallResult>& aReturnValue,
    ErrorResult& aError);
Maybe<already_AddRefed<UniFFIPointer>> UniFFIFixturesReadPointer(
    const GlobalObject& aGlobal, uint64_t aId, const ArrayBuffer& aArrayBuff,
    long aPosition, ErrorResult& aError);
bool UniFFIFixturesWritePointer(const GlobalObject& aGlobal, uint64_t aId,
                                const UniFFIPointer& aPtr,
                                const ArrayBuffer& aArrayBuff, long aPosition,
                                ErrorResult& aError);
#endif
}  // namespace mozilla::uniffi

namespace mozilla::dom {

// Implement the interface using the generated functions

already_AddRefed<Promise> UniFFIScaffolding::CallAsync(
    const GlobalObject& aGlobal, uint64_t aId,
    const Sequence<ScaffoldingType>& aArgs, ErrorResult& aError) {
  Maybe<already_AddRefed<Promise>> firstTry =
      uniffi::UniFFICallAsync(aGlobal, aId, aArgs, aError);
  if (firstTry.isSome()) {
    return firstTry.extract();
  }
#ifdef MOZ_UNIFFI_FIXTURES
  Maybe<already_AddRefed<Promise>> secondTry =
      uniffi::UniFFIFixturesCallAsync(aGlobal, aId, aArgs, aError);
  if (secondTry.isSome()) {
    return secondTry.extract();
  }
#endif

  aError.ThrowUnknownError(
      nsPrintfCString("Unknown function id: %" PRIu64, aId));
  return nullptr;
}

void UniFFIScaffolding::CallSync(
    const GlobalObject& aGlobal, uint64_t aId,
    const Sequence<ScaffoldingType>& aArgs,
    RootedDictionary<UniFFIScaffoldingCallResult>& aReturnValue,
    ErrorResult& aError) {
  if (uniffi::UniFFICallSync(aGlobal, aId, aArgs, aReturnValue, aError)) {
    return;
  }
#ifdef MOZ_UNIFFI_FIXTURES
  if (uniffi::UniFFIFixturesCallSync(aGlobal, aId, aArgs, aReturnValue,
                                     aError)) {
    return;
  }
#endif

  aError.ThrowUnknownError(
      nsPrintfCString("Unknown function id: %" PRIu64, aId));
}

already_AddRefed<UniFFIPointer> UniFFIScaffolding::ReadPointer(
    const GlobalObject& aGlobal, uint64_t aId, const ArrayBuffer& aArrayBuff,
    long aPosition, ErrorResult& aError) {
  Maybe<already_AddRefed<UniFFIPointer>> firstTry =
      uniffi::UniFFIReadPointer(aGlobal, aId, aArrayBuff, aPosition, aError);
  if (firstTry.isSome()) {
    return firstTry.extract();
  }
#ifdef MOZ_UNIFFI_FIXTURES
  Maybe<already_AddRefed<UniFFIPointer>> secondTry =
      uniffi::UniFFIFixturesReadPointer(aGlobal, aId, aArrayBuff, aPosition,
                                        aError);
  if (secondTry.isSome()) {
    return secondTry.extract();
  }
#endif

  aError.ThrowUnknownError(nsPrintfCString("Unknown object id: %" PRIu64, aId));
  return nullptr;
}

void UniFFIScaffolding::WritePointer(const GlobalObject& aGlobal, uint64_t aId,
                                     const UniFFIPointer& aPtr,
                                     const ArrayBuffer& aArrayBuff,
                                     long aPosition, ErrorResult& aError) {
  if (uniffi::UniFFIWritePointer(aGlobal, aId, aPtr, aArrayBuff, aPosition,
                                 aError)) {
    return;
  }
#ifdef MOZ_UNIFFI_FIXTURES
  if (uniffi::UniFFIFixturesWritePointer(aGlobal, aId, aPtr, aArrayBuff,
                                         aPosition, aError)) {
    return;
  }
#endif

  aError.ThrowUnknownError(nsPrintfCString("Unknown object id: %" PRIu64, aId));
}

}  // namespace mozilla::dom
