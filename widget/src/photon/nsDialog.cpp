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

#include "nsPhWidgetLog.h"
#include "nsDialog.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <Pt.h>

#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsStringUtil.h"
#include "nsGfxCIID.h"
//#include "resource.h"


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
// nsDialog destructor
//
//-------------------------------------------------------------------------
nsDialog::~nsDialog()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsDialog::~nsDialog - Not Implemented.\n"));
}

//-------------------------------------------------------------------------
//
// Create the native Photon Dialog widget (just a window)
//
//-------------------------------------------------------------------------

NS_METHOD nsDialog::CreateNative( PtWidget_t *parentWindow )
{
  nsWindow::CreateNative( parentWindow );
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsDialog::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kInsDialogIID, NS_IDIALOG_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kInsDialogIID)) {
      *aInstancePtr = (void*) ((nsIDialog*)this);
      AddRef();
      result = NS_OK;
  }

  return result;
}


//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsDialog::SetLabel(const nsString& aText)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsDialog::SetLabel - Not Implemented.\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsDialog::GetLabel(nsString& aBuffer)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsDialog::GetLabel - Not Implemented.\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsDialog::OnPaint(nsPaintEvent &aEvent)
{
  return PR_FALSE;
}

PRBool nsDialog::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}


