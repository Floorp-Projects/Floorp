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
#include "nsIFileURL.h"
#include "nsIStringBundle.h"
#include "nsEnumeratorUtils.h"
#include "nsCRT.h"


static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);

NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

char nsFilePicker::mLastUsedDirectory[PATH_MAX+1] = { 0 };

#define MAX_EXTENSION_LENGTH PATH_MAX

//-------------------------------------------------------------------------
//
// nsFilePicker constructor
//
//-------------------------------------------------------------------------
nsFilePicker::nsFilePicker()
	: mParentWidget( nsnull )
  , mUnicodeEncoder(nsnull)
  , mUnicodeDecoder(nsnull)
{
  mDisplayDirectory = do_CreateInstance("@mozilla.org/file/local;1");
	char *path = getenv( "HOME" );
	if( path ) {
		mDisplayDirectory->InitWithNativePath( nsDependentCString(path) );
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

  char *title = ConvertToFileSystemCharset(mTitle.get());
  if (nsnull == title)
    title = ToNewCString(mTitle);

  nsCAutoString initialDir;
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

	char extensionBuffer[MAX_EXTENSION_LENGTH+1] = "*";
	if( !mFilterList.IsEmpty() ) {
		char *text = ConvertToFileSystemCharset( mFilterList.get( ) );
		if( text ) {
			extensionBuffer[0] = 0;

			/* eliminate the ';' and the duplicates */
			char buffer[MAX_EXTENSION_LENGTH+1], buf[MAX_EXTENSION_LENGTH+1], *q, *delims = "; ", *dummy;
			strcpy( buffer, text );
			q = strtok_r( buffer, delims, &dummy );
			while( q ) {
				sprintf( buf, "%s ", q );
				if( !strstr( extensionBuffer, buf ) )
					strcat( extensionBuffer, buf );
				q = strtok_r( NULL, delims, &dummy );
				}

			nsMemory::Free( text );
			}
		}
	else if (!mDefaultExtension.IsEmpty()) {
		// Someone was cool and told us what to do
		char *convertedExt = ConvertToFileSystemCharset(mDefaultExtension.get());
		if (!convertedExt) {
			mDefaultExtension.ToCString(extensionBuffer, MAX_EXTENSION_LENGTH);
			}
		else {
			PL_strncpyz(extensionBuffer, convertedExt, MAX_EXTENSION_LENGTH+1);
			nsMemory::Free( convertedExt );
			}
		}

	PtFileSelectionInfo_t info;
	memset( &info, 0, sizeof( info ) );

	if( PtFileSelection( mParentWidget, NULL, title, initialDir.get(),
		extensionBuffer, btn1, "&Cancel", "nsd", &info, flags ) ) {
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
			nsresult rv = NS_NewISupportsArray(getter_AddRefs(mFiles));
			NS_ENSURE_SUCCESS(rv,rv);

			for( int i=0; i<minfo->nitems; i++ ) {
				nsCOMPtr<nsILocalFile> file = do_CreateInstance("@mozilla.org/file/local;1", &rv);
				NS_ENSURE_SUCCESS(rv,rv);
	
				nsCString s ( minfo->multipath[i] );
				rv = file->InitWithNativePath( s );
				NS_ENSURE_SUCCESS(rv,rv);
	
				rv = mFiles->AppendElement(file);
				NS_ENSURE_SUCCESS(rv,rv);
				}

			PtFSFreeInfo( &info ); /* clean the info structure if the multiple mode is set */
			}
		}

	PL_strncpyz( mLastUsedDirectory, info.path, PATH_MAX+1 );
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
NS_IMETHODIMP nsFilePicker::GetFileURL(nsIFileURL **aFileURL)
{
  nsCOMPtr<nsILocalFile> file(do_CreateInstance("@mozilla.org/file/local;1"));
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);
  file->InitWithNativePath(mFile);

  nsCOMPtr<nsIURI> uri;
  NS_NewFileURI(getter_AddRefs(uri), file);
  nsCOMPtr<nsIFileURL> fileURL(do_QueryInterface(uri));
  NS_ENSURE_TRUE(fileURL, NS_ERROR_FAILURE);
  
  NS_ADDREF(*aFileURL = fileURL);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDefaultString(const PRUnichar *aString)
{
  mDefault = aString;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetDefaultString(PRUnichar **aString)
{
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
//
// The default extension to use for files
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDefaultExtension(PRUnichar **aExtension)
{
  *aExtension = ToNewUnicode(mDefaultExtension);
  if (!*aExtension)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::SetDefaultExtension(const PRUnichar *aExtension)
{
	mDefaultExtension = aExtension;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::SetDisplayDirectory(nsILocalFile *aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetDisplayDirectory(nsILocalFile **aDirectory)
{
  *aDirectory = mDisplayDirectory;
  NS_IF_ADDREF(*aDirectory);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::InitNative(nsIWidget *aParent,
                                       const PRUnichar *aTitle,
                                       PRInt16 aMode)
{
	if( aParent ) mParentWidget = (PtWidget_t *)aParent->GetNativeData(NS_NATIVE_WIDGET);
  mTitle.SetLength(0);
  mTitle.Append(aTitle);
  mMode = aMode;
  return NS_OK;
}


NS_IMETHODIMP
nsFilePicker::AppendFilter(const PRUnichar *aTitle, const PRUnichar *aFilter)
{
  mFilterList.Append(aFilter);
	mFilterList.Append(PRUnichar(' '));

  return NS_OK;
}


//-------------------------------------------------------------------------
void nsFilePicker::GetFileSystemCharset(nsCString & fileSystemCharset)
{
  static nsCAutoString aCharset;
  nsresult rv;

  if (aCharset.Length() < 1) {
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
      rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aCharset);

    NS_ASSERTION(NS_SUCCEEDED(rv), "error getting platform charset");
    if (NS_FAILED(rv))
      aCharset.Assign(NS_LITERAL_CSTRING("windows-1252"));
  }
  fileSystemCharset = aCharset;
}


//-------------------------------------------------------------------------
char * nsFilePicker::ConvertToFileSystemCharset(const PRUnichar *inString)
{
  char *outString = nsnull;
  nsresult rv = NS_OK;

  // get file system charset and create a unicode encoder
  if (nsnull == mUnicodeEncoder) {
    nsCAutoString fileSystemCharset;
    GetFileSystemCharset(fileSystemCharset);

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &rv); 
    if (NS_SUCCEEDED(rv)) {
      rv = ccm->GetUnicodeEncoderRaw(fileSystemCharset.get(), &mUnicodeEncoder);
    }
  }

  // converts from unicode to the file system charset
  if (NS_SUCCEEDED(rv)) {
    PRInt32 inLength = nsCRT::strlen(inString);
    PRInt32 outLength;
    rv = mUnicodeEncoder->GetMaxLength(inString, inLength, &outLength);
    if (NS_SUCCEEDED(rv)) {
      outString = NS_STATIC_CAST( char*, nsMemory::Alloc( outLength+1 ) );
      if (nsnull == outString) {
        return nsnull;
      }
      rv = mUnicodeEncoder->Convert(inString, &inLength, outString, &outLength);
      if (NS_SUCCEEDED(rv)) {
        outString[outLength] = '\0';
      }
    }
  }
  
  return NS_SUCCEEDED(rv) ? outString : nsnull;
}

//-------------------------------------------------------------------------
PRUnichar * nsFilePicker::ConvertFromFileSystemCharset(const char *inString)
{
  PRUnichar *outString = nsnull;
  nsresult rv = NS_OK;

  // get file system charset and create a unicode encoder
  if (nsnull == mUnicodeDecoder) {
    nsCAutoString fileSystemCharset;
    GetFileSystemCharset(fileSystemCharset);

    nsCOMPtr<nsICharsetConverterManager> ccm = 
             do_GetService(kCharsetConverterManagerCID, &rv); 
    if (NS_SUCCEEDED(rv)) {
      rv = ccm->GetUnicodeDecoderRaw(fileSystemCharset.get(), &mUnicodeDecoder);
    }
  }

  // converts from the file system charset to unicode
  if (NS_SUCCEEDED(rv)) {
    PRInt32 inLength = strlen(inString);
    PRInt32 outLength;
    rv = mUnicodeDecoder->GetMaxLength(inString, inLength, &outLength);
    if (NS_SUCCEEDED(rv)) {
      outString = NS_STATIC_CAST( PRUnichar*, nsMemory::Alloc( (outLength+1) * sizeof( PRUnichar ) ) );
      if (nsnull == outString) {
        return nsnull;
      }
      rv = mUnicodeDecoder->Convert(inString, &inLength, outString, &outLength);
      if (NS_SUCCEEDED(rv)) {
        outString[outLength] = 0;
      }
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv), "error charset conversion");
  return NS_SUCCEEDED(rv) ? outString : nsnull;
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
