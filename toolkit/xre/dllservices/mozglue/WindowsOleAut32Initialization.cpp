/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/WindowsOleAut32Initialization.h"

#include "nsWindowsDllInterceptor.h"

#include <oleauto.h>

namespace mozilla {

static WindowsDllInterceptor sOleAut32Intercept;
static WindowsDllInterceptor::FuncHookType<decltype(&::VariantClear)>
    stub_VariantClear;

HRESULT WINAPI patched_VariantClear(VARIANTARG* aVariant) {
  bool isString = aVariant && aVariant->vt == VT_BSTR;
  HRESULT result = stub_VariantClear(aVariant);
  if (isString && SUCCEEDED(result)) {
    aVariant->bstrVal = nullptr;
  }
  return result;
}

bool WindowsOleAut32Initialization() {
  sOleAut32Intercept.Init("oleaut32.dll");
  return stub_VariantClear.Set(sOleAut32Intercept, "VariantClear",
                               &patched_VariantClear);
}

}  // namespace mozilla
