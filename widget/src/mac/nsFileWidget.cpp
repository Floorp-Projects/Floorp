/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsFileWidget.h"
#include "nsStringUtil.h"
#include <StandardFile.h>
#include "nsMacControl.h"

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
}

NS_IMETHODIMP nsFileWidget::Create(nsIWidget        *aParent,
                          const nsRect     &aRect,
                          EVENT_CALLBACK   aHandleEventFunction,
                          nsIDeviceContext *aContext,
                          nsIAppShell      *aAppShell,
                          nsIToolkit       *aToolkit,
                          nsWidgetInitData *aInitData) 
{
  nsString title("Open");
  Create(aParent, title, eMode_load, aContext, aAppShell, aToolkit, aInitData);
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP   nsFileWidget:: Create(nsIWidget  *aParent,
                             nsString&   aTitle,
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


/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
*/ 
nsresult nsFileWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
    if (aIID.Equals(kIFileWidgetIID)) {
        *aInstancePtr = (void*) ((nsIFileWidget*)this);
        AddRef();
        return NS_OK;
    }

    return nsWindow::QueryInterface(aIID,aInstancePtr);
}



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
	
    mFile = filePath;
    mFileSpec = fileSpec;
  }
  
  return userClicksOK;
}


//
// myProc
//
// An event filter proc for NavServices so the dialogs will be movable-modals. However,
// this doesn't seem to work as of yet...I'll play around with it some more.
//
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


//
// PutFile
//
// Use NavServices to do a PutFile. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, the location to put the file and the name, etc is in the FSSpec.
//
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
		
		// Display the get file dialog
		anErr = ::NavPutFile(
					NULL,
					&reply,
					&dialogOptions,
					eventProc,
					typeToSave,
					creatorToSave,
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
		::DisposeRoutineDescriptor(eventProc);
		
	return retVal;
	
} // PutFile



//
// GetFile
//
// Use NavServices to do a GetFile. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, the selected file is in the FSSpec.
//
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
		::DisposeRoutineDescriptor(eventProc);
		
	return retVal;

} // GetFile


//
// GetFolder
//
// Use NavServices to do a PutFile. Returns PR_TRUE if the user presses OK in the dialog. If
// they do so, the folder location is in the FSSpec.
//
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
		::DisposeRoutineDescriptor(eventProc);
		
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
    aFilterList.Append('\0');
    aFilterList.Append(filter);
    aFilterList.Append('\0');
  }
  aFilterList.Append('\0'); 
}

//-------------------------------------------------------------------------
//
// Set the list of filters
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsFileWidget::SetFilterList(PRUint32 aNumberOfFilters,const nsString aTitles[],const nsString aFilters[])
{
  mNumberOfFilters  = aNumberOfFilters;
  mTitles           = aTitles;
  mFilters          = aFilters;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------

NS_IMETHODIMP  nsFileWidget::GetFile(nsString& aFile)
{
  aFile.SetLength(0);
  aFile.Append(mFile);
  return NS_OK;
}

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

NS_IMETHODIMP  nsFileWidget::SetDefaultString(nsString& aString)
{
  mDefault = aString;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::SetDisplayDirectory(nsString& aDirectory)
{
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------

NS_METHOD  nsFileWidget::GetDisplayDirectory(nsString& aDirectory)
{
  aDirectory = mDisplayDirectory;
  return NS_OK;
}


nsFileDlgResults  nsFileWidget::GetFile(
	    nsIWidget        * aParent,
	    nsString         & promptString,
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
	    
nsFileDlgResults  nsFileWidget::GetFolder(
	    nsIWidget        * aParent,
	    nsString         & promptString,
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
	    
nsFileDlgResults  nsFileWidget::PutFile(
	    nsIWidget        * aParent,
	    nsString         & promptString,
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

//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
}

