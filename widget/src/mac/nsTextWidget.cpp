/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTextWidget.h"
#include "nsIRenderingContext.h"
#include "nsCRT.h"

#include <ToolUtils.h>
#include <Appearance.h>
#include <memory>

using std::auto_ptr;

#if TARGET_CARBON || (UNIVERSAL_INTERFACES_VERSION >= 0x0330)
#include <ControlDefinitions.h>
#endif

NS_IMPL_ADDREF(nsTextWidget)
NS_IMPL_RELEASE(nsTextWidget)


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

typedef		SInt32			MessageT;
typedef   PRUint32    Uint32;
const MessageT	cmd_Cut				= 12;	// nil
const MessageT	cmd_Copy			= 13;	// nil
const MessageT	cmd_Paste			= 14;	// nil
const MessageT	cmd_Clear			= 15;	// nil
const MessageT	cmd_SelectAll		= 16;	// nil

enum
{
	menu_Apple = 128,
	menu_File,
	menu_Edit
};


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsTextWidget::nsTextWidget() : nsMacControl(), nsITextWidget(), Repeater()
{
  WIDGET_SET_CLASSNAME("nsTextWidget");
  SetControlType(kControlEditTextProc);
  mIsPassword = PR_FALSE;
  mIsReadOnly = PR_FALSE;

  AcceptFocusOnClick(PR_TRUE);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsTextWidget::~nsTextWidget()
{
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsTextWidget::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
	if (aInitData)
	{
		nsTextWidgetInitData* initData = (nsTextWidgetInitData*)aInitData;
		if (initData->mIsPassword)
		{
			SetControlType(kControlEditTextPasswordProc);
			mIsPassword = PR_TRUE;
		}
		else if (mIsReadOnly)
		{
			SetControlType(kControlStaticTextProc);
			mIsReadOnly = PR_TRUE;
		}
	}

	Inherited::Create(aParent, aRect, aHandleEventFunction,
						aContext, aAppShell, aToolkit, aInitData);

	return NS_OK;
}


//-------------------------------------------------------------------------
//	Destroy
//
//-------------------------------------------------------------------------
// The repeater in this widget needs to use out of band notification
// to sever its ties with the nsTimer. If we just rely on the 
// dtor to do it, it will never get called because the nsTimer holds a ref to
// this object.
//
NS_IMETHODIMP
nsTextWidget::Destroy()
{
  Inherited::Destroy();
  if (mRepeating) RemoveFromRepeatList();
  if (mIdling) RemoveFromIdleList();
  return NS_OK;
}


//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_INTERFACE_MAP_BEGIN(nsTextWidget)
  NS_INTERFACE_MAP_ENTRY(nsITextWidget)
NS_INTERFACE_MAP_END_INHERITING(Inherited)

#pragma mark -
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsTextWidget::DispatchMouseEvent(nsMouseEvent &aEvent)
{
	PRBool eventHandled = nsWindow::DispatchMouseEvent(aEvent);	// we don't want the 'Inherited' nsMacControl behavior

  EventRecord* theOSEvent;

	if ((! eventHandled) && (mControl != nsnull))
	{
		switch (aEvent.message)
		{
			case NS_MOUSE_LEFT_DOUBLECLICK:
			case NS_MOUSE_LEFT_BUTTON_DOWN:
				Point thePoint;
				thePoint.h = aEvent.point.x;
				thePoint.v = aEvent.point.y;

				short theModifiers;
				theOSEvent = (EventRecord*)aEvent.nativeMsg;
				if (theOSEvent)
				{
					theModifiers = theOSEvent->modifiers;
				}
				else
				{
					if (aEvent.isShift)		theModifiers = shiftKey;
					if (aEvent.isControl)	theModifiers |= controlKey;
					if (aEvent.isAlt)		theModifiers |= optionKey;
				}
				StartDraw();
				::HandleControlClick(mControl, thePoint, theModifiers, nil);
				EndDraw();
				eventHandled = PR_TRUE;
				break;

			case NS_MOUSE_ENTER:
				SetCursor(eCursor_select);
				break;

			case NS_MOUSE_EXIT:
				SetCursor(eCursor_standard);
				break;
		}
	}
	return (eventHandled);
}

//-------------------------------------------------------------------------
//	DispatchWindowEvent
//
//		Handle the following events: keys, focus, edit menu commands.
//		The strategy for key events is to use the native OS event
//		when it exists, otherwise use the Raptor nsKeyEvent.
//-------------------------------------------------------------------------
PRBool nsTextWidget::DispatchWindowEvent(nsGUIEvent &aEvent)
{
	// filter cursor keys
	PRBool passKeyEvent = PR_TRUE;
	switch (aEvent.message)
	{
		case NS_KEY_DOWN:
		case NS_KEY_UP:
		{
			// hack: if Enter is pressed, pass Return
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
//				case NS_VK_PAGE_UP:
//				case NS_VK_PAGE_DOWN:
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

	// handle the message here if nobody else processed it already
	if ((! eventHandled) && (mControl != nsnull))
	{
		switch (aEvent.message)
		{
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
//								case cmd_Undo:

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
										PL_strncpyz(cRepOfSelection.get(),
										            NS_LossyConvertUCS2toASCII(selection).get(),
										            selectionLen + 1);
										
										// copy it to the scrapMgr
#if TARGET_CARBON
                    ::ClearCurrentScrap();
                    ScrapRef scrap;
                    ::GetCurrentScrap(&scrap);
                    ::PutScrapFlavor(scrap, 'TEXT', 0L /* ??? */, selectionLen, cRepOfSelection.get());
#else
										::ZeroScrap();
										::PutScrap ( selectionLen, 'TEXT', cRepOfSelection.get() );
#endif						
										// if we're cutting, remove the text from the widget
										if ( menuItem == cmd_Cut ) {
											unused = 0;
											str.Cut ( startSel, selectionLen );
											SetText ( str, unused );
											SetSelection(startSel, startSel);
										}
									} // if there is a selection
									eventHandled = PR_TRUE;
									break;
								}
									
								case cmd_Paste:
								{
									long scrapOffset;
                  Handle scrapH = ::NewHandle(0);
#if TARGET_CARBON
                  ScrapRef scrap;
                  OSStatus err;
                  err = ::GetCurrentScrap(&scrap);
                  if (err != noErr) return NS_ERROR_FAILURE;
                  err = ::GetScrapFlavorSize(scrap, 'TEXT', &scrapOffset);
                  // XXX uhh.. i don't think this is right..
                  long scrapLen = scrapOffset;
                  if ( scrapOffset > 0 )
#else             
									long scrapLen = ::GetScrap(scrapH, 'TEXT', &scrapOffset);
									if (scrapLen > 0)
#endif
									{
										::HLock(scrapH);

										// truncate to the first line
										char* cr = strchr((char*)*scrapH, '\r');
										if (cr != nil)
											scrapLen = cr - *scrapH;

										// paste text
										nsString str;
										str.AssignWithConversion((char*)*scrapH, scrapLen);
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

				case NS_GOTFOCUS:
				{
					StartDraw();
					ActivateControl(mControl);
					OSErr err = SetKeyboardFocus(mWindowPtr, mControl, kControlFocusNextPart);
					if (err == noErr ) {
						nsRect rect = mBounds;
						rect.x = rect.y = 0;
						Rect macRect;
						nsRectToMacRect(rect, macRect);
						::InsetRect(&macRect, 2, 2);
						::DrawThemeFocusRect(&macRect, true);
					}
					EndDraw();
					StartIdling();
					break;
				}

				case NS_LOSTFOCUS:
				{
					StopIdling();
					StartDraw();
					OSErr err = SetKeyboardFocus(mWindowPtr, mControl, kControlFocusNoPart);
					if ( err == noErr ) {
						nsRect rect = mBounds;
						rect.x = rect.y = 0;
						Rect macRect;
						nsRectToMacRect(rect, macRect);
						::InsetRect(&macRect, 2, 2);
						::DrawThemeFocusRect(&macRect, false);
					}
					DeactivateControl(mControl);
					EndDraw();
					break;
				}

				case NS_KEY_DOWN:
				{
			  		char theChar;
			  		unsigned short theKey;
			  		unsigned short theModifiers;
			  		EventRecord* theOSEvent = (EventRecord*)aEvent.nativeMsg;
		  			nsKeyEvent* keyEvent = (nsKeyEvent*)&aEvent;
			  		if (theOSEvent)
			  		{
			  			theChar = (theOSEvent->message & charCodeMask);
			  			theKey  = (theOSEvent->message & keyCodeMask) >> 8;
			  			theModifiers = theOSEvent->modifiers;
			  		}
			  		else
			  		{
			  			theChar = keyEvent->keyCode;
			  			theKey  = 0;		// what else?
			  			if (keyEvent->isShift)		theModifiers = shiftKey;
			  			if (keyEvent->isControl)	theModifiers |= controlKey;
			  			if (keyEvent->isAlt)		theModifiers |= optionKey;

			  		}
			  		if (theChar != NS_VK_RETURN)	// don't pass Return: nsTextWidget is a single line editor
			  		{
						StartDraw();		  			
			  			::HandleControlKey(mControl, theKey, theChar, theModifiers);
						EndDraw();
			  			eventHandled = PR_TRUE;
			  		}
					break;
				}
		}
	}
	return (eventHandled);
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsTextWidget::OnPaint(nsPaintEvent &aEvent)
{
	Inherited::OnPaint(aEvent);
	
	/* draw a box.*/
	if (mVisible && mIsReadOnly)
	{
		nsRect ctlRect = mBounds;
		ctlRect.x = ctlRect.y = 0;
		//ctlRect.Deflate(1, 1);
		mTempRenderingContext->SetColor(NS_RGB(0, 0, 0));
		mTempRenderingContext->DrawRect(ctlRect);
	}
	
	return PR_FALSE;
}

#pragma mark -

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsTextWidget::SetPassword(PRBool aIsPassword)
{
	nsresult	theResult = NS_OK;
	
	// note: it is assumed that we can't have a read-only password field
	if (aIsPassword != mIsPassword)
	{
		mIsPassword = aIsPassword;
		short newControlType = (aIsPassword ? kControlEditTextPasswordProc : kControlEditTextProc);
		SetControlType(newControlType);
		
		theResult = CreateOrReplaceMacControl(newControlType);
	}
	return theResult;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD  nsTextWidget::SetReadOnly(PRBool aReadOnlyFlag, PRBool& aOldFlag)
{
	nsresult	theResult = NS_OK;

	// note: it is assumed that we can't have a read-only password field
	aOldFlag = mIsReadOnly;
	if (aReadOnlyFlag != mIsReadOnly)
	{
		mIsReadOnly = aReadOnlyFlag;
		short newControlType = (aReadOnlyFlag ? kControlStaticTextProc : kControlEditTextProc);
		SetControlType(newControlType);

		theResult = CreateOrReplaceMacControl(newControlType);
	}
	return theResult;
}

#pragma mark -

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsTextWidget::SetMaxTextLength(PRUint32 aChars)
{
	//¥TODO: install a kControlEditTextKeyFilterTag proc
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD  nsTextWidget::GetText(nsString& aTextBuffer, PRUint32 /*aBufferSize*/, PRUint32& aSize) 
{
	if (!mControl)
		return NS_ERROR_NOT_INITIALIZED;

	ResType	textTag = (mIsPassword ? kControlEditTextPasswordTag : kControlEditTextTextTag);

	Size textSize;
	::GetControlDataSize(mControl, kControlNoPart, textTag, &textSize);

	char* str = new char[textSize];
	if (str)
	{
		::GetControlData(mControl, kControlNoPart, textTag, textSize, (Ptr)str, &textSize);
		aTextBuffer.SetLength(0);
		aTextBuffer.AppendWithConversion(str, textSize);
		aSize = textSize;
		delete [] str;
	}
	else
	{
		aSize = 0;
		return NS_ERROR_OUT_OF_MEMORY;
	}

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Assumes aSize is |out| only and does not yet have a value.
//-------------------------------------------------------------------------
NS_METHOD  nsTextWidget::SetText(const nsString& aText, PRUint32& outSize)
{
	outSize = aText.Length();
	
	if (!mControl)
		return NS_ERROR_NOT_INITIALIZED;

	ResType	textTag = (mIsPassword ? kControlEditTextPasswordTag : kControlEditTextTextTag);
	::SetControlData(mControl, kControlNoPart, textTag, outSize,
	                 (Ptr)NS_LossyConvertUCS2toASCII(aText).get());
	Invalidate(PR_FALSE);

	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD  nsTextWidget::InsertText(const nsString &aText, PRUint32 aStartPos, PRUint32 aEndPos, PRUint32& aSize)
{
	if (!mControl)
		return NS_ERROR_NOT_INITIALIZED;

	Size textSize;
	::GetControlDataSize(mControl, kControlNoPart, kControlEditTextTextTag, &textSize);

	nsString textStr;
	PRUint32 outTextSize;
	nsresult retVal = GetText(textStr, textSize, outTextSize);
	if (retVal != NS_OK)
		return retVal;

	if (((PRInt32)aStartPos == -1L) && ((PRInt32)aEndPos == -1L))
	{
	  textStr.Append(aText);
	}
	else
	{
		if ((PRInt32)aStartPos < 0)
			aStartPos = 0;
		if ((PRInt32)aEndPos < 0)
			aStartPos = 0;
		if (aEndPos < aStartPos)
			aEndPos = aStartPos;
		textStr.Cut(aStartPos, aEndPos - aStartPos);
		textStr.Insert(aText, aStartPos);
	}

  aSize = textStr.Length();
	return (SetText(textStr, aSize));
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD  nsTextWidget::RemoveText()
{
	if (!mControl)
		return NS_ERROR_NOT_INITIALIZED;

	StartDraw();
	::SetControlData(mControl, kControlNoPart, kControlEditTextTextTag, 0, (Ptr)"");
	EndDraw();
  return NS_OK;
}

#pragma mark -
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsTextWidget::SelectAll()
{
	if (!mControl)
		return NS_ERROR_NOT_INITIALIZED;

	Size textSize;
	::GetControlDataSize(mControl, kControlNoPart, kControlEditTextTextTag, &textSize);

  return (SetSelection(0, textSize));
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD  nsTextWidget::SetSelection(PRUint32 aStartSel, PRUint32 aEndSel)
{
	if (!mControl)
		return NS_ERROR_NOT_INITIALIZED;

	ControlEditTextSelectionRec textSelectionRec;
	textSelectionRec.selStart = aStartSel;
	textSelectionRec.selEnd   = aEndSel;

	StartDraw();
	::SetControlData(mControl, kControlNoPart, kControlEditTextSelectionTag,
											sizeof(textSelectionRec), (Ptr)&textSelectionRec);
	EndDraw();

  return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD  nsTextWidget::GetSelection(PRUint32 *aStartSel, PRUint32 *aEndSel)
{
	if (!mControl)
		return NS_ERROR_NOT_INITIALIZED;

	ControlEditTextSelectionRec textSelectionRec;

	Size dataSize = sizeof(textSelectionRec);
	::GetControlData(mControl, kControlNoPart, kControlEditTextSelectionTag,
											dataSize, (Ptr)&textSelectionRec, &dataSize);

	*aStartSel = textSelectionRec.selStart;
	*aEndSel = textSelectionRec.selEnd;

  return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD  nsTextWidget::SetCaretPosition(PRUint32 aPosition)
{
	return(SetSelection(aPosition, aPosition));
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_METHOD nsTextWidget::GetCaretPosition(PRUint32& aPos)
{
	PRUint32 startSel, endSel;
	nsresult retVal = GetSelection(&startSel, &endSel);
	aPos = startSel;
  return retVal;
}

#pragma mark -
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void	nsTextWidget::RepeatAction(const EventRecord& inMacEvent)
{
	if (mControl)
	{
		StartDraw();
		// This should really live in the window but
		// we have to set the graphic environment
		IdleControls(mWindowPtr);
		EndDraw();
	}
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsTextWidget::GetRectForMacControl(nsRect &outRect)
{
	outRect = mBounds;
	outRect.x = outRect.y = 0;
	// inset to make space for border
	if (mIsReadOnly)
		outRect.Deflate(1, 1);
	else
		outRect.Deflate(4, 4);
}

