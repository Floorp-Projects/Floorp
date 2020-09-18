/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPaper_h__
#define nsPaper_h__

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/Maybe.h"
#include "nsIPaper.h"
#include "nsISupportsImpl.h"
#include "js/RootingAPI.h"
#include "nsString.h"

struct JSContext;

namespace mozilla {

// Simple struct that can be used off the main thread to hold all the info from
// an nsPaper instance.
struct PaperInfo {
  using MarginDouble = mozilla::gfx::MarginDouble;
  using SizeDouble = mozilla::gfx::SizeDouble;

  PaperInfo() = default;
  PaperInfo(const nsAString& aId, const nsAString& aName,
            const SizeDouble& aSize,
            const Maybe<MarginDouble>& aUnwriteableMargin)
      : mId(aId),
        mName(aName),
        mSize(aSize),
        mUnwriteableMargin(aUnwriteableMargin) {}

  const nsString mId;
  const nsString mName;

  SizeDouble mSize;

  // The margins may not be known by some back-ends.
  const Maybe<MarginDouble> mUnwriteableMargin{Nothing()};
};

}  // namespace mozilla

class nsPrinterBase;

class nsPaper final : public nsIPaper {
  using Promise = mozilla::dom::Promise;

 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS(nsPaper)
  NS_DECL_NSIPAPER

  nsPaper() = delete;
  explicit nsPaper(const mozilla::PaperInfo&);
  nsPaper(nsPrinterBase&, const mozilla::PaperInfo&);

 private:
  ~nsPaper();

  // null if not associated with a printer (for "Save-to-PDF" paper sizes)
  RefPtr<nsPrinterBase> mPrinter;

  RefPtr<Promise> mMarginPromise;
  const mozilla::PaperInfo mInfo;
};

#endif /* nsPaper_h__ */
