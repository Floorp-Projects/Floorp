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


NS_IMPL_ADDREF(nsTextWidget)
NS_IMPL_RELEASE(nsTextWidget)

#define DBG 0


/**-------------------------------------------------------------------------------
 * nsTextWidget Constructor
 * @update  dc 09/10/98
 */
nsTextWidget::nsTextWidget(): nsWindow()
{
  mIsPasswordCallBacksInstalled = PR_FALSE;
  mMakeReadOnly=PR_FALSE;
  mMakePassword=PR_FALSE;
  mTE_Data = nsnull;
  strcpy(gInstanceClassName, "nsTextWidget");
  //mBackground = NS_RGB(124, 124, 124);
}


/**-------------------------------------------------------------------------------
 * The create method for a nsTextWidget, using a nsIWidget as the parent
 * @update  dc 09/10/98
 * @param  aParent -- the widget which will be this widgets parent in the tree
 * @param  aRect -- The bounds in parental coordinates of this widget
 * @param  aHandleEventFunction -- Procedures to be executed for this widget
 * @param  aContext -- device context to be used by this widget
 * @param  aAppShell -- 
 * @param  aToolkit -- toolkit to be used by this widget
 * @param  aInitData -- Initialization data used by frames
 * @return -- NS_OK if everything was created correctly
 */ 
NS_IMETHODIMP nsTextWidget::Create(nsIWidget *aParent,const nsRect &aRect,EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,nsIAppShell *aAppShell,nsIToolkit *aToolkit, nsWidgetInitData *aInitData) 
{
LongRect		destRect,viewRect;
PRUint32		teFlags=0;
GrafPtr			curport;
PRInt32			offx,offy;


  mParent = aParent;
  aParent->AddChild(this);

  if (DBG) fprintf(stderr, "aParent 0x%x\n", aParent);
	
	WindowPtr window = nsnull;

  if (aParent) {
    window = (WindowPtr) aParent->GetNativeData(NS_NATIVE_WIDGET);
  }
	else if (aAppShell){
  		 	window = (WindowPtr) aAppShell->GetNativeData(NS_NATIVE_SHELL);
  }

  mIsMainWindow = PR_FALSE;
  mWindowMadeHere = PR_TRUE;
	mWindowRecord = (WindowRecord*)window;
	mWindowPtr = (WindowPtr)window;
  
  NS_ASSERTION(window!=nsnull,"The WindowPtr for the widget cannot be null")
	if (window){
	  InitToolkit(aToolkit, aParent);

	  if (DBG) fprintf(stderr, "Parent 0x%x\n", window);

		// Set the bounds to the local rect
		SetBounds(aRect);
		
		mWindowRegion = NewRgn();
		SetRectRgn(mWindowRegion,aRect.x,aRect.y,aRect.x+aRect.width,aRect.y+aRect.height);		 

	  // save the event callback function
	  mEventCallback = aHandleEventFunction;
	  	  
	  // Initialize the TE record
	  CalcOffset(offx,offy);
	  
	  viewRect.left = aRect.x;
	  viewRect.top = aRect.y;
	  viewRect.right = aRect.x+aRect.width;
	  viewRect.bottom = aRect.y+aRect.height;
	  destRect = viewRect;
	  ::GetPort(&curport);
	  ::SetPort(mWindowPtr);
	  //::SetOrigin(-offx,-offy);
		WENew(&destRect,&viewRect,teFlags,&mTE_Data);
		//::SetOrigin(0,0);
		::SetPort(curport);
		
	  InitDeviceContext(mContext, (nsNativeWidget)mWindowPtr);
	}
	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * The create method for a button, using a nsNativeWidget as the parent
 * @update  dc 09/10/98
 * @param  aParent -- the widget which will be this widgets parent in the tree
 * @param  aRect -- The bounds in parental coordinates of this widget
 * @param  aHandleEventFunction -- Procedures to be executed for this widget
 * @param  aContext -- device context to be used by this widget
 * @param  aAppShell -- 
 * @param  aToolkit -- toolkit to be used by this widget
 * @param  aInitData -- Initialization data used by frames
 * @return -- NS_OK if everything was created correctly
 */ 
NS_IMETHODIMP nsTextWidget::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
nsWindow		*theNsWindow=nsnull;
nsRefData		*theRefData;

	if(0!=aParent){
		theRefData = (nsRefData*)(((WindowPeek)aParent)->refCon);
		theNsWindow = (nsWindow*)theRefData->GetCurWidget();
	}
		
	if(nsnull!=theNsWindow){
		Create(theNsWindow, aRect,aHandleEventFunction, aContext, aAppShell, aToolkit, aInitData);
	}

	//NS_ERROR("This Widget must not use this Create method");
	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Destuctor for the nsTextWidget
 * @update  dc 08/31/98
 */ 
nsTextWidget::~nsTextWidget()
{
	if(mTE_Data!=nsnull)
		WEDispose(mTE_Data);
}

/**-------------------------------------------------------------------------------
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @update  dc 08/31/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */ 
nsresult nsTextWidget::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  static NS_DEFINE_IID(kITextWidgetIID, NS_ITEXTWIDGET_IID);

  if (aIID.Equals(kITextWidgetIID)) {
    AddRef();
    *aInstancePtr = (void**)(nsITextWidget*)this;
    return NS_OK;
  }
  return nsWindow::QueryInterface(aIID, aInstancePtr);
}

//-------------------------------------------------------------------------
/**
 * The onPaint handler for a nsTextWidget -- this may change, inherited from windows
 * @param aEvent -- The paint event to respond to
 * @return -- PR_TRUE if painted, false otherwise
 */ 
PRBool nsTextWidget::OnPaint(nsPaintEvent & aEvent)
{
PRInt32							offx,offy;
nsRect							therect;
Rect								macrect;
GrafPtr							theport;
RgnHandle						thergn;
	
	CalcOffset(offx,offy);
	::GetPort(&theport);
	::SetPort(mWindowPtr);
	::SetOrigin(-offx,-offy);
	GetBounds(therect);
	nsRectToMacRect(therect,macrect);
	thergn = ::NewRgn();
	::GetClip(thergn);
	::ClipRect(&macrect);
	::EraseRect(&macrect);
	::PenSize(1,1);
	WEActivate(mTE_Data);
	WEUpdate(nsnull,mTE_Data);
	::FrameRect(&macrect);
	::SetClip(thergn);
	::SetOrigin(0,0);
	::SetPort(theport);
	
  return PR_FALSE;
}

//-------------------------------------------------------------------------
/**
 * The Keydown handler for a nsTextWidget
 * @param aKey -- the key pressed
 * @param aModifiers -- the modifiers of the key pressed, like command, shift, etc.
 * @return -- PR_TRUE if painted, false otherwise
 */ 
void nsTextWidget::PrimitiveKeyDown(PRInt16	aKey,PRInt16 aModifiers)
{
PRBool 		result=PR_TRUE;
GrafPtr		thePort;
RgnHandle	theRgn;
nsRect		theRect;
Rect			macRect;
PRInt32		offX,offY;

	CalcOffset(offX,offY);
	::GetPort(&thePort);
	::SetPort(mWindowPtr);

	GetBounds(theRect);
	nsRectToMacRect(theRect,macRect);
	theRgn = ::NewRgn();
	::GetClip(theRgn);
	::ClipRect(&macRect);
	WEKey(aKey,aModifiers,mTE_Data);
	::SetClip(theRgn);
	::DisposeRgn(theRgn);
	::SetPort(thePort);
	
}

/**-------------------------------------------------------------------------------
 * DispatchMouseEvent handle an event for this nsTextWidget
 * @update  dc 10/10/98
 * @Param aEvent -- The mouse event to respond to for this button
 * @return -- True if the event was handled, PR_FALSE if we did not handle it.
 */ 
PRBool nsTextWidget::DispatchMouseEvent(nsMouseEvent &aEvent)
{
PRBool 		result=PR_TRUE;
Point			mouseLoc;
PRInt16 	modifiers=0;
nsRect		therect;
Rect			macrect;
PRInt32		offx,offy;
GrafPtr		theport;
	
	switch (aEvent.message){
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			CalcOffset(offx,offy);
			::GetPort(&theport);
			::SetPort(mWindowPtr);
			GetBounds(therect);
			nsRectToMacRect(therect,macrect);
			::ClipRect(&macrect);
			mouseLoc.h = aEvent.point.x;
			mouseLoc.v = aEvent.point.y;
			WEClick(mouseLoc,modifiers,aEvent.time,mTE_Data);
			::SetPort(theport);
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

/**-------------------------------------------------------------------------------
 * Resizes the button, currently handles by nsWindow
 * @update  dc 08/31/98
 * @Param aEvent -- The event for this resize
 * @return -- True if the event was handled, PR_FALSE is always return for now
 */ 
PRBool nsTextWidget::OnResize(nsSizeEvent &aEvent)
{
  return PR_FALSE;
}

/**-------------------------------------------------------------------------------
 * Sets the password flag.  If true, asteriks are the onlything displayed
 * @update  dc 10/10/98
 * @Param aIsPassword -- if true, password attribute is used.
 * @return -- if everything is ok
 */ 
NS_METHOD nsTextWidget::SetPassword(PRBool aIsPassword)
{
  if ( aIsPassword) {
    mMakePassword = PR_TRUE;
    return NS_OK;
  }
  
	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Sets the readonly flag.  If true, you can only read the text
 * @update  dc 10/10/98
 * @Param aReadOnlyFlag -- if true, readoly attribute is used.
 * @Param aOldFlag -- the old setting
 * @return -- if everything is ok
 */ 
NS_METHOD  nsTextWidget::SetReadOnly(PRBool aReadOnlyFlag, PRBool& aOldFlag)
{
	aOldFlag = mMakeReadOnly;
  mMakeReadOnly = aReadOnlyFlag;
	return NS_OK;  	
}

/**-------------------------------------------------------------------------------
 * Sets the maximum number of characters allowed.
 * @update  dc 10/10/98
 * @Param aChars -- number of characters allowed
 * @return -- if everything is ok
 */ 
NS_METHOD nsTextWidget::SetMaxTextLength(PRUint32 aChars)
{
  return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Gets the text from this widget
 * @update  dc 10/10/98
 * @Param aTextBuffer -- string to fill in
 * @Param aBufferSize -- size of the buffer passed in
 * @Param aSize -- size of the string passed out
 * @return -- if everything is ok
 */ 
NS_METHOD  nsTextWidget::GetText(nsString& aTextBuffer, PRUint32 aBufferSize, PRUint32& aSize) 
{
Handle				thetext;
PRInt32 			len,i;
char					*str;

  thetext = WEGetText(mTE_Data);
  len = WEGetTextLength(mTE_Data);
  
  //HLock(thetext);
  str = new char[len+1];

  for(i=0;i<len;i++)
  	str[i] = (*thetext)[i];
  str[len] = 0;
  //HUnlock(thetext);	
  
  aTextBuffer.SetLength(0);
  aTextBuffer.Append(str);
	aSize = aTextBuffer.Length();
	
	delete [] str;
	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Resize this widget
 * @update  dc 10/01/98
 * @param   aWidth -- x offset in widget local coordinates
 * @param   aHeight -- y offset in widget local coordinates
 * @param   aRepaint -- indicates if a repaint is needed.
 * @return  PR_TRUE if the pt is contained in the widget
 */ 
NS_IMETHODIMP nsTextWidget::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
nsSizeEvent 	event;
nsEventStatus	eventStatus;
LongRect			macRect;

  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  
   if(nsnull!=mWindowRegion)
  	::DisposeRgn(mWindowRegion);
	mWindowRegion = NewRgn();
	SetRectRgn(mWindowRegion,mBounds.x,mBounds.y,mBounds.x+mBounds.width,mBounds.y+mBounds.height);
	
	if(mTE_Data != nsnull){
		macRect.top = mBounds.y;
		macRect.left = mBounds.x;
		macRect.bottom = mBounds.y+mBounds.height;
		macRect.right = mBounds.x+mBounds.width;
		WESetDestRect(&macRect,mTE_Data);
		WESetViewRect(&macRect,mTE_Data);
	}
			 
  if (aRepaint){
  	UpdateVisibilityFlag();
  	UpdateDisplay();
  }
  
  event.message = NS_SIZE;
  event.point.x = 0;
  event.point.y = 0;
  event.windowSize = &mBounds;
  event.eventStructType = NS_SIZE_EVENT;
  event.widget = this;
 	this->DispatchEvent(&event, eventStatus);
	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Resize this widget
 * @update dc 10/01/98
 * @param  aX -- left part of rectangle
 * @param  aY -- top of rectangle
 * @param  aWidth -- x offset in widget local coordinates
 * @param  aHeight -- y offset in widget local coordinates
 * @param  aW
 * @return  PR_TRUE
 */ 
NS_IMETHODIMP nsTextWidget::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
nsSizeEvent 	event;
nsEventStatus	eventStatus;
LongRect			macRect;

  mBounds.x      = aX;
  mBounds.y      = aY;
  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  if(nsnull!=mWindowRegion)
  	::DisposeRgn(mWindowRegion);
	mWindowRegion = NewRgn();
	SetRectRgn(mWindowRegion,mBounds.x,mBounds.y,mBounds.x+mBounds.width,mBounds.y+mBounds.height);

	if(mTE_Data != nsnull){
		macRect.top = mBounds.y;
		macRect.left = mBounds.x;
		macRect.bottom = mBounds.y+mBounds.height;
		macRect.right = mBounds.x+mBounds.width;
		WESetDestRect(&macRect,mTE_Data);
		WESetViewRect(&macRect,mTE_Data);
	}

  if (aRepaint){
  	UpdateVisibilityFlag();
  	UpdateDisplay();
  }
  
  event.message = NS_SIZE;
  event.point.x = 0;
  event.point.y = 0;
  event.windowSize = &mBounds;
  event.widget = this;
  event.eventStructType = NS_SIZE_EVENT;
 	this->DispatchEvent(&event, eventStatus);
	return NS_OK;
}


/**-------------------------------------------------------------------------------
 * Set the text for this widget
 * @update  dc 10/01/98
 * @param   aText -- Text to use in this widget
 * @param   aSize -- size of the text to use here
 * @return  PR_TRUE 
 */ 
PRUint32  nsTextWidget::SetText(const nsString& aText, PRUint32& aSize)
{ 
char 			buffer[256];
PRInt32 	len;
PRInt32		offx,offy;
GrafPtr		theport;

	CalcOffset(offx,offy);
	::GetPort(&theport);
	::SetPort(mWindowPtr);
	//::SetOrigin(-offx,-offy);
 
 	this->RemoveText();
	aText.ToCString(buffer,255);
	len = strlen(buffer);

	WEInsert(buffer,len,0,0,mTE_Data);

	aSize = len;
	//::SetOrigin(0,0);
	::SetPort(theport);
  return NS_OK;
}

/**-------------------------------------------------------------------------------
 * insert text into this widget
 * @update dc 10/01/98
 * @param aText -- Text to use in this widget
 * @param aStartPos - Starting position
 * @param aEndtPos - Ending position
 * @param aSize -- size of the text to use here
 * @return PR_TRUE
 */ 
PRUint32  nsTextWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aSize)
{ 
char 			buffer[256];
PRInt32 	len;
PRInt32		offX,offY;
GrafPtr		thePort;

	CalcOffset(offX,offY);
	::GetPort(&thePort);
	::SetPort(mWindowPtr);
	//::SetOrigin(-offx,-offy);

	aText.ToCString(buffer,255);
	len = strlen(buffer);
	
	WEInsert(buffer,len,0,0,mTE_Data);
	aSize = len;
	//::SetOrigin(0,0);
	::SetPort(thePort);
  return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Remove all the text for this widget
 * @update  dc 10/01/98
 * @return  PR_TRUE
 */ 
NS_METHOD  nsTextWidget::RemoveText()
{
	WESetSelection(0, 32000,mTE_Data);
	WEDelete(mTE_Data);
  return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Select all the text for this widget
 * @update  dc 10/01/98
 * @return  PR_TRUE
 */ 
NS_METHOD nsTextWidget::SelectAll()
{
	WESetSelection(0, 32000,mTE_Data);
  return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Set the text for this widget
 * @update  dc 10/01/98
 * @param   aStartSel -- start index of the text
 * @param   aEndSel -- end index of the text
 * @return  PR_TRUE 
 */ 
NS_METHOD  nsTextWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
  WESetSelection(aStartSel, aEndSel,mTE_Data);
  return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Get the text for this widget
 * @update  dc 10/01/98
 * @param   aStartSel -- start index of the text
 * @param   aEndSel -- end index of the text
 * @return  PR_TRUE
 */ 
NS_METHOD  nsTextWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
	WEGetSelection((long*)aStartSel,(long*)aEndSel,mTE_Data);
  return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Set the caret position for this widget
 * @update  dc 10/01/98
 * @param   aPosition -- postition of text
 * @return  PR_TRUE 
 */ 
NS_METHOD  nsTextWidget::SetCaretPosition(PRUint32 aPosition)
{
  //mHelper->SetCaretPosition(aPosition);
  return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Get the caret position for this widget
 * @update  dc 10/01/98
 * @param   aPosition -- postition of text
 * @return  PR_TRUE if the pt is contained in the widget
 */ 
NS_METHOD nsTextWidget::GetCaretPosition(PRUint32& aPos)
{
  //return mHelper->GetCaretPosition();
  return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Set the autoerase feature
 * @update  dc 10/01/98
 * @return  PR_TRUE
 */ 
PRBool nsTextWidget::AutoErase()
{
  //return mHelper->AutoErase();
  return PR_TRUE;
}




