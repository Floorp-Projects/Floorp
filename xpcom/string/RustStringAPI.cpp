/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "nsString.h"

// Extern "C" utilities used by the rust nsString bindings.

// Provide rust bindings to the nsA[C]String types
extern "C" {

// This is a no-op on release, so we ifdef it out such that using it in release
// results in a linker error.
#ifdef DEBUG
void Gecko_IncrementStringAdoptCount(void* aData) {
  MOZ_LOG_CTOR(aData, "StringAdopt", 1);
}
#elif defined(MOZ_DEBUG_RUST)
void Gecko_IncrementStringAdoptCount(void* aData) {}
#endif

void Gecko_FinalizeCString(nsACString* aThis) { aThis->~nsACString(); }

void Gecko_AssignCString(nsACString* aThis, const nsACString* aOther) {
  aThis->Assign(*aOther);
}

void Gecko_TakeFromCString(nsACString* aThis, nsACString* aOther) {
  aThis->Assign(std::move(*aOther));
}

void Gecko_AppendCString(nsACString* aThis, const nsACString* aOther) {
  aThis->Append(*aOther);
}

void Gecko_SetLengthCString(nsACString* aThis, uint32_t aLength) {
  aThis->SetLength(aLength);
}

bool Gecko_FallibleAssignCString(nsACString* aThis, const nsACString* aOther) {
  return aThis->Assign(*aOther, mozilla::fallible);
}

bool Gecko_FallibleTakeFromCString(nsACString* aThis, nsACString* aOther) {
  return aThis->Assign(std::move(*aOther), mozilla::fallible);
}

bool Gecko_FallibleAppendCString(nsACString* aThis, const nsACString* aOther) {
  return aThis->Append(*aOther, mozilla::fallible);
}

bool Gecko_FallibleSetLengthCString(nsACString* aThis, uint32_t aLength) {
  return aThis->SetLength(aLength, mozilla::fallible);
}

char* Gecko_BeginWritingCString(nsACString* aThis) {
  return aThis->BeginWriting();
}

char* Gecko_FallibleBeginWritingCString(nsACString* aThis) {
  return aThis->BeginWriting(mozilla::fallible);
}

uint32_t Gecko_StartBulkWriteCString(nsACString* aThis, uint32_t aCapacity,
                                     uint32_t aUnitsToPreserve,
                                     bool aAllowShrinking) {
  return aThis->StartBulkWriteImpl(aCapacity, aUnitsToPreserve, aAllowShrinking)
      .unwrapOr(UINT32_MAX);
}

void Gecko_FinalizeString(nsAString* aThis) { aThis->~nsAString(); }

void Gecko_AssignString(nsAString* aThis, const nsAString* aOther) {
  aThis->Assign(*aOther);
}

void Gecko_TakeFromString(nsAString* aThis, nsAString* aOther) {
  aThis->Assign(std::move(*aOther));
}

void Gecko_AppendString(nsAString* aThis, const nsAString* aOther) {
  aThis->Append(*aOther);
}

void Gecko_SetLengthString(nsAString* aThis, uint32_t aLength) {
  aThis->SetLength(aLength);
}

bool Gecko_FallibleAssignString(nsAString* aThis, const nsAString* aOther) {
  return aThis->Assign(*aOther, mozilla::fallible);
}

bool Gecko_FallibleTakeFromString(nsAString* aThis, nsAString* aOther) {
  return aThis->Assign(std::move(*aOther), mozilla::fallible);
}

bool Gecko_FallibleAppendString(nsAString* aThis, const nsAString* aOther) {
  return aThis->Append(*aOther, mozilla::fallible);
}

bool Gecko_FallibleSetLengthString(nsAString* aThis, uint32_t aLength) {
  return aThis->SetLength(aLength, mozilla::fallible);
}

char16_t* Gecko_BeginWritingString(nsAString* aThis) {
  return aThis->BeginWriting();
}

char16_t* Gecko_FallibleBeginWritingString(nsAString* aThis) {
  return aThis->BeginWriting(mozilla::fallible);
}

uint32_t Gecko_StartBulkWriteString(nsAString* aThis, uint32_t aCapacity,
                                    uint32_t aUnitsToPreserve,
                                    bool aAllowShrinking) {
  return aThis->StartBulkWriteImpl(aCapacity, aUnitsToPreserve, aAllowShrinking)
      .unwrapOr(UINT32_MAX);
}

}  // extern "C"
