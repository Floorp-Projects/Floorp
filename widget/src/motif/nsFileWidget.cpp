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
#include <Xm/FileSB.h>
#include "nsXtEventHandler.h"
#include "nsStringUtil.h"

extern XtAppContext gAppContext;

NS_IMPL_ADDREF(nsFileWidget)
NS_IMPL_RELEASE(nsFileWidget)

//-------------------------------------------------------------------------
//
// nsFileWidget constructor
//
//-------------------------------------------------------------------------
nsFileWidget::nsFileWidget() : nsWindow(), nsIFileWidget()
{
  //NS_INIT_REFCNT();

  mNumberOfFilters = 0;
}

void nsFileWidget::Create(nsIWidget        *aParent,
                          const nsRect     &aRect,
                          EVENT_CALLBACK   aHandleEventFunction,
                          nsIDeviceContext *aContext,
                          nsIAppShell      *aAppShell,
                          nsIToolkit       *aToolkit,
                          nsWidgetInitData *aInitData) 
{
  nsString title("Load");
  Create(aParent, title, eMode_load, aContext, aAppShell, aToolkit, aInitData);
}


//-------------------------------------------------------------------------
void   nsFileWidget:: Create(nsIWidget  *aParent,
                             nsString&   aTitle,
                             nsMode      aMode,
                             nsIDeviceContext *aContext,
                             nsIAppShell *aAppShell,
                             nsIToolkit *aToolkit,
                             void       *aInitData)
{
  //mWnd = (aParent) ? aParent->GetNativeData(NS_NATIVE_WINDOW) : 0;
  mTitle.SetLength(0);
  mTitle.Append(aTitle);
  mMode = aMode;

  Widget parentWidget = nsnull;

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aInitData ;
  }

  InitToolkit(aToolkit, aParent);
  InitDeviceContext(aContext, parentWidget);

  mWidget = XmCreateFileSelectionDialog(parentWidget, "filesb", NULL, 0);

  NS_ALLOC_STR_BUF(title, aTitle, 256);
  XmString str;
  str = XmStringCreate(title, XmFONTLIST_DEFAULT_TAG);
  XtVaSetValues(mWidget, XmNdialogTitle, str, nsnull);
  NS_FREE_STR_BUF(title);
  XmStringFree(str);


  XtAddCallback(mWidget, XmNcancelCallback, nsXtWidget_FSBCancel_Callback, this);
  XtAddCallback(mWidget, XmNokCallback, nsXtWidget_FSBOk_Callback, this);

  //XtManageChild(mWidget);
}

void nsFileWidget::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsFileWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  //  nsresult result = nsWindow::QueryObject(aIID, aInstancePtr);

    nsresult result = NS_NOINTERFACE;
    static NS_DEFINE_IID(kInsFileWidgetIID, NS_IFILEWIDGET_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsFileWidgetIID)) {
        *aInstancePtr = (void*) ((nsIFileWidget*)this);
        AddRef();
        result = NS_OK;
    }

    return result;
}



//-------------------------------------------------------------------------
//
// Ok's the dialog
//
//-------------------------------------------------------------------------
void nsFileWidget::OnOk()
{
  XtUnmanageChild(mWidget);
  mWasCancelled  = PR_FALSE;
  mIOwnEventLoop = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Cancel the dialog
//
//-------------------------------------------------------------------------
void nsFileWidget::OnCancel()
{
  XtUnmanageChild(mWidget);
  mWasCancelled  = PR_TRUE;
  mIOwnEventLoop = PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
PRBool nsFileWidget::Show()
{
  nsresult result = nsEventStatus_eIgnore;
  XtManageChild(mWidget);

  // XXX Kludge: gAppContext is a global set in nsAppShell
  XEvent event;
  mIOwnEventLoop = PR_TRUE;
  while (mIOwnEventLoop) {
    XtAppNextEvent(gAppContext, &event);
    XtDispatchEvent(&event);
  }

  if (!mWasCancelled) {
    XmString str;
    char *   fileBuf;
    XtVaGetValues(mWidget, XmNdirSpec, &str, nsnull);
    if (XmStringGetLtoR(str, XmFONTLIST_DEFAULT_TAG, &fileBuf)) {
      // Set user-selected location of file or directory
      mFile.SetLength(0);
      mFile.Append(fileBuf);
      XmStringFree(str);
      XtFree(fileBuf);
    }
  }

  /*char fileBuffer[MAX_PATH];
  fileBuffer[0] = '\0';
  OPENFILENAME ofn;
  memset(&ofn, 0, sizeof(ofn));

  ofn.lStructSize = sizeof(ofn);

  nsString filterList;
  GetFilterListArray(filterList);
  char *filterBuffer = filterList.ToNewCString();
  char *title = mTitle.ToNewCString();
  ofn.lpstrTitle = title;
  ofn.lpstrFilter = filterBuffer;
  ofn.nFilterIndex = 1;
  ofn.hwndOwner = mWnd;
  ofn.lpstrFile = fileBuffer;
  ofn.nMaxFile = MAX_PATH;
  ofn.Flags = OFN_SHAREAWARE | OFN_NOCHANGEDIR | OFN_LONGNAMES | OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY;
  
  BOOL result;

    // Save current directory, so we can reset if it changes.
  char* currentDirectory = new char[MAX_PATH+1];
  VERIFY(::GetCurrentDirectory(MAX_PATH, currentDirectory) > 0);

  if (mMode == eMode_load) {
    result = GetOpenFileName(&ofn);
  }
  else if (mMode == eMode_save) {
    result = GetSaveFileName(&ofn);
  }
  else {
    NS_ASSERTION(0, "Only load and save are supported modes"); 
  }

  VERIFY(::SetCurrentDirectory(currentDirectory));
  
   // Clean up filter buffers
  delete filterBuffer;
  delete title;

   // Set user-selected location of file or directory
  mFile.SetLength(0);
  if (result==PR_TRUE) {
    mFile.Append(fileBuffer);
  }
 */ 
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

void nsFileWidget::SetFilterList(PRUint32 aNumberOfFilters,const nsString aTitles[],const nsString aFilters[])
{
  mNumberOfFilters  = aNumberOfFilters;
  mTitles           = aTitles;
  mFilters          = aFilters;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------

void  nsFileWidget::GetFile(nsString& aFile)
{
  aFile.SetLength(0);
  aFile.Append(mFile);
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------

void  nsFileWidget::SetDefaultString(nsString& aString)
{
  mDefault = aString;
}



//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
}

