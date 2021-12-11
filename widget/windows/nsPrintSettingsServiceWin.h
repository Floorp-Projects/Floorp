/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsServiceWin_h
#define nsPrintSettingsServiceWin_h

#include "mozilla/embedding/PPrinting.h"
#include "nsPrintSettingsService.h"

class nsIPrintSettings;

class nsPrintSettingsServiceWin final : public nsPrintSettingsService {
 public:
  nsPrintSettingsServiceWin() {}

  NS_IMETHODIMP SerializeToPrintData(
      nsIPrintSettings* aSettings,
      mozilla::embedding::PrintData* data) override;

  NS_IMETHODIMP DeserializeToPrintSettings(
      const mozilla::embedding::PrintData& data,
      nsIPrintSettings* settings) override;

  nsresult _CreatePrintSettings(nsIPrintSettings** _retval) override;
};

#endif  // nsPrintSettingsServiceWin_h
