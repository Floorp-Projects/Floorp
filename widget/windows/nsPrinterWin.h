/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinterWin_h_
#define nsPrinterWin_h_

#include "nsPrinterBase.h"

class nsPrinterWin final : public nsPrinterBase {
 public:
  NS_IMETHOD GetName(nsAString& aName) override;
  PrintSettingsInitializer DefaultSettings() const final;
  bool SupportsDuplex() const final;
  bool SupportsColor() const final;
  bool SupportsCollation() const final;
  nsTArray<mozilla::PaperInfo> PaperList() const final;
  MarginDouble GetMarginsForPaper(uint64_t aId) const final;

  nsPrinterWin() = delete;
  static already_AddRefed<nsPrinterWin> Create(const nsAString& aName);

 private:
  explicit nsPrinterWin(const nsAString& aName);
  ~nsPrinterWin() = default;

  nsresult EnsurePaperList();

  const nsString mName;
};

#endif  // nsPrinterWin_h_
