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
#ifndef MacMacEventHandler_h__
#define MacMacEventHandler_h__

#include <Events.h>
#include <MacWindows.h>
#include <TextServices.h>
#include "prtypes.h"
#include "nsCOMPtr.h"
#include "nsGUIEvent.h"
#include "nsDeleteObserver.h"

class nsWindow;
class nsMacWindow;


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

class nsMacFocusHandler : public nsDeleteObserver
{
public:
  nsMacFocusHandler();
  virtual				~nsMacFocusHandler();

	void 					SetFocus(nsWindow *aFocusedWidget);
	nsWindow*			GetFocus()	{return(mFocusedWidget);}

	void 					SetWidgetHit(nsWindow *aWidgetHit);
	void 					SetWidgetPointed(nsWindow *aWidgetPointed);

	nsWindow*			GetWidgetHit()			{return(mWidgetHit);}
	nsWindow*			GetWidgetPointed()	{return(mWidgetPointed);}

	// DeleteObserver
	virtual void	NotifyDelete(void* aDeletedObject);

private:
	nsWindow*			mFocusedWidget;
	nsWindow*			mWidgetHit;
	nsWindow*			mWidgetPointed;
};


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

extern nsMacFocusHandler	gFocusHandler;


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

class nsMacEventHandler
{
public:
		nsMacEventHandler(nsMacWindow* aTopLevelWidget);
		virtual ~nsMacEventHandler();

		virtual PRBool	HandleOSEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleMenuCommand(EventRecord& aOSEvent, long aMenuResult);
		
		// Tell Gecko that a drag event has occurred and should go into Gecko
		virtual PRBool	DragEvent ( unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers ) ;
		//virtual PRBool	TrackDrag ( Point aMouseGlobal, UInt32 aKeyModifiers ) ;

		//
		// TSM Event Handlers
		//
		virtual long 		HandlePositionToOffset(Point aPoint,short* regionClass);
		virtual PRBool 	HandleOffsetToPosition(long offset,Point* position);
		virtual PRBool	HandleUpdateInputArea(char* text,Size text_size, ScriptCode textScript,long fixedLength,TextRangeArray* textRangeArray);
		
protected:
#if 1
		virtual void			InitializeKeyEvent(nsKeyEvent& aKeyEvent, EventRecord& aOSEvent, nsWindow* focusedWidget, PRUint32 message);
		virtual PRBool		IsSpecialRaptorKey(UInt32 macKeyCode);
		virtual PRUint32	ConvertKeyEventToUnicode(EventRecord& aOSEvent);
#endif
		virtual PRBool	HandleKeyEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleActivateEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleUpdateEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleMouseDownEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleMouseUpEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleMouseMoveEvent(EventRecord& aOSEvent);

		virtual void		ConvertOSEventToMouseEvent(
												EventRecord&	aOSEvent,
												nsMouseEvent&	aMouseEvent,
												PRUint32		aMessage);
		virtual PRBool	HandleStartComposition(void);
		virtual PRBool	HandleEndComposition(void);
		virtual PRBool  HandleTextEvent(PRUint32 textRangeCount, nsTextRangeArray textRangeArray);

protected:
	static PRBool	mMouseInWidgetHit;
  static PRBool	mInBackground;

	nsMacWindow*	mTopLevelWidget;
	RgnHandle			mUpdateRgn;
	TSMDocumentID	mTSMDocument;
	PRBool				mIMEIsComposing;
	PRUnichar*		mIMECompositionString;
	size_t				mIMECompositionStringSize;
	size_t				mIMECompositionStringLength;
};

#endif // MacMacEventHandler_h__
