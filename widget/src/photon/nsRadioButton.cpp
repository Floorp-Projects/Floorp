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

#include "nsRadioButton.h"
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


NS_IMPL_ADDREF(nsRadioButton)
NS_IMPL_RELEASE(nsRadioButton)

//-------------------------------------------------------------------------
//
// nsRadioButton constructor
//
//-------------------------------------------------------------------------
nsRadioButton::nsRadioButton() : nsWidget(), nsIRadioButton()
{
  NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// nsRadioButton destructor
//
//-------------------------------------------------------------------------
nsRadioButton::~nsRadioButton()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsRadioButton::~nsRadioButton - Not Implemented!\n"));
}

nsresult nsRadioButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsRadioButton:QueryInterface, mWidget=%p\n", mWidget));

    if (NULL == aInstancePtr)
	{
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kIRadioButton, NS_IRADIOBUTTON_IID);
    if (aIID.Equals(kIRadioButton)) {
        *aInstancePtr = (void*) ((nsIRadioButton*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return nsWidget::QueryInterface(aIID,aInstancePtr);
}

//-------------------------------------------------------------------------
//
// Set the RadioButton State
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::SetState(const PRBool aState)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsRadioButton:SetState, mWidget=%p new state is <%d>\n", mWidget, aState));
  nsresult res = NS_ERROR_FAILURE;

  mState = aState;
  if (mWidget)
  {
    PtArg_t arg;

    if (mState)
      PtSetArg( &arg, Pt_ARG_FLAGS, Pt_SET, Pt_SET );
    else
      PtSetArg( &arg, Pt_ARG_FLAGS, 0, Pt_SET );
	
    if( PtSetResources( mWidget, 1, &arg ) == 0 )
      res = NS_OK;  
  }

  return res;  
}

//-------------------------------------------------------------------------
//
// Get the RadioButton State
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::GetState(PRBool & aState)
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsRadioButton:GetState, mWidget=%p\n", mWidget));
  nsresult res = NS_ERROR_FAILURE;

  if (mWidget)
  {
    PtArg_t arg;
    long    *flags;

    PtSetArg( &arg, Pt_ARG_FLAGS, &flags, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      if( *flags & Pt_SET )
        mState = PR_TRUE;
      else
        mState = PR_FALSE;

      res = NS_OK;
    }
  }

  aState = mState;

  return res;  
}

//-------------------------------------------------------------------------
//
// Set this button label
//
//-------------------------------------------------------------------------
NS_METHOD nsRadioButton::SetLabel(const nsString& aText)
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsRadioButton:SetLabel, mWidget=%p\n", mWidget));
  if( mWidget )
  {
    PtArg_t arg;
    
    NS_ALLOC_STR_BUF(label, aText, aText.Length());

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
NS_METHOD nsRadioButton::GetLabel(nsString& aBuffer)
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsRadioButton::GetLabel\n"));

  aBuffer.SetLength(0);

  if( mWidget )
  {
    PtArg_t arg;
    char    *label;    

    PtSetArg( &arg, Pt_ARG_TEXT_STRING, &label, 0 );
    if( PtGetResources( mWidget, 1, &arg ) == 0 )
    {
      aBuffer.Append( label );
      res = NS_OK;
    }
  }

  return res;
}

//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsRadioButton::OnMove(PRInt32, PRInt32)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsRadioButton::OnMove - Not Implemented\n"));
  return PR_FALSE;
}

PRBool nsRadioButton::OnPaint()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsRadioButton::OnPaint - Not Implemented\n"));
  return PR_FALSE;
}

PRBool nsRadioButton::OnResize(nsRect &aWindowRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsRadioButton::OnResize - Not Implemented\n"));
  return PR_FALSE;
}


/**
 * Renders the RadioButton for Printing
 *
 **/
NS_METHOD nsRadioButton::Paint(nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsRadioButton::Paint - Not Implemented\n"));
  return NS_OK;
}


NS_METHOD nsRadioButton::CreateNative( PtWidget_t* aParent )
{
  nsresult  res = NS_ERROR_FAILURE;
  PtArg_t   arg[15];
  PhPoint_t pos;
  PhDim_t   dim;

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsRadioButton::CreateNative\n"));

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width;
  dim.h = mBounds.height;

  PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
  PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
  PtSetArg( &arg[2], Pt_ARG_BORDER_WIDTH, 0, 0 );
  PtSetArg( &arg[3], Pt_ARG_INDICATOR_TYPE, Pt_RADIO, 0 );
  PtSetArg( &arg[4], Pt_ARG_INDICATOR_COLOR, Pg_BLACK, 0 );
  PtSetArg( &arg[5], Pt_ARG_SPACING, 0, 0 );
  PtSetArg( &arg[6], Pt_ARG_MARGIN_TOP, 0, 0 );
  PtSetArg( &arg[7], Pt_ARG_MARGIN_LEFT, 0, 0 );
  PtSetArg( &arg[8], Pt_ARG_MARGIN_BOTTOM, 0, 0 );
  PtSetArg( &arg[9], Pt_ARG_MARGIN_RIGHT, 0, 0 );
  PtSetArg( &arg[10], Pt_ARG_MARGIN_WIDTH, 0, 0 );
  PtSetArg( &arg[11], Pt_ARG_MARGIN_HEIGHT, 0, 0 );
  PtSetArg( &arg[12], Pt_ARG_VERTICAL_ALIGNMENT, Pt_TOP, 0 );
//  PtSetArg( &arg[13], Pt_ARG_TEXT_FONT, "helv08", 0 );

  mWidget = PtCreateWidget( PtToggleButton, aParent, 13, arg );
  if( mWidget )
  {
    PtAddEventHandler( mWidget,
      Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY,
      RawEventHandler, this );

    res = NS_OK;
  }

  return res;  
}

