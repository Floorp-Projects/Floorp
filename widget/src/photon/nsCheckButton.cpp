/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCheckButton.h"
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


NS_IMPL_ADDREF(nsCheckButton)
NS_IMPL_RELEASE(nsCheckButton)

//-------------------------------------------------------------------------
//
// nsCheckButton constructor
//
//-------------------------------------------------------------------------
nsCheckButton::nsCheckButton() : nsWidget(), nsICheckButton(),
  mState(PR_FALSE)
{
  NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// nsCheckButton destructor
//
//-------------------------------------------------------------------------
nsCheckButton::~nsCheckButton()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsCheckButton::~nsCheckButton - Not Implemented!\n"));
}

/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @modify gpk 8/4/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsCheckButton::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsCheckButton:QueryInterface, mWidget=%p\n", mWidget));

    if (NULL == aInstancePtr)
	{
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kICheckButton, NS_ICHECKBUTTON_IID);
    if (aIID.Equals(kICheckButton)) {
        *aInstancePtr = (void*) ((nsICheckButton*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return nsWidget::QueryInterface(aIID,aInstancePtr);
}

//-------------------------------------------------------------------------
//
// Set the CheckButton State
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::SetState(const PRBool aState)
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsCheckButton:SetState, mWidget=%p new state is <%d>\n", mWidget, aState));
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
// Get the CheckButton State
//
//-------------------------------------------------------------------------
NS_METHOD nsCheckButton::GetState(PRBool & aState)
{
//  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsCheckButton:GetState, mWidget=%p\n", mWidget));
  nsresult res = NS_ERROR_FAILURE;

  if (mWidget)
  {
    PtArg_t arg;
    long    *flags;
	
    PtSetArg( &arg, Pt_ARG_FLAGS, &flags, 0);
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
NS_METHOD nsCheckButton::SetLabel(const nsString& aText)
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsCheckButton:SetLabel, mWidget=%p\n", mWidget));
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
NS_METHOD nsCheckButton::GetLabel(nsString& aBuffer)
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsCheckButton::GetLabel\n"));

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
PRBool nsCheckButton::OnMove(PRInt32, PRInt32)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsCheckButton::OnMove - Not Implemented\n"));
  return PR_FALSE;
}

PRBool nsCheckButton::OnPaint()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsCheckButton::OnPaint - Not Implemented\n"));
  return PR_FALSE;
}

PRBool nsCheckButton::OnResize(nsRect &aWindowRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsCheckButton::OnResize - Not Implemented\n"));
  return PR_FALSE;
}


/**
 * Renders the CheckButton for Printing
 *
 **/
NS_METHOD nsCheckButton::Paint(nsIRenderingContext& aRenderingContext,
                          const nsRect& aDirtyRect)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsCheckButton::Paint - Not Implemented\n"));
  return NS_OK;
}


NS_METHOD nsCheckButton::CreateNative( PtWidget_t* aParent )
{
  nsresult  res = NS_ERROR_FAILURE;
  PtArg_t   arg[15];
  PhPoint_t pos;
  PhDim_t   dim;

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsCheckButton::CreateNative this=<%p>\n", this));

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width; // Correct for border width
  dim.h = mBounds.height;

  PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
  PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
  PtSetArg( &arg[2], Pt_ARG_BORDER_WIDTH, 0, 0 );
  PtSetArg( &arg[3], Pt_ARG_INDICATOR_TYPE, Pt_CHECK, 0 );
  PtSetArg( &arg[4], Pt_ARG_INDICATOR_COLOR, Pg_BLACK, 0 );
  PtSetArg( &arg[5], Pt_ARG_SPACING, 0, 0 );
  PtSetArg( &arg[6], Pt_ARG_MARGIN_TOP, 0, 0 );
  PtSetArg( &arg[7], Pt_ARG_MARGIN_LEFT, 0, 0 );
  PtSetArg( &arg[8], Pt_ARG_MARGIN_BOTTOM, 0, 0 );
  PtSetArg( &arg[9], Pt_ARG_MARGIN_RIGHT, 0, 0 );
  PtSetArg( &arg[10], Pt_ARG_MARGIN_WIDTH, 0, 0 );
  PtSetArg( &arg[11], Pt_ARG_MARGIN_HEIGHT, 0, 0 );
  PtSetArg( &arg[12], Pt_ARG_VERTICAL_ALIGNMENT, Pt_TOP, 0 );
  PtSetArg( &arg[13], Pt_ARG_FLAGS, 0, Pt_SELECTABLE );

  mWidget = PtCreateWidget( PtToggleButton, aParent, 14, arg );
  if( mWidget )
  {
    PtAddEventHandler( mWidget,
      Ph_EV_BUT_PRESS | Ph_EV_BUT_RELEASE |Ph_EV_BOUNDARY,
      RawEventHandler, this );

    res = NS_OK;
  }

  return res;  
}

  
