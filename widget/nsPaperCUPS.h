/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPaperCUPS_h__
#define nsPaperCUPS_h__

#include "cups/cups.h"
#include "nsIPaper.h"
#include "nsISupportsImpl.h"
#include "nsString.h"

class nsPaperCUPS final : public nsIPaper {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPAPER
  nsPaperCUPS() = delete;
  nsPaperCUPS(const cups_size_t& aInfo, const char* aName)
      : mInfo(aInfo), mName(NS_ConvertUTF8toUTF16(aName)) {}

 private:
  ~nsPaperCUPS() = default;

  cups_size_t mInfo;
  nsString mName;
};

#endif /* nsPaperCUPS_h__ */
