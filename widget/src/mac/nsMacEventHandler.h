/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
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


#if UNIVERSAL_INTERFACES_VERSION < 0x0337
enum {
  kEventMouseWheelAxisX         = 0,
  kEventMouseWheelAxisY         = 1
};
typedef UInt16                          EventMouseWheelAxis;
#endif


#if !TARGET_CARBON
//
// struct PhantomScrollbarData
//
// When creating the phantom scrollbar for a Gecko instance, create
// one of these structures and stick it in the control's refCon. It 
// is used not only to identify our scrollbar from any others, but
// also to pass data to the scrollbar's action proc about which
// widget is the one the mouse is over.
//
struct PhantomScrollbarData
{
  PhantomScrollbarData ( ) 
    : mTag(kUniqueTag), mWidgetToGetEvent(nsnull) { }
  
  enum ResType { kUniqueTag = 'mozz' };
  
  ResType mTag;                     // should always be kUniqueTag
  nsIWidget* mWidgetToGetEvent;     // for the action proc, the widget to get the event
}; 
#endif


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
		virtual nsresult	UnicodeHandleUpdateInputArea(PRUnichar* text, long charCount, long fixedLength,TextRangeArray* textRangeArray);
		virtual nsresult	ResetInputState();
		virtual PRBool		HandleUKeyEvent(PRUnichar* text, long charCount, EventRecord& aOSEvent);
		
		//
		// Synthetic events, generated internally to do things at specific times and
		// not have to rely on hacking up EventRecords to fake it.
		//
		virtual PRBool UpdateEvent ( ) ;
		virtual PRBool ResizeEvent ( WindowRef inWindow ) ;
		virtual PRBool Scroll ( EventMouseWheelAxis inAxis, PRInt32 inDelta, const Point& inMouseLoc );
		 
protected:
#if 1
		virtual void InitializeKeyEvent(nsKeyEvent& aKeyEvent, EventRecord& aOSEvent, 
                              nsWindow* aFocusedWidget, PRUint32 aMessage, 
                              PRBool* aIsChar=nsnull, PRBool aConvertChar=PR_TRUE);
		virtual PRBool		IsSpecialRaptorKey(UInt32 macKeyCode);
		virtual PRUint32	ConvertKeyEventToUnicode(EventRecord& aOSEvent);
#endif
		virtual PRBool	HandleKeyEvent(EventRecord& aOSEvent);
		virtual PRBool	HandleActivateEvent(EventRecord& aOSEvent);
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

#if !TARGET_CARBON
  ControlActionUPP mControlActionProc;
#endif
  
	nsMacWindow*	mTopLevelWidget;
	RgnHandle			mUpdateRgn;
	TSMDocumentID	mTSMDocument;
	nsPoint 		mIMEPos;
	PRBool				mIMEIsComposing;
	nsAutoString		*mIMECompositionStr;
};

#endif // MacMacEventHandler_h__
