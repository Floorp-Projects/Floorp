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

#include "nsTextAreaWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"

#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"

#include <Pt.h>
#include "nsPhWidgetLog.h"

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

NS_IMPL_ADDREF(nsTextAreaWidget)
NS_IMPL_RELEASE(nsTextAreaWidget)


//-------------------------------------------------------------------------
//
// nsTextAreaWidget constructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::nsTextAreaWidget() : nsTextHelper()
{
  NS_INIT_REFCNT();
  mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextAreaWidget destructor
//
//-------------------------------------------------------------------------
nsTextAreaWidget::~nsTextAreaWidget()
{
}

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTextAreaWidget::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

    static NS_DEFINE_IID(kInsTextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);
    if (result == NS_NOINTERFACE && aIID.Equals(kInsTextAreaWidgetIID)) {
        *aInstancePtr = (void*) ((nsITextAreaWidget*)this);
        NS_ADDREF_THIS();
        result = NS_OK;
    }

    return result;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsTextAreaWidget::OnPaint()
{
  return PR_FALSE;
}

// Not Implemented
PRBool nsTextAreaWidget::OnResize(nsRect &aWindowRect)
{
  return PR_FALSE;
}

NS_METHOD nsTextAreaWidget::CreateNative( PtWidget_t* aParent )
{
  nsresult  res = NS_ERROR_FAILURE;
  PtArg_t   arg[5];
  PhPoint_t pos;
  PhDim_t   dim;

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width - 4; // Correct for border width
  dim.h = mBounds.height - 4;

  PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
  PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
  PtSetArg( &arg[2], Pt_ARG_BORDER_WIDTH, 2, 0 );
  mWidget = PtCreateWidget( PtMultiText, aParent, 3, arg );
  if( mWidget )
  {
    res = NS_OK;
	PtAddEventHandler(mWidget, Ph_EV_KEY, RawEventHandler, this);

	/* Add an Activate Callback */
//    PtAddCallback(mWidget, Pt_CB_ACTIVATE, handle_activate_event, this);
  }

  return res;  
}

int nsTextAreaWidget::RawEventHandler(PtWidget_t *aWidget, void *aData, PtCallbackInfo_t *aCbInfo )
{
  nsTextAreaWidget *me = (nsTextAreaWidget *) aData;	/* Mozilla object that created the event */

  if( aCbInfo->reason == Pt_CB_RAW )
  {
    PhEvent_t* event = aCbInfo->event;

    switch( event->type )
    {
    case Ph_EV_KEY:
    {
      PhKeyEvent_t* keyev = (PhKeyEvent_t*) PhGetData( event );
	  me->DispatchKeyEvent(keyev);
      break;
    }
    default:
	  break;	
    }
  } 

  return (Pt_CONTINUE);
}
