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

#include "nsMacEventHandler.h"

#include "nsWindow.h"
#include "nsMacWindow.h"
#include "nsToolkit.h"
#include "prinrval.h"

#include <ToolUtils.h>
#include <DiskInit.h>
#include <LowMem.h>
#include <TextServices.h>
#include <UnicodeConverter.h>
#include <Script.h>

// from MacHeaders.c
#ifndef topLeft
	#define topLeft(r)	(((Point *) &(r))[0])
#endif
#ifndef botRight
	#define botRight(r)	(((Point *) &(r))[1])
#endif

PRBool	nsMacEventHandler::mInBackground = PR_FALSE;

//-------------------------------------------------------------------------
//
// nsMacEventHandler constructor/destructor
//
//-------------------------------------------------------------------------
nsMacEventHandler::nsMacEventHandler(nsMacWindow* aTopLevelWidget)
{
	OSErr	err;
	InterfaceTypeList supportedServices;
	
	mTopLevelWidget			= aTopLevelWidget;
	mLastWidgetHit			= nsnull;
	mLastWidgetPointed	= nsnull;
	mTSMDocument				= nsnull;
	
	//
	// create a TSMDocument for this window.  We are allocating a TSM document for
	// each Mac window
	//
	supportedServices[0] = kTextService;
	err = ::NewTSMDocument(1,supportedServices,&mTSMDocument,(long)this);
	NS_ASSERTION(err==noErr,"nsMacEventHandler::nsMacEventHandler: NewTSMDocument failed.");

	printf("nsMacEventHandler::nsMacEventHandler: created TSMDocument[%p]\n",mTSMDocument);
		
	mIMEIsComposing = PR_FALSE;
	mIMECompositionString = nsnull;
	mIMECompositionStringSize = 0;
	mIMECompositionStringLength = 0;

}


nsMacEventHandler::~nsMacEventHandler()
{
	if (mLastWidgetPointed)
		mLastWidgetPointed->RemoveDeleteObserver(this);

	if (mLastWidgetHit)
		mLastWidgetHit->RemoveDeleteObserver(this);
		
	if (mTSMDocument)
		(void)::DeleteTSMDocument(mTSMDocument);
}


void nsMacEventHandler::NotifyDelete(void* aDeletedObject)
{
	if (mLastWidgetPointed == aDeletedObject)
		mLastWidgetPointed = nsnull;

	if (mLastWidgetHit == aDeletedObject)
		mLastWidgetHit = nsnull;
}


#pragma mark -
//-------------------------------------------------------------------------
//
// HandleOSEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleOSEvent ( EventRecord& aOSEvent )
{
	PRBool retVal = PR_FALSE;

	switch (aOSEvent.what)
	{
		case keyUp:
		case keyDown:
		case autoKey:
			retVal = HandleKeyEvent(aOSEvent);
			break;

		case activateEvt:
			retVal = HandleActivateEvent(aOSEvent);
			break;

		case updateEvt:
			retVal = HandleUpdateEvent(aOSEvent);
			break;

		case mouseDown:
			retVal = HandleMouseDownEvent(aOSEvent);
			break;

		case mouseUp:
			retVal = HandleMouseUpEvent(aOSEvent);
			break;

		case osEvt:
			unsigned char eventType = ((aOSEvent.message >> 24) & 0x00ff);
			if (eventType == suspendResumeMessage)
			{
				if ((aOSEvent.message & 1) == resumeFlag)
					mInBackground = PR_FALSE;		// resume message
				else
					mInBackground = PR_TRUE;		// suspend message
			}
			else {
				if (! mInBackground)
					retVal = HandleMouseMoveEvent(aOSEvent);
			}
			break;
	
		case nullEvent:
			if (! mInBackground)
				retVal = HandleMouseMoveEvent(aOSEvent);
			break;
	}

	return retVal;
}


//-------------------------------------------------------------------------
//
// Handle Menu commands
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMenuCommand(
  EventRecord& aOSEvent,
  long         aMenuResult)
{
	// get the focused widget
	nsWindow* focusedWidget = mTopLevelWidget;
	nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)mTopLevelWidget->GetToolkit()) );
	if (toolkit)
		focusedWidget = toolkit->GetFocus();
	
	if (!focusedWidget)
	{
	  NS_WARNING("Throwing away menu event because there is no focused widget");
		return PR_FALSE;
	}
	
	// nsEvent
	nsMenuEvent menuEvent;
	menuEvent.eventStructType = NS_MENU_EVENT;
	menuEvent.message		= NS_MENU_SELECTED;
	menuEvent.point.x		= aOSEvent.where.h;
	menuEvent.point.y		= aOSEvent.where.v;
	menuEvent.time			= PR_IntervalNow();

	// nsGUIEvent
	menuEvent.widget		= focusedWidget;
	menuEvent.nativeMsg	= (void*)&aOSEvent;

	// nsMenuEvent
	menuEvent.mMenuItem	= nsnull;						//¥TODO: initialize mMenuItem
	menuEvent.mCommand	= aMenuResult;

	// dispatch the menu event: if it is not processed by the focused widget,
	// propagate the event through the different parents all the way up to the window
	PRBool eventHandled = focusedWidget->DispatchWindowEvent(menuEvent);
	
	if (! eventHandled)
	{
		nsCOMPtr<nsWindow> grandParent;
		nsCOMPtr<nsWindow> parent ( dont_AddRef((nsWindow*)focusedWidget->GetParent()) );
		while (parent)
		{
			menuEvent.widget = parent;
			eventHandled = parent->DispatchWindowEvent(menuEvent);
			if (eventHandled)
			{
				break;
			}
			else
			{
				grandParent = dont_AddRef((nsWindow*)parent->GetParent());
				parent = grandParent;
			}
		}
	}

	return eventHandled;
}


//-------------------------------------------------------------------------
//
// DragEvent
//
//-------------------------------------------------------------------------
//
// Someone on the outside told us that something related to a drag is happening. The
// exact event type is passed in as |aMessage|. We need to send this event into Gecko 
// for processing. Create a Gecko event (using the appropriate message type) and pass
// it along.
//
// ¥¥¥THIS REALLY NEEDS TO BE CLEANED UP! TOO MUCH CODE COPIED FROM ConvertOSEventToMouseEvent
//
PRBool nsMacEventHandler::DragEvent ( unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers )
{
	nsMouseEvent geckoEvent;

	// convert the mouse to local coordinates. We have to do all the funny port origin
	// stuff just in case it has been changed.
	Point hitPointLocal = aMouseGlobal;
#if TARGET_CARBON
	GrafPtr grafPort = reinterpret_cast<GrafPtr>(mTopLevelWidget->GetNativeData(NS_NATIVE_GRAPHIC));
	::SetPort(grafPort);
	Rect savePortRect;
	::GetPortBounds(grafPort, &savePortRect);
#else
	GrafPtr grafPort = static_cast<GrafPort*>(mTopLevelWidget->GetNativeData(NS_NATIVE_GRAPHIC));
	::SetPort(grafPort);
	Rect savePortRect = grafPort->portRect;
#endif
	::SetOrigin(0, 0);
	::GlobalToLocal(&hitPointLocal);
	::SetOrigin(savePortRect.left, savePortRect.top);
	nsPoint widgetHitPoint(hitPointLocal.h, hitPointLocal.v);

	nsWindow* widgetHit = mTopLevelWidget->FindWidgetHit(hitPointLocal);
	if ( widgetHit ) {
		// adjust from local coordinates to window coordinates in case the top level widget
		// isn't at 0, 0
		nsRect bounds;
		mTopLevelWidget->GetBounds(bounds);
		nsPoint widgetOrigin(bounds.x, bounds.y);
		widgetHit->LocalToWindowCoordinate(widgetOrigin);
		widgetHitPoint.MoveBy(-widgetOrigin.x, -widgetOrigin.y);
	}
	else
	  widgetHit = mTopLevelWidget;
		
	// nsEvent
	geckoEvent.eventStructType = NS_DRAGDROP_EVENT;
	geckoEvent.message = aMessage;
	geckoEvent.point = widgetHitPoint;
	geckoEvent.time	= PR_IntervalNow();

	// nsGUIEvent
	geckoEvent.widget = widgetHit;
	geckoEvent.nativeMsg = nsnull;

	// nsInputEvent
	geckoEvent.isShift = ((aKeyModifiers & shiftKey) != 0);
	geckoEvent.isControl = ((aKeyModifiers & controlKey) != 0);
	geckoEvent.isAlt = ((aKeyModifiers & optionKey) != 0);
	geckoEvent.isCommand = ((aKeyModifiers & cmdKey) != 0);

	// nsMouseEvent
	geckoEvent.clickCount = 1;
	
	widgetHit->DispatchMouseEvent(geckoEvent);
	
	return PR_TRUE;
	
} // DropOccurred


#pragma mark -

//-------------------------------------------------------------------------
//
// ConvertMacToRaptorKeyCode
//
//-------------------------------------------------------------------------


// Key code constants
enum
{
	kEscapeKeyCode			= 0x35,
	kShiftKeyCode				= 0x38,
	kCapsLockKeyCode		= 0x39,
	kControlKeyCode			= 0x3B,
	kOptionkeyCode			= 0x3A,		// left and right option keys
	kClearKeyCode				= 0x47,
	
	// function keys
	kF1KeyCode					= 0x7A,
	kF2KeyCode					= 0x78,
	kF3KeyCode					= 0x63,
	kF4KeyCode					= 0x76,
	kF5KeyCode					= 0x60,
	kF6KeyCode					= 0x61,
	kF7KeyCode					= 0x62,
	kF8KeyCode					= 0x64,
	kF9KeyCode					= 0x65,
	kF10KeyCode					= 0x6D,
	kF11KeyCode					= 0x67,
	kF12KeyCode					= 0x6F,
	kF13KeyCode					= 0x69,
	kF14KeyCode					= 0x6B,
	kF15KeyCode					= 0x71,
	
	kPrintScreenKeyCode	= kF13KeyCode,
	kScrollLockKeyCode	= kF14KeyCode,
	kPauseKeyCode				= kF15KeyCode,
	
	// keypad
	kKeypad0KeyCode			= 0x52,
	kKeypad1KeyCode			= 0x53,
	kKeypad2KeyCode			= 0x54,
	kKeypad3KeyCode			= 0x55,
	kKeypad4KeyCode			= 0x56,
	kKeypad5KeyCode			= 0x57,
	kKeypad6KeyCode			= 0x58,
	kKeypad7KeyCode			= 0x59,
	kKeypad8KeyCode			= 0x5B,
	kKeypad9KeyCode			= 0x5C,
	
	kKeypadMultiplyKeyCode	= 0x43,
	kKeypadAddKeyCode				= 0x45,
	kKeypadSubtractKeyCode	= 0x4E,
	kKeypadDecimalKeyCode		= 0x41,
	kKeypadDivideKeyCode		= 0x4B,
	kKeypadEqualsKeyCode		= 0x51,			// no correpsonding raptor key code
	
	kInsertKeyCode					= 0x72,				// also help key
	kDeleteKeyCode					= 0x75,				// also forward delete key
	kTabKeyCode							= 0x30,
	kHomeKeyCode						= 0x73,	
	kEndKeyCode							= 0x77,
	kPageUpKeyCode					= 0x74,
	kPageDownKeyCode				= 0x79,
	kLeftArrowKeyCode				= 0x7B,
	kRightArrowKeyCode			= 0x7C,
	kUpArrowKeyCode					= 0x7E,
	kDownArrowKeyCode				= 0x7D
	
};

static PRUint32 ConvertMacToRaptorKeyCode(UInt32 eventMessage, UInt32 eventModifiers)
{
	UInt8			charCode = (eventMessage & charCodeMask);
	UInt8			keyCode = (eventMessage & keyCodeMask) >> 8;
	PRUint32	raptorKeyCode = 0;
	
	switch (keyCode)
	{
//	case ??							:				raptorKeyCode = NS_VK_CANCEL;		break;			// don't know what this key means. Nor does joki

// modifiers. We don't get separate events for these
		case kEscapeKeyCode:				raptorKeyCode = NS_VK_ESCAPE;					break;
		case kShiftKeyCode:					raptorKeyCode = NS_VK_SHIFT;					break;
		case kCapsLockKeyCode:			raptorKeyCode = NS_VK_CAPS_LOCK;			break;
		case kControlKeyCode:				raptorKeyCode = NS_VK_CONTROL;				break;
		case kOptionkeyCode:				raptorKeyCode = NS_VK_ALT;						break;
		case kClearKeyCode:					raptorKeyCode = NS_VK_CLEAR;					break;

// function keys
		case kF1KeyCode:						raptorKeyCode = NS_VK_F1;							break;
		case kF2KeyCode:						raptorKeyCode = NS_VK_F2;							break;
		case kF3KeyCode:						raptorKeyCode = NS_VK_F3;							break;
		case kF4KeyCode:						raptorKeyCode = NS_VK_F4;							break;
		case kF5KeyCode:						raptorKeyCode = NS_VK_F5;							break;
		case kF6KeyCode:						raptorKeyCode = NS_VK_F6;							break;
		case kF7KeyCode:						raptorKeyCode = NS_VK_F7;							break;
		case kF8KeyCode:						raptorKeyCode = NS_VK_F8;							break;
		case kF9KeyCode:						raptorKeyCode = NS_VK_F9;							break;
		case kF10KeyCode:						raptorKeyCode = NS_VK_F10;						break;
		case kF11KeyCode:						raptorKeyCode = NS_VK_F11;						break;
		case kF12KeyCode:						raptorKeyCode = NS_VK_F12;						break;
//	case kF13KeyCode:						raptorKeyCode = NS_VK_F13;						break;		// clash with the 3 below
//	case kF14KeyCode:						raptorKeyCode = NS_VK_F14;						break;
//	case kF15KeyCode:						raptorKeyCode = NS_VK_F15;						break;
		case kPauseKeyCode:					raptorKeyCode = NS_VK_PAUSE;					break;
		case kScrollLockKeyCode:		raptorKeyCode = NS_VK_SCROLL_LOCK;		break;
		case kPrintScreenKeyCode:		raptorKeyCode = NS_VK_PRINTSCREEN;		break;
	
// keypad
		case kKeypad0KeyCode:				raptorKeyCode = NS_VK_NUMPAD0;				break;
		case kKeypad1KeyCode:				raptorKeyCode = NS_VK_NUMPAD1;				break;
		case kKeypad2KeyCode:				raptorKeyCode = NS_VK_NUMPAD2;				break;
		case kKeypad3KeyCode:				raptorKeyCode = NS_VK_NUMPAD3;				break;
		case kKeypad4KeyCode:				raptorKeyCode = NS_VK_NUMPAD4;				break;
		case kKeypad5KeyCode:				raptorKeyCode = NS_VK_NUMPAD5;				break;
		case kKeypad6KeyCode:				raptorKeyCode = NS_VK_NUMPAD6;				break;
		case kKeypad7KeyCode:				raptorKeyCode = NS_VK_NUMPAD7;				break;
		case kKeypad8KeyCode:				raptorKeyCode = NS_VK_NUMPAD8;				break;
		case kKeypad9KeyCode:				raptorKeyCode = NS_VK_NUMPAD9;				break;

		case kKeypadMultiplyKeyCode:	raptorKeyCode = NS_VK_MULTIPLY;			break;
		case kKeypadAddKeyCode:				raptorKeyCode = NS_VK_ADD;					break;
		case kKeypadSubtractKeyCode:	raptorKeyCode = NS_VK_SUBTRACT;			break;
		case kKeypadDecimalKeyCode:		raptorKeyCode = NS_VK_DECIMAL;			break;
		case kKeypadDivideKeyCode:		raptorKeyCode = NS_VK_DIVIDE;				break;
//	case ??								:				raptorKeyCode = NS_VK_SEPARATOR;		break;


// these may clash with forward delete and help
//	case kInsertKeyCode:				raptorKeyCode = NS_VK_INSERT;					break;
//	case kDeleteKeyCode:				raptorKeyCode = NS_VK_DELETE;					break;

		default:
		
				// if we haven't gotten the key code already, look at the char code
				switch (charCode)
				{
					case kReturnCharCode:				raptorKeyCode = NS_VK_RETURN;				break;
					case kEnterCharCode:				raptorKeyCode = NS_VK_RETURN;				break;			// fix me!
					case kBackspaceCharCode:		raptorKeyCode = NS_VK_BACK;					break;
					case kDeleteCharCode:				raptorKeyCode = NS_VK_DELETE;				break;
					case kTabCharCode:					raptorKeyCode = NS_VK_TAB;					break;

					case kHomeCharCode:					raptorKeyCode = NS_VK_HOME;					break;
					case kEndCharCode:					raptorKeyCode = NS_VK_END;					break;
					case kPageUpCharCode:				raptorKeyCode = NS_VK_PAGE_UP;			break;
					case kPageDownCharCode:			raptorKeyCode = NS_VK_PAGE_DOWN;		break;

					case kLeftArrowCharCode:		raptorKeyCode = NS_VK_LEFT;					break;
					case kRightArrowCharCode:		raptorKeyCode = NS_VK_RIGHT;				break;
					case kUpArrowCharCode:			raptorKeyCode = NS_VK_UP;						break;
					case kDownArrowCharCode:		raptorKeyCode = NS_VK_DOWN;					break;
					case ' ':										raptorKeyCode = NS_VK_SPACE;				break;
					case ';':										raptorKeyCode = NS_VK_SEMICOLON;		break;
					case '=':										raptorKeyCode = NS_VK_EQUALS;				break;
					case ',':										raptorKeyCode = NS_VK_COMMA;				break;
					case '.':										raptorKeyCode = NS_VK_PERIOD;				break;
					case '/':										raptorKeyCode = NS_VK_SLASH;				break;
					case '`':										raptorKeyCode = NS_VK_BACK_QUOTE;		break;
					case '{':
					case '[':										raptorKeyCode = NS_VK_OPEN_BRACKET;	break;
					case '\\':									raptorKeyCode = NS_VK_BACK_SLASH;		break;
					case '}':
					case ']':										raptorKeyCode = NS_VK_CLOSE_BRACKET;	break;
					case '\'':
					case '"':										raptorKeyCode = NS_VK_QUOTE;				break;
					
					default:
						
						if (charCode >= '0' && charCode <= '9')		// numerals
						{
							raptorKeyCode = charCode;
						}
						else if (charCode >= 'a' && charCode <= 'z')		// lowercase
						{
							raptorKeyCode = toupper(charCode);
						}
						else if (charCode >= 'A' && charCode <= 'Z')		// uppercase
						{
							raptorKeyCode = charCode;
						}

						break;
				}
	}

	return raptorKeyCode;
}


//-------------------------------------------------------------------------
//
// InitializeKeyEvent
//
//-------------------------------------------------------------------------

void nsMacEventHandler::InitializeKeyEvent(nsKeyEvent& aKeyEvent, EventRecord& aOSEvent, nsWindow* focusedWidget, PRUint32 message)
{
	//
	// initalize the basic message parts
	//
	aKeyEvent.eventStructType = NS_KEY_EVENT;
	aKeyEvent.message 		= message;
	aKeyEvent.point.x			= 0;
	aKeyEvent.point.y			= 0;
	aKeyEvent.time				= PR_IntervalNow();
	
	//
	// initalize the GUI event parts
	//
	aKeyEvent.widget			= focusedWidget;
	aKeyEvent.nativeMsg		= (void*)&aOSEvent;
	
	//
	// nsInputEvent parts
	//
	aKeyEvent.isShift			= ((aOSEvent.modifiers & shiftKey) != 0);
	aKeyEvent.isControl		= ((aOSEvent.modifiers & controlKey) != 0);
	aKeyEvent.isAlt				= ((aOSEvent.modifiers & optionKey) != 0);
	aKeyEvent.isCommand		= ((aOSEvent.modifiers & cmdKey) != 0);
	
	//
	// nsKeyEvent parts
	//
	aKeyEvent.keyCode		= ConvertMacToRaptorKeyCode(aOSEvent.message, aOSEvent.modifiers);
	aKeyEvent.charCode	= aOSEvent.message & charCodeMask;		// will be translated to Unicode, see ConvertKeyEventToUnicode
}


//-------------------------------------------------------------------------
//
// IsSpecialRaptorKey
//
//-------------------------------------------------------------------------

PRBool nsMacEventHandler::IsSpecialRaptorKey(UInt32 macKeyCode)
{
	PRBool	isSpecial;

	// 
	// this table is used to make the macintosh virtual key generation behave the same
	// as windows and linux
	//	
	switch (macKeyCode)
	{
// modifiers. We don't get separate events for these
		case kEscapeKeyCode:				isSpecial = PR_TRUE; break;
		case kShiftKeyCode:					isSpecial = PR_TRUE; break;
		case kCapsLockKeyCode:			isSpecial = PR_TRUE; break;
		case kControlKeyCode:				isSpecial = PR_TRUE; break;
		case kOptionkeyCode:				isSpecial = PR_TRUE; break;
		case kClearKeyCode:					isSpecial = PR_TRUE; break;

// function keys
		case kF1KeyCode:					isSpecial = PR_TRUE; break;
		case kF2KeyCode:					isSpecial = PR_TRUE; break;
		case kF3KeyCode:					isSpecial = PR_TRUE; break;
		case kF4KeyCode:					isSpecial = PR_TRUE; break;
		case kF5KeyCode:					isSpecial = PR_TRUE; break;
		case kF6KeyCode:					isSpecial = PR_TRUE; break;
		case kF7KeyCode:					isSpecial = PR_TRUE; break;
		case kF8KeyCode:					isSpecial = PR_TRUE; break;
		case kF9KeyCode:					isSpecial = PR_TRUE; break;
		case kF10KeyCode:					isSpecial = PR_TRUE; break;
		case kF11KeyCode:					isSpecial = PR_TRUE; break;
		case kF12KeyCode:					isSpecial = PR_TRUE; break;
		case kPauseKeyCode:				isSpecial = PR_TRUE; break;
		case kScrollLockKeyCode:	isSpecial = PR_TRUE; break;
		case kPrintScreenKeyCode:	isSpecial = PR_TRUE; break;
	
// keypad
		case kKeypad0KeyCode:				isSpecial = PR_TRUE; break;
		case kKeypad1KeyCode:				isSpecial = PR_TRUE; break;
		case kKeypad2KeyCode:				isSpecial = PR_TRUE; break;
		case kKeypad3KeyCode:				isSpecial = PR_TRUE; break;
		case kKeypad4KeyCode:				isSpecial = PR_TRUE; break;
		case kKeypad5KeyCode:				isSpecial = PR_TRUE; break;
		case kKeypad6KeyCode:				isSpecial = PR_TRUE; break;
		case kKeypad7KeyCode:				isSpecial = PR_TRUE; break;
		case kKeypad8KeyCode:				isSpecial = PR_TRUE; break;
		case kKeypad9KeyCode:				isSpecial = PR_TRUE; break;

		case kKeypadMultiplyKeyCode:		isSpecial = PR_TRUE; break;
		case kKeypadAddKeyCode:					isSpecial = PR_TRUE; break;
		case kKeypadSubtractKeyCode:		isSpecial = PR_TRUE; break;
		case kKeypadDecimalKeyCode:			isSpecial = PR_TRUE; break;
		case kKeypadDivideKeyCode:			isSpecial = PR_TRUE; break;	


		case kDeleteKeyCode:				isSpecial = PR_TRUE; break;
		case kTabKeyCode:						isSpecial = PR_TRUE; break;

		case kHomeKeyCode:					isSpecial = PR_TRUE; break;	
		case kEndKeyCode:						isSpecial = PR_TRUE; break;
		case kPageUpKeyCode:				isSpecial = PR_TRUE; break;
		case kPageDownKeyCode:			isSpecial = PR_TRUE; break;
		case kLeftArrowKeyCode:			isSpecial = PR_TRUE; break;
		case kRightArrowKeyCode:		isSpecial = PR_TRUE; break;
		case kUpArrowKeyCode:				isSpecial = PR_TRUE; break;
		case kDownArrowKeyCode:			isSpecial = PR_TRUE; break;
		
		default:							isSpecial = PR_FALSE; break;
	}
	return isSpecial;
}


//-------------------------------------------------------------------------
//
// ConvertKeyEventToUnicode
//
//-------------------------------------------------------------------------

void nsMacEventHandler::ConvertKeyEventToUnicode(nsKeyEvent& aKeyEvent, EventRecord& aOSEvent)
{
	aKeyEvent.charCode = 0;

	char charResult = aOSEvent.message & charCodeMask;
	
	//
	// get the script of text for Unicode conversion
	//
	ScriptCode textScript = (ScriptCode)GetScriptManagerVariable(smKeyScript);

	//
	// convert our script code (smKeyScript) to a TextEncoding 
	//
	TextEncoding textEncodingFromScript;
	OSErr err = ::UpgradeScriptInfoToTextEncoding(textScript, kTextLanguageDontCare, kTextRegionDontCare, nsnull,
											&textEncodingFromScript);
	NS_ASSERTION(err == noErr, "nsMacEventHandler::ConvertKeyEventToUnicode: UpgradeScriptInfoToTextEncoding failed.");
	if (err != noErr) return;
	
	TextToUnicodeInfo	textToUnicodeInfo;
	err = ::CreateTextToUnicodeInfoByEncoding(textEncodingFromScript,&textToUnicodeInfo);
	NS_ASSERTION(err == noErr, "nsMacEventHandler::ConvertKeyEventToUnicode: CreateUnicodeToTextInfoByEncoding failed.");
	if (err != noErr) return;

	//
	// convert to Unicode
	//
	ByteCount result_size, source_read;
	PRUnichar unicharResult;
	err = ::ConvertFromTextToUnicode(textToUnicodeInfo,
									sizeof(char),&charResult,
									kUnicodeLooseMappingsMask,
									0,NULL,NULL,NULL,
									sizeof(PRUnichar),&source_read,
									&result_size,&unicharResult);
	::DisposeTextToUnicodeInfo(&textToUnicodeInfo);
	NS_ASSERTION(err == noErr, "nsMacEventHandler::ConvertKeyEventToUnicode: ConverFromTextToUnicode failed.");
	if (err != noErr) return;

	aKeyEvent.charCode = unicharResult;
}


//-------------------------------------------------------------------------
//
// HandleKeyEvent
//
//-------------------------------------------------------------------------

PRBool nsMacEventHandler::HandleKeyEvent(EventRecord& aOSEvent)
{
	nsresult result;

	// get the focused widget
	nsWindow* focusedWidget = mTopLevelWidget;	
	nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)mTopLevelWidget->GetToolkit()) );
	if (toolkit)
		focusedWidget = toolkit->GetFocus();
	
	if (!focusedWidget)
	{
  	NS_WARNING("Throwing away key event because there is no focused widget");
		return PR_FALSE;
	}
	
	// nsEvent
	nsKeyEvent	keyEvent;
	switch (aOSEvent.what)
	{
		case keyUp:
			InitializeKeyEvent(keyEvent,aOSEvent,focusedWidget,NS_KEY_UP);
			result = focusedWidget->DispatchWindowEvent(keyEvent);
			break;

		case keyDown:	
		case autoKey:
			InitializeKeyEvent(keyEvent,aOSEvent,focusedWidget,NS_KEY_DOWN);
			result = focusedWidget->DispatchWindowEvent(keyEvent);
			if (! IsSpecialRaptorKey((aOSEvent.message & keyCodeMask) >> 8))
			{
				InitializeKeyEvent(keyEvent,aOSEvent,focusedWidget,NS_KEY_PRESS);
				ConvertKeyEventToUnicode(keyEvent,aOSEvent);
				result = focusedWidget->DispatchWindowEvent(keyEvent);
			}
			break;
	}

	return result;
}

#pragma mark -
//-------------------------------------------------------------------------
//
// HandleActivateEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleActivateEvent(EventRecord& aOSEvent)
{
	OSErr	err;
	nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)mTopLevelWidget->GetToolkit()) );
	if (toolkit)
	{
		Boolean isActive = ((aOSEvent.modifiers & activeFlag) != 0);
		if (isActive)
		{
			//
			// Activate The TSMDocument associated with this handler
			//
			if (mTSMDocument)
				err = ::ActivateTSMDocument(mTSMDocument);
#if 0
			NS_ASSERTION(err==noErr,"nsMacEventHandler::HandleActivateEvent: ActivateTSMDocument failed");
			printf("nsEventHandler::HandleActivateEvent: ActivateTSMDocument[%p]\n",mTSMDocument);
#endif
			
			//¥TODO: retrieve the focused widget for that window
			
			nsWindow*	focusedWidget = mTopLevelWidget;
			toolkit->SetFocus(focusedWidget);
			nsIMenuBar* menuBar = focusedWidget->GetMenuBar();
			if (menuBar)
			{
			  MenuHandle menuHandle = nsnull;

			  //menuBar->GetNativeData((void *)menuHandle);
			  //::SetMenuBar((Handle)menuHandle);
			  menuBar->Paint();
			}
			else
			{
			  //¥TODO:	if the focusedWidget doesn't have a menubar,
			  //				look all the way up to the window
			  //				until one of the parents has a menubar
			}

			//¥TODO: set the menu bar here
		}
		else
		{
			//
			// Deactivate the TSMDocument assoicated with this EventHandler
			//
			if (mTSMDocument)
				err = ::DeactivateTSMDocument(mTSMDocument);
#if 0
			NS_ASSERTION(err==noErr,"nsMacEventHandler::HandleActivateEvent: DeactivateTSMDocument failed");
			printf("nsEventHandler::HandleActivateEvent: DeactivateTSMDocument[%p]\n",mTSMDocument);
#endif
			
			//¥TODO: save the focused widget for that window
			toolkit->SetFocus(nsnull);
	
			//nsIMenuBar* menuBarInterface = mTopLevelWidget->GetMenuBar();
			//if (menuBarInterface)
			//{
				//Handle menuBar = ::GetMenuBar(); // Get a copy of the menu list
				//menuBarInterface->SetNativeData((void*)menuBar);
			//}
		}
	}
	return PR_TRUE;
}


//-------------------------------------------------------------------------
//
// HandleUpdateEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleUpdateEvent(EventRecord& aOSEvent)
{
	mTopLevelWidget->HandleUpdateEvent();

	return PR_TRUE;
}


//-------------------------------------------------------------------------
//
// HandleMouseDownEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMouseDownEvent(
														EventRecord&		aOSEvent)
{
	PRBool retVal = PR_FALSE;

	WindowPtr		whichWindow;
	short partCode = ::FindWindow(aOSEvent.where, &whichWindow);

	switch (partCode)
	{
		case inDrag:
		{
			Point macPoint;
#if TARGET_CARBON
			Rect portRect;
			::GetPortBounds(GetWindowPort(whichWindow), &portRect);
			macPoint = topLeft(portRect);
#else
			macPoint = topLeft(whichWindow->portRect);
#endif
			::LocalToGlobal(&macPoint);
			mTopLevelWidget->Move(macPoint.h, macPoint.v);
			break;
		}

		case inGrow:
		{
#if TARGET_CARBON
			Rect macRect;
			::GetWindowPortBounds ( whichWindow, &macRect );
#else
			Rect macRect = whichWindow->portRect;
#endif
			::LocalToGlobal(&topLeft(macRect));
			::LocalToGlobal(&botRight(macRect));
			mTopLevelWidget->Resize(macRect.right - macRect.left + 1, macRect.bottom - macRect.top + 1, PR_FALSE);
			break;
		}

		case inGoAway:
		{
			mTopLevelWidget->Destroy();
			break;
		}

		case inContent:
		{
			nsMouseEvent mouseEvent;
			ConvertOSEventToMouseEvent(aOSEvent, mouseEvent, NS_MOUSE_LEFT_BUTTON_DOWN);
			nsWindow* widgetHit = (nsWindow*)mouseEvent.widget;
			if (widgetHit)
			{
				// set the focus on the widget hit, if it accepts it
				if (widgetHit->AcceptFocusOnClick())
				{
					nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)widgetHit->GetToolkit()) );
					if (toolkit)
						toolkit->SetFocus(widgetHit);
				}

				// dispatch the event
				retVal = widgetHit->DispatchMouseEvent(mouseEvent);
			}

			if (mLastWidgetHit)
				mLastWidgetHit->RemoveDeleteObserver(this);

			mLastWidgetHit = widgetHit;
			mMouseInWidgetHit = PR_TRUE;

			if (mLastWidgetHit)
				mLastWidgetHit->AddDeleteObserver(this);
			break;
		}


		case inZoomIn:
		case inZoomOut:
		{
			// Now that we have found the partcode it is ok to actually zoom the window
			ZoomWindow(whichWindow, partCode, (whichWindow == FrontWindow()));
			
#if TARGET_CARBON
			Rect macRect;
			::GetWindowPortBounds(whichWindow, &macRect);
#else
			Rect macRect = whichWindow->portRect;
#endif
			::LocalToGlobal(&topLeft(macRect));
			::LocalToGlobal(&botRight(macRect));
			mTopLevelWidget->Resize(macRect.right - macRect.left, macRect.bottom - macRect.top, PR_FALSE);
			break;
		}
	}
	return retVal;
}


//-------------------------------------------------------------------------
//
// HandleMouseUpEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMouseUpEvent(
														EventRecord&		aOSEvent)
{
	PRBool retVal = PR_FALSE;

	nsMouseEvent mouseEvent;
	ConvertOSEventToMouseEvent(aOSEvent, mouseEvent, NS_MOUSE_LEFT_BUTTON_UP);

	nsWindow* widgetReleased = (nsWindow*)mouseEvent.widget;
	if ((widgetReleased != nsnull) && (widgetReleased != mLastWidgetHit))
		retVal |= widgetReleased->DispatchMouseEvent(mouseEvent);

	if (mLastWidgetHit)
	{
		mLastWidgetHit->RemoveDeleteObserver(this);

		mouseEvent.widget = mLastWidgetHit;
		retVal |= mLastWidgetHit->DispatchMouseEvent(mouseEvent);

		mLastWidgetHit = nsnull;
	}

	return retVal;
}


//-------------------------------------------------------------------------
//
// HandleMouseMoveEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMouseMoveEvent(
														EventRecord&		aOSEvent)
{
	PRBool retVal = PR_FALSE;

	nsMouseEvent mouseEvent;
	ConvertOSEventToMouseEvent(aOSEvent, mouseEvent, NS_MOUSE_MOVE);

	if (mLastWidgetHit)
	{
		Point macPoint = aOSEvent.where;
		::GlobalToLocal(&macPoint);
		PRBool inWidgetHit = mLastWidgetHit->PointInWidget(macPoint);
		if (inWidgetHit != mMouseInWidgetHit)
		{
			mouseEvent.message = (inWidgetHit ? NS_MOUSE_ENTER : NS_MOUSE_EXIT);
			mMouseInWidgetHit = inWidgetHit;
		}
		retVal |= mLastWidgetHit->DispatchMouseEvent(mouseEvent);
	}
	else
	{
		nsWindow* widgetPointed = (nsWindow*)mouseEvent.widget;
		if (widgetPointed != mLastWidgetPointed)
		{
			if (mLastWidgetPointed)
			{
				mLastWidgetPointed->RemoveDeleteObserver(this);

				mouseEvent.widget = mLastWidgetPointed;
					mouseEvent.message = NS_MOUSE_EXIT;
					retVal |= mLastWidgetPointed->DispatchMouseEvent(mouseEvent);
					mLastWidgetPointed = nsnull;
				mouseEvent.widget = widgetPointed;
			}

			if (widgetPointed)
			{
				widgetPointed->AddDeleteObserver(this);

				mLastWidgetPointed = widgetPointed;
				mouseEvent.message = NS_MOUSE_ENTER;
				retVal |= widgetPointed->DispatchMouseEvent(mouseEvent);
			}
		}
		else
		{
			if (widgetPointed)
				retVal |= widgetPointed->DispatchMouseEvent(mouseEvent);
		}
	}

	return retVal;
}


#pragma mark -
//-------------------------------------------------------------------------
//
// ConvertOSEventToMouseEvent
//
//-------------------------------------------------------------------------
void nsMacEventHandler::ConvertOSEventToMouseEvent(
														EventRecord&		aOSEvent,
														nsMouseEvent&		aMouseEvent,
														PRUint32				aMessage)
{
		static long		lastWhen = 0;
		static Point	lastWhere = {0, 0};
		static short	lastClickCount = 0;

	// get the click count
	if (((aOSEvent.when - lastWhen) < ::LMGetDoubleTime())
		&& (abs(aOSEvent.where.h - lastWhere.h) < 5)
		&& (abs(aOSEvent.where.v - lastWhere.v) < 5)) 
	{
		if (aOSEvent.what == mouseDown)
		{
			lastClickCount ++;
			if (lastClickCount == 2)
				aMessage = NS_MOUSE_LEFT_DOUBLECLICK;
		}
	}
	else
	{
		if (! ::StillDown())
			lastClickCount = 0;
	}
	lastWhen  = aOSEvent.when;
	lastWhere = aOSEvent.where;

	// get the widget hit and the hit point inside that widget
	Point hitPoint = aOSEvent.where;
#if TARGET_CARBON
	GrafPtr grafPort = reinterpret_cast<GrafPtr>(mTopLevelWidget->GetNativeData(NS_NATIVE_GRAPHIC));
	::SetPort(grafPort);
	Rect savePortRect;
	::GetPortBounds(grafPort, &savePortRect);
#else
	GrafPtr grafPort = static_cast<GrafPort*>(mTopLevelWidget->GetNativeData(NS_NATIVE_GRAPHIC));
	::SetPort(grafPort);
	Rect savePortRect = grafPort->portRect;
#endif
	::SetOrigin(0, 0);
	::GlobalToLocal(&hitPoint);
	::SetOrigin(savePortRect.left, savePortRect.top);
	nsPoint widgetHitPoint(hitPoint.h, hitPoint.v);

	// if the mouse button is still down, send events to the last widget hit
	nsWindow* widgetHit = nsnull;
	if (mLastWidgetHit)
	{
 		if (::StillDown() || aMessage == NS_MOUSE_LEFT_BUTTON_UP)
	 		widgetHit = mLastWidgetHit;
	 	else
	 	{
	 		// Patch: some widgets can eat mouseUp events (text widgets in TEClick, sbars in TrackControl).
	 		// In that case, fall back to the normal case.
	 		mLastWidgetHit->RemoveDeleteObserver(this);
	 		mLastWidgetHit = nsnull;
	 	}
	}

	if (! widgetHit)
		widgetHit = mTopLevelWidget->FindWidgetHit(hitPoint);

	if (widgetHit)
	{
		nsRect bounds;
		widgetHit->GetBounds(bounds);
		nsPoint widgetOrigin(bounds.x, bounds.y);
		widgetHit->LocalToWindowCoordinate(widgetOrigin);
		widgetHitPoint.MoveBy(-widgetOrigin.x, -widgetOrigin.y);
	}

	// nsEvent
	aMouseEvent.eventStructType = NS_MOUSE_EVENT;
	aMouseEvent.message		= aMessage;
	aMouseEvent.point			= widgetHitPoint;
	aMouseEvent.time			= PR_IntervalNow();

	// nsGUIEvent
	aMouseEvent.widget		= widgetHit;
	aMouseEvent.nativeMsg	= (void*)&aOSEvent;

	// nsInputEvent
	aMouseEvent.isShift		= ((aOSEvent.modifiers & shiftKey) != 0);
	aMouseEvent.isControl	= ((aOSEvent.modifiers & controlKey) != 0);
	aMouseEvent.isAlt			= ((aOSEvent.modifiers & optionKey) != 0);
	aMouseEvent.isCommand	= ((aOSEvent.modifiers & cmdKey) != 0);

	// nsMouseEvent
	aMouseEvent.clickCount = lastClickCount;
}

//-------------------------------------------------------------------------
//
// HandlePositionToOffsetEvent
//
//-------------------------------------------------------------------------
long nsMacEventHandler::HandlePositionToOffset(Point aPoint,short* regionClass)
{
	*regionClass = kTSMOutsideOfBody;
	return 0;
}

//-------------------------------------------------------------------------
//
// HandleOffsetToPosition Event
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleOffsetToPosition(long offset,Point* thePoint)
{
	(*thePoint).v = 0;
	(*thePoint).h = 0;
	return PR_TRUE;
}

//-------------------------------------------------------------------------
//
// HandleUpdate Event
//
//-------------------------------------------------------------------------
 nsMacEventHandler::HandleUpdateInputArea(char* text,Size text_size, ScriptCode textScript,long fixedLength,TextRangeArray* textRangeList)
{
	TextToUnicodeInfo	textToUnicodeInfo;
	TextEncoding		textEncodingFromScript;
	ByteCount			source_read;
	ByteOffset			sourceOffset[2], destinationOffset[2];
	ItemCount			destinationLength;
	nsTextRangeArray	xpTextRangeArray;
	PRBool				rv;
	int					i;
	OSErr				err;
	
	//
	// if we aren't in composition mode alredy, signal the backing store w/ the mode change
	//	
	if (!mIMEIsComposing) {
		mIMEIsComposing = PR_TRUE;
		HandleStartComposition();
	}

	//
	// convert our script code (smKeyScript) to a TextEncoding 
	//
	err = ::UpgradeScriptInfoToTextEncoding(textScript,kTextLanguageDontCare,kTextRegionDontCare,nsnull,
											&textEncodingFromScript);
	NS_ASSERTION(err==noErr,"nsMacEventHandler::UpdateInputArea: UpgradeScriptInfoToTextEncoding failed.");
	if (err!=noErr) { return PR_FALSE; }
	
	err = ::CreateTextToUnicodeInfoByEncoding(textEncodingFromScript,&textToUnicodeInfo);
	NS_ASSERTION(err==noErr,"nsMacEventHandler::UpdateInputArea: CreateUnicodeToTextInfoByEncoding failed.");
	if (err!=noErr) { return PR_FALSE; }

	
	if (mIMECompositionStringSize < text_size+32) {
		mIMECompositionStringSize = text_size+32;
		if (mIMECompositionString!=nsnull) delete [] mIMECompositionString;
		mIMECompositionString = new PRUnichar[mIMECompositionStringSize];
	}

	//
	// build up the nsGUIEvent text range array and convert the destination string to Unicode
	//
	if (textRangeList!=NULL) {
	
		xpTextRangeArray = new nsTextRange[textRangeList->fNumOfRanges];
		NS_ASSERTION(xpTextRangeArray!=NULL,"nsMacEventHandler::UpdateInputArea: xpTextRangeArray memory allocation failed.");
		if (xpTextRangeArray==NULL) { ::DisposeTextToUnicodeInfo(&textToUnicodeInfo); return PR_FALSE; }

		//
		// the TEC offset mapping capabilities won't work here because you need to have unique, ordered offsets
		//  so instead we iterate over the range list and map each range individually.  it's probably faster than
		//  trying to do collapse all the ranges into a single offset list
		//
		for(i=0;i<textRangeList->fNumOfRanges;i++) {			
			
			sourceOffset[0] = textRangeList->fRange[i].fStart;
			sourceOffset[1] = textRangeList->fRange[i].fEnd;
			
			err = ::ConvertFromTextToUnicode(textToUnicodeInfo,text_size,text,kUnicodeLooseMappingsMask,
							2,sourceOffset,&destinationLength,destinationOffset,
							mIMECompositionStringSize*sizeof(PRUnichar),
							&source_read,&mIMECompositionStringLength,mIMECompositionString);
			NS_ASSERTION(err==noErr,"nsMacEventHandler::UpdateInputArea: ConvertFromTextToUnicode failed.\n");
			if (err!=noErr) { ::DisposeTextToUnicodeInfo(&textToUnicodeInfo); return PR_FALSE; }
			
			if (destinationLength==2) {
				xpTextRangeArray[i].mStartOffset = destinationOffset[0]/sizeof(PRUnichar);
				xpTextRangeArray[i].mEndOffset = destinationOffset[1]/sizeof(PRUnichar);
				xpTextRangeArray[i].mRangeType = textRangeList->fRange[i].fHiliteStyle;
			} else {
				xpTextRangeArray[i].mStartOffset = destinationOffset[0]/sizeof(PRUnichar);
				xpTextRangeArray[i].mEndOffset = destinationOffset[0]/sizeof(PRUnichar);
				xpTextRangeArray[i].mRangeType = textRangeList->fRange[i].fHiliteStyle;
			}			
		}
	} else {

		err = ::ConvertFromTextToUnicode(textToUnicodeInfo,text_size,text,kUnicodeLooseMappingsMask,
						0,NULL,NULL,NULL,
						mIMECompositionStringSize*sizeof(PRUnichar),
						&source_read,&mIMECompositionStringLength,mIMECompositionString);
		NS_ASSERTION(err==noErr,"nsMacEventHandler::UpdateInputArea: ConvertFromTextToUnicode failed.\n");
		if (err!=noErr) { ::DisposeTextToUnicodeInfo(&textToUnicodeInfo); return PR_FALSE; }

		xpTextRangeArray = new nsTextRange[1];
		xpTextRangeArray[0].mStartOffset = 0;
		xpTextRangeArray[0].mEndOffset = (mIMECompositionStringLength/sizeof(PRUnichar));
		xpTextRangeArray[0].mRangeType = NS_TEXTRANGE_RAWINPUT;		
	}

	//
	// null terminate the string for the XP-stuff
	//
	mIMECompositionString[mIMECompositionStringLength/sizeof(PRUnichar)] = (PRUnichar)0;

	if (textRangeList==NULL)
		rv = HandleTextEvent(1,xpTextRangeArray);
	else
		rv = HandleTextEvent(textRangeList->fNumOfRanges,xpTextRangeArray);
		
	::DisposeTextToUnicodeInfo(&textToUnicodeInfo);
	
	//
	// text_size incldues the null-terminator which isn't included in the fixedLength
	//
	if (fixedLength==-1 || fixedLength==text_size-1) {
		HandleEndComposition();
		mIMEIsComposing = PR_FALSE;
	}
	
	return rv;
}

//-------------------------------------------------------------------------
//
// HandleStartComposition
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleStartComposition(void)
{
	// 
	// get the focused widget [tague: may need to rethink this later]
	//
	nsWindow* focusedWidget = mTopLevelWidget;	
	nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)mTopLevelWidget->GetToolkit()) );
	if (toolkit)
		focusedWidget = toolkit->GetFocus();
	
	if (!focusedWidget)
	{
  	NS_WARNING("Throwing away start composition event because there is no focused widget");
		return PR_FALSE;
	}
	
	//
	// create the nsCompositionEvent
	//
	nsCompositionEvent		compositionEvent;

	compositionEvent.eventStructType = NS_COMPOSITION_START;
	compositionEvent.message = NS_COMPOSITION_START;
	compositionEvent.point.x = 0;
	compositionEvent.point.y = 0;
	compositionEvent.time = PR_IntervalNow();

	//
	// nsGUIEvent parts
	//
	compositionEvent.widget	= focusedWidget;
	compositionEvent.nativeMsg	= (void*)nsnull;		// no native message for this

	return(focusedWidget->DispatchWindowEvent(compositionEvent));
}

//-------------------------------------------------------------------------
//
// HandleEndComposition
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleEndComposition(void)
{
	// 
	// get the focused widget [tague: may need to rethink this later]
	//
	nsWindow* focusedWidget = mTopLevelWidget;	
	nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)mTopLevelWidget->GetToolkit()) );
	if (toolkit)
		focusedWidget = toolkit->GetFocus();
	
	if (!focusedWidget)
	{
  	NS_WARNING("Throwing away end composition event because there is no focused widget");
		return PR_FALSE;
	}
	
	//
	// create the nsCompositionEvent
	//
	nsCompositionEvent		compositionEvent;

	compositionEvent.eventStructType = NS_COMPOSITION_END;
	compositionEvent.message = NS_COMPOSITION_END;
	compositionEvent.point.x = 0;
	compositionEvent.point.y = 0;
	compositionEvent.time = PR_IntervalNow();

	//
	// nsGUIEvent parts
	//
	compositionEvent.widget	= focusedWidget;
	compositionEvent.nativeMsg	= (void*)nsnull;		// no native message for this

	return(focusedWidget->DispatchWindowEvent(compositionEvent));
}

//-------------------------------------------------------------------------
//
// HandleTextEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleTextEvent(PRUint32 textRangeCount, nsTextRangeArray textRangeArray)
{
	// 
	// get the focused widget [tague: may need to rethink this later]
	//
	nsWindow* focusedWidget = mTopLevelWidget;	
	nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)mTopLevelWidget->GetToolkit()) );
	if (toolkit)
		focusedWidget = toolkit->GetFocus();
	
	if (!focusedWidget)
	{
  	NS_WARNING("Throwing away text event because there is no focused widget");
		return PR_FALSE;
	}
	
	//
	// create the nsCompositionEvent
	//
	nsTextEvent		textEvent;

	textEvent.eventStructType = NS_TEXT_EVENT;
	textEvent.message = NS_TEXT_EVENT;
	textEvent.point.x = 0;
	textEvent.point.y = 0;
	textEvent.time = PR_IntervalNow();
	textEvent.theText = mIMECompositionString;
	textEvent.rangeCount = textRangeCount;
	textEvent.rangeArray = textRangeArray;
	
	//
	// nsGUIEvent parts
	//
	textEvent.widget	= focusedWidget;
	textEvent.nativeMsg	= (void*)nsnull;		// no native message for this

	if (NS_SUCCEEDED(focusedWidget->DispatchWindowEvent(textEvent))) 
		return PR_TRUE;
	else 
		return PR_FALSE;
}
