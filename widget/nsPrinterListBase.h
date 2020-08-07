/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterListBase_h__
#define nsPrinterListBase_h__

#include "nsIPrinterList.h"
#include "nsISupportsImpl.h"

class nsPrinterListBase : public nsIPrinterList {
 public:
  NS_DECL_ISUPPORTS

  // No copy or move, we're an identity.
  nsPrinterListBase(const nsPrinterListBase&) = delete;
  nsPrinterListBase(nsPrinterListBase&&) = delete;

 protected:
  nsPrinterListBase();
  virtual ~nsPrinterListBase();
};

#endif
