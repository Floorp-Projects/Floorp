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
#ifndef MacMacEventHandler_h__
#define MacMacEventHandler_h__

#include <Events.h>
#include <MacWindows.h>
#include <TextServices.h>
#include "nsDeleteObserver.h"
#include "nsCOMPtr.h"
#include "nsGUIEvent.h"
#include "prtypes.h"


class nsWindow;
class nsMacWindow;

class nsMacEventHandler : public nsDeleteObserver
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
		virtual long 	HandlePositionToOffset(Point aPoint,short* regionClass);
		virtual PRBool 	HandleOffsetToPosition(long offset,Point* position);
		virtual PRBool	HandleUpdate(Handle textHandle,ScriptCode script,long fixedLength);
		
protected:
		virtual PRBool	HandleKeyEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleActivateEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleUpdateEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleMouseDownEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleMouseUpEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleMouseMoveEvent(EventRecord& aOSEvent);

		virtual void	ConvertOSEventToMouseEvent(
									EventRecord&	aOSEvent,
									nsMouseEvent&	aMouseEvent,
									PRUint32		aMessage);
		virtual PRBool	HandleStartComposition(void);
		virtual PRBool	HandleEndComposition(void);
		virtual PRBool  HandleTextEvent(void);

public:
	virtual void	NotifyDelete(void* aDeletedObject);

protected:
	nsMacWindow*	mTopLevelWidget;
	nsWindow*			mLastWidgetHit;
	PRBool				mMouseInWidgetHit;
	nsWindow*			mLastWidgetPointed;
	RgnHandle			mUpdateRgn;
	TSMDocumentID		mTSMDocument;
	PRBool				mIMEIsComposing;
	PRUnichar			*mIMECompositionString;
	size_t				mIMECompositionStringSize;
	size_t				mIMECompositionStringLength;
  static PRBool	mInBackground;

};

#endif // MacMacEventHandler_h__
