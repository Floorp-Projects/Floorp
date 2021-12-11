/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterWin_h_
#define nsPrinterWin_h_

#include "nsPrinterBase.h"
#include "mozilla/DataMutex.h"
#include "nsTArrayForwardDeclare.h"

class nsPrinterWin final : public nsPrinterBase {
 public:
  NS_IMETHOD GetName(nsAString& aName) override;
  NS_IMETHOD GetSystemName(nsAString& aName) override;
  bool SupportsDuplex() const final;
  bool SupportsColor() const final;
  bool SupportsMonochrome() const final;
  bool SupportsCollation() const final;
  PrinterInfo CreatePrinterInfo() const final;
  MarginDouble GetMarginsForPaper(nsString aPaperId) const final;

  nsPrinterWin() = delete;
  static already_AddRefed<nsPrinterWin> Create(
      const mozilla::CommonPaperInfoArray* aPaperInfoArray,
      const nsAString& aName);

 private:
  nsPrinterWin(const mozilla::CommonPaperInfoArray* aPaperInfoArray,
               const nsAString& aName);
  ~nsPrinterWin() = default;

  nsTArray<uint8_t> CopyDefaultDevmodeW() const;
  nsTArray<mozilla::PaperInfo> PaperList() const;
  PrintSettingsInitializer DefaultSettings() const;

  const nsString mName;
  mutable mozilla::DataMutex<nsTArray<uint8_t>> mDefaultDevmodeWStorage;
  // Even though some documentation seems to suggest that you should be able to
  // use printer drivers on separate threads if you have separate handles, we
  // see threading issues with multiple drivers. This Mutex is used to lock
  // around all calls to DeviceCapabilitiesW, DocumentPropertiesW and
  // CreateICW/DCW, to hopefully prevent these issues.
  mutable mozilla::Mutex mDriverMutex{"nsPrinterWin::Driver"};
};

#endif  // nsPrinterWin_h_
