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

#include "nsButton.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <Pt.h>

#include "nsILookAndFeel.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"

#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"

#include "nsPhWidgetLog.h"

static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);


NS_IMPL_ADDREF(nsButton)
NS_IMPL_RELEASE(nsButton)

//-------------------------------------------------------------------------
//
// nsButton constructor
//
//-------------------------------------------------------------------------
nsButton::nsButton() : nsWidget(), nsIButton()
{
  NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// nsButton destructor
//
//-------------------------------------------------------------------------
nsButton::~nsButton()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsButton::~nsButton - Not Implemented!\n"));
}

/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @modify gpk 8/4/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsButton:QueryInterface, mWidget=%p\n", mWidget));

    if (NULL == aInstancePtr)
	{
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kIButton, NS_IBUTTON_IID);
    if (aIID.Equals(kIButton)) {
        *aInstancePtr = (void*) ((nsIButton*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return nsWidget::QueryInterface(aIID,aInstancePtr);
}


//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsButton::SetLabel(const nsString& aText)
{
  nsresult res = NS_ERROR_FAILURE;

  mLabel = aText;

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsButton:SetLabel, mWidget=%p\n", mWidget));
  if( mWidget )
  {
    PtArg_t arg;
    
    NS_ALLOC_STR_BUF(label, aText, 256);

    PtSetArg( &arg, Pt_ARG_TEXT_STRING, label, 0 );
    if( PtSetResources( mWidget, 1, &arg ) == 0 )
      res = NS_OK;

    NS_FREE_STR_BUF(label);
  }

  return res;
}

//-------------------------------------------------------------------------
//
// Get this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsButton::GetLabel(nsString& aBuffer)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsButton::GetLabel\n"));

  aBuffer = mLabel;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsButton::OnMove(PRInt32, PRInt32)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsButton::OnMove - Not Implemented\n"));
  return PR_FALSE;
}

PRBool nsButton::OnPaint()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsButton::OnPaint - Not Implemented\n"));
  return PR_FALSE;
}

PRBool nsButton::OnResize(nsRect &aWindowRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsButton::OnResize - Not Implemented\n"));
  return PR_FALSE;
}


/**
 * Renders the Button for Printing
 *
 **/
NS_METHOD nsButton::Paint(nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsButton::Paint - Not Implemented\n"));
  return NS_OK;
}


NS_METHOD nsButton::CreateNative( PtWidget_t* aParent )
{
  nsresult  res = NS_ERROR_FAILURE;
  PtArg_t   arg[5];
  PhPoint_t pos;
  PhDim_t   dim;
  const unsigned short BorderWidth = 2;
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsButton::CreateNative at (%d,%d) for (%d,%d)\n",mBounds.x,mBounds.y, mBounds.width, mBounds.height));

  NS_PRECONDITION(aParent, "nsButton::CreateNative aParent is NULL");

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width - (2*BorderWidth); // Correct for border width
  dim.h = mBounds.height - (2*BorderWidth);

  PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
  PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
  PtSetArg( &arg[2], Pt_ARG_BORDER_WIDTH, BorderWidth, 0 );

  mWidget = PtCreateWidget( PtButton, aParent, 3, arg );
  if( mWidget )
  {
    PtAddEventHandler( mWidget,
      Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY,
      RawEventHandler, this );

    res = NS_OK;

    /* Add an Activate Callback */
//    PtAddCallback(mWidget, Pt_CB_ACTIVATE, handle_activate_event, this);
  }

  NS_POSTCONDITION(mWidget, "nsButton::CreateNative Failed to create Native Button");

  return res;  
}

#if 0
int nsButton::handle_activate_event (PtWidget_t *aWidget, void *aData, PtCallbackInfo_t *aCbinfo )
{
  nsButton *me = (nsButton *) aData;

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsButton::handle_activate_event widget=<%p>\n",me));

  if (me)
  {
    nsGUIEvent event;

    event.widget = me;
    NS_IF_ADDREF(event.widget);
    event.time = 0; //gdk_event_get_time((GdkEvent*)ge);
    event.message = NS_MOUSE_LEFT_BUTTON_UP;
	
    event.point.x = 0;
    event.point.y = 0;
    event.eventStructType = NS_GUI_EVENT;

    PRBool result = me->DispatchWindowEvent(&event);
  }
  
  return (Pt_CONTINUE);
}
#endif
  