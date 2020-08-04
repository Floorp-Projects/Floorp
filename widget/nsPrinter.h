/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPrinter_h__
#define nsPrinter_h__

#include "nsPrinterBase.h"
#include "nsString.h"

class nsPrinter final : public nsPrinterBase {
 public:
  NS_IMETHOD GetName(nsAString& aName) override;
  NS_IMETHOD GetPaperList(nsTArray<RefPtr<nsIPaper>>& aPaperList) override;
  bool SupportsDuplex() const final { return false; }
  bool SupportsColor() const final { return false; }

  nsPrinter() = delete;

  static already_AddRefed<nsPrinter> Create(const nsAString& aName);

 private:
  explicit nsPrinter(const nsAString& aName);
  ~nsPrinter();

  const nsString mName;
};

#endif /* nsPrinter_h__ */
