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
 */

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsIFileChannel.h"
#include "nsILocalFile.h"
#include <Navigation.h>

#define	kMaxTypeListCount	10
#define kMaxTypesPerFilter	9

/**
 * Native Mac FileSelector wrapper
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
  NS_IMETHOD GetDisplayDirectory(nsILocalFile * *aDisplayDirectory);
  NS_IMETHOD SetDisplayDirectory(nsILocalFile * aDisplayDirectory);
  NS_IMETHOD GetFile(nsILocalFile * *aFile);
  NS_IMETHOD GetFileURL(nsIFileURL * *aFileURL);
  NS_IMETHOD Show(PRInt16 *_retval); 
  NS_IMETHOD AppendFilter(const PRUnichar *aTitle,  const PRUnichar *aFilter) ;

protected:

  NS_IMETHOD InitNative(nsIWidget *aParent, const PRUnichar *aTitle, PRInt16 aMode);

  NS_IMETHOD            OnOk();
  NS_IMETHOD            OnCancel();

    // actual implementations of get/put dialogs using NavServices
  PRInt16 PutLocalFile(Str255 & inTitle, Str255 & inDefaultName, FSSpec* outFileSpec) ;
  PRInt16 GetLocalFile(Str255 & inTitle, FSSpec* outFileSpec);
  PRInt16 GetLocalFolder(Str255 & inTitle, FSSpec* outFileSpec);

  void MapFilterToFileTypes ( ) ;
  Boolean IsTypeInFilterList ( ResType inType ) ;
  Boolean IsExtensionInFilterList ( StrFileName & inFileName ) ;
  
    // filter routine for file types
  static pascal Boolean FileDialogFilterProc ( AEDesc* theItem, void* info,
                                                NavCallBackUserData callbackUD,
                                                NavFilterModes filterMode ) ;
                                                
  PRBool                 mWasCancelled;
  PRBool                 mAllFilesDisplayed;
  nsString               mTitle;
  PRInt16                mMode;
  nsCOMPtr<nsILocalFile> mFile;
  nsString               mDefault;
  nsCOMPtr<nsILocalFile> mDisplayDirectory;

  nsStringArray          mFilters; 
  nsStringArray          mTitles;
  nsCStringArray         mFlatFilters;        // all the filters from mFilters, but one per string
  
  NavTypeListPtr         mTypeLists[kMaxTypeListCount];

};

#endif // nsFilePicker_h__
