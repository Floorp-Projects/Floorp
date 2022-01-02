/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsPaper.h"
#include "nsPaperMargin.h"
#include "nsPrinterBase.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/dom/Promise.h"

using mozilla::ErrorResult;
using mozilla::PaperInfo;

NS_IMPL_CYCLE_COLLECTION(nsPaper, mMarginPromise, mPrinter)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsPaper)
  NS_INTERFACE_MAP_ENTRY(nsIPaper)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPaper)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsPaper)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsPaper)

nsPaper::nsPaper(const mozilla::PaperInfo& aInfo)
    : mPrinter(nullptr), mInfo(aInfo) {}

nsPaper::nsPaper(nsPrinterBase& aPrinter, const mozilla::PaperInfo& aInfo)
    : mPrinter(&aPrinter), mInfo(aInfo) {}

nsPaper::~nsPaper() = default;

NS_IMETHODIMP
nsPaper::GetId(nsAString& aId) {
  aId = mInfo.mId;
  return NS_OK;
}

NS_IMETHODIMP
nsPaper::GetName(nsAString& aName) {
  aName = mInfo.mName.IsEmpty() ? mInfo.mId : mInfo.mName;
  return NS_OK;
}

NS_IMETHODIMP
nsPaper::GetWidth(double* aWidth) {
  NS_ENSURE_ARG_POINTER(aWidth);
  *aWidth = mInfo.mSize.Width();
  return NS_OK;
}

NS_IMETHODIMP
nsPaper::GetHeight(double* aHeight) {
  NS_ENSURE_ARG_POINTER(aHeight);
  *aHeight = mInfo.mSize.Height();
  return NS_OK;
}

NS_IMETHODIMP
nsPaper::GetUnwriteableMargin(JSContext* aCx, Promise** aPromise) {
  if (RefPtr<Promise> existing = mMarginPromise) {
    existing.forget(aPromise);
    return NS_OK;
  }
  ErrorResult rv;
  RefPtr<Promise> promise = Promise::Create(xpc::CurrentNativeGlobal(aCx), rv);
  if (MOZ_UNLIKELY(rv.Failed())) {
    return rv.StealNSResult();
  }

  mMarginPromise = promise;

  if (mInfo.mUnwriteableMargin) {
    auto margin = mozilla::MakeRefPtr<nsPaperMargin>(*mInfo.mUnwriteableMargin);
    mMarginPromise->MaybeResolve(margin);
  } else {
    if (mPrinter) {
      mPrinter->QueryMarginsForPaper(*mMarginPromise, mInfo.mId);
    } else {
      MOZ_ASSERT_UNREACHABLE("common paper sizes should know their margins");
      mMarginPromise->MaybeRejectWithNotSupportedError("Margins unavailable");
    }
  }

  promise.forget(aPromise);
  return NS_OK;
}
