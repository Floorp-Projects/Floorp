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
                             nsMode      aMode,
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
  StringToStr255(mTitle,title);
  StringToStr255(mDefault,defaultName);
    
  StandardFileReply reply;

  if (mMode == eMode_load) {
    PRInt32 numTypes = -1; 				// DO NO FILTERING FOR NOW! -1 on the Mac means no filtering is done
    SFTypeList typeList;
    StandardGetFile (nsnull, numTypes, typeList, &reply );
  }
  else if (mMode == eMode_save) {
  	StandardPutFile (title, defaultName, &reply );
  }
  else {
    NS_ASSERTION(0, "Only load and save are supported modes"); 
  }
   // Clean up filter buffers
  delete[] filterBuffer;

	if (!reply.sfGood) return PR_FALSE;

	nsNativeFileSpec		fileSpec(reply.sfFile);
	nsFilePath					filePath(fileSpec);
	
	mFile = filePath;
	
   // Set user-selected location of file or directory
  //Str255ToString(reply.sfFile.name, mFile);  
  
  return PR_TRUE;
}

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


//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
}

