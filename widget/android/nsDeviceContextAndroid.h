/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsDeviceContextAndroid_h__
#define nsDeviceContextAndroid_h__

#include "nsIDeviceContextSpec.h"
#include "nsCOMPtr.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/PrintPromise.h"

class nsDeviceContextSpecAndroid final : public nsIDeviceContextSpec {
 private:
  virtual ~nsDeviceContextSpecAndroid();

 public:
  using IntSize = mozilla::gfx::IntSize;

  NS_DECL_ISUPPORTS

  already_AddRefed<PrintTarget> MakePrintTarget() final;

  NS_IMETHOD Init(nsIPrintSettings* aPS, bool aIsPrintPreview) override;
  NS_IMETHOD BeginDocument(const nsAString& aTitle,
                           const nsAString& aPrintToFileName,
                           int32_t aStartPage, int32_t aEndPage) override;
  RefPtr<mozilla::gfx::PrintEndDocumentPromise> EndDocument() override;
  NS_IMETHOD BeginPage(const IntSize& aSizeInPoints) override { return NS_OK; }
  NS_IMETHOD EndPage() override { return NS_OK; }

 private:
  nsresult DoEndDocument();
  nsCOMPtr<nsIFile> mTempFile;
};
#endif  // nsDeviceContextAndroid_h__
