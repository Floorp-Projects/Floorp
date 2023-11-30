/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsBaseFilePicker_h__
#define nsBaseFilePicker_h__

#include "nsISupports.h"
#include "nsIFilePicker.h"
#include "nsISimpleEnumerator.h"
#include "nsArrayEnumerator.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsTArray.h"

class nsPIDOMWindowOuter;
class nsIWidget;

class nsBaseFilePicker : public nsIFilePicker {
  class AsyncShowFilePicker;

 public:
  nsBaseFilePicker();
  virtual ~nsBaseFilePicker();

  NS_IMETHOD Init(mozIDOMWindowProxy* aParent, const nsAString& aTitle,
                  nsIFilePicker::Mode aMode) override;
  NS_IMETHOD IsModeSupported(nsIFilePicker::Mode aMode, JSContext* aCx,
                             mozilla::dom::Promise** aPromise) override;
  NS_IMETHOD Open(nsIFilePickerShownCallback* aCallback) override;
  NS_IMETHOD AppendFilters(int32_t filterMask) override;
  NS_IMETHOD AppendRawFilter(const nsAString& aFilter) override;
  NS_IMETHOD GetCapture(nsIFilePicker::CaptureTarget* aCapture) override;
  NS_IMETHOD SetCapture(nsIFilePicker::CaptureTarget aCapture) override;
  NS_IMETHOD GetFilterIndex(int32_t* aFilterIndex) override;
  NS_IMETHOD SetFilterIndex(int32_t aFilterIndex) override;
  NS_IMETHOD GetFiles(nsISimpleEnumerator** aFiles) override;
  NS_IMETHOD GetDisplayDirectory(nsIFile** aDisplayDirectory) override;
  NS_IMETHOD SetDisplayDirectory(nsIFile* aDisplayDirectory) override;
  NS_IMETHOD GetDisplaySpecialDirectory(nsAString& aDisplayDirectory) override;
  NS_IMETHOD SetDisplaySpecialDirectory(
      const nsAString& aDisplayDirectory) override;
  NS_IMETHOD GetAddToRecentDocs(bool* aFlag) override;
  NS_IMETHOD SetAddToRecentDocs(bool aFlag) override;
  NS_IMETHOD GetMode(nsIFilePicker::Mode* aMode) override;
  NS_IMETHOD SetOkButtonLabel(const nsAString& aLabel) override;
  NS_IMETHOD GetOkButtonLabel(nsAString& aLabel) override;

  NS_IMETHOD GetDomFileOrDirectory(nsISupports** aValue) override;
  NS_IMETHOD GetDomFileOrDirectoryEnumerator(
      nsISimpleEnumerator** aValue) override;

 protected:
  virtual void InitNative(nsIWidget* aParent, const nsAString& aTitle) = 0;
  virtual nsresult Show(nsIFilePicker::ResultCode* _retval) = 0;

  virtual nsresult ResolveSpecialDirectory(const nsAString& aSpecialDirectory);

  bool mAddToRecentDocs;
  nsCOMPtr<nsIFile> mDisplayDirectory;
  nsString mDisplaySpecialDirectory;

  nsCOMPtr<nsPIDOMWindowOuter> mParent;
  nsIFilePicker::Mode mMode;
  nsString mOkButtonLabel;
  nsTArray<nsString> mRawFilters;
};

#endif  // nsBaseFilePicker_h__
