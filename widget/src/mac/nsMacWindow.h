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
#ifndef MacWindow_h__
#define MacWindow_h__

#include <memory>	// for auto_ptr
#include <Controls.h>

using std::auto_ptr;

#include "nsWindow.h"
#include "nsMacEventHandler.h"
#include "nsIEventSink.h"
#include "nsIMacTextInputEventSink.h"
#include "nsPIWidgetMac.h"
#include "nsPIEventSinkStandalone.h"

#if TARGET_CARBON
#include <CarbonEvents.h>
#endif

class nsMacEventHandler;
struct PhantomScrollbarData;

//-------------------------------------------------------------------------
//
// nsMacWindow
//
//-------------------------------------------------------------------------
//	MacOS native window

class nsMacWindow : public nsChildWindow, public nsIEventSink, public nsPIWidgetMac, 
                    public nsPIEventSinkStandalone, public nsIMacTextInputEventSink
{
private:
	typedef nsChildWindow Inherited;

public:
    nsMacWindow();
    virtual ~nsMacWindow();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIEVENTSINK 
    NS_DECL_NSPIWIDGETMAC
    NS_DECL_NSPIEVENTSINKSTANDALONE
    NS_DECL_NSIMACTEXTINPUTEVENTSINK
    
/*
    // nsIWidget interface
    NS_IMETHOD            Create(nsIWidget *aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIAppShell *aAppShell = nsnull,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);
*/
    NS_IMETHOD              Create(nsNativeWidget aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIAppShell *aAppShell = nsnull,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);

     // Utility method for implementing both Create(nsIWidget ...) and
     // Create(nsNativeWidget...)

    virtual nsresult        StandardCreate(nsIWidget *aParent,
				                            const nsRect &aRect,
				                            EVENT_CALLBACK aHandleEventFunction,
				                            nsIDeviceContext *aContext,
				                            nsIAppShell *aAppShell,
				                            nsIToolkit *aToolkit,
				                            nsWidgetInitData *aInitData,
				                            nsNativeWidget aNativeParent = nsnull);

    NS_IMETHOD              Show(PRBool aState);
    NS_IMETHOD              ConstrainPosition(PRBool aAllowSlop,
                                              PRInt32 *aX, PRInt32 *aY);
    NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD              PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                        nsIWidget *aWidget, PRBool aActivate);
    NS_IMETHOD              SetSizeMode(PRInt32 aMode);

    NS_IMETHOD              Resize(PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);
    NS_IMETHOD            	GetScreenBounds(nsRect &aRect);
    virtual PRBool          OnPaint(nsPaintEvent &event);

    NS_IMETHOD              SetTitle(const nsAString& aTitle);

    void                    UserStateForResize();

  	// nsIKBStateControl interface
  	NS_IMETHOD ResetInputState();

    void              		MoveToGlobalPoint(PRInt32 aX, PRInt32 aY);

    void IsActive(PRBool* aActive);
    void SetIsActive(PRBool aActive);
    WindowPtr GetWindowTop(WindowPtr baseWindowRef);
    void UpdateWindowMenubar(WindowPtr parentWindow, PRBool enableFlag);

protected:

  void InstallBorderlessDefProc ( WindowPtr inWindow ) ;
  void RemoveBorderlessDefProc ( WindowPtr inWindow ) ;

	pascal static OSErr DragTrackingHandler ( DragTrackingMessage theMessage, WindowPtr theWindow, 
										void *handlerRefCon, DragReference theDrag );
	pascal static OSErr DragReceiveHandler (WindowPtr theWindow,
												void *handlerRefCon, DragReference theDragRef) ;
	DragTrackingHandlerUPP mDragTrackingHandlerUPP;
	DragReceiveHandlerUPP mDragReceiveHandlerUPP;

#if TARGET_CARBON
  pascal static OSStatus WindowEventHandler ( EventHandlerCallRef inHandlerChain, 
                                               EventRef inEvent, void* userData ) ;
  pascal static OSStatus ScrollEventHandler ( EventHandlerCallRef inHandlerChain, 
                                               EventRef inEvent, void* userData ) ;
#endif

	PRPackedBool                    mWindowMadeHere; // true if we created the window
	PRPackedBool                    mIsSheet;        // true if the window is a sheet (Mac OS X)
	PRPackedBool                    mIgnoreDeactivate;  // true if this window has a (Mac OS X) sheet opening
	PRPackedBool                    mAcceptsActivation;
	PRPackedBool                    mIsActive;
	PRPackedBool                    mZoomOnShow;
	PRPackedBool                    mZooming;
	PRPackedBool                    mResizeIsFromUs;    // we originated the resize, prevent infinite recursion
  PRBool                          mShown;             // whether the window was actually shown on screen
	Point                           mBoundsOffset;      // offset from window structure to content
	auto_ptr<nsMacEventHandler>     mMacEventHandler;
	nsIWidget                      *mOffsetParent;
	
#if !TARGET_CARBON
	ControlHandle      mPhantomScrollbar;  // a native scrollbar for the scrollwheel
	PhantomScrollbarData* mPhantomScrollbarData;
#endif
};

#endif // MacWindow_h__
