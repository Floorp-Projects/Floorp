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

#include "nsIFontMetrics.h"
#include "nsIDeviceContext.h"
#include "nsFont.h"
#include "nsFontUtils.h"
#include "nsToolkit.h"
#include "nsGfxUtils.h"

#include "nsMacControl.h"
#include "nsColor.h"
#include "nsFontMetricsMac.h"

#include "nsIServiceManager.h"
#include "nsIPlatformCharset.h"

#include <Carbon/Carbon.h>

#if 0
void DumpControlState(ControlHandle inControl, const char* message)
{
  if (!message) message = "gdb called";
  
  CGrafPtr curPort;
  ::GetPort((GrafPtr*)&curPort);
  Rect portBounds;
  ::GetPortBounds(curPort, &portBounds);

  Rect controlBounds = {0, 0, 0, 0};
  if (inControl)
    ::GetControlBounds(inControl, &controlBounds);
    
  printf("%20s -- port %p bounds %d, %d, %d, %d, control bounds %d, %d, %d, %d\n", message, curPort,
    portBounds.left, portBounds.top, portBounds.right, portBounds.bottom,
    controlBounds.left, controlBounds.top, controlBounds.right, controlBounds.bottom);
}
#endif


static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
// TODO: leaks, need to release when unloading the dll
nsIUnicodeEncoder * nsMacControl::mUnicodeEncoder = nsnull;
nsIUnicodeDecoder * nsMacControl::mUnicodeDecoder = nsnull;

#pragma mark -

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsMacControl::nsMacControl()
: mWidgetArmed(PR_FALSE)
, mMouseInButton(PR_FALSE)
, mValue(0)
, mMin(0)
, mMax(0)
, mControl(nsnull)
, mControlType(pushButProc)
, mControlEventHandler(nsnull)
, mWindowEventHandler(nsnull)
, mLastValue(0)
, mLastHilite(0)
{
  AcceptFocusOnClick(PR_FALSE);
}

/**-------------------------------------------------------------------------
 * See documentation in nsMacControl.h
 * @update -- 12/10/98 dwc
 */
NS_IMETHODIMP nsMacControl::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData) 
{
	Inherited::Create(aParent, aRect, aHandleEventFunction,
						aContext, aAppShell, aToolkit, aInitData);
  
	// create native control. mBounds has been set up at this point
	nsresult		theResult = CreateOrReplaceMacControl(mControlType);

	mLastBounds = mBounds;

	return theResult;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
nsMacControl::~nsMacControl()
{
	if (mControl)
	{
		Show(PR_FALSE);
		ClearControl();
		mControl = nsnull;
	}
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMacControl::Destroy()
{
	if (mOnDestroyCalled)
		return NS_OK;

	// Hide the control to avoid drawing.  Even if we're very careful
	// and avoid drawing the control ourselves after Destroy() is
	// called, the system still might draw it, and it might wind up
	// in the wrong location.
	Show(PR_FALSE);

	return Inherited::Destroy();
}

#pragma mark -
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsMacControl::OnPaint(nsPaintEvent &aEvent)
{
	if (mControl && mVisible)
	{
		// turn off drawing for setup to avoid ugliness
		Boolean		isVisible = ::IsControlVisible(mControl);
		::SetControlVisibility(mControl, false, false);

		// update title
		if (mLabel != mLastLabel)
		{
			NSStringSetControlTitle(mControl,mLabel);
		}

		// update bounds
		if (mBounds != mLastBounds)
		{
			nsRect ctlRect;
			GetRectForMacControl(ctlRect);
			Rect macRect;
			nsRectToMacRect(ctlRect, macRect);

			::SetControlBounds(mControl, &macRect);

			mLastBounds = mBounds;

#if 0
			// The widget rect can be larger than the control
			// so the rect can be erased here to set up the
			// background.  Unfortunately, the background color
			// isn't properly set up (bug 5685).
			//
			// Since the only native control in use at the moment
			// is the scrollbar, and the scrollbar does occupy
			// the entire widget rect, there's no need to erase
			// at all.
			nsRect bounds = mBounds;
			bounds.x = bounds. y = 0;
			nsRectToMacRect(bounds, macRect);
			::EraseRect(&macRect);
#endif
		}

		// update value
		if (mValue != mLastValue)
		{
			mLastValue = mValue;
			::SetControl32BitValue(mControl, mValue);
		}

		// update hilite
		SetupControlHiliteState();

		::SetControlVisibility(mControl, isVisible, false);

		// Draw the control
		::DrawOneControl(mControl);
	}
	return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool  nsMacControl::DispatchMouseEvent(nsMouseEvent &aEvent)
{
	PRBool eatEvent = PR_FALSE;
	switch (aEvent.message)
	{
		case NS_MOUSE_LEFT_DOUBLECLICK:
		case NS_MOUSE_LEFT_BUTTON_DOWN:
			if (mEnabled)
			{
				mMouseInButton = PR_TRUE;
				mWidgetArmed = PR_TRUE;
				Invalidate(PR_TRUE);
			}
			break;

		case NS_MOUSE_LEFT_BUTTON_UP:
			// if the widget was not armed, eat the event
			if (!mWidgetArmed)
				eatEvent = PR_TRUE;
			// if the mouseUp happened on another widget, eat the event too
			// (the widget which got the mouseDown is always notified of the mouseUp)
			if (! mMouseInButton)
				eatEvent = PR_TRUE;
			mWidgetArmed = PR_FALSE;
			if (mMouseInButton)
				Invalidate(PR_TRUE);
			break;

		case NS_MOUSE_EXIT:
			mMouseInButton = PR_FALSE;
			if (mWidgetArmed)
				Invalidate(PR_TRUE);
			break;

		case NS_MOUSE_ENTER:
			mMouseInButton = PR_TRUE;
			if (mWidgetArmed)
				Invalidate(PR_TRUE);
			break;
	}
	if (eatEvent)
		return PR_TRUE;
	return (Inherited::DispatchMouseEvent(aEvent));
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void  nsMacControl::ControlChanged(PRInt32 aNewValue)
{
	if (aNewValue != mValue)
	{
		mValue = aNewValue;
		mLastValue = mValue;	// safely assume that the control has been repainted already

		nsGUIEvent guiEvent(PR_TRUE, NS_CONTROL_CHANGE, this);
 		guiEvent.time	 	= PR_IntervalNow();
		Inherited::DispatchWindowEvent(guiEvent);
	}
}


#pragma mark -
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacControl::Enable(PRBool bState)
{
  PRBool priorState = mEnabled;
  Inherited::Enable(bState);
  if ( priorState != bState )
    Invalidate(PR_FALSE);
  return NS_OK;
}
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacControl::Show(PRBool bState)
{
  Inherited::Show(bState);
  if (mControl)
  {
  	::SetControlVisibility(mControl, bState, false);		// don't redraw
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this control font
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacControl::SetFont(const nsFont &aFont)
{
	Inherited::SetFont(aFont);	

	SetupMacControlFont();
	
 	return NS_OK;
}

#pragma mark -

//-------------------------------------------------------------------------
//
// Get the rect which the Mac control uses. This may be different for
// different controls, so this method allows overriding
//
//-------------------------------------------------------------------------
void nsMacControl::GetRectForMacControl(nsRect &outRect)
{
		outRect = mBounds;
		outRect.x = outRect.y = 0;
}

//-------------------------------------------------------------------------
//
// Get the current hilite state of the control
//
//-------------------------------------------------------------------------
ControlPartCode nsMacControl::GetControlHiliteState()
{
  // update hilite
  PRInt16 curHilite = kControlInactivePart;

  // Popups don't show up as active to the window manager, but if there's
  // a popup visible, its UI elements want to have an active appearance.
  PRBool isPopup = PR_FALSE;
  nsCOMPtr<nsIWidget> windowWidget;
  nsToolkit::GetTopWidget(mWindowPtr, getter_AddRefs(windowWidget));
  if (windowWidget) {
    nsWindowType windowType;
    if (NS_SUCCEEDED(windowWidget->GetWindowType(windowType)) &&
        windowType == eWindowType_popup) {
      isPopup = PR_TRUE;
    }
  }

  if (mEnabled && (isPopup || ::IsWindowActive(mWindowPtr)))
    if (mWidgetArmed && mMouseInButton)
      curHilite = kControlLabelPart;
    else
      curHilite = kControlNoPart;

  return curHilite;
}

void
nsMacControl::SetupControlHiliteState()
{
  PRInt16 curHilite = GetControlHiliteState();
  if (curHilite != mLastHilite) {
    mLastHilite = curHilite;
    ::HiliteControl(mControl, curHilite);
  }
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

nsresult nsMacControl::CreateOrReplaceMacControl(short inControlType)
{
  nsresult rv = NS_ERROR_NULL_POINTER;
  nsRect controlRect;
  GetRectForMacControl(controlRect);
  Rect macRect;
  nsRectToMacRect(controlRect, macRect);

  ClearControl();

  if (mWindowPtr) {
    mControl = ::NewControl(mWindowPtr, &macRect, "\p", PR_FALSE,
                            mValue, mMin, mMax, inControlType, nsnull);

    if (mControl) {
      InstallEventHandlerOnControl();
      SetupControlHiliteState();

      // need to reset the font now
      // XXX to do: transfer the text in the old control over too
      if (mFontMetrics)
        SetupMacControlFont();

      if (mVisible)
        ::ShowControl(mControl);

      rv = NS_OK;
    }
  }

  return rv;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsMacControl::ClearControl()
{
	RemoveEventHandlerFromControl();
	if (mControl)
	{
		::DisposeControl(mControl);
		mControl = nsnull;
	}
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsMacControl::SetupMacControlFont()
{
	NS_PRECONDITION(mFontMetrics != nsnull, "No font metrics in SetupMacControlFont");
	NS_PRECONDITION(mContext != nsnull, "No context metrics in SetupMacControlFont");
	
	TextStyle		theStyle;
	// if needed, impose a min size of 9pt on the control font
	if (theStyle.tsSize < 9)
		theStyle.tsSize = 9;
	
	ControlFontStyleRec fontStyleRec;
	fontStyleRec.flags = (kControlUseFontMask | kControlUseFaceMask | kControlUseSizeMask);
	fontStyleRec.font = theStyle.tsFont;
	fontStyleRec.size = theStyle.tsSize;
	fontStyleRec.style = theStyle.tsFace;
	::SetControlFontStyle(mControl, &fontStyleRec);
}

#pragma mark -

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

void nsMacControl::StringToStr255(const nsAString& aText, Str255& aStr255)
{
	nsresult rv = NS_OK;
	nsAString::const_iterator begin;
	const PRUnichar *text = aText.BeginReading(begin).get();

	// get file system charset and create a unicode encoder
	if (nsnull == mUnicodeEncoder) {
		nsCAutoString fileSystemCharset;
		GetFileSystemCharset(fileSystemCharset);

		nsCOMPtr<nsICharsetConverterManager> ccm = 
		         do_GetService(kCharsetConverterManagerCID, &rv); 
		if (NS_SUCCEEDED(rv)) {
			rv = ccm->GetUnicodeEncoderRaw(fileSystemCharset.get(), &mUnicodeEncoder);
            if (NS_SUCCEEDED(rv)) {
              rv = mUnicodeEncoder->SetOutputErrorBehavior(nsIUnicodeEncoder::kOnError_Replace, nsnull, (PRUnichar)'?');
            }
		}
	}

	// converts from unicode to the file system charset
	if (NS_SUCCEEDED(rv)) {
		PRInt32 inLength = aText.Length();
		PRInt32 outLength = 255;
		rv = mUnicodeEncoder->Convert(text, &inLength, (char *) &aStr255[1], &outLength);
		if (NS_SUCCEEDED(rv))
			aStr255[0] = outLength;
	}

	if (NS_FAILED(rv)) {
//		NS_ASSERTION(0, "error: charset conversion");
		NS_LossyConvertUTF16toASCII buffer(Substring(aText,0,254));
		PRInt32 len = buffer.Length();
		memcpy(&aStr255[1], buffer.get(), len);
		aStr255[0] = len;
	}
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

void nsMacControl::Str255ToString(const Str255& aStr255, nsString& aText)
{
	nsresult rv = NS_OK;
	
	// get file system charset and create a unicode encoder
	if (nsnull == mUnicodeDecoder) {
		nsCAutoString fileSystemCharset;
		GetFileSystemCharset(fileSystemCharset);

		nsCOMPtr<nsICharsetConverterManager> ccm = 
		         do_GetService(kCharsetConverterManagerCID, &rv); 
		if (NS_SUCCEEDED(rv)) {
			rv = ccm->GetUnicodeDecoderRaw(fileSystemCharset.get(), &mUnicodeDecoder);
		}
	}
  
	// converts from the file system charset to unicode
	if (NS_SUCCEEDED(rv)) {
		PRUnichar buffer[512];
		PRInt32 inLength = aStr255[0];
		PRInt32 outLength = 512;
		rv = mUnicodeDecoder->Convert((char *) &aStr255[1], &inLength, buffer, &outLength);
		if (NS_SUCCEEDED(rv)) {
			aText.Assign(buffer, outLength);
		}
	}
	
	if (NS_FAILED(rv)) {
//		NS_ASSERTION(0, "error: charset conversion");
		aText.AssignWithConversion((char *) &aStr255[1], aStr255[0]);
	}
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

void nsMacControl::NSStringSetControlTitle(ControlHandle theControl, nsString title)
{	
  // wow, it sure is nice being able to use core foundation ;)
  CFStringRef str = CFStringCreateWithCharacters(NULL, (const UniChar*)title.get(), title.Length());
  SetControlTitleWithCFString(theControl, str);
  CFRelease(str);
}
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsMacControl::SetupMacControlFontForScript(short theScript)
{
	short					themeFontID;
	Str255					themeFontName;
	SInt16					themeFontSize;
	Style					themeFontStyle;
	TextStyle				theStyle;
	OSErr					err;

	NS_PRECONDITION(mFontMetrics != nsnull, "No font metrics in SetupMacControlFont");
	NS_PRECONDITION(mContext != nsnull, "No context metrics in SetupMacControlFont");

	//
	// take the script and select and override font
	//
	err = ::GetThemeFont(kThemeSystemFont,theScript,themeFontName,&themeFontSize,&themeFontStyle);
	NS_ASSERTION(err==noErr,"nsMenu::NSStringNewMenu: GetThemeFont failed.");
	::GetFNum(themeFontName,&themeFontID);
	
	// if needed, impose a min size of 9pt on the control font
	if (theStyle.tsSize < 9)
		theStyle.tsSize = 9;
	
	ControlFontStyleRec fontStyleRec;
	fontStyleRec.flags = (kControlUseFontMask | kControlUseFaceMask | kControlUseSizeMask);
	fontStyleRec.font = themeFontID;
	fontStyleRec.size = theStyle.tsSize;
	fontStyleRec.style = theStyle.tsFace;
	::SetControlFontStyle(mControl, &fontStyleRec);
}
//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsMacControl::GetFileSystemCharset(nsCString & fileSystemCharset)
{
  static nsCAutoString aCharset;
  nsresult rv;

  if (aCharset.IsEmpty()) {
    nsCOMPtr <nsIPlatformCharset> platformCharset = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
	  if (NS_SUCCEEDED(rv)) 
		  rv = platformCharset->GetCharset(kPlatformCharsetSel_FileName, aCharset);

    NS_ASSERTION(NS_SUCCEEDED(rv), "error getting platform charset");
	  if (NS_FAILED(rv)) 
		  aCharset.AssignLiteral("x-mac-roman");
  }
  fileSystemCharset = aCharset;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------

OSStatus nsMacControl::InstallEventHandlerOnControl()
{
  const EventTypeSpec kControlEventList[] = {
    // Installing a kEventControlDraw handler causes harmless but ugly visual
    // imperfections in scrollbar tracks on Mac OS X 10.4.0 - 10.4.2.  This is
    // fixed in 10.4.3.  Bug 300058.
    { kEventClassControl, kEventControlDraw },
  };

  static EventHandlerUPP sControlEventHandlerUPP;
  if (!sControlEventHandlerUPP)
    sControlEventHandlerUPP = ::NewEventHandlerUPP(ControlEventHandler);

  OSStatus err =
   ::InstallControlEventHandler(mControl,
                                sControlEventHandlerUPP,
                                GetEventTypeCount(kControlEventList),
                                kControlEventList,
                                (void*)this,
                                &mControlEventHandler);
  NS_ENSURE_TRUE(err == noErr, err);

  const EventTypeSpec kWindowEventList[] = {
    { kEventClassWindow, kEventWindowActivated },
    { kEventClassWindow, kEventWindowDeactivated },
  };

  static EventHandlerUPP sWindowEventHandlerUPP;
  if (!sWindowEventHandlerUPP)
    sWindowEventHandlerUPP = ::NewEventHandlerUPP(WindowEventHandler);

  err = ::InstallWindowEventHandler(mWindowPtr,
                                    sWindowEventHandlerUPP,
                                    GetEventTypeCount(kWindowEventList),
                                    kWindowEventList,
                                    (void*)this,
                                    &mWindowEventHandler);
  return err;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
void nsMacControl::RemoveEventHandlerFromControl()
{
  if (mControlEventHandler) {
    ::RemoveEventHandler(mControlEventHandler);
    mControlEventHandler = nsnull;
  }

  if (mWindowEventHandler) {
    ::RemoveEventHandler(mWindowEventHandler);
    mWindowEventHandler = nsnull;
  }
}

//-------------------------------------------------------------------------
//
// At present, this handles only { kEventClassControl, kEventControlDraw }.
//
//-------------------------------------------------------------------------
// static
pascal OSStatus
nsMacControl::ControlEventHandler(EventHandlerCallRef aHandlerCallRef,
                                  EventRef            aEvent,
                                  void*               aUserData)
{
  nsMacControl* self = NS_STATIC_CAST(nsMacControl*, aUserData);

  PRBool wasDrawing = self->IsDrawing();

  if (wasDrawing) {
    if (!self->IsQDStateOK()) {
      // If you're here, you must be drawing the control inside |TrackControl|.
      // The converse is not necessarily true.
      //
      // In the |TrackControl| loop, something on a PLEvent messed with the
      // QD state.  The state can be fixed so the control draws in the proper
      // place by setting the port and origin before calling the next handler,
      // but it's extremely likely that the QD state was wrong when the
      // Control Manager looked at the mouse position, so the control's
      // current value will be incorrect.  Nobody wants to draw a control
      // that shows the wrong value (ex. scroll thumb position), so don't
      // draw it.  The subclass is responsible for catching this case in
      // its TrackControl action proc and doing something smart about it,
      // like fixing the port state and posting a fake event to force the
      // Control Manager to reread the value.
      //
      // This works in concert with |nsNativeScrollBar::DoScrollAction|.
      return noErr;
    }
  }
  else {
    self->StartDraw();
  }

  OSStatus err = ::CallNextEventHandler(aHandlerCallRef, aEvent);

  if (!wasDrawing) {
    self->EndDraw();
  }

  return err;
}

//-------------------------------------------------------------------------
//
// Returns true if the port and origin are set properly for this control.
// Useful to determine whether the Control Manager is likely to have been
// confused when it calls back into nsMacControl or a subclass.
//
//-------------------------------------------------------------------------
PRBool nsMacControl::IsQDStateOK()
{
  CGrafPtr controlPort = ::GetWindowPort(mWindowPtr);
  CGrafPtr currentPort;
  ::GetPort(&currentPort);

  if (controlPort != currentPort) {
    return PR_FALSE;
  }

  nsRect controlBounds;
  GetBounds(controlBounds);
  LocalToWindowCoordinate(controlBounds);
  Rect currentBounds;
  ::GetPortBounds(currentPort, &currentBounds);

  if (-controlBounds.x != currentBounds.left ||
      -controlBounds.y != currentBounds.top) {
    return PR_FALSE;
  }

  return PR_TRUE;
}

//-------------------------------------------------------------------------
//
// At present, this handles only
//  { kEventClassWindow, kEventWindowActivated },
//  { kEventClassWindow, kEventWindowDeactivated }
//
//-------------------------------------------------------------------------
// static
pascal OSStatus
nsMacControl::WindowEventHandler(EventHandlerCallRef aHandlerCallRef,
                                 EventRef            aEvent,
                                 void*               aUserData)
{
  nsMacControl* self = NS_STATIC_CAST(nsMacControl*, aUserData);

  // HiliteControl will cause the control to draw, so take care to only
  // call SetupControlHiliteState if the control is supposed to be visible.
  if (self->mVisible && self->ContainerHierarchyIsVisible())
    self->SetupControlHiliteState();

  return ::CallNextEventHandler(aHandlerCallRef, aEvent);
}
