/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
 */

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#include "nsILocalFile.h"
#include "nsISimpleEnumerator.h"
#include "nsISupportsArray.h"

#include "nsICharsetConverterManager.h"
#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsdefs.h"
#include <windows.h>
/**
 * Native Windows FileSelector wrapper
 */

class nsFilePicker : public nsBaseFilePicker
{
public:
  nsFilePicker(); 
  virtual ~nsFilePicker();

  NS_DECL_ISUPPORTS

    // nsIFilePicker (less what's in nsBaseFilePicker)
  NS_IMETHOD GetDefaultString(PRUnichar * *aDefaultString);
  NS_IMETHOD SetDefaultString(const PRUnichar * aDefaultString);
  NS_IMETHOD GetDefaultExtension(PRUnichar * *aDefaultExtension);
  NS_IMETHOD SetDefaultExtension(const PRUnichar * aDefaultExtension);
  NS_IMETHOD GetDisplayDirectory(nsILocalFile * *aDisplayDirectory);
  NS_IMETHOD SetDisplayDirectory(nsILocalFile * aDisplayDirectory);
  NS_IMETHOD GetFilterIndex(PRInt32 *aFilterIndex);
  NS_IMETHOD SetFilterIndex(PRInt32 aFilterIndex);
  NS_IMETHOD GetFile(nsILocalFile * *aFile);
  NS_IMETHOD GetFileURL(nsIFileURL * *aFileURL);
  NS_IMETHOD GetFiles(nsISimpleEnumerator **aFiles);
  NS_IMETHOD Show(PRInt16 *aReturnVal); 
  NS_IMETHOD ShowW(PRInt16 *aReturnVal); 
  NS_IMETHOD AppendFilter(const PRUnichar *aTitle,  const PRUnichar *aFilter) ;

protected:
  /* method from nsBaseFilePicker */
  NS_IMETHOD InitNative(nsIWidget *aParent,
                        const PRUnichar *aTitle,
                        PRInt16 aMode);


  void GetFilterListArray(nsString& aFilterList);
  static void GetFileSystemCharset(nsCString & fileSystemCharset);
  char * ConvertToFileSystemCharset(const PRUnichar *inString);
  PRUnichar * ConvertFromFileSystemCharset(const char *inString);

  nsIWidget              *mParentWidget;
  nsString               mTitle;
  PRInt16                mMode;
  nsCString              mFile;
  nsString               mDefault;
  nsString               mDefaultExtension;
  nsString               mFilterList;
  nsIUnicodeEncoder*     mUnicodeEncoder;
  nsIUnicodeDecoder*     mUnicodeDecoder;
  nsCOMPtr<nsILocalFile> mDisplayDirectory;
  PRInt16                mSelectedType;
  nsCOMPtr <nsISupportsArray> mFiles;
  static char            mLastUsedDirectory[];
  nsString               mUnicodeFile;
  static nsString        mLastUsedUnicodeDirectory;
};

#endif // nsFilePicker_h__
