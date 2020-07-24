/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterWin_h_
#define nsPrinterWin_h_

#include "nsIPrinter.h"

#include "mozilla/Maybe.h"
#include "nsIPaper.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

class nsPrinterWin final : public nsIPrinter {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRINTER
  nsPrinterWin() = delete;
  explicit nsPrinterWin(const nsAString& aName);

 private:
  ~nsPrinterWin() = default;

  nsresult EnsurePaperList();

  nsString mName;
  nsTArray<RefPtr<nsIPaper>> mPaperList;
  Maybe<bool> mSupportsDuplex;
};

#endif  // nsPrinterWin_h_
