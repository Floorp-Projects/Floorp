/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "nsDialog.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"

#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsStringUtil.h"
#include "nsGfxCIID.h"
#include "resource.h"
#include "nslog.h"

NS_IMPL_LOG(nsDialogLog)
#define PRINTF NS_LOG_PRINTF(nsDialogLog)
#define FLUSH  NS_LOG_FLUSH(nsDialogLog)

NS_IMPL_ADDREF(nsDialog)
NS_IMPL_RELEASE(nsDialog)

//-------------------------------------------------------------------------
//
// nsDialog constructor
//
//-------------------------------------------------------------------------
nsDialog::nsDialog() : nsWindow(), nsIDialog()
{
  NS_INIT_REFCNT();
}


//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsDialog::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return nsWindow::Create(aParent,aRect,aHandleEventFunction,aContext,aAppShell,aToolkit,aInitData);
  
}


//-------------------------------------------------------------------------
//
// nsDialog destructor
//
//-------------------------------------------------------------------------
nsDialog::~nsDialog()
{
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsDialog::SetLabel(const nsString& aText)
{
#if 0
  NS_ALLOC_STR_BUF(label, aText, 256);
  VERIFY(::SetWindowText(mWnd, label));
  NS_FREE_STR_BUF(label);
#endif
  PRINTF("nsDialog::SetLabel not implemented\n");
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsDialog::GetLabel(nsString& aBuffer)
{
#if 0
  int actualSize = ::GetWindowTextLength(mWnd)+1;
  NS_ALLOC_CHAR_BUF(label, 256, actualSize);
  ::GetWindowText(mWnd, label, actualSize);
  aBuffer.SetLength(0);
  aBuffer.Append(label);
  NS_FREE_CHAR_BUF(label);
#endif
  PRINTF("nsDialog::SetLabel not implemented\n");
  return NS_OK;
}



//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsDialog::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWindow::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kInsDialogIID, NS_IDIALOG_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kInsDialogIID)) {
      *aInstancePtr = (void*) ((nsIDialog*)this);
      NS_ADDREF_THIS();
      result = NS_OK;
  }

  return result;
}


//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsDialog::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsDialog::OnPaint(nsRect &r)
{
  return nsWindow::OnPaint(r);
}

PRBool nsDialog::OnResize(nsRect &aWindowRect)
{
  return nsWindow::OnResize(aWindowRect);
}

//-------------------------------------------------------------------------
//
// get position/dimensions
//
//-------------------------------------------------------------------------

NS_METHOD nsDialog::GetBounds(nsRect &aRect)
{
  return nsWindow::GetBounds(aRect);
}

