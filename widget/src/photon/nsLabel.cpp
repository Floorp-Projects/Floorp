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

#include "nsLabel.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include "nsStringUtil.h"
#include <Pt.h>

#include "nsPhWidgetLog.h"

NS_IMPL_ADDREF_INHERITED(nsLabel, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsLabel, nsWidget)

//-------------------------------------------------------------------------
//
// nsLabel constructor
//
//-------------------------------------------------------------------------
nsLabel::nsLabel() : nsWidget(), nsILabel()
{
  NS_INIT_REFCNT();
  mAlignment = eAlign_Left;
}

//-------------------------------------------------------------------------
//
// nsLabel destructor
//
//-------------------------------------------------------------------------
nsLabel::~nsLabel()
{
}


//-------------------------------------------------------------------------
//
// Create the nativeLabel widget
//
//-------------------------------------------------------------------------
NS_METHOD  nsLabel::CreateNative( PtWidget_t *aParent )
{
  nsresult  res = NS_ERROR_FAILURE;
  PtArg_t   arg[5];
  PhPoint_t pos;
  PhDim_t   dim;
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsLabel::CreateNative at (%d,%d) for (%d,%d)\n",mBounds.x,mBounds.y, mBounds.width, mBounds.height));
  NS_PRECONDITION(aParent, "nsLabel::CreateNative aParent is NULL");

  pos.x = mBounds.x;
  pos.y = mBounds.y;
  dim.w = mBounds.width;
  dim.h = mBounds.height;

  PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
  PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
  PtSetArg( &arg[2], Pt_ARG_BORDER_WIDTH, 0, 0 );

  mWidget = PtCreateWidget( PtButton, aParent, 3, arg );
  if( mWidget )
  {
    res = NS_OK;
  }

  NS_POSTCONDITION(mWidget, "nsLabel::CreateNative Failed to create native label");

  return res;  
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    nsLabelInitData* data = (nsLabelInitData *) aInitData;
    mAlignment = data->mAlignment;
  }
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set alignment
//
//-------------------------------------------------------------------------
NS_METHOD nsLabel::SetAlignment(nsLabelAlignment aAlignment)
{
  mAlignment = aAlignment;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsLabel::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);

  static NS_DEFINE_IID(kILabelIID, NS_ILABEL_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kILabelIID)) {
      *aInstancePtr = (void*) ((nsILabel*)this);
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
NS_METHOD nsLabel::SetLabel(const nsString& aText)
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsLabel:SetLabel\n"));

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
NS_METHOD nsLabel::GetLabel(nsString& aBuffer)
{
  nsresult res = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_DEBUG,("nsLabel:GetLabel\n"));

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
// paint message. Don't send the paint out
//
//-------------------------------------------------------------------------
PRBool nsLabel::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

PRBool nsLabel::OnPaint(nsPaintEvent &aEvent)
{
  //printf("** nsLabel::OnPaint **\n");
  return PR_FALSE;
}

PRBool nsLabel::OnResize(nsSizeEvent &aEvent)
{
    return PR_FALSE;
}
