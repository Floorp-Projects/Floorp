/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "nsClipboard.h"

// XXXX #include "nsDataObj.h"
#include "nsISupportsArray.h"
#include "nsIClipboardOwner.h"
#include "nsIDataFlavor.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"

// interface definitions
static NS_DEFINE_IID(kIDataFlavorIID,    NS_IDATAFLAVOR_IID);

static NS_DEFINE_IID(kIWidgetIID,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kWindowCID,         NS_WINDOW_CID);

NS_IMPL_ADDREF_INHERITED(nsClipboard, nsBaseClipboard)
NS_IMPL_RELEASE_INHERITED(nsClipboard, nsBaseClipboard)

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
  printf("nsClipboard::nsClipboard()\n");

  //NS_INIT_REFCNT();
  mIgnoreEmptyNotification = PR_FALSE;
  mWindow         = nsnull;
  mClipboardOwner = nsnull;
  mTransferable   = nsnull;

  // Create a Native window for the shell container...
  //nsresult rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID, (void**)&mWindow);
  //mWindow->Show(PR_FALSE);
  //mWindow->Resize(1,1,PR_FALSE);
}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
  printf("nsClipboard::~nsClipboard()\n");  

  NS_IF_RELEASE(mWindow);
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsClipboard::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  printf("nsClipboard::QueryInterface()\n");

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  static NS_DEFINE_IID(kIClipboard, NS_ICLIPBOARD_IID);
  if (aIID.Equals(kIClipboard)) {
    *aInstancePtr = (void*) ((nsIClipboard*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::SetNativeClipboardData()
{
  printf("nsClipboard::SetNativeClipboardData()\n");
  
  mIgnoreEmptyNotification = PR_TRUE;

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  



  return NS_OK;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::GetNativeClipboardData(nsITransferable * aTransferable)
{
  printf("nsClipboard::GetNativeClipboardData()");

  // make sure we have a good transferable
  if (nsnull == aTransferable) {
    return NS_ERROR_FAILURE;
  }
  
  // aTransferable->GetTransferData(flavor, data, length);
  // copy data to nsString.

  return NS_OK;
}


/**
  * No-op.
  *
  */
NS_IMETHODIMP nsClipboard::ForceDataToClipboard()
{
  printf("nsClipboard::ForceDataToClipboard()\n");

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}



void nsClipboard::SetTopLevelWidget(GtkWidget* w)
{
  // Don't set up any more event handlers if we're being called twice
  // for the same toplevel widget
  if (mWidget == w)
    return;

  mWidget = w;

  // Respond to requests for the selection:
  gtk_signal_connect(GTK_OBJECT(mWidget), "selection_get",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionRequestCB),
                     0);

  // When someone else takes the selection away:
  gtk_signal_connect(GTK_OBJECT(mWidget), "selection_clear_event",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionClearCB),
                     0);

  // Set up the paste handler:
  gtk_signal_connect(GTK_OBJECT(mWidget), "selection_received",
                     GTK_SIGNAL_FUNC(nsClipboard::SelectionReceivedCB),
                     0);

  // Hmm, sometimes we need this, sometimes not.  I'm not clear why:
  // Register all the target types we handle:
  gtk_selection_add_target(mWidget, GDK_SELECTION_PRIMARY,
                           GDK_SELECTION_TYPE_STRING,
			   GDK_SELECTION_TYPE_STRING);
  // Need to add entries for whatever it is that emacs uses
  // Need to add entries for XIF and HTML

}
