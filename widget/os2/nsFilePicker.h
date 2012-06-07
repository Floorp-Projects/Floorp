/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#include "nsISimpleEnumerator.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsICharsetConverterManager.h"
#include "nsBaseFilePicker.h"
#include "nsString.h"

#define INCL_DOS
#define INCL_NLS
#define INCL_WIN
#define INCL_WINSTDFILE
#include <os2.h>

/**
 * Native OS/2 FileSelector wrapper
 */

class nsFilePicker : public nsBaseFilePicker
{
public:
  nsFilePicker(); 
  virtual ~nsFilePicker();

  static void ReleaseGlobals();

  NS_DECL_ISUPPORTS

    // nsIFilePicker (less what's in nsBaseFilePicker)
  NS_IMETHOD GetDefaultString(nsAString& aDefaultString);
  NS_IMETHOD SetDefaultString(const nsAString& aDefaultString);
  NS_IMETHOD GetDefaultExtension(nsAString& aDefaultExtension);
  NS_IMETHOD SetDefaultExtension(const nsAString& aDefaultExtension);
  NS_IMETHOD GetFilterIndex(PRInt32 *aFilterIndex);
  NS_IMETHOD SetFilterIndex(PRInt32 aFilterIndex);
  NS_IMETHOD GetFile(nsIFile * *aFile);
  NS_IMETHOD GetFileURL(nsIURI * *aFileURL);
  NS_IMETHOD GetFiles(nsISimpleEnumerator **aFiles);
  NS_IMETHOD Show(PRInt16 *_retval); 
  NS_IMETHOD AppendFilter(const nsAString& aTitle, const nsAString& aFilter);

protected:
  /* method from nsBaseFilePicker */
  virtual void InitNative(nsIWidget *aParent, const nsAString& aTitle,
                          PRInt16 aMode);


  void GetFilterListArray(nsString& aFilterList);
  static void GetFileSystemCharset(nsCString & fileSystemCharset);
  char * ConvertToFileSystemCharset(const nsAString& inString);
  PRUnichar * ConvertFromFileSystemCharset(const char *inString);

  HWND                   mWnd;
  nsString               mTitle;
  PRInt16                mMode;
  nsCString              mFile;
  nsString               mDefault;
  nsString               mDefaultExtension;
  nsTArray<nsString>     mFilters;
  nsTArray<nsString>     mTitles;
  nsIUnicodeEncoder*     mUnicodeEncoder;
  nsIUnicodeDecoder*     mUnicodeDecoder;
  PRInt16                mSelectedType;
  nsCOMArray<nsIFile> mFiles;
  static char            mLastUsedDirectory[];
};

#endif // nsFilePicker_h__
