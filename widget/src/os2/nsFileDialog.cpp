/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsWidgetDefs.h"
#include "nsIToolkit.h"
#include "nsFileDialog.h"
#include "nsDirPicker.h"

#include <stdlib.h>

// File dialog.
//
// ToDo: types array

nsFileDialog::nsFileDialog() : mWndOwner( 0), mCFilters(0)
{
   NS_INIT_REFCNT();
   memset( &mFileDlg, 0, sizeof mFileDlg);
   mFileDlg.cbSize = sizeof mFileDlg;
   mFileDlg.fl = FDS_CENTER | FDS_ENABLEFILELB;
   strcpy( mFileDlg.szFullFile, "*.*");
}

nsFileDialog::~nsFileDialog()
{
   if( mFileDlg.pszTitle)
      free( mFileDlg.pszTitle);
}

NS_IMPL_ISUPPORTS(nsFileDialog,NS_GET_IID(nsIFileWidget))

nsresult nsFileDialog::Create( nsIWidget        *aParent,
                               const nsString    &aTitle,
                               nsFileDlgMode     aMode,
                               nsIDeviceContext *aContext,
                               nsIAppShell      *aAppShell,
                               nsIToolkit       *aToolkit,
                               void             *aInitData)
{
   // set owner
   if( aParent)
      mWndOwner = (HWND) aParent->GetNativeData( NS_NATIVE_WIDGET);
   else
      mWndOwner = HWND_DESKTOP;

   if( mFileDlg.pszTitle)
   {
      free( mFileDlg.pszTitle);
      mFileDlg.pszTitle = 0;
   }
   if( aTitle.Length() > 0)
      mFileDlg.pszTitle = strdup( gModuleData.ConvertFromUcs( aTitle));

   if( aMode == eMode_load)
      mFileDlg.fl |= FDS_OPEN_DIALOG;
   else if( aMode == eMode_save)
      mFileDlg.fl |= FDS_SAVEAS_DIALOG;
   else
      NS_ASSERTION(0, "Dodgy file dialog type");

   return NS_OK;
}

// FQ Filename (I suspect this method will vanish)
nsresult nsFileDialog::SetDefaultString( const nsString &aString)
{
   gModuleData.ConvertFromUcs( aString, mFileDlg.szFullFile, CCHMAXPATH);
   return NS_OK;
}

nsresult nsFileDialog::SetDisplayDirectory( const nsFileSpec &aDirectory)
{
   // first copy the file part of whatever we have into 'buff'
   char  buff[CCHMAXPATH] = "";
   char *lastslash = strrchr( mFileDlg.szFullFile, '\\');

   strcpy( buff, lastslash ? lastslash + 1 : mFileDlg.szFullFile);

   // Now copy directory from filespec into filedlg
   strcpy( mFileDlg.szFullFile, nsNSPRPath(aDirectory));

   // Ensure there's a trailing backslash...
   if( '\\' != lastchar( mFileDlg.szFullFile))
      strcat( mFileDlg.szFullFile, "\\");

   // ...and stick the file back on.
   strcat( mFileDlg.szFullFile, buff);

#ifdef DEBUG
   printf( "SetDisplayDir, szFullFile = %s\n", mFileDlg.szFullFile);
#endif

   return NS_OK;
}

nsresult nsFileDialog::GetDisplayDirectory( nsFileSpec &aDirectory)
{
   char buff[CCHMAXPATH] = "";
   strcpy( buff, mFileDlg.szFullFile);
   printf( "mFileDlg.szFullFile = %s\n", buff);
   char *lastslash = strrchr( buff, '\\');

   if( lastslash && '\0' != *lastslash)
      *lastslash = '\0';
   else
      // no directory set
      *buff = '\0';

   aDirectory = (const char*) buff;

   return NS_OK;
}

nsresult nsFileDialog::SetFilterList( PRUint32 aNumberOfFilters,
                                      const nsString aTitles[],
                                      const nsString aFilters[])
{
   //
   // XXX really need a subclassed file dialog for this.
   //     Just using the papszITypeList doesn't work because that's meant to
   //     be .TYPE eas.
   //

   return NS_OK;
}

nsresult nsFileDialog::GetSelectedType( PRInt16 &theType)
{
   theType = 0;
   return NS_OK;
}

// return false if cancel happens.
PRBool nsFileDialog::Show()
{
   PRBool rc = PR_TRUE;

   WinFileDlg( HWND_DESKTOP, mWndOwner, &mFileDlg);
   if( mFileDlg.lReturn == DID_CANCEL)
      rc = PR_FALSE;

   return rc;
}

nsresult nsFileDialog::GetFile( nsFileSpec &aSpec)
{
   aSpec = mFileDlg.szFullFile;
   return NS_OK;
}

// Methods for folks who can't be bothered to call Create() & Show()
nsFileDlgResults nsFileDialog::GetFile( nsIWidget      *aParent,
                                        const nsString &promptString,
                                        nsFileSpec     &theFileSpec)
{
   nsFileDlgResults rc = nsFileDlgResults_Cancel;

   Create( aParent, promptString, eMode_load);

   if( Show())
   {
      rc = nsFileDlgResults_OK;
      GetFile( theFileSpec);
   }

   return rc;
}

nsFileDlgResults nsFileDialog::PutFile( nsIWidget      *aParent,
                                        const nsString  &promptString,
                                        nsFileSpec      &theFileSpec)
{
   nsFileDlgResults rc = nsFileDlgResults_Cancel;

   Create( aParent, promptString, eMode_save);

   if( Show())
   {
      rc = nsFileDlgResults_OK;
      GetFile( theFileSpec);
   }

   return rc;
}
  
nsFileDlgResults nsFileDialog::GetFolder( nsIWidget      *aParent,
                                          const nsString &promptString,
                                          nsFileSpec     &theFileSpec)
{
   nsFileDlgResults rc = nsFileDlgResults_Cancel;

   HWND hwndOwner = HWND_DESKTOP;
   if( aParent)
      hwndOwner = (HWND) aParent->GetNativeData( NS_NATIVE_WIDGET);

   DIRPICKER dp = { { 0 }, 0, TRUE, 0 }; // modal dialog

   gModuleData.ConvertFromUcs( promptString, dp.szFullFile, CCHMAXPATH);

   HWND ret = FS_PickDirectory( HWND_DESKTOP, hwndOwner,
                                gModuleData.hModResources, &dp);

   if( ret && dp.lReturn == DID_OK)
   {
      theFileSpec = dp.szFullFile;
      strcpy( mFileDlg.szFullFile, dp.szFullFile); // just in case...
      rc = nsFileDlgResults_OK;
   }

   return rc;
}
