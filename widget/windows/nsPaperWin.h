/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPaperWin_h_
#define nsPaperWin_h_

#include "nsIPaper.h"

#include <wtypes.h>

#include "mozilla/gfx/Rect.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

class nsPaperWin final : public nsIPaper {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAPER
  nsPaperWin() = delete;
  nsPaperWin(WORD aId, const nsAString& aName, const nsAString& aPrinterName,
             double aWidth, double aHeight);

 private:
  ~nsPaperWin() = default;

  void EnsureUnwriteableMargin();

  using SizeDouble = mozilla::gfx::SizeDouble;
  using MarginDouble = mozilla::gfx::MarginDouble;

  WORD mId;
  nsString mName;
  nsString mPrinterName;
  SizeDouble mSize;
  MarginDouble mUnwriteableMargin;
  bool mUnwriteableMarginRetrieved = false;
};

#endif  // nsPaperWin_h_
