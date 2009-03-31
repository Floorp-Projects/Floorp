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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adrian Mardare <amardare@qnx.com>
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


#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsNetUtil.h"
#include "nsWindow.h"
#include "nsIServiceManager.h"
#include "nsIPlatformCharset.h"
#include "nsFilePicker.h"
#include "nsILocalFile.h"
#include "nsIURL.h"
#include "nsIStringBundle.h"
#include "nsEnumeratorUtils.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

char nsFilePicker::mLastUsedDirectory[PATH_MAX+1] = { 0 };

#define MAX_EXTENSION_LENGTH PATH_MAX

//-------------------------------------------------------------------------
//
// nsFilePicker constructor
//
//-------------------------------------------------------------------------
nsFilePicker::nsFilePicker()
  : mParentWidget(nsnull)
  , mUnicodeEncoder(nsnull)
  , mUnicodeDecoder(nsnull)
{
  char *path = getenv("HOME");
  if (path) {
    nsCOMPtr<nsILocalFile> displayDirectory = do_CreateInstance("@mozilla.org/file/local;1");
    if (displayDirectory) {
      nsresult rv = displayDirectory->InitWithNativePath(nsDependentCString(path));
      PRBool cond;
      if (NS_SUCCEEDED(rv) &&
          NS_SUCCEEDED(displayDirectory->Exists(&cond)) &&
          cond &&
          NS_SUCCEEDED(displayDirectory->IsDirectory(&cond)) &&
          cond)
        mDisplayDirectory = displayDirectory;
    }
  }
}

//-------------------------------------------------------------------------
//
// nsFilePicker destructor
//
//-------------------------------------------------------------------------
nsFilePicker::~nsFilePicker()
{
  NS_IF_RELEASE(mUnicodeEncoder);
  NS_IF_RELEASE(mUnicodeDecoder);
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::Show(PRInt16 *aReturnVal)
{
	PRInt32 flags = 0;
	char *btn1;

	NS_ENSURE_ARG_POINTER(aReturnVal);

  if (mMode == modeGetFolder) {
		flags |= Pt_FSR_SELECT_DIRS|Pt_FSR_NO_SELECT_FILES;
		btn1 = "&Select";
  }
  else if (mMode == modeOpen) {
		btn1 = "&Open";
  }
  else if (mMode == modeSave) {
		flags |= Pt_FSR_NO_FCHECK;
		btn1 = "&Save";
  }
	else if( mMode == modeOpenMultiple ) {
		flags |= Pt_FSR_MULTIPLE;
		btn1 = "&Select";
		}
  else {
    printf("nsFilePicker::Show() wrong mode");
    return PR_FALSE;
  }

  char *title = ToNewUTF8String( mTitle );

  nsCAutoString initialDir;
  if (mDisplayDirectory)
    mDisplayDirectory->GetNativePath(initialDir);
  // If no display directory, re-use the last one.
  if(initialDir.IsEmpty()) {
    // Allocate copy of last used dir.
    initialDir = mLastUsedDirectory;
  }

  if( !mDefault.IsEmpty() ) {
    initialDir.AppendWithConversion( NS_LITERAL_STRING( "/" ) );
    initialDir.AppendWithConversion( mDefault );
  }

	nsCAutoString extensionBuffer('*');
	if( !mFilterList.IsEmpty() ) {
		char *text = ToNewUTF8String( mFilterList );
		if( text ) {
			extensionBuffer.Truncate(0);

			/* eliminate the ';' and the duplicates */
			char buffer[MAX_EXTENSION_LENGTH+1], buf[MAX_EXTENSION_LENGTH+1], *q, *delims = "; ", *dummy;
			strcpy( buffer, text );
			q = strtok_r( buffer, delims, &dummy );
			while( q ) {
				sprintf( buf, "%s ", q );
				if ( !strstr(extensionBuffer.get(), buf ) )
					extensionBuffer.Append(buf);
				q = strtok_r( NULL, delims, &dummy );
				}

			nsMemory::Free( text );
			}
		}
	else if (!mDefaultExtension.IsEmpty()) {
		// Someone was cool and told us what to do
		char *convertedExt = ToNewUTF8String( mDefaultExtension );
		if (!convertedExt) {
			LossyCopyUTF16toASCII(mDefaultExtension, extensionBuffer);
			}
		else {
			extensionBuffer.Assign(convertedExt);
			nsMemory::Free( convertedExt );
			}
		}

	PtFileSelectionInfo_t info;
	memset( &info, 0, sizeof( info ) );

	if( PtFileSelection( mParentWidget, NULL, title, initialDir.get(),
		extensionBuffer.get(), btn1, "&Cancel", "nsd", &info, flags ) ) {
			if (title) nsMemory::Free( title );
			return NS_ERROR_FAILURE;
			}

	*aReturnVal = returnOK;

	if( info.ret == Pt_FSDIALOG_BTN2 ) {
		*aReturnVal = returnCancel;
		}
	else if( mMode != modeOpenMultiple ) {
		mFile.SetLength(0);
		mFile.Append( info.path );

		if( mMode == modeSave ) {
			nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
			NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

			file->InitWithNativePath( mFile );
			
			PRBool exists = PR_FALSE;
			file->Exists(&exists);
			if (exists)
				*aReturnVal = returnReplace;
			}
		}
	else { /* here mMode is modeOpenMultiple */
		PtFileSelectorInfo_t *minfo = info.minfo;
		if( minfo ) {
			nsresult rv;

			for( int i=0; i<minfo->nitems; i++ ) {
				nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
				NS_ENSURE_SUCCESS(rv,rv);
	
				nsCString s ( minfo->multipath[i] );
				rv = file->InitWithNativePath( s );
				NS_ENSURE_SUCCESS(rv,rv);
	
				rv = mFiles.AppendObject(file);
				NS_ENSURE_SUCCESS(rv,rv);
				}

			PtFSFreeInfo( &info ); /* clean the info structure if the multiple mode is set */
			}
		}

  PL_strncpyz( mLastUsedDirectory, info.path, PATH_MAX+1 );
  if (!mDisplayDirectory)
    mDisplayDirectory = do_CreateInstance("@mozilla.org/file/local;1");
  if (mDisplayDirectory)
    mDisplayDirectory->InitWithNativePath( nsDependentCString(mLastUsedDirectory) );

  if( title ) nsMemory::Free( title );
		
  return NS_OK;

// TODO: implement filters
}



NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(aFile);

  if (mFile.IsEmpty())
      return NS_OK;

  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
    
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  file->InitWithNativePath(mFile);

  NS_ADDREF(*aFile = file);

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetFiles(nsISimpleEnumerator **aFiles)
{
 	NS_ENSURE_ARG_POINTER(aFiles);
 	return NS_NewArrayEnumerator(aFiles, mFiles);
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFileURL(nsIURI **aFileURL)
{
  *aFileURL = nsnull;
  nsCOMPtr<nsILocalFile> file;
  nsresult rv = GetFile(getter_AddRefs(file));
  if (!file)
    return rv;

  return NS_NewFileURI(aFileURL, file);
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDefaultString(const nsAString& aString)
{
  mDefault = aString;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(nsAString& aString)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// The default extension to use for files
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDefaultExtension(nsAString& aExtension)
{
  aExtension = mDefaultExtension;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetDefaultExtension(const nsAString& aExtension)
{
	mDefaultExtension = aExtension;
  return NS_OK;
}

//-------------------------------------------------------------------------
void nsFilePicker::InitNative(nsIWidget *aParent,
                              const nsAString& aTitle,
                              PRInt16 aMode)
{
	mParentWidget = (PtWidget_t *)aParent->GetNativeData(NS_NATIVE_WIDGET);
  mTitle.SetLength(0);
  mTitle.Append(aTitle);
  mMode = aMode;
}


NS_IMETHODIMP
nsFilePicker::AppendFilter(const nsAString& aTitle, const nsAString& aFilter)
{
  mFilterList.Append(aFilter);
	mFilterList.Append(PRUnichar(' '));

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the filter index
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetFilterIndex(PRInt32 *aFilterIndex)
{
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetFilterIndex(PRInt32 aFilterIndex)
{
  return NS_OK;
}
