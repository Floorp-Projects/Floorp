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

#define DBG 0
extern XtAppContext gAppContext;

//-------------------------------------------------------------------------
//
// nsFileWidget constructor
//
//-------------------------------------------------------------------------
nsFileWidget::nsFileWidget(nsISupports *aOuter) : nsWindow(aOuter)
{
  //mWnd = NULL;
  mNumberOfFilters = 0;
}

void nsFileWidget::Create(nsIWidget        *aParent,
                          const nsRect     &aRect,
                          EVENT_CALLBACK   aHandleEventFunction,
                          nsIDeviceContext *aContext,
                          nsIToolkit       *aToolkit,
                          nsWidgetInitData *aInitData) 
{
  nsString title("Load");
  Create(aParent, title, eMode_load, aContext, aToolkit, aInitData);
}


//-------------------------------------------------------------------------
void   nsFileWidget:: Create(nsIWidget  *aParent,
                             nsString&   aTitle,
                             nsMode      aMode,
                             nsIDeviceContext *aContext,
                             nsIToolkit *aToolkit,
                             void       *aInitData)
{
  //mWnd = (aParent) ? aParent->GetNativeData(NS_NATIVE_WINDOW) : 0;
  mTitle.SetLength(0);
  mTitle.Append(aTitle);
  mMode = aMode;

  Widget parentWidget = nsnull;

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aInitData ;
  }

  if (DBG) fprintf(stderr, "Parent 0x%x\n", parentWidget);

  mWidget = XmCreateFileSelectionDialog(parentWidget, "filesb", NULL, 0);
  //XtVaSetValues(mWidget, XmNdialogType, XmDIALOG_FULL_APPLICATION_MODAL, nsnull);

  XtAddCallback(mWidget, XmNcancelCallback, nsXtWidget_FSBCancel_Callback, this);
  XtAddCallback(mWidget, XmNokCallback, nsXtWidget_FSBOk_Callback, this);

  //XtManageChild(mWidget);
}

void nsFileWidget::Create(nsNativeWindow aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsFileWidget::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kIFileWidgetIID,    NS_IFILEWIDGET_IID);

  if (aIID.Equals(kIFileWidgetIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  return nsWindow::QueryInterface(aIID, aInstancePtr);
}


//-------------------------------------------------------------------------
//
// Ok's the dialog
//
//-------------------------------------------------------------------------
void nsFileWidget::OnOk()
{
  XtUnmanageChild(mWidget);
}

//-------------------------------------------------------------------------
//
// Cancel the dialog
//
//-------------------------------------------------------------------------
void nsFileWidget::OnCancel()
{
  XtUnmanageChild(mWidget);
}


//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
void nsFileWidget::Show(PRBool bState)
{
  nsresult result = nsEventStatus_eIgnore;
  XtManageChild(mWidget);

  XEvent event;
  for (;;) {
    XtAppNextEvent(gAppContext, &event);
    XtDispatchEvent(&event);
    printf("%d\n", event.type);
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
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
}

#define GET_OUTER() ((nsFileWidget*) ((char*)this - nsFileWidget::GetOuterOffset()))

//----------------------------------------------------------------------

BASE_IWIDGET_IMPL_NO_SHOW(nsFileWidget, AggFileWidget);

void nsFileWidget::AggFileWidget::Create( nsIWidget *aParent,
                                      nsString& aTitle,
                                      nsMode aMode,
                                      nsIDeviceContext *aContext,
                                      nsIToolkit *aToolkit,
                                      void *aInitData)
{
  GET_OUTER()->Create(aParent, aTitle, aMode, aContext, aToolkit, aInitData);
}

void nsFileWidget::AggFileWidget::OnOk()
{
  GET_OUTER()->OnOk();
}

void nsFileWidget::AggFileWidget::OnCancel()
{
  GET_OUTER()->OnCancel();
}

void nsFileWidget::AggFileWidget::Show(PRBool bState)
{
  GET_OUTER()->Show(bState);
}

void nsFileWidget::AggFileWidget::GetFile(nsString& aFile)
{
  GET_OUTER()->GetFile(aFile);
}

void nsFileWidget::AggFileWidget::SetFilterList(PRUint32 aNumberOfFilters,
                                                const nsString aTitles[],
                                                const nsString aFilters[])
{
  GET_OUTER()->SetFilterList(aNumberOfFilters, aTitles, aFilters);
}

PRBool nsFileWidget::AggFileWidget::Show()
{
  GET_OUTER()->Show(PR_TRUE);
  return PR_TRUE;
}


