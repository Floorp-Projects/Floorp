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

#define DBG 0


//=================================================================
/*  Constructor
 *  @update  dc 09/10/98
 *  @param   aOuter -- nsISupports object
 *  @return  NONE
 */
nsTextWidget::nsTextWidget(nsISupports *aOuter): nsWindow(aOuter)
{
  mIsPasswordCallBacksInstalled = PR_FALSE;
  mMakeReadOnly=PR_FALSE;
  mMakePassword=PR_FALSE;
  mTE_Data = nsnull;
  //mBackground = NS_RGB(124, 124, 124);
}

//=================================================================
/*  Destructor
 *  @update  dc 09/10/98
 *  @param   NONE
 *  @return  NONE
 */
nsTextWidget::~nsTextWidget()
{
	if(mTE_Data!=nsnull)
		WEDispose(mTE_Data);
}

//=================================================================
/*  Function to create the TextWidget and all its neccisary data
 *  @update  dc 09/10/98
 *  @param   aParent --
 *  @param   aRect --
 *  @param   aHandleEventFunction --
 *  @param   aContext --
 *  @param   aAppShell --
 *  @param   aToolKit -- 
 *  @param   aInitData --  
 *  @return  NONE
 */
void nsTextWidget::Create(nsIWidget *aParent,const nsRect &aRect,EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,nsIAppShell *aAppShell,nsIToolkit *aToolkit, nsWidgetInitData *aInitData) 
{
LongRect		destRect,viewRect;
PRUint32		teFlags=0;
GrafPtr			curport;


  mParent = aParent;
  aParent->AddChild(this);

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);
	
	WindowPtr window = nsnull;

  if (aParent) 
    window = (WindowPtr) aParent->GetNativeData(NS_NATIVE_WIDGET);
	else 
		if (aAppShell)
    	window = (WindowPtr) aAppShell->GetNativeData(NS_NATIVE_SHELL);

  mIsMainWindow = PR_FALSE;
  mWindowMadeHere = PR_TRUE;
	mWindowRecord = (WindowRecord*)window;
	mWindowPtr = (WindowPtr)window;
  
  NS_ASSERTION(window!=nsnull,"The WindowPtr for the widget cannot be null")
	if (window)
		{
	  InitToolkit(aToolkit, aParent);

	  if (DBG) fprintf(stderr, "Parent 0x%x\n", window);

		// Set the bounds to the local rect
		SetBounds(aRect);
		
		mWindowRegion = NewRgn();
		SetRectRgn(mWindowRegion,aRect.x,aRect.y,aRect.x+aRect.width,aRect.y+aRect.height);		 


	  // save the event callback function
	  mEventCallback = aHandleEventFunction;
	  	  
	  // Initialize the TE record
	  viewRect.left = aRect.x;
	  viewRect.top = aRect.y;
	  viewRect.right = aRect.x+aRect.width;
	  viewRect.bottom = aRect.y+aRect.height;
	  destRect = viewRect;
	  ::GetPort(&curport);
	  ::SetPort(mWindowPtr);
		WENew(&destRect,&viewRect,teFlags,&mTE_Data);
		::SetPort(curport);
		
	  InitDeviceContext(mContext, (nsNativeWidget)mWindowPtr);
		}

}

//=================================================================
/*  Function to create the TextWidget and all its neccisary data
 *  @update  dc 09/10/98
 *  @param   aParent --
 *  @param   aRect --
 *  @param   aHandleEventFunction --
 *  @param   aContext --
 *  @param   aAppShell --
 *  @param   aToolKit -- 
 *  @param   aInitData --  
 *  @return  NONE
 */
void nsTextWidget::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
}


//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------
nsresult nsTextWidget::QueryObject(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);

  if (aIID.Equals(kITextWidgetIID)) {
    AddRef();
    *aInstancePtr = (void**) &mAggWidget;
    return NS_OK;
  }
  return nsWindow::QueryObject(aIID, aInstancePtr);
}


//-------------------------------------------------------------------------
//
// paint, resizes message - ignore
//
//-------------------------------------------------------------------------
PRBool nsTextWidget::OnPaint(nsPaintEvent & aEvent)
{
nsRect							therect;
Rect								macrect;
GrafPtr							theport;
RGBColor						blackcolor = {0,0,0};
RgnHandle						thergn;
	
	::GetPort(&theport);
	::SetPort(mWindowPtr);
	GetBounds(therect);
	nsRectToMacRect(therect,macrect);
	thergn = ::NewRgn();
	::GetClip(thergn);
	::ClipRect(&macrect);
	//::EraseRoundRect(&macrect,10,10);
	//::PenSize(1,1);
	//::FrameRoundRect(&macrect,10,10); 

	WEActivate(mTE_Data);
	WEUpdate(nsnull,mTE_Data);
	::PenSize(1,1);
	::SetClip(thergn);
	::SetPort(theport);
	
  return PR_FALSE;
}

//--------------------------------------------------------------
void nsTextWidget::PrimitiveKeyDown(PRInt16	aKey,PRInt16 aModifiers)
{
PRBool 	result=PR_TRUE;
GrafPtr				theport;
RgnHandle			thergn;
nsRect				therect;
Rect					macrect;

	::GetPort(&theport);
	::SetPort(mWindowPtr);
	GetBounds(therect);
	nsRectToMacRect(therect,macrect);
	thergn = ::NewRgn();
	::GetClip(thergn);
	::ClipRect(&macrect);
	WEKey(aKey,aModifiers,mTE_Data);
	::SetClip(thergn);
	::SetPort(theport);
	
}

//--------------------------------------------------------------


PRBool 
nsTextWidget::DispatchMouseEvent(nsMouseEvent &aEvent)
{
PRBool 	result=PR_TRUE;
Point		mouseLoc;
PRInt16 modifiers=0;
nsRect	therect;
Rect		macrect;
	
	switch (aEvent.message)
		{
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			::SetPort(mWindowPtr);
			GetBounds(therect);
			nsRectToMacRect(therect,macrect);
			::ClipRect(&macrect);
			mouseLoc.h = aEvent.point.x;
			mouseLoc.v = aEvent.point.y;
			WEClick(mouseLoc,modifiers,aEvent.time,mTE_Data);
			result = PR_FALSE;
			break;
		case NS_MOUSE_LEFT_BUTTON_UP:
			break;
		case NS_MOUSE_EXIT:
			break;
		case NS_MOUSE_ENTER:
			break;
		}
	return result;
}

//--------------------------------------------------------------
PRBool nsTextWidget::OnResize(nsSizeEvent &aEvent)
{
  return PR_FALSE;
}

//--------------------------------------------------------------
void nsTextWidget::SetPassword(PRBool aIsPassword)
{
  if ( aIsPassword) {
    mMakePassword = PR_TRUE;
    return;
  }

  if (aIsPassword) 
  	{
    if (!mIsPasswordCallBacksInstalled) 
    	{
      mIsPasswordCallBacksInstalled = PR_TRUE;
    	}
  	}
 	else 
 		{
    if (mIsPasswordCallBacksInstalled) 
    	{
      mIsPasswordCallBacksInstalled = PR_FALSE;
    	}
  	}
  //mHelper->SetPassword(aIsPassword);
}

//--------------------------------------------------------------
PRBool  nsTextWidget::SetReadOnly(PRBool aReadOnlyFlag)
{
  if ( aReadOnlyFlag) 
  	{
    mMakeReadOnly = PR_TRUE;
    return PR_TRUE;
  	}
  	
}

//--------------------------------------------------------------
void nsTextWidget::SetMaxTextLength(PRUint32 aChars)
{
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) 
{
Handle				thetext;
PRInt32 			len,i;
char					*str;

  thetext = WEGetText(mTE_Data);
  len = WEGetTextLength(mTE_Data);
  
  HLock(thetext);
  str = new char[len];
  for(i=0;i<len;i++)
  	str[i] = (*thetext)[i];
  HUnlock(thetext);	
  
  aTextBuffer.SetLength(0);
  aTextBuffer.Append(str);
	delete str;
	return aTextBuffer.Length();
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::SetText(const nsString& aText)
{ 
char buffer[256];
PRInt32 len;

	this->RemoveText();
	aText.ToCString(buffer,255);
	len = strlen(buffer);

	WEInsert(buffer,len,0,0,mTE_Data);

	return len;
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos)
{ 
char buffer[256];
PRInt32 len;

	aText.ToCString(buffer,255);
	len = strlen(buffer);
	
	WEInsert(buffer,len,0,0,mTE_Data);
	return(len);
}

//--------------------------------------------------------------
void  nsTextWidget::RemoveText()
{
	WESetSelection(0, 32000,mTE_Data);
	WEDelete(mTE_Data);
}

//--------------------------------------------------------------
void nsTextWidget::SelectAll()
{
	WESetSelection(0, 32000,mTE_Data);
}


//--------------------------------------------------------------
void  nsTextWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  WESetSelection(aStartSel, aEndSel,mTE_Data);
}


//--------------------------------------------------------------
void  nsTextWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
	WEGetSelection((long*)aStartSel,(long*)aEndSel,mTE_Data);
}

//--------------------------------------------------------------
void  nsTextWidget::SetCaretPosition(PRUint32 aPosition)
{
  //mHelper->SetCaretPosition(aPosition);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::GetCaretPosition()
{
  //return mHelper->GetCaretPosition();
}

//--------------------------------------------------------------
PRBool nsTextWidget::AutoErase()
{
  //return mHelper->AutoErase();
}



//--------------------------------------------------------------
#define GET_OUTER() ((nsTextWidget*) ((char*)this - nsTextWidget::GetOuterOffset()))


//--------------------------------------------------------------
void nsTextWidget::AggTextWidget::SetMaxTextLength(PRUint32 aChars)
{
  GET_OUTER()->SetMaxTextLength(aChars);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::AggTextWidget::GetText(nsString& aTextBuffer, PRUint32 aBufferSize) {
  return GET_OUTER()->GetText(aTextBuffer, aBufferSize);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::AggTextWidget::SetText(const nsString& aText)
{ 
  return GET_OUTER()->SetText(aText);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::AggTextWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos)
{ 
  return GET_OUTER()->InsertText(aText, aStartPos, aEndPos);
}

//--------------------------------------------------------------
void  nsTextWidget::AggTextWidget::RemoveText()
{
  GET_OUTER()->RemoveText();
}

//--------------------------------------------------------------
void  nsTextWidget::AggTextWidget::SetPassword(PRBool aIsPassword)
{
  GET_OUTER()->SetPassword(aIsPassword);
}

//--------------------------------------------------------------
PRBool  nsTextWidget::AggTextWidget::SetReadOnly(PRBool aReadOnlyFlag)
{
  return GET_OUTER()->SetReadOnly(aReadOnlyFlag);
}

//--------------------------------------------------------------
void nsTextWidget::AggTextWidget::SelectAll()
{
  GET_OUTER()->SelectAll();
}


//--------------------------------------------------------------
void  nsTextWidget::AggTextWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  GET_OUTER()->SetSelection(aStartSel, aEndSel);
}


//--------------------------------------------------------------
void  nsTextWidget::AggTextWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
  GET_OUTER()->GetSelection(aStartSel, aEndSel);
}

//--------------------------------------------------------------
void  nsTextWidget::AggTextWidget::SetCaretPosition(PRUint32 aPosition)
{
  GET_OUTER()->SetCaretPosition(aPosition);
}

//--------------------------------------------------------------
PRUint32  nsTextWidget::AggTextWidget::GetCaretPosition()
{
  return GET_OUTER()->GetCaretPosition();
}

PRBool nsTextWidget::AggTextWidget::AutoErase()
{
  return GET_OUTER()->AutoErase();
}


//----------------------------------------------------------------------

BASE_IWIDGET_IMPL(nsTextWidget, AggTextWidget);

