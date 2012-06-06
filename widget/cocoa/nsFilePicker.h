/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFilePicker_h_
#define nsFilePicker_h_

#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsIFileChannel.h"
#include "nsIFile.h"
#include "nsCOMArray.h"
#include "nsTArray.h"

class nsILocalFileMac;
@class NSArray;

class nsFilePicker : public nsBaseFilePicker
{
public:
  nsFilePicker(); 
  virtual ~nsFilePicker();

  NS_DECL_ISUPPORTS

  // nsIFilePicker (less what's in nsBaseFilePicker)
  NS_IMETHOD GetDefaultString(nsAString& aDefaultString);
  NS_IMETHOD SetDefaultString(const nsAString& aDefaultString);
  NS_IMETHOD GetDefaultExtension(nsAString& aDefaultExtension);
  NS_IMETHOD GetFilterIndex(PRInt32 *aFilterIndex);
  NS_IMETHOD SetFilterIndex(PRInt32 aFilterIndex);
  NS_IMETHOD SetDefaultExtension(const nsAString& aDefaultExtension);
  NS_IMETHOD GetFile(nsIFile * *aFile);
  NS_IMETHOD GetFileURL(nsIURI * *aFileURL);
  NS_IMETHOD GetFiles(nsISimpleEnumerator **aFiles);
  NS_IMETHOD Show(PRInt16 *_retval); 
  NS_IMETHOD AppendFilter(const nsAString& aTitle, const nsAString& aFilter);

  /**
   * Returns the current filter list in the format used by Cocoa's NSSavePanel
   * and NSOpenPanel.
   * Returns nil if no filter currently apply.
   */
  NSArray* GetFilterList();

protected:

  virtual void InitNative(nsIWidget *aParent, const nsAString& aTitle, PRInt16 aMode);

  // actual implementations of get/put dialogs using NSOpenPanel & NSSavePanel
  // aFile is an existing but unspecified file. These functions must specify it.
  //
  // will return |returnCancel| or |returnOK| as result.
  PRInt16 GetLocalFiles(const nsString& inTitle, bool inAllowMultiple, nsCOMArray<nsIFile>& outFiles);
  PRInt16 GetLocalFolder(const nsString& inTitle, nsIFile** outFile);
  PRInt16 PutLocalFile(const nsString& inTitle, const nsString& inDefaultName, nsIFile** outFile);

  void     SetDialogTitle(const nsString& inTitle, id aDialog);
  NSString *PanelDefaultDirectory();
  NSView* GetAccessoryView();
                                                
  nsString               mTitle;
  PRInt16                mMode;
  nsCOMArray<nsIFile>    mFiles;
  nsString               mDefault;

  nsTArray<nsString>     mFilters; 
  nsTArray<nsString>     mTitles;

  PRInt32                mSelectedTypeIndex;
};

#endif // nsFilePicker_h_
