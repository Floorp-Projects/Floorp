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

#include <ConditionalMacros.h>
#include <Events.h>
#include <MacWindows.h>
#include <TextServices.h>
#include <Controls.h>
#include "prtypes.h"
#include "nsCOMPtr.h"
#include "nsGUIEvent.h"
#include "nsDeleteObserver.h"
#include "nsString.h"

class nsWindow;
class nsMacWindow;


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

class nsMacEventDispatchHandler : public nsDeleteObserver
{
public:
	nsMacEventDispatchHandler();
	virtual			~nsMacEventDispatchHandler();

	void			DispatchGuiEvent(nsWindow *aWidget, PRUint32 aEventType);
	void			DispatchSizeModeEvent(nsWindow *aWidget, nsSizeMode aMode);

    void 			SetFocus(nsWindow *aFocusedWidget);

	void 			SetActivated(nsWindow *aActiveWidget);
	nsWindow*		GetActive()	{return(mActiveWidget);}
	void			SetDeactivated(nsWindow *aActiveWidget);

	void 			SetWidgetHit(nsWindow *aWidgetHit);
	void 			SetWidgetPointed(nsWindow *aWidgetPointed);

	nsWindow*		GetWidgetHit()		{return(mWidgetHit);}
	nsWindow*		GetWidgetPointed()	{return(mWidgetPointed);}

	// DeleteObserver
	virtual void	NotifyDelete(void* aDeletedObject);

private:

	nsWindow*	mActiveWidget;
	nsWindow*	mWidgetHit;
	nsWindow*	mWidgetPointed;
};


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

extern nsMacEventDispatchHandler	gEventDispatchHandler;


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
		virtual nsresult 	HandleOffsetToPosition(long offset,Point* position);
		virtual nsresult	HandleUpdateInputArea(char* text,Size text_size, ScriptCode textScript,long fixedLength,TextRangeArray* textRangeArray);
		virtual nsresult	ResetInputState();
		
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
		virtual nsresult	HandleStartComposition(void);
		virtual nsresult	HandleEndComposition(void);
		virtual nsresult  HandleTextEvent(PRUint32 textRangeCount, nsTextRangeArray textRangeArray);

protected:
	static PRBool	sMouseInWidgetHit;
  static PRBool	sInBackground;

  ControlActionUPP mControlActionProc;

	nsMacWindow*	mTopLevelWidget;
	RgnHandle			mUpdateRgn;
	TSMDocumentID	mTSMDocument;
	nsPoint 		mIMEPos;
	PRBool				mIMEIsComposing;
	nsAutoString		*mIMECompositionStr;

};

#endif // MacMacEventHandler_h__
