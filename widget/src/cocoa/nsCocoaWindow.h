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
#ifndef MacWindow_h__
#define MacWindow_h__

#undef DARWIN
#import <Cocoa/Cocoa.h>

#include "nsIEventSink.h"

//#include <memory>	// for auto_ptr
#include <Controls.h>

//using std::auto_ptr;

#include "nsBaseWidget.h"

//#include "nsMacEventHandler.h"

class nsCocoaWindow;


@interface WindowDelegate : NSObject
{
  nsCocoaWindow* mGeckoWindow;
}
- (id)initWithGeckoWindow:(nsCocoaWindow*)geckoWind;
- (void)windowDidResize:(NSNotification *)aNotification;
@end



//-------------------------------------------------------------------------
//
// nsCocoaWindow
//
//-------------------------------------------------------------------------
//	Cocoa native window

class nsCocoaWindow : public nsBaseWidget, public nsIEventSink
{
private:
	typedef nsBaseWidget Inherited;

public:

    enum { kTitleBarHeight = 20 };

    nsCocoaWindow();
    virtual ~nsCocoaWindow();

    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIEVENTSINK 
      
    NS_IMETHOD              Create(nsNativeWidget aParent,
                                     const nsRect &aRect,
                                     EVENT_CALLBACK aHandleEventFunction,
                                     nsIDeviceContext *aContext,
                                     nsIAppShell *aAppShell = nsnull,
                                     nsIToolkit *aToolkit = nsnull,
                                     nsWidgetInitData *aInitData = nsnull);

    NS_IMETHOD              Create(nsIWidget* aParent,
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
    
    NS_IMETHOD              Enable(PRBool aState) { return NS_OK; }
    NS_IMETHOD              SetModal(PRBool aState) { return NS_OK; }
    NS_IMETHOD              IsVisible(PRBool & aState) { return NS_OK; }
    NS_IMETHOD              SetFocus(PRBool aState=PR_FALSE) { return NS_OK; }
    NS_IMETHOD SetMenuBar(nsIMenuBar * aMenuBar) { return NS_OK; }
    NS_IMETHOD ShowMenuBar(PRBool aShow) { return NS_OK; }
    NS_IMETHOD WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect) { return NS_OK; }
    NS_IMETHOD ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect) { return NS_OK; }
    
    virtual void* GetNativeData(PRUint32 aDataType) ;

    NS_IMETHOD              ConstrainPosition(PRInt32 *aX, PRInt32 *aY);
    NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD              PlaceBehind(nsIWidget *aWidget, PRBool aActivate);
    NS_IMETHOD              SetSizeMode(PRInt32 aMode);
    void                    CalculateAndSetZoomedSize();

    NS_IMETHOD              Resize(PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);
    NS_IMETHOD              Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint);
    NS_IMETHOD            	GetScreenBounds(nsRect &aRect);
    virtual PRBool          OnPaint(nsPaintEvent &event);
    void                    ReportSizeEvent();

		NS_IMETHOD              SetTitle(const nsString& aTitle);

    virtual nsIFontMetrics* GetFont(void) { return nsnull; }
    NS_IMETHOD SetFont(const nsFont &aFont) { return NS_OK; }
    NS_IMETHOD Invalidate(const nsRect & aRect, PRBool aIsSynchronous) { return NS_OK; }
    NS_IMETHOD Invalidate(PRBool aIsSynchronous) { return NS_OK; };
    NS_IMETHOD Update() { return NS_OK; }
    NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect) { return NS_OK; }
    NS_IMETHOD SetColorMap(nsColorMap *aColorMap) { return NS_OK; }
    NS_IMETHOD BeginResizingChildren(void) { return NS_OK; }
    NS_IMETHOD EndResizingChildren(void) { return NS_OK; }
    NS_IMETHOD GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight) { return NS_OK; }
    NS_IMETHOD SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight) { return NS_OK; }
    NS_IMETHOD DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus) ;
    NS_IMETHOD CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent) { return NS_OK; }
    
		// be notified that a some form of drag event needs to go into Gecko
	virtual PRBool 			DragEvent ( unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers ) ;

      // Helpers to prevent recursive resizing during live-resize
    PRBool IsResizing ( ) const { return mIsResizing; }
    void StartResizing ( ) { mIsResizing = PR_TRUE; }
    void StopResizing ( ) { mIsResizing = PR_FALSE; }
    
    void                    ComeToFront();

  	// nsIKBStateControl interface
  	NS_IMETHOD ResetInputState();

    void              		MoveToGlobalPoint(PRInt32 aX, PRInt32 aY);

    void IsActive(PRBool* aActive);
    void SetIsActive(PRBool aActive);
protected:

#if 0
	pascal static OSErr DragTrackingHandler ( DragTrackingMessage theMessage, WindowPtr theWindow, 
										void *handlerRefCon, DragReference theDrag );
	pascal static OSErr DragReceiveHandler (WindowPtr theWindow,
												void *handlerRefCon, DragReference theDragRef) ;
	DragTrackingHandlerUPP mDragTrackingHandlerUPP;
	DragReceiveHandlerUPP mDragReceiveHandlerUPP;
#endif

#if 0
  pascal static OSStatus WindowEventHandler ( EventHandlerCallRef inHandlerChain, 
                                               EventRef inEvent, void* userData ) ;
  pascal static OSStatus ScrollEventHandler ( EventHandlerCallRef inHandlerChain, 
                                               EventRef inEvent, void* userData ) ;
#endif

#if 0
	PRBool							mIsDialog;       // true if the window is a dialog
	auto_ptr<nsMacEventHandler>		mMacEventHandler;
	nsIWidget                      *mOffsetParent;
	PRBool                          mAcceptsActivation;
	PRBool                          mIsActive;
	PRBool                          mZoomOnShow;	
#endif // COCOA

	PRBool            mIsResizing;     // we originated the resize, prevent infinite recursion
	PRBool            mWindowMadeHere; // true if we created the window, false for embedding
  NSWindow*         mWindow;         // our cocoa window [STRONG]
  WindowDelegate*   mDelegate;       // our delegate for processing window msgs [STRONG]

};

#endif // MacWindow_h__
