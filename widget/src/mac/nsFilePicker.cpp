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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 */

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"

#include "nsStringUtil.h"
#include <StandardFile.h>
#if USE_IC
# include <ICAPI.h>
#endif
#include "nsMacControl.h"
#include "nsCarbonHelpers.h"

#include "nsFilePicker.h"
#include "nsLocalFile.h"




NS_IMPL_ISUPPORTS1(nsFilePicker, nsIFilePicker)

//-------------------------------------------------------------------------
//
// nsFilePicker constructor
//
//-------------------------------------------------------------------------
nsFilePicker::nsFilePicker()
{
  NS_INIT_REFCNT();

  mIOwnEventLoop = PR_FALSE;
  mNumberOfFilters = 0;
  
  // Zero out the type lists
  for (int i = 0; i < kMaxTypeListCount; i++)
  {
  	mTypeLists[i] = 0L;
  }
}

//-------------------------------------------------------------------------
//
// nsFilePicker destructor
//
//-------------------------------------------------------------------------
nsFilePicker::~nsFilePicker()
{
	// Destroy any filters we have built
	if (mNumberOfFilters)
	{
	  for (int i = 0; i < kMaxTypeListCount; i++)
	  {
	  	if (mTypeLists[i])
	  		DisposePtr((Ptr)mTypeLists[i]);
	  }
	}
}

//-------------------------------------------------------------------------
//
// Ok's the dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::OnOk()
{
  mWasCancelled  = PR_FALSE;
  mIOwnEventLoop = PR_FALSE;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Cancel the dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::OnCancel()
{
  mWasCancelled  = PR_TRUE;
  mIOwnEventLoop = PR_FALSE;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::Show(PRInt16 *retval)
{
  NS_ENSURE_ARG_POINTER(retval);

  nsString filterList;
  GetFilterListArray(filterList);
  char *filterBuffer = filterList.ToNewCString();

  Str255 title;
  Str255 defaultName;
  nsMacControl::StringToStr255(mTitle,title);
  nsMacControl::StringToStr255(mDefault,defaultName);
    
  FSSpec theFile;
  PRBool userClicksOK = PR_FALSE;
  
  // XXX Ignore the filter list for now....
  
  if (mMode == modeLoad)
    userClicksOK = GetFile ( title, &theFile );
  else if (mMode == modeSave)
    userClicksOK = PutFile ( title, defaultName, &theFile );
  else if (mMode == modeGetFolder)
    userClicksOK = GetFolder ( title, &theFile );  

  // Clean up filter buffers
  delete[] filterBuffer;

  if ( userClicksOK ) {
    /*    nsNativeFileSpec fileSpec(theFile);
    nsFilePath filePath(fileSpec);
	
    mFile = theFile;
    mFileSpec = fileSpec;
    */
  }
  
  *retVal = userClicksOK;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// GetFile
//
// Use NavServices to do a GetFile. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, the selected file is in the FSSpec.
//
//-------------------------------------------------------------------------
PRBool
nsFilePicker :: GetFile ( Str255 & inTitle, /* filter list here later */ FSSpec* outSpec )
{
 	PRBool retVal = PR_FALSE;
	NavReplyRecord reply;
	NavDialogOptions dialogOptions;
	NavEventUPP eventProc = NewNavEventProc(myProc);  // doesn't really matter if this fails

	OSErr anErr = NavGetDefaultDialogOptions(&dialogOptions);
	if (anErr == noErr)	{	
		// Set the options for how the get file dialog will appear
		dialogOptions.dialogOptionFlags |= kNavNoTypePopup;
		dialogOptions.dialogOptionFlags |= kNavDontAutoTranslate;
		dialogOptions.dialogOptionFlags |= kNavDontAddTranslateItems;
		dialogOptions.dialogOptionFlags ^= kNavAllowMultipleFiles;
		::BlockMoveData(inTitle, dialogOptions.message, *inTitle + 1);
		
		// Display the get file dialog
		anErr = ::NavGetFile(
					NULL,
					&reply,
					&dialogOptions,
					eventProc,
					NULL, // preview proc
					NULL, // filter proc
					NULL, //typeList,
					NULL); // callbackUD	
	
		// See if the user has selected save
		if (anErr == noErr && reply.validRecord) {
			AEKeyword	theKeyword;
			DescType	actualType;
			Size		actualSize;
			FSSpec		theFSSpec;
			
			// Get the FSSpec for the file to be opened
			anErr = AEGetNthPtr(&(reply.selection), 1, typeFSS, &theKeyword, &actualType,
				&theFSSpec, sizeof(theFSSpec), &actualSize);
			
			if (anErr == noErr) {
				*outSpec = theFSSpec;	// Return the FSSpec
				mSelectResult = nsFileDlgResults_OK;
				
				// Some housekeeping for Nav Services 
				::NavDisposeReply(&reply);
				
				retVal = PR_TRUE;
			}

		} // if user clicked OK	
	} // if can get dialog options
	
	if ( eventProc )
		::DisposeNavEventUPP(eventProc);
		
	return retVal;

} // GetFile


//-------------------------------------------------------------------------
//
// GetFolder
//
// Use NavServices to do a PutFile. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, the folder location is in the FSSpec.
//
//-------------------------------------------------------------------------
PRBool
nsFilePicker :: GetFolder ( Str255 & inTitle, FSSpec* outSpec  )
{
 	PRBool retVal = PR_FALSE;
	NavReplyRecord reply;
	NavDialogOptions dialogOptions;
	NavEventUPP eventProc = NewNavEventProc(myProc);  // doesn't really matter if this fails

	OSErr anErr = NavGetDefaultDialogOptions(&dialogOptions);
	if (anErr == noErr)	{	
		// Set the options for how the get file dialog will appear
		dialogOptions.dialogOptionFlags |= kNavNoTypePopup;
		dialogOptions.dialogOptionFlags |= kNavDontAutoTranslate;
		dialogOptions.dialogOptionFlags |= kNavDontAddTranslateItems;
		dialogOptions.dialogOptionFlags ^= kNavAllowMultipleFiles;
		::BlockMoveData(inTitle, dialogOptions.message, *inTitle + 1);
		
		// Display the get file dialog
		anErr = ::NavChooseFolder(
					NULL,
					&reply,
					&dialogOptions,
					eventProc,
					NULL, // filter proc
					NULL); // callbackUD	
	
		// See if the user has selected save
		if (anErr == noErr && reply.validRecord) {
			AEKeyword	theKeyword;
			DescType	actualType;
			Size		actualSize;
			FSSpec		theFSSpec;
			
			// Get the FSSpec for the file to be opened
			anErr = AEGetNthPtr(&(reply.selection), 1, typeFSS, &theKeyword, &actualType,
				&theFSSpec, sizeof(theFSSpec), &actualSize);
			
			if (anErr == noErr) {
				*outSpec = theFSSpec;	// Return the FSSpec
				mSelectResult = nsFileDlgResults_OK;
				
				// Some housekeeping for Nav Services 
				::NavDisposeReply(&reply);
				
				retVal = PR_TRUE;
			}

		} // if user clicked OK	
	} // if can get dialog options
	
	if ( eventProc )
		::DisposeNavEventUPP(eventProc);
		
	return retVal;


} // GetFolder


//-------------------------------------------------------------------------
//
// myProc
//
// An event filter proc for NavServices so the dialogs will be movable-modals. However,
// this doesn't seem to work as of yet...I'll play around with it some more.
//
//-------------------------------------------------------------------------
pascal void myProc ( NavEventCallbackMessage msg, NavCBRecPtr cbRec, NavCallBackUserData data ) ;
pascal void myProc ( NavEventCallbackMessage msg, NavCBRecPtr cbRec, NavCallBackUserData data )
{
	WindowPtr window = reinterpret_cast<WindowPtr>(cbRec->eventData.eventDataParms.event->message);
	switch ( msg ) {
		case kNavCBEvent:
			switch ( cbRec->eventData.eventDataParms.event->what ) {
				case updateEvt:
					::BeginUpdate(window);
					::EndUpdate(window);
					break;
			}
			break;
	}
}

//-------------------------------------------------------------------------
//
// Set the list of filters
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsFilePicker::SetFilterList(PRInt32 aNumberOfFilters,
                                          const PRUnichar **aTitles,
                                          const PRUnichar **filters)
{
	mNumberOfFilters  = aNumberOfFilters;
	mTitles           = aTitles;
	mFilters          = aFilters;
	
#if USE_IC  // FOR NOW JUST BYPASS ALL THIS CODE

	unsigned char	typeTemp[256];
	unsigned char	tempChar;

	OSType			tempOSType;
	ICInstance		icInstance;
	ICError			icErr;
	Handle			mappings = NewHandleClear(4);
	ICAttr			attr;
	ICMapEntry		icEntry;
	
	icErr = ICStart(&icInstance, 'MOZZ');
	if (icErr == noErr)
	{
		icErr = ICFindConfigFile(icInstance, 0, nil);
		if (icErr == noErr)
		{
			icErr = ICFindPrefHandle(icInstance, kICMapping, &attr, mappings);
			if (icErr != noErr)
				goto bail_w_IC;
		}
		else
			goto bail_w_IC;
	}
	else
		goto bail_wo_IC;
	
	if (aNumberOfFilters)
	{
		// First we allocate the memory for the Mac type lists
		for (PRUint32 loop1 = 0; loop1 < mNumberOfFilters; loop1++)
		{
			mTypeLists[loop1] =
				(NavTypeListPtr)NewPtrClear(sizeof(NavTypeList) + kMaxTypesPerFilter * sizeof(OSType));
			
			if (mTypeLists[loop1] == nil)
				goto bail_w_IC;
		}
		
		// Now loop through each of the filter strings
	  	for (PRUint32 loop1 = 0; loop1 < mNumberOfFilters; loop1++)
	  	{
	  		const nsString& filter = mFilters[loop1];
		  	PRUint32 filterIndex = 0;			// Index into the filter string

	  		if (filter[filterIndex])
	  		{
		  		PRUint32 typeTempIndex = 1;			// Index into the temp string for a single filter type
		  		PRUint32 typesInThisFilter = 0;		// Count for # of types in this filter
				bool finishedThisFilter = false;	// Flag so we know when we're finsihed with the filter
	  			do	// Loop throught the characters of filter string
		  		{
		  			if ((tempChar == ';') || (tempChar == 0))
		  			{ // End of filter type reached
		  				typeTemp[typeTempIndex] = 0;
		  				typeTemp[0] = typeTempIndex - 1;
		  				
		  				icErr = ICMapEntriesFilename(icInstance, mappings, typeTemp, &icEntry);
		  				if (icErr != icPrefNotFoundErr)
		  				{
		  					bool addToList = true;
		  					tempOSType = icEntry.file_type;
		  					for (PRUint32 typeIndex = 0; typeIndex < typesInThisFilter; typeIndex++)
		  					{
		  						if (mTypeLists[loop1]->osType[typeIndex] == tempOSType)
		  						{
		  							addToList = false;
		  							break;
		  						}
		  					}
		  					if (addToList)
		  						mTypeLists[loop1]->osType[typesInThisFilter++] = tempOSType;
		  				}
		  				
		  				typeTempIndex = 0;			// Reset the temp string for the type
		  				typeTemp[0] = 0;
		  				if (tempChar == 0)
		  					finishedThisFilter = true;
		  			}
		  			else
		  			{
		  				typeTemp[typeTempIndex++] = tempChar;
		  			}
		  			
		  			filterIndex++;
		  		} while (!finishedThisFilter);
		  		
		  		// Set hoe many OSTypes we actually found
		  		mTypeLists[loop1]->osTypeCount = typesInThisFilter;
	  		}
		}
	}

bail_w_IC:
	ICStop(icInstance);

bail_wo_IC:

#endif	// FOR NOW JUST BYPASS ALL THIS CODE

  return NS_OK;
}

NS_IMETHODIMP nsFilePicker::GetFile(nsILocalFile **aFile)
{
  NS_ENSURE_ARG_POINTER(*aFile);


  nsCOMPtr<nsILocalFile> file(do_CreateInstance("component://mozilla/file/local"));
    
  NS_ENSURE_TRUE(file, NS_ERROR_FAILURE);

  file->InitWithPath(nsCAutoString(mFile));

  NS_ADDREF(*aFile = file);

  return NS_OK;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::GetSelectedFilter(PRInt32 *aType)
{
  NS_ENSURE_ARG_POINTER(aType);
  *aType = mSelectedType;
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

NS_IMETHODIMP nsFilePicker::Create(nsIDOMWindow *aParent,
                                   const PRUnichar *aTitle,
                                   PRInt16 aMode)
{
  return nsBaseFilePicker::Create(aParent, aTitle, aMode);
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsFilePicker::CreateNative(nsIWidget *aParent,
                                         const PRUnichar *aTitle,
                                         PRInt16 aMode)
{

  mTitle = aTitle;
  mMode = aMode;

	mWindowPtr = nsnull;

  return NS_OK;
}
