/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrintSettingsServiceX_h
#define nsPrintSettingsServiceX_h

#include "nsPrintSettingsService.h"

namespace mozilla {
namespace embedding {
class PrintData;
}  // namespace embedding
}  // namespace mozilla

class nsPrintSettingsServiceX final : public nsPrintSettingsService {
 public:
  nsPrintSettingsServiceX() {}

  NS_IMETHODIMP SerializeToPrintData(
      nsIPrintSettings* aSettings,
      mozilla::embedding::PrintData* data) override;

  NS_IMETHODIMP DeserializeToPrintSettings(
      const mozilla::embedding::PrintData& data,
      nsIPrintSettings* settings) override;

 protected:
  nsresult _CreatePrintSettings(nsIPrintSettings** _retval) override;
};

#endif  // nsPrintSettingsServiceX_h
