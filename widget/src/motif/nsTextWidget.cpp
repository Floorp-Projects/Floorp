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

#include "nsTextWidget.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"

#include <Xm/Text.h>

#define DBG 0

//-------------------------------------------------------------------------
//
// nsTextWidget constructor
//
//-------------------------------------------------------------------------
nsTextWidget::nsTextWidget(nsISupports *aOuter) : nsTextHelper(aOuter),
  mIsPasswordCallBacksInstalled(PR_FALSE)
{
  //mBackground = NS_RGB(124, 124, 124);
}

//-------------------------------------------------------------------------
//
// nsTextWidget destructor
//
//-------------------------------------------------------------------------
nsTextWidget::~nsTextWidget()
{
}

//-------------------------------------------------------------------------
void nsButton::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
  Widget parentWidget = nsnull;

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);

  if (aParent) {
    parentWidget = (Widget) aParent->GetNativeData(NS_NATIVE_WIDGET);
  } else {
    parentWidget = (Widget) aInitData ;
  }

  if (DBG) fprintf(stderr, "Parent 0x%x\n", parentWidget);

  mWidget = ::XtVaCreateManagedWidget("button",
                                    xmTextWidgetClass, 
                                    parentWidget,
                                    XmNwidth, aRect.width,
                                    XmNheight, aRect.height,
                                    XmNrecomputeSize, False,
                                    XmNhighlightOnEnter, False,
		                                XmNx, aRect.x,
		                                XmNy, aRect.y, 
                                    nsnull);

  if (DBG) fprintf(stderr, "Button 0x%x  this 0x%x\n", mWidget, this);

  // save the event callback function
  mEventCallback = aHandleEventFunction;

  InitCallbacks();

}

//-------------------------------------------------------------------------
void nsButton::Create(nsNativeWindow aParent,
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
nsresult nsTextWidget::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);

  if (aIID.Equals(kITextWidgetIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  return nsWindow::QueryInterface(aIID, aInstancePtr);
}


//-------------------------------------------------------------------------
//
// move, paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextWidget::OnMove(PRInt32, PRInt32)
{
  return PR_FALSE;
}

//--------------------------------------------------------------
PRBool nsTextWidget::OnPaint()
{
  return PR_FALSE;
}


//--------------------------------------------------------------
PRBool nsTextWidget::OnResize(nsRect &aWindowRect)
{
  return PR_FALSE;
}


//--------------------------------------------------------------
#define GET_OUTER() ((nsButton*) ((char*)this - nsButton::GetOuterOffset()))


//--------------------------------------------------------------
void TextWidget::AggTextWidget::SetMaxTextLength(PRUint32 aChars)
{
  GET_OUTER()->SetMaxTextLength(aChars);
}

//--------------------------------------------------------------
PRUint32  TextWidget::AggTextWidget::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) {
  return GET_OUTER()->GetText(aTextBuffer, aBufferSize);
}

//--------------------------------------------------------------
PRUint32  TextWidget::AggTextWidget::SetText(const nsString& aText)
{ 
  return GET_OUTER()->SetText(aText);
}

//--------------------------------------------------------------
PRUint32  TextWidget::AggTextWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos)
{ 
  return GET_OUTER()->InsertText(aText, aStartPos, aEndPos);
}

//--------------------------------------------------------------
void  TextWidget::AggTextWidget::RemoveText()
{
  GET_OUTER()->RemoveText();
}

//--------------------------------------------------------------
void  TextWidget::AggTextWidget::SetPassword(PRBool aIsPassword)
{
  GET_OUTER()->SetPassword(aIsPassword);
  if (aIsPassword) {
    if (!mIsPasswordCallBacksInstalled) {
      XtAddCallback(mWidget, XmNmodifyVerifyCallback, nsXtWidget_Text_Callback, NULL);
      XtAddCallback(mWidget, XmNactivateCallback,     nsXtWidget_Text_Callback, NULL);
      mIsPasswordCallBacksInstalled = PR_TRUE;
    }
  } else {
    if (mIsPasswordCallBacksInstalled) {
      XtRemoveCallback(mWidget, XmNmodifyVerifyCallback, nsXtWidget_Text_Callback, NULL);
      XtRemoveCallback(mWidget, XmNactivateCallback,     nsXtWidget_Text_Callback, NULL);
      mIsPasswordCallBacksInstalled = PR_FALSE;
    }
  }
}

//--------------------------------------------------------------
PRBool  TextWidget::AggTextWidget::SetReadOnly(PRBool aReadOnlyFlag)
{
  GET_OUTER()->SetReadOnly(aReadOnlyFlag);
}

//--------------------------------------------------------------
void TextWidget::AggTextWidget::SelectAll()
{
  GET_OUTER()->SelectAll();
}


//--------------------------------------------------------------
void  TextWidget::AggTextWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  GET_OUTER()->SetSelection(aStartSel, aEndSel);
}


//--------------------------------------------------------------
void  TextWidget::AggTextWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  GET_OUTER()->GetSelection(aStartSel, aEndSel);
}

//--------------------------------------------------------------
void  TextWidget::AggTextWidget::SetCaretPosition(PRUint32 aPosition)
{
  GET_OUTER()->SetCaretPosition(aPosition);
}

//--------------------------------------------------------------
PRUint32  TextWidget::AggTextWidget::GetCaretPosition()
{
  return GET_OUTER()->GetCaretPosition();
}

PRBool TextWidget::AggTextWidget::AutoErase()
{
  return GET_OUTER()->AutoErase();
}


//----------------------------------------------------------------------

BASE_IWIDGET_IMPL(nsTextWidget, AggTextWidget);

