/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Steve Dagley <sdagley@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsFilePicker_h__
#define nsFilePicker_h__

#include <Navigation.h>

#include "nsBaseFilePicker.h"
#include "nsString.h"
#include "nsIFileChannel.h"
#include "nsILocalFile.h"
#include "nsCOMArray.h"

class nsILocalFileMac;

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
  NS_IMETHOD GetDefaultString(nsAString& aDefaultString);
  NS_IMETHOD SetDefaultString(const nsAString& aDefaultString);
  NS_IMETHOD GetDefaultExtension(nsAString& aDefaultExtension);
  NS_IMETHOD SetDefaultExtension(const nsAString& aDefaultExtension);
  NS_IMETHOD GetDisplayDirectory(nsILocalFile * *aDisplayDirectory);
  NS_IMETHOD SetDisplayDirectory(nsILocalFile * aDisplayDirectory);
  NS_IMETHOD GetFilterIndex(PRInt32 *aFilterIndex);
  NS_IMETHOD SetFilterIndex(PRInt32 aFilterIndex);
  NS_IMETHOD GetFile(nsILocalFile * *aFile);
  NS_IMETHOD GetFileURL(nsIFileURL * *aFileURL);
  NS_IMETHOD GetFiles(nsISimpleEnumerator **aFiles);
  NS_IMETHOD Show(PRInt16 *_retval); 
  NS_IMETHOD AppendFilter(const nsAString& aTitle, const nsAString& aFilter);

protected:

  virtual void InitNative(nsIWidget *aParent, const nsAString& aTitle,
                          PRInt16 aMode);

    // actual implementations of get/put dialogs using NavServices
  PRInt16 GetLocalFiles(const nsString& inTitle, PRBool inAllowMultiple, nsCOMArray<nsILocalFile>& outFiles);
  PRInt16 GetLocalFolder(const nsString& inTitle, nsILocalFile** outFile);
  PRInt16 PutLocalFile(const nsString& inTitle, const nsString& inDefaultName, nsILocalFile** outFile);
  
  void MapFilterToFileTypes ( ) ;
#if TARGET_CARBON
  void SetupFormatMenuItems (NavDialogCreationOptions* dialogCreateOptions) ;
#else
  void SetupFormatMenuItems (NavDialogOptions* dialogOptions) ;
#endif
  Boolean IsTypeInFilterList ( ResType inType ) ;
  Boolean IsExtensionInFilterList ( StrFileName & inFileName ) ;
  void HandleShowPopupMenuSelect( NavCBRecPtr callBackParms ) ;
  
    // filter routine for file dialog events
  static pascal void FileDialogEventHandlerProc( NavEventCallbackMessage msg,
                                                 NavCBRecPtr cbRec,
                                                 NavCallBackUserData callbackUD ) ;
  
    // filter routine for file types
  static pascal Boolean FileDialogFilterProc ( AEDesc* theItem, void* info,
                                                NavCallBackUserData callbackUD,
                                                NavFilterModes filterMode ) ;
                                                
  PRPackedBool           mAllFilesDisplayed;
  PRPackedBool           mApplicationsDisplayed;
  nsString               mTitle;
  PRInt16                mMode;
  nsCOMArray<nsILocalFile> mFiles;
  nsString               mDefault;
  nsCOMPtr<nsILocalFile> mDisplayDirectory;

  nsStringArray          mFilters; 
  nsStringArray          mTitles;
  nsCStringArray         mFlatFilters;        // all the filters from mFilters, but one per string
  
  NavTypeListPtr         mTypeLists[kMaxTypeListCount];

  static OSType          sCurrentProcessSignature;
  
  PRInt32                mSelectedType;
  PRInt32                mTypeOffset;       // Work around Nav Services 1.0 bug
};

#endif // nsFilePicker_h__
