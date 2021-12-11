/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Variant.h"

#include "nsCOMPtr.h"

extern "C" {

using namespace mozilla::storage;

/**
 * Return the data type of the given variant.  This method used to be exposed
 * to XPCOM, but since bug 1507540 it's marked [notxpcom] in the interface
 * definition, so we need this C function to access it from Rust.
 */
uint16_t NS_GetDataType(nsIVariant* aVariant) {
  return aVariant->GetDataType();
}

// Convenience functions to create Storage variants from Rust.
void NS_NewStorageNullVariant(nsIVariant** aVariant) {
  nsCOMPtr<nsIVariant> variant = new NullVariant();
  variant.forget(aVariant);
}

void NS_NewStorageBooleanVariant(bool aValue, nsIVariant** aVariant) {
  nsCOMPtr<nsIVariant> variant = new BooleanVariant(aValue);
  variant.forget(aVariant);
}

void NS_NewStorageIntegerVariant(int64_t aValue, nsIVariant** aVariant) {
  nsCOMPtr<nsIVariant> variant = new IntegerVariant(aValue);
  variant.forget(aVariant);
}

void NS_NewStorageFloatVariant(double aValue, nsIVariant** aVariant) {
  nsCOMPtr<nsIVariant> variant = new FloatVariant(aValue);
  variant.forget(aVariant);
}

void NS_NewStorageTextVariant(const nsAString& aValue, nsIVariant** aVariant) {
  nsCOMPtr<nsIVariant> variant = new TextVariant(aValue);
  variant.forget(aVariant);
}

void NS_NewStorageUTF8TextVariant(const nsACString& aValue,
                                  nsIVariant** aVariant) {
  nsCOMPtr<nsIVariant> variant = new UTF8TextVariant(aValue);
  variant.forget(aVariant);
}

}  // extern "C"
