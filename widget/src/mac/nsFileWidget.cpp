/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsFileWidget.h"
#include "nsStringUtil.h"
#include <StandardFile.h>
#if USE_IC
# include <ICAPI.h>
#endif
#include "nsMacControl.h"
#include "nsCarbonHelpers.h"
#include "nsWatchTask.h"
#include "nsFileSpec.h"

#define DBG 0

NS_IMPL_ADDREF(nsFileWidget)
NS_IMPL_RELEASE(nsFileWidget)


//-------------------------------------------------------------------------
//
// nsFileWidget constructor
//
//-------------------------------------------------------------------------
nsFileWidget::nsFileWidget()
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
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
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
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFileWidget::Create(nsIWidget        *aParent,
                          const nsRect     &aRect,
                          EVENT_CALLBACK   aHandleEventFunction,
                          nsIDeviceContext *aContext,
                          nsIAppShell      *aAppShell,
                          nsIToolkit       *aToolkit,
                          nsWidgetInitData *aInitData) 
{
  Create(aParent, NS_ConvertASCIItoUCS2("Open"), eMode_load, aContext, aAppShell, aToolkit, aInitData);
  return NS_OK;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
NS_IMETHODIMP   nsFileWidget:: Create(nsIWidget  *aParent,
                             const nsString&   aTitle,
                             nsFileDlgMode    aMode,
                             nsIDeviceContext *aContext,
                             nsIAppShell *aAppShell,
                             nsIToolkit *aToolkit,
                             void       *aInitData)
{
  mTitle = aTitle;
  mMode = aMode;

	mWindowPtr = nsnull;

  return NS_OK;

}


NS_INTERFACE_MAP_BEGIN(nsFileWidget)
  NS_INTERFACE_MAP_ENTRY(nsIFileWidget)
NS_INTERFACE_MAP_END_INHERITING(nsWindow)



NS_IMETHODIMP nsFileWidget::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Ok's the dialog
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsFileWidget::OnOk()
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
NS_IMETHODIMP nsFileWidget::OnCancel()
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
PRBool nsFileWidget::Show()
{
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
  
  if (mMode == eMode_load)
    userClicksOK = GetFile ( title, &theFile );
  else if (mMode == eMode_save)
    userClicksOK = PutFile ( title, defaultName, &theFile );
  else if (mMode == eMode_getfolder)
    userClicksOK = GetFolder ( title, &theFile );  

  // Clean up filter buffers
  delete[] filterBuffer;

  if ( userClicksOK ) {
    nsNativeFileSpec fileSpec(theFile);
    nsFilePath filePath(fileSpec);
	
    mFile.AssignWithConversion( NS_STATIC_CAST(const char*, filePath) );
    mFileSpec = fileSpec;
  }
  
  return userClicksOK;
}


//-------------------------------------------------------------------------
//
// myProc
//
// An event filter proc for NavServices so the dialogs will be movable-modals. However,
// this doesn't seem to work as of yet...I'll play around with it some more.
//
//-------------------------------------------------------------------------
static pascal void myProc ( NavEventCallbackMessage msg, NavCBRecPtr cbRec, NavCallBackUserData data )
{
	switch ( msg ) {
	case kNavCBEvent:
		switch ( cbRec->eventData.eventDataParms.event->what ) {
		case updateEvt:
        	WindowPtr window = reinterpret_cast<WindowPtr>(cbRec->eventData.eventDataParms.event->message);
		    if (window) {
				::BeginUpdate(window);
				::EndUpdate(window);
			}
			break;
		}
		break;
	}
}


//-------------------------------------------------------------------------
//
// PutFile
//
// Use NavServices to do a PutFile. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, the location to put the file and the name, etc is in the FSSpec.
//
//-------------------------------------------------------------------------
PRBool
nsFileWidget :: PutFile ( Str255 & inTitle, Str255 & inDefaultName, FSSpec* outSpec )
{
 	PRBool retVal = PR_FALSE;
	NavReplyRecord reply;
	NavDialogOptions dialogOptions;
	NavEventUPP eventProc = NewNavEventProc(myProc);  // doesn't really matter if this fails
	OSType				typeToSave = 'TEXT';
	OSType				creatorToSave = 'MOZZ';

	OSErr anErr = NavGetDefaultDialogOptions(&dialogOptions);
	if (anErr == noErr)	{	
		// Set the options for how the get file dialog will appear
		dialogOptions.dialogOptionFlags |= kNavNoTypePopup;
		dialogOptions.dialogOptionFlags |= kNavDontAutoTranslate;
		dialogOptions.dialogOptionFlags |= kNavDontAddTranslateItems;
		dialogOptions.dialogOptionFlags ^= kNavAllowMultipleFiles;
		::BlockMoveData(inTitle, dialogOptions.message, *inTitle + 1);
		::BlockMoveData(inDefaultName, dialogOptions.savedFileName, *inDefaultName + 1);
		
		// Display the get file dialog
    nsWatchTask::GetTask().Suspend();  
		anErr = ::NavPutFile(
					NULL,
					&reply,
					&dialogOptions,
					eventProc,
					typeToSave,
					creatorToSave,
					NULL); // callbackUD	
    nsWatchTask::GetTask().Resume();  
	
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
				
				if (reply.replacing)
					mSelectResult = nsFileDlgResults_Replace;
				else
					mSelectResult = nsFileDlgResults_OK;
				
				// Some housekeeping for Nav Services 
				::NavCompleteSave(&reply, kNavTranslateInPlace);
				::NavDisposeReply(&reply);
				
				retVal = PR_TRUE;
			}

		} // if user clicked OK	
	} // if can get dialog options
	
	if ( eventProc )
		::DisposeNavEventUPP(eventProc);
	
	return retVal;
	
} // PutFile



//-------------------------------------------------------------------------
//
// GetFile
//
// Use NavServices to do a GetFile. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, the selected file is in the FSSpec.
//
//-------------------------------------------------------------------------
PRBool
nsFileWidget :: GetFile ( Str255 & inTitle, /* filter list here later */ FSSpec* outSpec )
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
    nsWatchTask::GetTask().Suspend();  
		anErr = ::NavGetFile(
					NULL,
					&reply,
					&dialogOptions,
					eventProc,
					NULL, // preview proc
					NULL, // filter proc
					NULL, //typeList,
					NULL); // callbackUD	
    nsWatchTask::GetTask().Resume();  
	
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
nsFileWidget :: GetFolder ( Str255 & inTitle, FSSpec* outSpec  )
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
    nsWatchTask::GetTask().Suspend();  
		anErr = ::NavChooseFolder(
					NULL,
					&reply,
					&dialogOptions,
					eventProc,
					NULL, // filter proc
					NULL); // callbackUD	
    nsWatchTask::GetTask().Resume();  
	
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
// Convert filter titles + filters into a Windows filter string
//
//-------------------------------------------------------------------------

void nsFileWidget::GetFilterListArray(nsString& aFilterList)
{
  aFilterList.SetLength(0);
  for (PRUint32 i = 0; i < mNumberOfFilters; i++) {
    const nsString& title = mTitles[i];
    const nsString& filter = mFilters[i];
    
    aFilterList.Append(title);
    aFilterList.AppendWithConversion('\0');
    aFilterList.Append(filter);
    aFilterList.AppendWithConversion('\0');
  }
  aFilterList.AppendWithConversion('\0'); 
}

//-------------------------------------------------------------------------
//
// Set the list of filters
//
// Note - Since the XP representation of filters is based on the Win32
//			file dialog mechanism of supplying a .ext list of files to
//			display we need to do some massaging to convert this into
//			something useful on the Mac.  Luckily Internet Config provides
//			a mechanism to get a file's Mac type based on the .ext so
//			we'll utilize that.
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsFileWidget::SetFilterList(PRUint32 aNumberOfFilters,const nsString aTitles[],const nsString aFilters[])
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

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
NS_IMETHODIMP  nsFileWidget::GetSelectedType(PRInt16& theType)
{
  theType = mSelectedType;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------

NS_IMETHODIMP  nsFileWidget::GetFile(nsFileSpec& aFile)
{
  aFile = mFileSpec;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------

NS_IMETHODIMP  nsFileWidget::SetDefaultString(const nsString& aString)
{
  mDefault = aString;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_IMETHODIMP  nsFileWidget::SetDisplayDirectory(const nsFileSpec& aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------

NS_IMETHODIMP  nsFileWidget::GetDisplayDirectory(nsFileSpec& aDirectory)
{
  aDirectory = mDisplayDirectory;
  return NS_OK;
}


//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
nsFileDlgResults  nsFileWidget::GetFile(
	    nsIWidget        * aParent,
	    const nsString   & promptString,
	    nsFileSpec       & theFileSpec)
{
	Create(aParent, promptString, eMode_load, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		theFileSpec = mFileSpec;
		return mSelectResult;
	}
	
	return nsFileDlgResults_Cancel;
}
	    
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
nsFileDlgResults  nsFileWidget::GetFolder(
	    nsIWidget        * aParent,
	    const nsString   & promptString,
	    nsFileSpec       & theFileSpec)
{
	Create(aParent, promptString, eMode_getfolder, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		theFileSpec = mFileSpec;
		return mSelectResult;
	}
	
	return nsFileDlgResults_Cancel;
}
	    
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
nsFileDlgResults  nsFileWidget::PutFile(
	    nsIWidget        * aParent,
	    const nsString         & promptString,
	    nsFileSpec       & theFileSpec)
{
	Create(aParent, promptString, eMode_save, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		theFileSpec = mFileSpec;
		return mSelectResult;
	}
	
	return nsFileDlgResults_Cancel;
}
