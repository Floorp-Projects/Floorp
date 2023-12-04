/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFilePicker_h_
#define nsFilePicker_h_

#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsIFile.h"
#include "nsCOMArray.h"
#include "nsTArray.h"

class nsILocalFileMac;
@class NSArray;

class nsFilePicker : public nsBaseFilePicker {
 public:
  nsFilePicker();
  using nsIFilePicker::ResultCode;

  NS_DECL_ISUPPORTS

  // nsIFilePicker (less what's in nsBaseFilePicker)
  NS_IMETHOD GetDefaultString(nsAString& aDefaultString) override;
  NS_IMETHOD SetDefaultString(const nsAString& aDefaultString) override;
  NS_IMETHOD GetDefaultExtension(nsAString& aDefaultExtension) override;
  NS_IMETHOD GetFilterIndex(int32_t* aFilterIndex) override;
  NS_IMETHOD SetFilterIndex(int32_t aFilterIndex) override;
  NS_IMETHOD SetDefaultExtension(const nsAString& aDefaultExtension) override;
  NS_IMETHOD GetFile(nsIFile** aFile) override;
  NS_IMETHOD GetFileURL(nsIURI** aFileURL) override;
  NS_IMETHOD GetFiles(nsISimpleEnumerator** aFiles) override;
  NS_IMETHOD AppendFilter(const nsAString& aTitle,
                          const nsAString& aFilter) override;

  /**
   * Returns the current filter list in the format used by Cocoa's NSSavePanel
   * and NSOpenPanel.
   * Returns nil if no filter currently apply.
   */
  NSArray* GetFilterList();

 protected:
  virtual ~nsFilePicker();

  virtual void InitNative(nsIWidget* aParent, const nsAString& aTitle) override;
  nsresult Show(ResultCode* _retval) override;

  // actual implementations of get/put dialogs using NSOpenPanel & NSSavePanel
  // aFile is an existing but unspecified file. These functions must specify it.
  //
  // will return |returnCancel| or |returnOK| as result.
  ResultCode GetLocalFiles(bool inAllowMultiple, nsCOMArray<nsIFile>& outFiles);
  ResultCode GetLocalFolder(nsIFile** outFile);
  ResultCode PutLocalFile(nsIFile** outFile);

  void SetDialogTitle(const nsString& inTitle, id aDialog);
  NSString* PanelDefaultDirectory();
  NSView* GetAccessoryView();

  nsString mTitle;
  nsCOMArray<nsIFile> mFiles;
  nsString mDefaultFilename;

  nsTArray<nsString> mFilters;
  nsTArray<nsString> mTitles;

  int32_t mSelectedTypeIndex;
};

#endif  // nsFilePicker_h_
