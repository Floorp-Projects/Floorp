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
  NS_DECL_NSIFILEPICKER

protected:
  /* method from nsBaseFilePicker */
  NS_IMETHOD CreateNative(nsIWidget *aParent,
                          const PRUnichar *aTitle,
                          PRInt16 aMode);


  NS_IMETHOD            OnOk();
  NS_IMETHOD            OnCancel();

  // actual implementations of get/put dialogs using NavServices
	PRBool PutFile ( Str255 & inTitle, Str255 & inDefaultName, FSSpec* outFileSpec ) ;
	PRBool GetFile ( Str255 & inTitle, FSSpec* outFileSpec ) ;
	PRBool GetFolder ( Str255 & inTitle, FSSpec* outFileSpec ) ;

  PRBool                 mIOwnEventLoop;
  PRBool                 mWasCancelled;
  nsString               mTitle;
  PRInt16                mMode;
  nsString               mFile;
  PRUint32               mNumberOfFilters;  
  const PRUnichar**      mTitles;
  const PRUnichar**      mFilters;
  nsString               mDefault;
  nsCOMPtr<nsILocalFile> mDisplayDirectory;
  nsCOMPtr<nsILocalFile> mFile;
  PRInt16                mSelectResult;
  
  void GetFilterListArray(nsString& aFilterList);
  
  NavTypeListPtr			mTypeLists[kMaxTypeListCount];
  PRInt16				mSelectedType;

};

#endif // nsFilePicker_h__
