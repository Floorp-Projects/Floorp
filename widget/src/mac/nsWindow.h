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
#ifndef Window_h__
#define Window_h__

#include "nsISupports.h"
#include "nsIPluginWidget.h"
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
#include <Quickdraw.h>

#define NSRGB_2_COLOREF(color) \
            RGB(NS_GET_R(color),NS_GET_G(color),NS_GET_B(color))

struct nsPluginPort;


//
// A nice little list structure. Doesn't own the memory in |mRectList|
//
typedef struct TRectArray
{
  TRectArray ( Rect* inRectList ) : mRectList(inRectList), mNumRects(0) { }
  PRInt32 Count ( ) const { return mNumRects; }
  
  Rect*     mRectList;
  PRInt32   mNumRects;

} TRectArray;

class CursorSpinner {
public:
    CursorSpinner();
    ~CursorSpinner();
    void StartSpinCursor();
    void StopSpinCursor();    

private:
    short                mSpinCursorFrame;
    EventLoopTimerUPP    mTimerUPP;
    EventLoopTimerRef    mTimerRef;
    
    short                GetNextCursorFrame();
    static pascal void   SpinCursor(EventLoopTimerRef inTimer, void *inUserData);
};

//-------------------------------------------------------------------------
//
// nsWindow
//
//-------------------------------------------------------------------------

class nsWindow :    public nsBaseWidget,
                    public nsDeleteObserved,
                    public nsIKBStateControl,
                    public nsIPluginWidget
{
private:
  typedef nsBaseWidget Inherited;

public:
                            nsWindow();
    virtual                 ~nsWindow();
    
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

    NS_IMETHOD                  ConstrainPosition(PRBool aAllowSlop,
                                                  PRInt32 *aX, PRInt32 *aY);
    NS_IMETHOD              Move(PRInt32 aX, PRInt32 aY);
    NS_IMETHOD              Resize(PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);
    NS_IMETHOD              Resize(PRInt32 aX, PRInt32 aY,PRInt32 aWidth,PRInt32 aHeight, PRBool aRepaint);

    NS_IMETHOD              Enable(PRBool aState);
    NS_IMETHOD              IsEnabled(PRBool *aState);
    NS_IMETHOD              SetFocus(PRBool aRaise);
    NS_IMETHOD              SetBounds(const nsRect &aRect);
    NS_IMETHOD              GetBounds(nsRect &aRect);

    virtual nsIFontMetrics* GetFont(void);
    NS_IMETHOD              SetFont(const nsFont &aFont);
    NS_IMETHOD              Validate();
    NS_IMETHOD              Invalidate(PRBool aIsSynchronous);
    NS_IMETHOD              Invalidate(const nsRect &aRect,PRBool aIsSynchronous);
    NS_IMETHOD              InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous);

    virtual void*           GetNativeData(PRUint32 aDataType);
    NS_IMETHOD              SetColorMap(nsColorMap *aColorMap);
    NS_IMETHOD              Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
    NS_IMETHOD              WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD              ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
    NS_IMETHOD              BeginResizingChildren(void);
    NS_IMETHOD              EndResizingChildren(void);

    static  PRBool          ConvertStatus(nsEventStatus aStatus);
    NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
    virtual PRBool          DispatchMouseEvent(nsMouseEvent &aEvent);

    virtual void          StartDraw(nsIRenderingContext* aRenderingContext = nsnull);
    virtual void          EndDraw();
    NS_IMETHOD            Update();
    virtual void          UpdateWidget(nsRect& aRect, nsIRenderingContext* aContext);
    
    virtual void          ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY);
    void                  LocalToWindowCoordinate(nsPoint& aPoint)            { ConvertToDeviceCoordinates(aPoint.x, aPoint.y); }
    void                  LocalToWindowCoordinate(nscoord& aX, nscoord& aY)       { ConvertToDeviceCoordinates(aX, aY); }
    void                  LocalToWindowCoordinate(nsRect& aRect)              { ConvertToDeviceCoordinates(aRect.x, aRect.y); }

    NS_IMETHOD            SetMenuBar(nsIMenuBar * aMenuBar);
    NS_IMETHOD            ShowMenuBar(PRBool aShow);
    virtual nsIMenuBar*   GetMenuBar();
    NS_IMETHOD        GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
    NS_IMETHOD        SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);
    
    NS_IMETHOD        SetCursor(nsCursor aCursor);
    static void       SetCursorResource(short aCursorResourceNum);

    NS_IMETHOD        CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent);
    NS_IMETHOD        SetTitle(const nsAString& title);
  
    NS_IMETHOD        GetAttention(PRInt32 aCycleCount);

    // Mac specific methods
    static void         nsRectToMacRect(const nsRect& aRect, Rect& aMacRect);
    PRBool              RgnIntersects(RgnHandle aTheRegion,RgnHandle aIntersectRgn);
    virtual void        CalcWindowRegions();

    virtual PRBool      PointInWidget(Point aThePoint);
    virtual nsWindow*   FindWidgetHit(Point aThePoint);

    virtual PRBool      DispatchWindowEvent(nsGUIEvent& event);
    virtual PRBool      DispatchWindowEvent(nsGUIEvent &event,nsEventStatus &aStatus);
    virtual nsresult    HandleUpdateEvent(RgnHandle regionToValidate);
    virtual void        AcceptFocusOnClick(PRBool aBool) { mAcceptFocusOnClick = aBool;};
    PRBool              AcceptFocusOnClick() { return mAcceptFocusOnClick;};
    void                Flash(nsPaintEvent  &aEvent);

    // nsIPluginWidget interface

    NS_IMETHOD              GetPluginClipRect(nsRect& outClipRect, nsPoint& outOrigin, PRBool& outWidgetVisible);
    NS_IMETHOD              StartDrawPlugin(void);
    NS_IMETHOD              EndDrawPlugin(void);

    // nsIKBStateControl interface
    NS_IMETHOD ResetInputState();

protected:

  PRBool          ReportDestroyEvent();
  PRBool          ReportMoveEvent();
  PRBool          ReportSizeEvent();

  void            CalcOffset(PRInt32 &aX,PRInt32 &aY);
  PRBool          ContainerHierarchyIsVisible();
  
  virtual PRBool      OnPaint(nsPaintEvent & aEvent);

  // our own impl of ::ScrollRect() that uses CopyBits so that it looks good. On 
  // Carbon, this just calls ::ScrollWindowRect()
  void          ScrollBits ( Rect & foo, PRInt32 inLeftDelta, PRInt32 inTopDelta ) ;

  void          CombineRects ( TRectArray & inRectArray ) ;
  void          SortRectsLeftToRight ( TRectArray & inRectArray ) ;

protected:
#if DEBUG
  const char*       gInstanceClassName;
#endif

  nsIWidget*        mParent;
  PRPackedBool      mIsTopWidgetWindow;
  PRPackedBool      mResizingChildren;
  PRPackedBool      mSaveVisible;
  PRPackedBool      mVisible;
  PRPackedBool      mEnabled;
  PRInt32           mPreferredWidth;
  PRInt32           mPreferredHeight;
  nsIFontMetrics*   mFontMetrics;
  nsIMenuBar*       mMenuBar;

  RgnHandle         mWindowRegion;
  RgnHandle         mVisRegion;
  WindowPtr         mWindowPtr;

  PRPackedBool      mDestroyCalled;
  PRPackedBool      mDestructorCalled;

  PRPackedBool      mAcceptFocusOnClick;

  PRPackedBool      mDrawing;
  PRPackedBool      mTempRenderingContextMadeHere;

  nsIRenderingContext*    mTempRenderingContext;
    
  nsPluginPort*     mPluginPort;

  
  // Routines for iterating over the rects of a region. Carbon and pre-Carbon
  // do this differently so provide a way to do both.
#if TARGET_CARBON
  static OSStatus PaintUpdateRectProc (UInt16 message, RgnHandle rgn, const Rect *rect, void *refCon);
  static OSStatus AddRectToArrayProc (UInt16 message, RgnHandle rgn, const Rect *rect, void *refCon);
  static OSStatus CountRectProc (UInt16 message, RgnHandle rgn, const Rect *rect, void *refCon);
#endif

  static void PaintUpdateRect (Rect * r, void* data) ;
  static void AddRectToArray (Rect * r, void* data) ;
  static void CountRect (Rect * r, void* data) ;
  
};


#if DEBUG
#define WIDGET_SET_CLASSNAME(n)   gInstanceClassName = (n)
#else
#define WIDGET_SET_CLASSNAME(n)   
#endif


//-------------------------------------------------------------------------
//
// nsChildWindow
//
//-------------------------------------------------------------------------

// Some XP Widgets (like the nsImageButton) include this header file
// and expect to find in it the definition of a "ChildWindow".

#include "nsChildWindow.h"

#define ChildWindow   nsChildWindow



#endif // Window_h__
