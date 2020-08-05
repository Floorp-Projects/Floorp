/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPaper_h__
#define nsPaper_h__

#include "mozilla/dom/ToJSValue.h"
#include "js/TypeDecls.h"
#include "nsIPaper.h"
#include "nsISupportsImpl.h"
#include "js/RootingAPI.h"
#include "nsString.h"

namespace mozilla {

// Simple struct that can be used off the main thread to hold all the info from
// an nsPaper instance.
struct PaperInfo {
  const nsString mName;
  const double mWidth;
  const double mHeight;
  const double mUnwriteableMarginTop;
  const double mUnwriteableMarginRight;
  const double mUnwriteableMarginBottom;
  const double mUnwriteableMarginLeft;
};

}  // namespace mozilla

class nsPaper final : public nsIPaper {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAPER
  nsPaper() = delete;

  explicit nsPaper(const mozilla::PaperInfo& aInfo) : mInfo(aInfo) {}

 private:
  ~nsPaper() = default;
  const mozilla::PaperInfo mInfo;
};

namespace mozilla {

// Turns this into a JSValue by wrapping it in an nsPaper struct.
//
// FIXME: If using WebIDL dictionaries we'd get this for ~free...
MOZ_MUST_USE inline bool ToJSValue(JSContext* aCx, const PaperInfo& aInfo,
                                   JS::MutableHandleValue aValue) {
  RefPtr<nsPaper> paper = new nsPaper(aInfo);
  return dom::ToJSValue(aCx, paper, aValue);
}

}  // namespace mozilla

#endif /* nsPaper_h__ */
