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

#include "nsTextAreaWidget.h"
#include "nsMacWindow.h"
#include "nsToolkit.h"
#include "nsColor.h"
#include "nsGUIEvent.h"
#include "nsString.h"
#include <memory>
#include <ToolUtils.h>

NS_IMPL_ADDREF(nsTextAreaWidget);
NS_IMPL_RELEASE(nsTextAreaWidget);

#define MARGIN	4

//-------------------------------------------------------------------------
//	¥ NOTE ABOUT MENU HANDLING ¥
//
//	The definitions below, as well as the NS_MENU_SELECTED code in
//	DispatchEvent() are temporary hacks only. They require the menu
//	resources to be created under Contructor with the standard
//	PowerPlant menu IDs. All that will go away because:
//	- nsTextWidget will be rewritten as an XP widget by the Editor team.
//	- menu handling will be rewritten by the XPApp team.
//-------------------------------------------------------------------------

#include <Scrap.h>
#include <PP_Messages.h>		// for PP standard menu commands
enum
{
	menu_Apple = 128,
	menu_File,
	menu_Edit
};


/**-------------------------------------------------------------------------------
 * nsTextAreaWidget Constructor
 * @update  dc 09/10/98
 */
nsTextAreaWidget::nsTextAreaWidget(): nsWindow()
{
  NS_INIT_REFCNT();
  mIsPasswordCallBacksInstalled = PR_FALSE;
  mMakeReadOnly = PR_FALSE;
  mMakePassword = PR_FALSE;
  mTE_Data = nsnull;
  strcpy(gInstanceClassName, "nsTextAreaWidget");
  //mBackground = NS_RGB(124, 124, 124);
}


/**-------------------------------------------------------------------------------
 * The create method for a nsTextAreaWidget, using a nsIWidget as the parent
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
NS_IMETHODIMP nsTextAreaWidget::Create(nsIWidget *aParent,const nsRect &aRect,EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,nsIAppShell *aAppShell,nsIToolkit *aToolkit, nsWidgetInitData *aInitData) 
{
	nsWindow::Create(aParent, aRect, aHandleEventFunction,
						aContext, aAppShell, aToolkit, aInitData);

	mWindowPtr = nsnull;
	if (aParent)
		mWindowPtr = (WindowPtr)aParent->GetNativeData(NS_NATIVE_DISPLAY);
	else if (aAppShell)
		mWindowPtr = (WindowPtr)aAppShell->GetNativeData(NS_NATIVE_SHELL);
  
	// Initialize the TE record
	LongRect destRect, viewRect;
	viewRect.top	= MARGIN;
	viewRect.left	= MARGIN;
	viewRect.bottom	= aRect.height - MARGIN;
	viewRect.right	= aRect.width - MARGIN;
	destRect = viewRect;

	StartDraw();
	PRUint32 teFlags = weDoAutoScroll | weDoOutlineHilite | weDoMonoStyled;
	if (!mVisible)
		teFlags |= weDoInhibitRedraw;
	WENew(&destRect, &viewRect, teFlags, &mTE_Data);
	EndDraw();
		
	return (mTE_Data ? NS_OK : NS_ERROR_FAILURE);
}

/**-------------------------------------------------------------------------------
 * Destuctor for the nsTextAreaWidget
 * @update  dc 08/31/98
 */ 
nsTextAreaWidget::~nsTextAreaWidget()
{
	if (mTE_Data)
		WEDispose(mTE_Data);
}

/**-------------------------------------------------------------------------------
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @update  dc 08/31/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 */ 
nsresult nsTextAreaWidget::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
	static NS_DEFINE_IID(kITextAreaWidgetIID, NS_ITEXTAREAWIDGET_IID);

	if (aIID.Equals(kITextAreaWidgetIID)) {
		AddRef();
		*aInstancePtr = (void**)(nsITextAreaWidget*)this;
		return NS_OK;
	}

	return nsWindow::QueryInterface(aIID, aInstancePtr);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsTextAreaWidget::Show(PRBool bState)
{
	if (! mTE_Data)
		return NS_ERROR_NOT_INITIALIZED;

	if (mVisible == bState)
		return NS_OK;

	Inherited::Show(bState);
	WEFeatureFlag(weFInhibitRedraw, (bState ? weBitClear : weBitSet), mTE_Data);

	return NS_OK;
}

//-------------------------------------------------------------------------
/**
 * The onPaint handler for a nsTextAreaWidget -- this may change, inherited from windows
 * @param aEvent -- The paint event to respond to
 * @return -- PR_TRUE if painted, false otherwise
 */ 
PRBool nsTextAreaWidget::OnPaint(nsPaintEvent & aEvent)
{
	if (! mTE_Data)
		return NS_ERROR_NOT_INITIALIZED;

	if (! mVisible)
		return PR_FALSE;

	StartDraw();
	{
		// erase all
		nsRect	rect;
		Rect	macRect;
		GetBounds(rect);
		rect.x = rect.y = 0;
		nsRectToMacRect(rect, macRect);
		::EraseRect(&macRect);

		// update text
		WEUpdate(nsnull, mTE_Data);

		// draw frame
		::FrameRect(&macRect);
	}
	EndDraw();
	
	return PR_FALSE;
}

//-------------------------------------------------------------------------
/**
 * The Keydown handler for a nsTextAreaWidget
 * @param aKey -- the key pressed
 * @param aModifiers -- the modifiers of the key pressed, like command, shift, etc.
 * @return -- PR_TRUE if painted, false otherwise
 */ 
void nsTextAreaWidget::PrimitiveKeyDown(PRInt16	aKey, PRInt16 aModifiers)
{
	if (! mTE_Data)
		return;

	StartDraw();
	WEKey(aKey, aModifiers, mTE_Data);
	EndDraw();
}

/**-------------------------------------------------------------------------------
 * DispatchMouseEvent handle an event for this nsTextAreaWidget
 * @update  dc 10/10/98
 * @Param aEvent -- The mouse event to respond to for this button
 * @return -- True if the event was handled, PR_FALSE if we did not handle it.
 */ 
PRBool nsTextAreaWidget::DispatchMouseEvent(nsMouseEvent &aEvent)
{
	if (! mTE_Data)
		return PR_FALSE;

	PRBool result = PR_FALSE;
	switch (aEvent.message)
	{
		case NS_MOUSE_LEFT_DOUBLECLICK:
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			Point mouseLoc;
			mouseLoc.h = aEvent.point.x;
			mouseLoc.v = aEvent.point.y;

			EventModifiers modifiers = 0;
			if (aEvent.nativeMsg != nsnull)
			{
				EventRecord* osEvent = (EventRecord*)aEvent.nativeMsg;
				modifiers = osEvent->modifiers;
			}

			StartDraw();
			WEClick(mouseLoc, modifiers, aEvent.time, mTE_Data);
			EndDraw();
			result = PR_TRUE;
			break;

		case NS_MOUSE_LEFT_BUTTON_UP:
			break;

		case NS_MOUSE_EXIT:
			SetCursor(eCursor_standard);
			break;

		case NS_MOUSE_ENTER:
			SetCursor(eCursor_select);
			break;
	}
	return result;
}

//-------------------------------------------------------------------------
PRBool nsTextAreaWidget::DispatchWindowEvent(nsGUIEvent &aEvent)
{
	// filter cursor keys
	PRBool passKeyEvent = PR_TRUE;
	switch (aEvent.message)
	{
		case NS_KEY_DOWN:
		case NS_KEY_UP:
		{
  		nsKeyEvent* keyEvent = (nsKeyEvent*)&aEvent;

#if 0
			// this hack is no longer needed, since Enter is being mapped to
			// VK_RETURN in the event handler
			if (keyEvent->keyCode == kEnterCharCode)
				keyEvent->keyCode = NS_VK_RETURN;
#endif
			
			// is this hack really needed?
			EventRecord* theOSEvent = (EventRecord*)aEvent.nativeMsg;
			if (theOSEvent && ((theOSEvent->message & charCodeMask) == kEnterCharCode))
				theOSEvent->message = (theOSEvent->message & ~charCodeMask) + kReturnCharCode;

			switch (keyEvent->keyCode)
			{
				case NS_VK_PAGE_UP:
				case NS_VK_PAGE_DOWN:
				case NS_VK_END:
				case NS_VK_HOME:
				case NS_VK_LEFT:
				case NS_VK_UP:
				case NS_VK_RIGHT:
				case NS_VK_DOWN:
					passKeyEvent = PR_FALSE;
					break;
			}
			break;
		}
	}

	// dispatch the message
	PRBool eventHandled = PR_FALSE;
	if (passKeyEvent)
		eventHandled = Inherited::DispatchWindowEvent(aEvent);

	if (! eventHandled)
	{
		switch ( aEvent.message )
	  	{
			case NS_GOTFOCUS:
			{
				if (mTE_Data)
				{
					StartIdling();
					StartDraw();
					WEActivate(mTE_Data);
					EndDraw();
				}
				break;
			}

			case NS_LOSTFOCUS:
			{
				if (mTE_Data)
				{
					StopIdling();
					StartDraw();
					WEDeactivate(mTE_Data);
					EndDraw();
				}
				break;
			}

	  		case NS_KEY_DOWN:
	  		{
		  		char theChar;
		  		EventModifiers theModifiers;
		  		EventRecord* theOSEvent = (EventRecord*)aEvent.nativeMsg;
		  		if (theOSEvent)
		  		{
		  			theChar = (theOSEvent->message & charCodeMask);
		  			theModifiers = theOSEvent->modifiers;
		  		}
		  		else
		  		{
		  			nsKeyEvent* keyEvent = (nsKeyEvent*)&aEvent;
		  			theChar = keyEvent->charCode;
		  			if (keyEvent->isShift)
		  				theModifiers = shiftKey;
		  			if (keyEvent->isControl)
		  				theModifiers |= controlKey;
		  			if (keyEvent->isAlt)
		  				theModifiers |= optionKey;
		  			if (keyEvent->isCommand)
		  				theModifiers |= cmdKey;
		  		}
	  			PrimitiveKeyDown(theChar, theModifiers);
	  			eventHandled = PR_TRUE;
		  		break;
		  	}
		  	
		  	case NS_MENU_SELECTED:
			{
				nsMenuEvent* menuEvent = (nsMenuEvent*)&aEvent;
				long menuID = HiWord(menuEvent->mCommand);
				long menuItem = LoWord(menuEvent->mCommand);
				switch (menuID)
				{
					case menu_Edit:
					{
						switch (menuItem)
						{
//							case cmd_Undo:

							case cmd_Cut:
							case cmd_Copy:
							{
								PRUint32 startSel = 0, endSel = 0;
								GetSelection ( &startSel, &endSel );
								if ( startSel != endSel ) {
									const Uint32 selectionLen = endSel - startSel;
									
									// extract out the selection into a different nsString so
									// we can keep it unicode as long as possible
									PRUint32 unused = 0;
									nsString str, selection;
									GetText(str, 0, unused );
									str.Mid(selection, startSel, selectionLen);
									
									// now |selection| holds the current selection in unicode.
									// We need to convert it to a c-string for MacOS.
									auto_ptr<char> cRepOfSelection(new char[selectionLen + 1]);
									selection.ToCString(cRepOfSelection.get(), selectionLen + 1);
									
									// copy it to the scrapMgr
									::ZeroScrap();
									::PutScrap ( selectionLen, 'TEXT', cRepOfSelection.get() );
								
									// if we're cutting, remove the text from the widget
									if ( menuItem == cmd_Cut ) {
										unused = 0;
										str.Cut ( startSel, selectionLen );
										SetText ( str, unused );
									}
								} // if there is a selection
								eventHandled = PR_TRUE;
								break;
							}
								
							case cmd_Paste:
							{
								long scrapOffset;
								Handle scrapH = ::NewHandle(0);
								long scrapLen = ::GetScrap(scrapH, 'TEXT', &scrapOffset);
								if (scrapLen > 0)
								{
									::HLock(scrapH);

									// truncate to the first line
									//char* cr = strchr((char*)*scrapH, '\r');
									//if (cr != nil)
									//	scrapLen = cr - *scrapH;

									// paste text
									nsString str;
									str.SetString((char*)*scrapH, scrapLen);
									PRUint32 startSel, endSel;
									GetSelection(&startSel, &endSel);
									PRUint32 outSize;
									InsertText(str, startSel, endSel, outSize);
									startSel += str.Length();
									SetSelection(startSel, startSel);

									::HUnlock(scrapH);
								}
								::DisposeHandle(scrapH);
								eventHandled = PR_TRUE;
								break;
							}
							case cmd_Clear:
							{
								nsString str;
								PRUint32 outSize;
								SetText(str, outSize);
								eventHandled = PR_TRUE;
								break;
							}
							case cmd_SelectAll:
							{
								SelectAll();
								eventHandled = PR_TRUE;
								break;
							}
						}
						break;
					}
				}
				break;
			} // case NS_MENU_SELECTED

	  	} // switch on which event message
	} // if key event not handled

	return (eventHandled);
}

/**-------------------------------------------------------------------------------
 * Sets the password flag.  If true, asteriks are the onlything displayed
 * @update  dc 10/10/98
 * @Param aIsPassword -- if true, password attribute is used.
 * @return -- if everything is ok
 */ 
NS_METHOD nsTextAreaWidget::SetPassword(PRBool aIsPassword)
{
	//¥TODO?: can we really have textAreas displayed as passwords?
	if (aIsPassword)
		mMakePassword = PR_TRUE;
  
	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Sets the readonly flag.  If true, you can only read the text
 * @update  dc 10/10/98
 * @Param aReadOnlyFlag -- if true, readoly attribute is used.
 * @Param aOldFlag -- the old setting
 * @return -- if everything is ok
 */ 
NS_METHOD  nsTextAreaWidget::SetReadOnly(PRBool aReadOnlyFlag, PRBool& aOldFlag)
{
	//¥TODO: toggle between read-only and editable modes
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
NS_METHOD nsTextAreaWidget::SetMaxTextLength(PRUint32 aChars)
{
	//¥TODO: implement this
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
NS_METHOD  nsTextAreaWidget::GetText(nsString& aTextBuffer, PRUint32 aBufferSize, PRUint32& aSize) 
{
	if (! mTE_Data)
		return NS_ERROR_NOT_INITIALIZED;

	Handle text = WEGetText(mTE_Data);
	PRInt32 len = WEGetTextLength(mTE_Data);
  
	char* str = new char[len+1];
	for (PRInt32 i = 0; i < len; i++)
		str[i] = (*text)[i];
	str[len] = 0;

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
NS_IMETHODIMP nsTextAreaWidget::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
	return Resize(0, 0, aWidth, aHeight, aRepaint);
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
NS_IMETHODIMP nsTextAreaWidget::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
	Inherited::Resize(aX, aY, aWidth, aHeight, aRepaint);

	if (! mTE_Data)
		return NS_ERROR_NOT_INITIALIZED;

	LongRect macRect;
	macRect.top		= mBounds.y + MARGIN;
	macRect.left	= mBounds.x + MARGIN;
	macRect.bottom	= mBounds.YMost() - MARGIN;
	macRect.right	= mBounds.XMost() - MARGIN;

	StartDraw();
	WESetDestRect(&macRect, mTE_Data);
	WESetViewRect(&macRect, mTE_Data);
	EndDraw();

	return NS_OK;
}


/**-------------------------------------------------------------------------------
 * Set the text for this widget
 * @update  dc 10/01/98
 * @param   aText -- Text to use in this widget
 * @param   outSize -- (out only) Length of text displayed in the text area
 * @return  PR_TRUE 
 */ 
PRUint32  nsTextAreaWidget::SetText(const nsString& aText, PRUint32& outSize)
{ 
	if (! mTE_Data)
		return NS_ERROR_NOT_INITIALIZED;

	outSize = aText.Length();
	const unsigned int bufferSize = outSize + 1;	// add 1 for null
 
 	this->RemoveText();
	auto_ptr<char> buffer(new char[bufferSize]);
	if (buffer.get())
	{
		aText.ToCString(buffer.get(), bufferSize);

		char* occur = buffer.get();		// replace LineFeed with Return
		while ((occur = strchr(occur, '\n')) != nil) {
			*occur = '\r';
		}

		StartDraw();
		WEInsert(buffer.get(), outSize, 0, 0, mTE_Data);
		EndDraw();
	}
	else
		return NS_ERROR_OUT_OF_MEMORY;

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
PRUint32  nsTextAreaWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aSize)
{ 
	if (! mTE_Data)
		return NS_ERROR_NOT_INITIALIZED;

	const unsigned int bufferSize = aText.Length();
	auto_ptr<char> buffer(new char[bufferSize + 1]);	// add 1 for null
	if (buffer.get())
	{
		aText.ToCString(buffer.get(), bufferSize + 1);

		char* occur = buffer.get();		// replace LineFeed with Return
		while ((occur = strchr(occur, '\n')) != nil) {
			*occur = '\r';
		}
		
		StartDraw();
		if (aStartPos == (PRUint32)-1L)
			aStartPos = WEGetTextLength(mTE_Data);
		if (aEndPos == (PRUint32)-1L)
			aEndPos = WEGetTextLength(mTE_Data);
		WESetSelection(aStartPos, aEndPos, mTE_Data);
		WEInsert(buffer.get(), bufferSize, 0, 0, mTE_Data);
		EndDraw();
	}
	else
		return NS_ERROR_OUT_OF_MEMORY;

	aSize = WEGetTextLength(mTE_Data);
	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Remove all the text for this widget
 * @update  dc 10/01/98
 * @return  PR_TRUE
 */ 
NS_METHOD  nsTextAreaWidget::RemoveText()
{
	if (! mTE_Data)
		return NS_ERROR_NOT_INITIALIZED;

	StartDraw();
	WESetSelection(0, WEGetTextLength(mTE_Data), mTE_Data);
	WEDelete(mTE_Data);
	EndDraw();

	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Select all the text for this widget
 * @update  dc 10/01/98
 * @return  PR_TRUE
 */ 
NS_METHOD nsTextAreaWidget::SelectAll()
{
	if (! mTE_Data)
		return NS_ERROR_NOT_INITIALIZED;

	StartDraw();
	WESetSelection(0, WEGetTextLength(mTE_Data), mTE_Data);
	EndDraw();

	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Set the text for this widget
 * @update  dc 10/01/98
 * @param   aStartSel -- start index of the text
 * @param   aEndSel -- end index of the text
 * @return  PR_TRUE 
 */ 
NS_METHOD  nsTextAreaWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
	if (! mTE_Data)
		return NS_ERROR_NOT_INITIALIZED;

	StartDraw();
	WESetSelection(aStartSel, aEndSel, mTE_Data);
	EndDraw();

	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Get the text for this widget
 * @update  dc 10/01/98
 * @param   aStartSel -- start index of the text
 * @param   aEndSel -- end index of the text
 * @return  PR_TRUE
 */ 
NS_METHOD  nsTextAreaWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
	if (! mTE_Data)
		return NS_ERROR_NOT_INITIALIZED;

	StartDraw();
	WEGetSelection((SInt32*)aStartSel, (SInt32*)aEndSel, mTE_Data);
	EndDraw();

	return NS_OK;
}

/**-------------------------------------------------------------------------------
 * Set the caret position for this widget
 * @update  dc 10/01/98
 * @param   aPosition -- postition of text
 * @return  PR_TRUE 
 */ 
NS_METHOD  nsTextAreaWidget::SetCaretPosition(PRUint32 aPosition)
{
	return SetSelection(aPosition, aPosition);
}

/**-------------------------------------------------------------------------------
 * Get the caret position for this widget
 * @update  dc 10/01/98
 * @param   aPosition -- postition of text
 * @return  PR_TRUE if the pt is contained in the widget
 */ 
NS_METHOD nsTextAreaWidget::GetCaretPosition(PRUint32& aPos)
{
	PRUint32 startSel, endSel;
	nsresult res = GetSelection(&startSel, &endSel);
	aPos = (res == NS_OK ? startSel : 0);
	return res;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void	nsTextAreaWidget::RepeatAction(const EventRecord& inMacEvent)
{
	if (mTE_Data)
	{
		UInt32 maxSleep = 0;
		StartDraw();
		WEIdle(&maxSleep, mTE_Data);
		EndDraw();
	}
}
