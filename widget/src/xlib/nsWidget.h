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
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   Quy Tonthat <quy@igelaus.com.au>
 *   B.J. Rossiter <bj@igelaus.com.au>
 *   Roland.Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#ifndef nsWidget_h__
#define nsWidget_h__

#include "nsBaseWidget.h"
#include "nsWeakReference.h"
#include "nsHashtable.h"
#include "prlog.h"
#include "nsIRegion.h"
#include "nsIXlibWindowService.h"
#include "nsIRollupListener.h"
#include "xlibrgb.h"

#ifdef DEBUG_blizzard
#define XLIB_WIDGET_NOISY
#endif

class nsWidget : public nsBaseWidget, public nsSupportsWeakReference
{
public:
  nsWidget();
  virtual ~nsWidget();

  NS_DECL_ISUPPORTS_INHERITED

  NS_IMETHOD Create(nsIWidget *aParent,
                    const nsRect &aRect,
                    EVENT_CALLBACK aHandleEventFunction,
                    nsIDeviceContext *aContext,
                    nsIAppShell *aAppShell = nsnull,
                    nsIToolkit *aToolkit = nsnull,
                    nsWidgetInitData *aInitData = nsnull);

  NS_IMETHOD Create(nsNativeWidget aParent,
                    const nsRect &aRect,
                    EVENT_CALLBACK aHandleEventFunction,
                    nsIDeviceContext *aContext,
                    nsIAppShell *aAppShell = nsnull,
                    nsIToolkit *aToolkit = nsnull,
                    nsWidgetInitData *aInitData = nsnull);

  virtual nsresult StandardWidgetCreate(nsIWidget *aParent,
                                        const nsRect &aRect,
                                        EVENT_CALLBACK aHandleEventFunction,
                                        nsIDeviceContext *aContext,
                                        nsIAppShell *aAppShell,
                                        nsIToolkit *aToolkit,
                                        nsWidgetInitData *aInitData,
                                        nsNativeWidget aNativeParent = nsnull);
  NS_IMETHOD Destroy();
  virtual nsIWidget *GetParent(void);
  NS_IMETHOD Show(PRBool bState);
  NS_IMETHOD IsVisible(PRBool &aState);

  NS_IMETHOD ConstrainPosition(PRInt32 *aX, PRInt32 *aY);
  NS_IMETHOD Move(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD Resize(PRInt32 aWidth,
                    PRInt32 aHeight,
                    PRBool   aRepaint);
  NS_IMETHOD Resize(PRInt32 aX,
                    PRInt32 aY,
                    PRInt32 aWidth,
                    PRInt32 aHeight,
                    PRBool   aRepaint);

  NS_IMETHOD Enable(PRBool bState);
  NS_IMETHOD              SetFocus(PRBool aRaise);
  NS_IMETHOD              SetName(const char * aName);
  NS_IMETHOD              SetBackgroundColor(const nscolor &aColor);
  virtual nsIFontMetrics* GetFont(void);
  NS_IMETHOD              SetFont(const nsFont &aFont);
  NS_IMETHOD              SetCursor(nsCursor aCursor);
  void                    LockCursor(PRBool aLock);

  NS_IMETHOD Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD              Invalidate(const nsRect & aRect, PRBool aIsSynchronous);
  NS_IMETHOD              Update();
  virtual void*           GetNativeData(PRUint32 aDataType);
  NS_IMETHOD              SetColorMap(nsColorMap *aColorMap);
  NS_IMETHOD              Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD              SetMenuBar(nsIMenuBar * aMenuBar); 
  NS_IMETHOD              ShowMenuBar(PRBool aShow);
  NS_IMETHOD              SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[]);   
  NS_IMETHOD              RemoveTooltips();
  NS_IMETHOD              UpdateTooltips(nsRect* aNewTips[]);
  NS_IMETHOD              WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect);
  NS_IMETHOD              ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect);
  NS_IMETHOD              BeginResizingChildren(void);
  NS_IMETHOD              EndResizingChildren(void);
  NS_IMETHOD              GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight);
  NS_IMETHOD              SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight);
  NS_IMETHOD              DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus);
  NS_IMETHOD              PreCreateWidget(nsWidgetInitData *aInitData);
  NS_IMETHOD              SetBounds(const nsRect &aRect);
  NS_IMETHOD              GetRequestedBounds(nsRect &aRect);

  NS_IMETHOD              CaptureRollupEvents(nsIRollupListener *aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent);
  NS_IMETHOD              SetTitle(const nsString& title);
#ifdef DEBUG
  void                    DebugPrintEvent(nsGUIEvent & aEvent,Window aWindow);
#endif

  virtual PRBool          OnPaint(nsPaintEvent &event);
  virtual PRBool          OnResize(nsSizeEvent &event);
  virtual PRBool          OnDeleteWindow(void);

  // KenF Added FIXME:
  virtual void						OnDestroy(void);
  virtual PRBool          DispatchMouseEvent(nsMouseEvent &aEvent);
  virtual PRBool          DispatchKeyEvent(nsKeyEvent &aKeyEvent);
  virtual PRBool          DispatchDestroyEvent(void);

  static nsWidget        * GetWidgetForWindow(Window aWindow);
  void                     SetVisibility(int aState); // using the X constants here
  void                     SetIonic(PRBool isIonic);
  static Window            GetFocusWindow(void);

  Cursor                   XlibCreateCursor(nsCursor aCursorType);

  PRBool DispatchWindowEvent(nsGUIEvent & aEvent);

//   static nsresult         SetXlibWindowCallback(nsXlibWindowCallback *aCallback);
//   static nsresult         XWindowCreated(Window aWindow);
//   static nsresult         XWindowDestroyed(Window aWindow);

  // these are for the wm protocols
  static Atom   WMDeleteWindow;
  static Atom   WMTakeFocus;
  static Atom   WMSaveYourself;
  static PRBool WMProtocolsInitialized;

  // Checks if parent is alive. nsWidget has a stub, nsWindow has real
  // thing. KenF
  void *CheckParent(long ThisWindow);

	// Deal with rollup for popups
	PRBool IsMouseInWindow(Window window, PRInt32 inMouseX, PRInt32 inMouseY);
	PRBool HandlePopup( PRInt32 inMouseX, PRInt32 inMouseY);

  void WidgetShow       (nsWidget *aWidget);
protected:

  nsCOMPtr<nsIRegion> mUpdateArea;
  // private event functions
  PRBool ConvertStatus(nsEventStatus aStatus);

  // create the native window for this class
  virtual void CreateNativeWindow(Window aParent, nsRect aRect,
                                  XSetWindowAttributes aAttr, unsigned long aMask);
  virtual void CreateNative(Window aParent, nsRect aRect);
  virtual void DestroyNative();
  static void  DestroyNativeChildren(Display *aDisplay, Window aWindow);
  void         Map(void);
  void         Unmap(void);

  // Let each sublclass set the event mask according to their needs
  virtual long GetEventMask();

  // these will add and delete a window
  static void  AddWindowCallback   (Window aWindow, nsWidget *aWidget);
  static void  DeleteWindowCallback(Window aWindow);

  // set up our wm hints
  void          SetUpWMHints(void);

  // here's how we add children
  // there's no geometry information here because that should be in the mBounds
  // in the widget
  void WidgetPut        (nsWidget *aWidget);
  void WidgetMove       (nsWidget *aWidget);
  void WidgetMoveResize (nsWidget *aWidget);
  void WidgetResize     (nsWidget *aWidget);
  // check to see whether or not a rect will intersect with the current scrolled area
  PRBool WidgetVisible  (nsRect   &aBounds);

  PRBool         mIsShown;
  PRBool mShown;
  int            mVisibility; // this is an int because that's the way X likes it
  PRUint32       mPreferredWidth;
  PRUint32       mPreferredHeight;

  nsIWidget   *  mParentWidget;
  Window         mParentWindow;

  // All widgets have at least these items.
  XlibRgbHandle *mXlibRgbHandle;
  Display *      mDisplay;
  Screen *       mScreen;
  Window         mBaseWindow;
  Visual *       mVisual;
  int            mDepth;
  unsigned long  mBackgroundPixel;
  PRUint32       mBorderRGB;
  unsigned long  mBorderPixel;
  //  GC             mGC;             // until we get gc pooling working...
  nsString       mName;           // name of the type of widget
  PRBool         mIsToplevel;
  nsRect         mRequestedSize;
  PRBool         mMapped;
  PRBool         mLastGrabFailed;

  static         Window                 mFocusWindow;

  // Changed to protected so nsWindow has access to it. KenF
protected:
  PRBool       mListenForResizes;     // If we're native we want to listen.
  static       nsHashtable *          gsWindowList;
  static       Cursor                 gsXlibCursorCache[eCursor_count_up_down + 1];

  static       nsXlibWindowCallback   gsWindowCreateCallback;
  static       nsXlibWindowCallback   gsWindowDestroyCallback;
  static       nsXlibEventDispatcher  gsEventDispatcher;

  // Variables for infomation about the current popup window and its listener
  static nsCOMPtr<nsIRollupListener> gRollupListener;
  static nsWeakPtr                   gRollupWidget;
	static PRBool	gRollupConsumeRollupEvent;
	
};

extern PRLogModuleInfo *XlibWidgetsLM;
extern PRLogModuleInfo *XlibScrollingLM;

#endif







