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
#ifndef Window_h__
#define Window_h__

#include "nsISupports.h"
#include "nsBaseWidget.h"
#include "nsDeleteObserver.h"

#include "nsIWidget.h"
#include "nsIKBStateControl.h"
#include "nsIAppShell.h"

#include "nsIMouseListener.h"
#include "nsIEventListener.h"
#include "nsString.h"

#include "nsIMenuBar.h"

#include "nsplugindefs.h"
#include <QuickDraw.h>

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))

struct nsPluginPort;

#undef DARWIN
#import <Cocoa/Cocoa.h>

class nsChildView;
class nsIEventSink;


@interface ChildView : NSQuickDrawView
{
    // the nsChildView that created the view. It retains this NSView, so
    // the link back to it must be weak.
  nsChildView* mGeckoChild;
  
    // allows us to redispatch events back to a centralized location
  nsIEventSink* mEventSink;
  
    // tag for our mouse enter/exit tracking rect
  NSTrackingRectTag mMouseEnterExitTag;
}

  // sets up our view, attaching it to its owning gecko view
- (id) initWithGeckoChild:(nsChildView*)child eventSink:(nsIEventSink*)sink;

  // access the nsIWidget associated with this view. DOES NOT ADDREF.
- (nsIWidget*) widget;

  // convert from one event system to the other for even dispatching
- (void) convert:(NSEvent*)inEvent message:(PRInt32)inMsg toGeckoEvent:(nsInputEvent*)outGeckoEvent;

@end



//-------------------------------------------------------------------------
//
// nsChildView
//
//-------------------------------------------------------------------------

class nsChildView : public nsBaseWidget, public nsDeleteObserved, public nsIKBStateControl
{
private:
  typedef nsBaseWidget Inherited;

public:
                            nsChildView();
    virtual                 ~nsChildView();
    
    NS_DECL_ISUPPORTS_INHERITED
    
    // nsIWidget interface
    NS_IMETHOD              Create(nsIWidget *aParent,
                                   const nsRect &aRect,
                                   EVENT_CALLBACK aHandleEventFunction,
                                   nsIDeviceContext *aContext,
                                   nsIAppShell *aAppShell = nsnull,
                                   nsIToolkit *aToolkit = nsnull,
                                   nsWidgetInitData *aInitData = nsnull);
    NS_IMETHOD              Create(nsNativeWidget aNativeParent,
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

    NS_IMETHOD              Destroy();
    virtual nsIWidget*      GetParent(void);

    NS_IMETHOD              Show(PRBool aState);
    NS_IMETHOD              IsVisible(PRBool & aState);

    NS_IMETHOD              ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                             PRBool *aForWindow);

    NS_IMETHOD                  ConstrainPosition(PRInt32 *aX, PRInt32 *aY);
    NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD              Resize(PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);
    NS_IMETHOD              Resize(PRInt32 aX, PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);

    NS_IMETHOD              Enable(PRBool bState);
    NS_IMETHOD              SetFocus(PRBool aRaise);
    NS_IMETHOD              SetBounds(const nsRect &aRect);
    NS_IMETHOD              GetBounds(nsRect &aRect);

    virtual nsIFontMetrics* GetFont(void);
    NS_IMETHOD              SetFont(const nsFont &aFont);
    NS_IMETHOD              Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD        Invalidate(const nsRect &aRect,PRBool aIsSynchronous);
    NS_IMETHOD        InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous);

    virtual void*           GetNativeData(PRUint32 aDataType);
    NS_IMETHOD              SetColorMap(nsColorMap *aColorMap);
    NS_IMETHOD              Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    NS_IMETHOD              WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD              ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD              BeginResizingChildren(void);
    NS_IMETHOD              EndResizingChildren(void);

    static  PRBool          ConvertStatus(nsEventStatus aStatus);
    NS_IMETHOD            DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
    virtual PRBool          DispatchMouseEvent(nsMouseEvent &aEvent);

    virtual void          StartDraw(nsIRenderingContext* aRenderingContext = nsnull);
    virtual void          EndDraw();
  NS_IMETHOD        Update();
  virtual void      UpdateWidget(nsRect& aRect, nsIRenderingContext* aContext);
    
    virtual void      ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);
  void          LocalToWindowCoordinate(nsPoint& aPoint)            { ConvertToDeviceCoordinates(aPoint.x, aPoint.y); }
  void          LocalToWindowCoordinate(nscoord& aX, nscoord& aY)       { ConvertToDeviceCoordinates(aX, aY); }
    void          LocalToWindowCoordinate(nsRect& aRect)              { ConvertToDeviceCoordinates(aRect.x, aRect.y); }

    NS_IMETHOD        SetMenuBar(nsIMenuBar * aMenuBar);
    NS_IMETHOD        ShowMenuBar(PRBool aShow);
    virtual nsIMenuBar*   GetMenuBar();
    NS_IMETHOD        GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD        SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);
    
    NS_IMETHOD        SetCursor(nsCursor aCursor);
    
    NS_IMETHOD        CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent);
    NS_IMETHOD        SetTitle(const nsString& title);
  
    NS_IMETHOD        GetAttention();

    // Mac specific methods
    void          nsRectToMacRect(const nsRect& aRect, Rect& aMacRect) const;
  virtual void      CalcWindowRegions();

  virtual PRBool      PointInWidget(Point aThePoint);
  virtual nsChildView*    FindWidgetHit(Point aThePoint);

  virtual PRBool      DispatchWindowEvent(nsGUIEvent& event);
  virtual PRBool      DispatchWindowEvent(nsGUIEvent &event,nsEventStatus &aStatus);
    virtual void      AcceptFocusOnClick(PRBool aBool) { mAcceptFocusOnClick = aBool;};
    PRBool          AcceptFocusOnClick() { return mAcceptFocusOnClick;};
  void          Flash(nsPaintEvent  &aEvent);

public:
    // nsIKBStateControl interface
    NS_IMETHOD ResetInputState();

protected:

  PRBool          ReportDestroyEvent();
  PRBool          ReportMoveEvent();
  PRBool          ReportSizeEvent();

  NS_IMETHOD        CalcOffset(PRInt32 &aX,PRInt32 &aY);

  virtual PRBool      OnPaint(nsPaintEvent & aEvent);

  // our own impl of ::ScrollRect() that uses CopyBits so that it looks good. On 
  // Carbon, this just calls ::ScrollWindowRect()
  void          ScrollBits ( Rect & foo, PRInt32 inLeftDelta, PRInt32 inTopDelta ) ;

protected:
#if DEBUG
  const char*       gInstanceClassName;
#endif

  nsIWidget*      mParent;    // my parent. could be a childview or a window [WEAK]
  NSWindow*       mWindow;    // shortcut to the top window, [WEAK]
  
  ChildView*      mView;      // my parallel cocoa view, [STRONG]

  PRBool          mDestroyCalled;
  PRBool          mDestructorCalled;
  PRBool          mVisible;

  nsIFontMetrics*     mFontMetrics;

  PRBool          mDrawing;
  nsIRenderingContext*    mTempRenderingContext;
  PRBool          mTempRenderingContextMadeHere;
    
  nsPluginPort*     mPluginPort;

  PRBool          mAcceptFocusOnClick;
    
};


#if DEBUG
#define WIDGET_SET_CLASSNAME(n)   gInstanceClassName = (n)
#else
#define WIDGET_SET_CLASSNAME(n)   
#endif



#endif // Window_h__
