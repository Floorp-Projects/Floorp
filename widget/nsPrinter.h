/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinter_h__
#define nsPrinter_h__

#include "nsIPaper.h"
#include "nsIPrinter.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

class nsPrinter final : public nsIPrinter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTER
  nsPrinter() = delete;
  nsPrinter(const nsAString& aName,
            const nsTArray<RefPtr<nsIPaper>>& aPaperList,
            const bool aSupportsDuplex = false,
            const bool aSupportsColor = false);

 private:
  ~nsPrinter() = default;

  nsString mName;
  nsTArray<RefPtr<nsIPaper>> mPaperList;
  bool mSupportsDuplex = false;
  bool mSupportsColor = false;
};

#endif /* nsPrinter_h__ */
