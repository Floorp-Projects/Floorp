/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterListCUPS_h__
#define nsPrinterListCUPS_h__

#include "nsPrinterListBase.h"
#include "nsStringFwd.h"

struct cups_dest_s;

class nsPrinterListCUPS final : public nsPrinterListBase {
  NS_DECL_NSIPRINTERLIST

#ifdef XP_MACOSX
  // This is implemented in nsDeviceContextSpecX. We could add a new class to
  // the class hierarchy instead and make this virtual, but it seems overkill
  // just for this.
  static void GetDisplayNameForPrinter(const cups_dest_s&, nsAString& aName);
#else
  static void GetDisplayNameForPrinter(const cups_dest_s&, nsAString& aName) {}
#endif

 private:
  ~nsPrinterListCUPS() override = default;
};

#endif
