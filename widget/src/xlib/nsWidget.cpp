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
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   Quy Tonthat <quy@igelaus.com.au>
 *   B.J. Rossiter <bj@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 *   L. David Baron <dbaron@fas.harvard.edu>
 *   Tim Copperfield <timecop@network.email.ne.jp>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#undef DEBUG_CURSORCACHE

#include "nsWidget.h"
#include "nsIServiceManager.h"
#include "nsAppShell.h"

#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include "nsXlibCursors.h"

#include "nsIEventListener.h"
#include "nsIMenuListener.h"
#include "nsIMouseListener.h"
#include "nsIRollupListener.h"
#include "nsGfxCIID.h"
#include "nsIMenuRollup.h"
#include "nsIRenderingContext.h"
#include "nsToolkit.h"

#include "xlibrgb.h"

static NS_DEFINE_CID(kRegionCID, NS_REGION_CID);

PRLogModuleInfo *XlibWidgetsLM = PR_NewLogModule("XlibWidgets");
PRLogModuleInfo *XlibScrollingLM = PR_NewLogModule("XlibScrolling");

// set up our static members here.
nsHashtable *nsWidget::gsWindowList = nsnull; // WEAK references to nsWidget*

// cursors hash table
Cursor nsWidget::gsXlibCursorCache[eCursor_count_up_down + 1];

// this is a class for generating keys for
// the list of windows managed by mozilla.

// this is possibly the class impl that will be
// called whenever a new window is created/destroyed

//nsXlibWindowCallback *nsWidget::mWindowCallback = nsnull;

nsXlibWindowCallback  nsWidget::gsWindowCreateCallback = nsnull;
nsXlibWindowCallback  nsWidget::gsWindowDestroyCallback = nsnull;
nsXlibEventDispatcher nsWidget::gsEventDispatcher = nsnull;

// this is for implemention the WM_PROTOCOL code
PRBool nsWidget::WMProtocolsInitialized = PR_FALSE;
Atom   nsWidget::WMDeleteWindow = 0;
Atom   nsWidget::WMTakeFocus = 0;
Atom   nsWidget::WMSaveYourself = 0;

// this is the window that has the focus
Window nsWidget::mFocusWindow = 0;

// For popup handling
nsCOMPtr<nsIRollupListener> nsWidget::gRollupListener;
nsWeakPtr          nsWidget::gRollupWidget;
PRBool             nsWidget::gRollupConsumeRollupEvent = PR_FALSE;

class nsWindowKey : public nsHashKey {
protected:
  Window mKey;

public:
  nsWindowKey(Window key) {
    mKey = key;
  }
  ~nsWindowKey(void) {
  }
  PRUint32 HashCode(void) const {
    return (PRUint32)mKey;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (mKey == ((nsWindowKey *)aKey)->mKey);
  }

  nsHashKey *Clone(void) const {
    return new nsWindowKey(mKey);
  }
};

nsWidget::nsWidget() // : nsBaseWidget()
{
  mPreferredWidth = 0;
  mPreferredHeight = 0;

  mDisplay = 0;
  mScreen = 0;
  mVisual = 0;
  mDepth = 0;

  mBaseWindow = 0;
  mBackground = NS_RGB(192, 192, 192);
  mBorderRGB = NS_RGB(192, 192, 192);
  /* |xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE)| would be the official 
   * way - but |nsAppShell::GetXlibRgbHandle()| one is little bit faster...*/
  mXlibRgbHandle = nsAppShell::GetXlibRgbHandle();
  mBackgroundPixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, mBackground);
  mBackground = NS_RGB(192, 192, 192);
  mBorderPixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, mBorderRGB);
  mParentWidget = nsnull;
  mName.AssignWithConversion("unnamed");
  mIsShown = PR_FALSE;
  mIsToplevel = PR_FALSE;
  mVisibility = VisibilityFullyObscured; // this is an X constant.
  mWindowType = eWindowType_child;
  mBorderStyle = eBorderStyle_default;

  // added KenF 
  mIsDestroying = PR_FALSE;
  mOnDestroyCalled = PR_FALSE;
  mListenForResizes = PR_FALSE;	// If we're native we need to listen.
  mMapped = PR_FALSE;


  mUpdateArea = do_CreateInstance(kRegionCID);
  if (mUpdateArea) {
    mUpdateArea->Init();
    mUpdateArea->SetTo(0, 0, 0, 0);
  }


}

// FIXME:
// Heavily modifying so functionally similar as gtk version. KenF
nsWidget::~nsWidget()
{
  //mIsDestroying = TRUE;

  if (mBaseWindow)
    Destroy();

}

// Borrowed heavily from GTK. This should go through heirarchy of XWindow
// windows, and destroy the appropriate children. 
// KenF
void
nsWidget::DestroyNativeChildren(Display *aDisplay, Window aWindow)
{
  Window       root_return;
  Window       parent_return;
  Window      *children_return = nsnull;
  unsigned int nchildren_return = 0;
  unsigned int i = 0;
  
  XQueryTree(aDisplay, aWindow, &root_return, &parent_return,
             &children_return, &nchildren_return);
  // walk the list of children
  for (i=0; i < nchildren_return; i++) {
    nsWidget *thisWidget = GetWidgetForWindow(children_return[i]);
    if (thisWidget) {
      thisWidget->Destroy();
    }
  }      

  // free up this struct
  if (children_return)
    XFree(children_return);
}

void
nsWidget::DestroyNative()
{
  NS_ASSERTION(mBaseWindow, "no native window");

  // We have to destroy the children ourselves before we call XDestroyWindow
  // because otherwise XDestroyWindow will destroy the windows and leave us
  // with dangling references.
  DestroyNativeChildren(mDisplay, mBaseWindow);

  XDestroyWindow(mDisplay, mBaseWindow);
  DeleteWindowCallback(mBaseWindow);
}

// Stub in nsWidget, real in nsWindow. KenF
void * nsWidget::CheckParent(long ThisWindow)
{
  return (void*)-1;
}

NS_IMETHODIMP nsWidget::Create(nsIWidget *aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               nsWidgetInitData *aInitData)
{
  // Do adding in SWC() KenF
  //mParentWidget = aParent;
  //NS_IF_ADDREF(mParentWidget);	// KenF FIXME

  return StandardWidgetCreate(aParent, aRect, aHandleEventFunction,
                              aContext, aAppShell, aToolkit, aInitData,
                              nsnull);
}

NS_IMETHODIMP nsWidget::Create(nsNativeWidget aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               nsWidgetInitData *aInitData)
{
  return(StandardWidgetCreate(nsnull, aRect, aHandleEventFunction,
                              aContext, aAppShell, aToolkit, aInitData,
                              aParent));
}

static NS_DEFINE_IID(kWindowServiceCID,NS_XLIB_WINDOW_SERVICE_CID);
static NS_DEFINE_IID(kWindowServiceIID,NS_XLIB_WINDOW_SERVICE_IID);

nsresult
nsWidget::StandardWidgetCreate(nsIWidget *aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               nsWidgetInitData *aInitData,
                               nsNativeWidget aNativeParent)
{
  unsigned long attr_mask;
  XSetWindowAttributes attr;
  Window parent=nsnull;

  NS_ASSERTION(!mBaseWindow, "already initialized");
  if (mBaseWindow) return NS_ERROR_ALREADY_INITIALIZED;

  mDisplay = xxlib_rgb_get_display(mXlibRgbHandle);
  mScreen = xxlib_rgb_get_screen(mXlibRgbHandle);
  mVisual = xxlib_rgb_get_visual(mXlibRgbHandle);
  mDepth = xxlib_rgb_get_depth(mXlibRgbHandle);

  mParentWidget = aParent;

  NS_IF_ADDREF(mParentWidget);

  mBounds = aRect;

  if (mBounds.width <= 0)
    mBounds.width = 1;
  if (mBounds.height <= 0)
    mBounds.height = 1;

  BaseCreate(mParentWidget, mBounds, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);

  if (aNativeParent) {
    parent = (Window)aNativeParent;
    mListenForResizes = PR_TRUE;
  } else if (aParent) {
    parent = (Window)aParent->GetNativeData(NS_NATIVE_WINDOW);
  } else {
    parent = XRootWindowOfScreen(mScreen);
  }

  if (nsnull != aInitData) {
    mWindowType = aInitData->mWindowType;
  }

  mParentWindow = parent;

  attr.bit_gravity       = NorthWestGravity;
  attr.event_mask        = GetEventMask();
  attr.colormap          = xxlib_rgb_get_cmap(mXlibRgbHandle);
  attr.background_pixel  = mBackgroundPixel;
  attr.border_pixel      = mBorderPixel;
  attr_mask = CWBitGravity | CWEventMask | CWBorderPixel | CWBackPixel;

  if (attr.colormap)
    attr_mask |= CWColormap;

  switch (mWindowType) {
  case eWindowType_dialog:
    mIsToplevel = PR_TRUE;
    parent = XRootWindowOfScreen(mScreen);
    mBaseWindow = XCreateWindow(mDisplay, parent, mBounds.x, mBounds.y,
                                mBounds.width, mBounds.height, 0,
                                mDepth, InputOutput, mVisual,
                                attr_mask, &attr);
    AddWindowCallback(mBaseWindow, this);
    SetUpWMHints();
    XSetTransientForHint(mDisplay, mBaseWindow, mParentWindow);
    break;

  case eWindowType_popup:
    mIsToplevel = PR_TRUE;
    attr_mask |= CWOverrideRedirect | CWSaveUnder;
    attr.save_under = True;
    attr.override_redirect = True;
    parent = XRootWindowOfScreen(mScreen);
    mBaseWindow = XCreateWindow(mDisplay, parent,
                                mBounds.x, mBounds.y,
                                mBounds.width, mBounds.height, 0,
                                mDepth, InputOutput, mVisual,
                                attr_mask, &attr);
    AddWindowCallback(mBaseWindow, this);
    SetUpWMHints();
    XSetTransientForHint(mDisplay, mBaseWindow, mParentWindow);
    break;

  case eWindowType_toplevel:
    mIsToplevel = PR_TRUE;
    parent = XRootWindowOfScreen(mScreen);
    mBaseWindow = XCreateWindow(mDisplay, parent, mBounds.x, mBounds.y,
                                mBounds.width, mBounds.height, 0,
                                mDepth, InputOutput, mVisual,
                                attr_mask, &attr);
    AddWindowCallback(mBaseWindow, this);
    SetUpWMHints();
    break;

  case eWindowType_child:
    mIsToplevel = PR_FALSE;
    // for some reason I need to do this, as opposed to the other cases. FIXME
    CreateNative(parent, mBounds);
    break;

  default:
    break;
  }
  
  return NS_OK;
}

// FIXME: Being heavily modified so functionally similar to gtk version.
// (just a test) KenF
NS_IMETHODIMP nsWidget::Destroy()
{

  // Dont reenter.
  if (mIsDestroying)
    return NS_OK;

  mIsDestroying = PR_TRUE;

  nsBaseWidget::Destroy();
  NS_IF_RELEASE(mParentWidget);	//????
  

  if (mBaseWindow) {
    DestroyNative();
    //mBaseWindow = nsnull;
    if (PR_FALSE == mOnDestroyCalled)
      OnDestroy();
    mBaseWindow = nsnull;
    mEventCallback = nsnull;
  }

  return NS_OK;

}

NS_IMETHODIMP nsWidget::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Move(PRInt32 aX, PRInt32 aY)
{
  //printf("nsWidget::Move aX=%i, aY=%i\n", aX, aY);
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::Move(x, y)\n"));
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Moving window 0x%lx to %d, %d\n", mBaseWindow, aX, aY));

  if((aX == mBounds.x) && (aY == mBounds.y) && !mIsToplevel) {
    //printf("discard this move\n");
    return NS_OK;
  }


  mBounds.x = aX;
  mBounds.y = aY;

  if (mWindowType == eWindowType_popup) {
    nsRect aRect, transRect;
    PRInt32 screenWidth = WidthOfScreen(mScreen);
    PRInt32 screenHeight = HeightOfScreen(mScreen);

    if (aX >= screenWidth)
      aX = screenWidth - mBounds.width;
    if (aY >= screenHeight)
      aY = screenHeight - mBounds.height;

    aRect.x = aX;
    aRect.y = aY;

    if (mParentWidget) {
      mParentWidget->WidgetToScreen(aRect, transRect);
    } else if (mParentWindow) {
      Window child;
      XTranslateCoordinates(mDisplay, mParentWindow,
                            XRootWindowOfScreen(mScreen),
                            aX, aY, &transRect.x, &transRect.y,
                            &child);
    }
    aX = transRect.x;
    aY = transRect.y;
  }

  mRequestedSize.x = aX;
  mRequestedSize.y = aY;
  if (mParentWidget) {
    ((nsWidget*)mParentWidget)->WidgetMove(this);
  } else {
    /* Workaround for bug 77344. I am not sure whether this is mandatory or not. */
    if (mDisplay)
      XMoveWindow(mDisplay, mBaseWindow, aX, aY);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Resize(PRInt32 aWidth,
                               PRInt32 aHeight,
                               PRBool   aRepaint)
{
  //printf("nsWidget::Resize aWidth=%i, Height=%i\n",aWidth, aHeight);
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::Resize(width, height)\n"));

  if (aWidth <= 0) {
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("*** width is %d, fixing.\n", aWidth));
    aWidth = 1;

  }
  if (aHeight <= 0) {
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("*** height is %d, fixing.\n", aHeight));
    aHeight = 1;
  }

  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Resizing window 0x%lx to %d, %d\n", mBaseWindow, aWidth, aHeight));
  mRequestedSize.width = mBounds.width = aWidth;
  mRequestedSize.height = mBounds.height = aHeight;

  if (mParentWidget) {
    ((nsWidget *)mParentWidget)->WidgetResize(this);
  } else {
     /* Workaround for bug 77344. I am not sure whether this is mandatory or not. */
    if (mDisplay)
      XResizeWindow(mDisplay, mBaseWindow, aWidth, aHeight);
  }

  return NS_OK;
}

NS_IMETHODIMP nsWidget::Resize(PRInt32 aX,
                               PRInt32 aY,
                               PRInt32 aWidth,
                               PRInt32 aHeight,
                               PRBool   aRepaint)
{
  //printf("nsWidget::Resize aX=%i, aY=%i, aWidth=%i, aHeight=%i\n", aX, aY, aWidth, aHeight);
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::Resize(x, y, width, height)\n"));

  if (aWidth <= 0) {
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("*** width is %d, fixing.\n", aWidth));
    aWidth = 1;
  }
  if (aHeight <= 0) {
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("*** height is %d, fixing.\n", aHeight));
    aHeight = 1;
  }
  if (aX < 0) {
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("*** x is %d, fixing.\n", aX));
    aX = 0;
  }
  if (aY < 0) {
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("*** y is %d, fixing.\n", aY));
    aY = 0;
  }
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG,
         ("Resizing window 0x%lx to %d, %d\n", mBaseWindow, aWidth, aHeight));
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, 
         ("Moving window 0x%lx to %d, %d\n", mBaseWindow, aX, aY));
  mRequestedSize.x = aX;
  mRequestedSize.y = aY;
  mRequestedSize.width = mBounds.width = aWidth;
  mRequestedSize.height = mBounds.height = aHeight;
  if (mParentWidget) {
    ((nsWidget *)mParentWidget)->WidgetMoveResize(this);
  } else {
    XMoveResizeWindow(mDisplay, mBaseWindow, aX, aY, aWidth, aHeight);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Enable(PRBool bState)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetFocus(PRBool aRaise)
{

  if (mBaseWindow) {
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::SetFocus() setting focus to 0x%lx\n", mBaseWindow));
    mFocusWindow = mBaseWindow;
  }
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetName(const char * aName)
{
  mName.AssignWithConversion(aName);

  return NS_OK;
}

Window
nsWidget::GetFocusWindow(void)
{
  return mFocusWindow;
}

NS_IMETHODIMP nsWidget::Invalidate(PRBool aIsSynchronous)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::Invalidate(sync)\n"));
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::Invalidate(rect, sync)\n"));
  return NS_OK;
}

nsIFontMetrics* nsWidget::GetFont(void)
{
  return nsnull;
}

NS_IMETHODIMP nsWidget::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetMenuBar(nsIMenuBar * aMenuBar)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::ShowMenuBar(PRBool aShow)
{
  return NS_OK;
}

void * nsWidget::GetNativeData(PRUint32 aDataType)
{
  switch (aDataType) {
  case NS_NATIVE_WIDGET:
  case NS_NATIVE_WINDOW:
  case NS_NATIVE_PLUGIN_PORT:
    return (void *)mBaseWindow;
    break;
  case NS_NATIVE_DISPLAY:
    return (void *)mDisplay;
    break;
  case NS_NATIVE_GRAPHIC:
    NS_ASSERTION(nsnull != mToolkit, "NULL toolkit, unable to get a GC");
    return (void *)NS_STATIC_CAST(nsToolkit*,mToolkit)->GetSharedGC();
    break;
  default:
    fprintf(stderr, "nsWidget::GetNativeData(%d) called with crap value.\n",
            aDataType);
    return NULL;
    break;
  }
}

NS_IMETHODIMP nsWidget::SetTooltips(PRUint32 aNumberOfTips,
                                    nsRect* aTooltipAreas[])
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::UpdateTooltips(nsRect* aNewTips[])
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::RemoveTooltips()
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetFont(const nsFont &aFont)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::BeginResizingChildren(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::EndResizingChildren(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetColorMap(nsColorMap *aColorMap)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Show(PRBool bState)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::Show()\n"));
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("state is %d\n", bState));

  if (bState) {
        Map();
  } else {
      Unmap();
    }
  return NS_OK;
}

NS_IMETHODIMP nsWidget::IsVisible(PRBool &aState)
{
  if (mVisibility != VisibilityFullyObscured) {
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::IsVisible: yes\n"));
    aState = PR_TRUE;
  }
  else {
    PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::IsVisible: no\n"));
    aState = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Update()
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::Update()\n"));
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetBackgroundColor(const nscolor &aColor)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWidget::SetBackgroundColor()\n"));

  nsBaseWidget::SetBackgroundColor(aColor);
  mBackgroundPixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, mBackground);
  // set the window attrib
  XSetWindowBackground(mDisplay, mBaseWindow, mBackgroundPixel);
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  return NS_OK;
}

NS_IMPL_ISUPPORTS_INHERITED1(nsWidget, nsBaseWidget, nsISupportsWeakReference)

NS_IMETHODIMP nsWidget::WidgetToScreen(const nsRect& aOldRect,
                                       nsRect& aNewRect)
{
  Window child;
  XTranslateCoordinates(mDisplay,
                        mBaseWindow,
                        XRootWindowOfScreen(mScreen),
                        aOldRect.x, aOldRect.y,
                        &aNewRect.x, &aNewRect.y,
                        &child);
  return NS_OK;
}

NS_IMETHODIMP nsWidget::ScreenToWidget(const nsRect& aOldRect,
                                       nsRect& aNewRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetCursor(nsCursor aCursor)
{

  if (!mBaseWindow)
    return NS_ERROR_FAILURE;

  /* don't bother setting if it it isnt mapped, duh */
  if (!mMapped)
    return NS_OK;
  
  // Only change cursor if it's changing
  if (aCursor != mCursor) {
    Cursor newCursor = None;

    newCursor = XlibCreateCursor(aCursor);

    if (None != newCursor) {
      mCursor = aCursor;
      XDefineCursor(mDisplay, mBaseWindow, newCursor);
      XFlush(mDisplay);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsWidget::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    SetWindowType(aInitData->mWindowType);
    SetBorderStyle(aInitData->mBorderStyle);
    
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

nsIWidget *nsWidget::GetParent(void)
{
  if (nsnull != mParentWidget) {
    NS_ADDREF(mParentWidget);
  }
  return mParentWidget;
}

void nsWidget::CreateNative(Window aParent, nsRect aRect)
{
  XSetWindowAttributes attr;
  unsigned long attr_mask;

  attr.bit_gravity       = NorthWestGravity;
  attr.event_mask        = GetEventMask();
  attr.colormap          = xxlib_rgb_get_cmap(mXlibRgbHandle);
  attr.background_pixel  = mBackgroundPixel;
  attr.border_pixel      = mBorderPixel;
  attr_mask = CWBitGravity | CWEventMask | CWBorderPixel | CWBackPixel;

  if (attr.colormap)
    attr_mask |= CWColormap;

  CreateNativeWindow(aParent, mBounds, attr, attr_mask);
}

/* virtual */ long
nsWidget::GetEventMask()
{
  long event_mask;

  event_mask = 
    ButtonMotionMask |
    Button1MotionMask |
    ButtonPressMask |
    ButtonReleaseMask |
    EnterWindowMask |
    ExposureMask |
    KeyPressMask |
    KeyReleaseMask |
    LeaveWindowMask |
    PointerMotionMask |
    StructureNotifyMask |
    VisibilityChangeMask |
    FocusChangeMask |
    OwnerGrabButtonMask;

  return event_mask;
}

void nsWidget::CreateNativeWindow(Window aParent, nsRect aRect,
                                  XSetWindowAttributes aAttr, unsigned long aMask)
{
  NS_ASSERTION(!mBaseWindow, "already initialized");
  if (mBaseWindow) return;

  mBaseWindow = XCreateWindow(mDisplay,
                              aParent,
                              aRect.x, aRect.y,
                              aRect.width, aRect.height,
                              0,                // border width
                              mDepth,           // depth
                              InputOutput,      // class
                              mVisual,          // visual
                              aMask,
                              &aAttr);

  mRequestedSize.height = mBounds.height = aRect.height;
  mRequestedSize.width = mBounds.width = aRect.width;
  AddWindowCallback(mBaseWindow, this);
}

nsWidget *
nsWidget::GetWidgetForWindow(Window aWindow)
{
  if (gsWindowList == nsnull) {
    return nsnull;
  }
  nsWindowKey window_key(aWindow);
  nsWidget *retval = (nsWidget *)gsWindowList->Get(&window_key);
  return retval;
}

void
nsWidget::AddWindowCallback(Window aWindow, nsWidget *aWidget)
{
  // make sure that the list has been initialized
  if (gsWindowList == nsnull) {
    gsWindowList = new nsHashtable();
  }
  nsWindowKey window_key(aWindow);
  gsWindowList->Put(&window_key, aWidget);

  // make sure that if someone is listening that we inform
  // them of the new window
  if (gsWindowCreateCallback) 
  {
    (*gsWindowCreateCallback)(aWindow);
  }
}

void
nsWidget::DeleteWindowCallback(Window aWindow)
{
  nsWindowKey window_key(aWindow);
  gsWindowList->Remove(&window_key);

  if (gsWindowList->Count() == 0) {
    delete gsWindowList;
    gsWindowList = nsnull;

    /* clear out the cursor cache */
#ifdef DEBUG_CURSORCACHE
    printf("freeing cursor cache\n");
#endif
    for (int i = 0; i < eCursor_count_up_down; i++)
      if (gsXlibCursorCache[i])
        XFreeCursor(nsAppShell::mDisplay, gsXlibCursorCache[i]);
  }

  if (gsWindowDestroyCallback) 
  {
    (*gsWindowDestroyCallback)(aWindow);
  }

}

#undef TRACE_PAINT
#undef TRACE_PAINT_FLASH

#ifdef TRACE_PAINT_FLASH
#include "nsXUtils.h" // for nsXUtils::XFlashWindow()
#endif

PRBool
nsWidget::OnPaint(nsPaintEvent &event)
{
  nsresult result = PR_FALSE;
  if (mEventCallback) {
    event.renderingContext = GetRenderingContext();
    if (event.renderingContext) {
      result = DispatchWindowEvent(event);
      NS_RELEASE(event.renderingContext);
    }
  }
  
#ifdef TRACE_PAINT
	static PRInt32 sPrintCount = 0;

    if (event.rect) 
	{
      printf("%4d nsWidget::OnPaint   (this=%p,name=%s,xid=%p,rect=%d,%d,%d,%d)\n", 
			 sPrintCount++,
			 (void *) this,
			 (const char *) nsCAutoString(mName),
			 (void *) mBaseWindow,
			 event.rect->x, 
			 event.rect->y,
			 event.rect->width, 
			 event.rect->height);
    }
    else 
	{
      printf("%4d nsWidget::OnPaint   (this=%p,name=%s,xid=%p,rect=none)\n", 
			 sPrintCount++,
			 (void *) this,
			 (const char *) nsCAutoString(mName),
			 (void *) mBaseWindow);
    }
#endif

#ifdef TRACE_PAINT_FLASH
    XRectangle ar;
    XRectangle * area = NULL;

    if (event.rect)
    {
      ar.x = event.rect->x;
      ar.y = event.rect->y;

      ar.width = event.rect->width;
      ar.height = event.rect->height;

      area = &ar;
    }

    nsXUtils::XFlashWindow(mDisplay,mBaseWindow,1,100000,area);
#endif


  return result;
}

PRBool nsWidget::IsMouseInWindow(Window window, PRInt32 inMouseX, PRInt32 inMouseY){

	XWindowAttributes inWindowAttributes;

  /* sometimes we get NULL window */
  if (!window)
    return PR_FALSE;

	// Get the origin (top left corner) coordinate and size
	if (XGetWindowAttributes(mDisplay, window, &inWindowAttributes) == 0) {
		fprintf(stderr, "Failed calling XGetWindowAttributes in nsWidget::IsMouseInWindow");
		return PR_FALSE;
	}
	
	// Note: These coordinates are now relative to the root window as popups are now created
	// with the root window as parent
	
	// Must get mouse click coordinates relative to root window
	int root_inMouse_x;
	int root_inMouse_y;
	Window returnedChild;
	Window rootWindow;
	rootWindow = XRootWindow(mDisplay, DefaultScreen(mDisplay));
	if (!XTranslateCoordinates(mDisplay, mBaseWindow, rootWindow,
		inMouseX, inMouseY,
		&root_inMouse_x, &root_inMouse_y, &returnedChild)){
		fprintf(stderr, "Could not get coordinates for origin coordinates for mouseclick\n");
		// should we return true or false??????
		return PR_FALSE;
	}
	//fprintf(stderr, "Here are the mouse click coordinates x:%i y%i\n", root_inMouse_x, root_inMouse_y);
	
	// Test using coordinates relative to root window if click was inside passed popup window
	if (root_inMouse_x > inWindowAttributes.x &&
			root_inMouse_x < (inWindowAttributes.x + inWindowAttributes.width) &&
      root_inMouse_y > inWindowAttributes.y &&
			root_inMouse_y < (inWindowAttributes.y + inWindowAttributes.height)){
    //fprintf(stderr, "Mouse click INSIDE passed popup\n");
		return PR_TRUE;
	}
	//fprintf(stderr, "Mouse click OUTSIDE of passed popup\n");
	return PR_FALSE;
}


//
// HandlePopup
//
// Deal with rollup of popups (xpmenus, etc)
// 
PRBool nsWidget::HandlePopup ( PRInt32 inMouseX, PRInt32 inMouseY )
{
	PRBool retVal = PR_FALSE;
	PRBool rollup = PR_FALSE;
	
	// The gRollupListener and gRollupWidget are both set to nsnull when a popup is no
	// longer visible
	
  nsCOMPtr<nsIWidget> rollupWidget = do_QueryReferent(gRollupWidget);
	
  if (rollupWidget && gRollupListener) {
    Window currentPopup = (Window)rollupWidget->GetNativeData(NS_NATIVE_WINDOW);

    if (!IsMouseInWindow(currentPopup, inMouseX, inMouseY)) {
			rollup = PR_TRUE;
			nsCOMPtr<nsIMenuRollup> menuRollup ( do_QueryInterface(gRollupListener) );
      if ( menuRollup ) {
        nsCOMPtr<nsISupportsArray> widgetChain;
        menuRollup->GetSubmenuWidgetChain ( getter_AddRefs(widgetChain) );
        if ( widgetChain ) {
          PRUint32 count = 0;
          widgetChain->Count ( &count );
          for ( PRUint32 i = 0; i < count; ++i ) {
            nsCOMPtr<nsISupports> genericWidget;
            widgetChain->GetElementAt ( i, getter_AddRefs(genericWidget) );
            nsCOMPtr<nsIWidget> widget ( do_QueryInterface(genericWidget) );
            if ( widget ) {
              Window currWindow = (Window)widget->GetNativeData(NS_NATIVE_WINDOW);
              if ( IsMouseInWindow(currWindow, inMouseX, inMouseY) ) {
                rollup = PR_FALSE;
                break;
              }
            }
          } // foreach parent menu widget
        }
			}	
		}				
	}
	
	if (rollup){
		gRollupListener->Rollup();
		retVal = PR_TRUE;
	}
	return retVal;
}


// Added KenF FIXME
void nsWidget::OnDestroy()
{
  mOnDestroyCalled = PR_TRUE;
  nsBaseWidget::OnDestroy();

  // M14/GTK creates a widget which is called kungFuDeathGrip
  // and assigns it "this". This might be because its making sure that this 
  // widget isn't destroyed? (still has a reference to it?)
  // Check into it. FIXME KenF
  DispatchDestroyEvent();
}

PRBool nsWidget::OnDeleteWindow(void)
{
  printf("nsWidget::OnDeleteWindow()\n");
  nsBaseWidget::OnDestroy();
  // emit a destroy signal
  return DispatchDestroyEvent();
}

PRBool nsWidget::DispatchDestroyEvent(void) {
  PRBool result = PR_FALSE;
  if (nsnull != mEventCallback) {
    nsGUIEvent event;
    event.nativeMsg = nsnull;
    event.eventStructType = NS_GUI_EVENT;
    event.message = NS_DESTROY;
    event.widget = this;
    event.time = 0;
    event.point.x = 0;
    event.point.y = 0;
    AddRef();
    result = DispatchWindowEvent(event);
    Release();
  }
  return result;
}

PRBool nsWidget::DispatchMouseEvent(nsMouseEvent& aEvent) 
{
  PRBool result = PR_FALSE;
  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

	// If there was a mouse down event, check if any popups need to be informed
	switch (aEvent.message) {
		case NS_MOUSE_LEFT_BUTTON_DOWN:
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    case NS_MOUSE_RIGHT_BUTTON_DOWN:			
			if (HandlePopup(aEvent.point.x, aEvent.point.y)){
				// Should we return here as GTK does?
				return PR_TRUE;
			}
			break;
	}
		
  if (nsnull != mEventCallback) {
    result = DispatchWindowEvent(aEvent);
    return result;
  }
  if (nsnull != mMouseListener) {
    switch (aEvent.message) {
    case NS_MOUSE_MOVE:
      // XXX this isn't handled in gtk for some reason.
      break;
    case NS_MOUSE_LEFT_BUTTON_DOWN:
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      result = ConvertStatus(mMouseListener->MousePressed(aEvent));
      break;
    case NS_MOUSE_LEFT_BUTTON_UP:
    case NS_MOUSE_MIDDLE_BUTTON_UP:
    case NS_MOUSE_RIGHT_BUTTON_UP:
      result = ConvertStatus(mMouseListener->MouseReleased(aEvent));
      result = ConvertStatus(mMouseListener->MouseClicked(aEvent));
      break;
    }
  }
  return result;
}

PRBool
nsWidget::OnResize(nsSizeEvent &event)
{

  mBounds.width = event.mWinWidth;
  mBounds.height = event.mWinHeight;

  nsresult result = PR_FALSE;
  if (mEventCallback) {
      result = DispatchWindowEvent(event);
  }
  return result;
}

PRBool nsWidget::DispatchWindowEvent(nsGUIEvent & aEvent)
{
  nsEventStatus status;
  DispatchEvent(&aEvent, status);
  return ConvertStatus(status);
}

PRBool nsWidget::DispatchKeyEvent(nsKeyEvent & aKeyEvent)
{
  if (mEventCallback) 
  {
    return DispatchWindowEvent(aKeyEvent);
  }

  return PR_FALSE;
}

//////////////////////////////////////////////////////////////////
//
// Turning TRACE_EVENTS on will cause printfs for all
// mouse events that are dispatched.
//
// These are extra noisy, and thus have their own switch:
//
// NS_MOUSE_MOVE
// NS_PAINT
// NS_MOUSE_ENTER, NS_MOUSE_EXIT
//
//////////////////////////////////////////////////////////////////

#undef TRACE_EVENTS
#undef TRACE_EVENTS_MOTION
#undef TRACE_EVENTS_PAINT
#undef TRACE_EVENTS_CROSSING
#undef DEBUG

#ifdef DEBUG
void
nsWidget::DebugPrintEvent(nsGUIEvent &   aEvent,
                          Window         aWindow)
{
#ifndef TRACE_EVENTS_MOTION
  if (aEvent.message == NS_MOUSE_MOVE)
  {
    return;
  }
#endif

#ifndef TRACE_EVENTS_PAINT
  if (aEvent.message == NS_PAINT)
  {
    return;
  }
#endif

#ifndef TRACE_EVENTS_CROSSING
  if (aEvent.message == NS_MOUSE_ENTER || aEvent.message == NS_MOUSE_EXIT)
  {
    return;
  }
#endif

  static int sPrintCount=0;

  nsCAutoString eventString;
  eventString.AssignWithConversion(debug_GuiEventToString(&aEvent));
  printf("%4d %-26s(this=%-8p , window=%-8p",
         sPrintCount++,
         (const char *) eventString,
         (void *) this,
         (void *) aWindow);
         
  printf(" , x=%-3d, y=%d)",aEvent.point.x,aEvent.point.y);

  printf("\n");
}
#endif // DEBUG
//////////////////////////////////////////////////////////////////


NS_IMETHODIMP nsWidget::DispatchEvent(nsGUIEvent * aEvent,
                                      nsEventStatus &aStatus)
{
#ifdef TRACE_EVENTS
  DebugPrintEvent(*aEvent,mBaseWindow);
#endif

  NS_ADDREF(aEvent->widget);

  if (nsnull != mMenuListener) {
    if (NS_MENU_EVENT == aEvent->eventStructType)
      aStatus = mMenuListener->MenuSelected(NS_STATIC_CAST(nsMenuEvent&, *aEvent));
  }

  aStatus = nsEventStatus_eIgnore;
  if (nsnull != mEventCallback) {
    aStatus = (*mEventCallback)(aEvent);
  }

  // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
    aStatus = mEventListener->ProcessEvent(*aEvent);
  }

  NS_RELEASE(aEvent->widget);

  return NS_OK;
}

PRBool nsWidget::ConvertStatus(nsEventStatus aStatus)
{
  switch(aStatus) {
    case nsEventStatus_eIgnore:
      return(PR_FALSE);
    case nsEventStatus_eConsumeNoDefault:
      return(PR_TRUE);
    case nsEventStatus_eConsumeDoDefault:
      return(PR_FALSE);
    default:
      NS_ASSERTION(0, "Illegal nsEventStatus enumeration value");
      break;
  }
  return(PR_FALSE);
}

void nsWidget::WidgetPut(nsWidget *aWidget)
{
  
}

void nsWidget::WidgetMove(nsWidget *aWidget)
{
  PR_LOG(XlibScrollingLM, PR_LOG_DEBUG, ("nsWidget::WidgetMove()\n"));
  XMoveWindow(aWidget->mDisplay, aWidget->mBaseWindow,
              aWidget->mRequestedSize.x, aWidget->mRequestedSize.y);
}

void nsWidget::WidgetResize(nsWidget *aWidget)
{
  PR_LOG(XlibScrollingLM, PR_LOG_DEBUG, ("nsWidget::WidgetResize()\n"));
  XResizeWindow(aWidget->mDisplay, aWidget->mBaseWindow,
                aWidget->mRequestedSize.width,
                aWidget->mRequestedSize.height);
}

void nsWidget::WidgetMoveResize(nsWidget *aWidget)
{
  PR_LOG(XlibScrollingLM, PR_LOG_DEBUG, ("nsWidget::WidgetMoveResize()\n"));
  XMoveResizeWindow(aWidget->mDisplay,
                    aWidget->mBaseWindow,
                    aWidget->mRequestedSize.x,
                    aWidget->mRequestedSize.y,
                    aWidget->mRequestedSize.width,
                    aWidget->mRequestedSize.height);
}

void nsWidget::WidgetShow(nsWidget *aWidget)
{
  aWidget->Map();
}

PRBool nsWidget::WidgetVisible(nsRect &aBounds)
{
  nsRect scrollArea;
  scrollArea.x = 0;
  scrollArea.y = 0;
  scrollArea.width = mRequestedSize.width + 1;
  scrollArea.height = mRequestedSize.height + 1;
  if (scrollArea.Intersects(aBounds)) {
    PR_LOG(XlibScrollingLM, PR_LOG_DEBUG, ("nsWidget::WidgetVisible(): widget is visible\n"));
    return PR_TRUE;
  }
  PR_LOG(XlibScrollingLM, PR_LOG_DEBUG, ("nsWidget::WidgetVisible(): widget is not visible\n"));
  return PR_FALSE;
}

void nsWidget::Map(void)
{
  if (!mMapped) {
     /* Workaround for bug 77344. I am not sure whether this is mandatory or not. */
    if (mDisplay)
      XMapWindow(mDisplay, mBaseWindow);
    mMapped = PR_TRUE;
  }
}

void nsWidget::Unmap(void)
{
  if (mMapped) {
     /* Workaround for bug 77344. I am not sure whether this is mandatory or not. */
    if (mDisplay)
      XUnmapWindow(mDisplay, mBaseWindow);
    mMapped = PR_FALSE;
  }
}

void nsWidget::SetVisibility(int aState)
{
  mVisibility = aState;
}

// create custom pixmap cursor from cursors in nsXlibCursors.h
Cursor nsWidget::XlibCreateCursor(nsCursor aCursorType)
{
  Pixmap cursor = None;
  Pixmap mask = None;
  XColor fg, bg;
  Cursor xcursor = None;
  PRUint8 newType = 0xff;

  fg.pixel = 0;
  fg.red = 0;
  fg.green = 0;
  fg.blue = 0;
  fg.flags = 0xf;

  bg.pixel = 0xffffffff;
  bg.red = 0xffff;
  bg.green = 0xffff;
  bg.blue = 0xffff;
  bg.flags = 0xf;

  /* lookup the cursor in cache */
  if ((xcursor = gsXlibCursorCache[aCursorType])) {
#ifdef DEBUG_CURSORCACHE
    printf("cached cursor found: %lx\n", xcursor);
#endif
    return xcursor;
  }

  /* handle cursor type */
  switch (aCursorType) {
    case eCursor_select:
      xcursor = XCreateFontCursor(mDisplay, XC_xterm);
      break;
    case eCursor_wait:
      xcursor = XCreateFontCursor(mDisplay, XC_watch);
      break;
    case eCursor_hyperlink:
      xcursor = XCreateFontCursor(mDisplay, XC_hand2);
      break;
    case eCursor_standard:
      xcursor = XCreateFontCursor(mDisplay, XC_left_ptr);
      break;
    case eCursor_sizeWE:
      xcursor = XCreateFontCursor(mDisplay, XC_sb_h_double_arrow);
      break;
    case eCursor_sizeNS:
      xcursor = XCreateFontCursor(mDisplay, XC_sb_v_double_arrow);
      break;
    case eCursor_sizeNW:
      xcursor = XCreateFontCursor(mDisplay, XC_top_left_corner);
      break;
    case eCursor_sizeSE:
      xcursor = XCreateFontCursor(mDisplay, XC_bottom_right_corner);
      break;
    case eCursor_sizeNE:
      xcursor = XCreateFontCursor(mDisplay, XC_top_right_corner);
      break;
    case eCursor_sizeSW:
      xcursor = XCreateFontCursor(mDisplay, XC_bottom_left_corner);
      break;
    case eCursor_arrow_south:
    case eCursor_arrow_south_plus:
      xcursor = XCreateFontCursor(mDisplay, XC_bottom_side);
      break;
    case eCursor_arrow_north:
    case eCursor_arrow_north_plus:
      xcursor = XCreateFontCursor(mDisplay, XC_top_side);
      break;
    case eCursor_arrow_east:
    case eCursor_arrow_east_plus:
      xcursor = XCreateFontCursor(mDisplay, XC_right_side);
      break;
    case eCursor_arrow_west:
    case eCursor_arrow_west_plus:
      xcursor = XCreateFontCursor(mDisplay, XC_left_side);
      break;
    case eCursor_crosshair:
      xcursor = XCreateFontCursor(mDisplay, XC_crosshair);
      break;
    case eCursor_move:
      xcursor = XCreateFontCursor(mDisplay, XC_fleur);
      break;
    case eCursor_help:
      newType = XLIB_QUESTION_ARROW;
      break;
    case eCursor_copy:
      newType = XLIB_COPY;
      break;
    case eCursor_alias:
      newType = XLIB_ALIAS;
      break;
    case eCursor_context_menu:
      newType = XLIB_CONTEXT_MENU;
      break;
    case eCursor_cell:
      xcursor = XCreateFontCursor(mDisplay, XC_plus);
      break;
    case eCursor_grab:
      newType = XLIB_HAND_GRAB;
      break;
    case eCursor_grabbing:
      newType = XLIB_HAND_GRABBING;
      break;
    case eCursor_spinning:
      newType = XLIB_SPINNING;
      break;
    case eCursor_count_up:
    case eCursor_count_down:
    case eCursor_count_up_down:
      // XXX: these CSS3 cursors need to be implemented
      // I simply have no idea how they should look like
      xcursor = XCreateFontCursor(mDisplay, XC_left_ptr);
      break;
    default:
      break;
  }

  /* if by now we dont have a xcursor, this means we have to make a custom one */
  if (!xcursor) {
    NS_ASSERTION(newType != 0xff, "Unknown cursor type and no standard cursor");
    
    /* we can use mBaseWindow for the pixmaps because SetCursor checks this
     * before calling here */
    cursor = XCreatePixmapFromBitmapData(mDisplay, mBaseWindow,
                        (char *)XlibCursors[newType].bits,
                        32, 32, 0xffffffff, 0x0, 1);

    mask = XCreatePixmapFromBitmapData(mDisplay, mBaseWindow,
                        (char *)XlibCursors[newType].mask_bits,
                        32, 32, 0xffffffff, 0x0, 1);

    xcursor = XCreatePixmapCursor(mDisplay, cursor, mask, &fg, &bg,
                                  XlibCursors[newType].hot_x,
                                  XlibCursors[newType].hot_y);

    XFreePixmap(mDisplay, mask);
    XFreePixmap(mDisplay, cursor);
  }

#ifdef DEBUG_CURSORCACHE
  printf("inserting cursor into the cache: %lx\n", xcursor);
#endif
  gsXlibCursorCache[aCursorType] = xcursor;
  
  return xcursor;
}

// nsresult
// nsWidget::SetXlibWindowCallback(nsXlibWindowCallback *aCallback)
// {
//   if (aCallback == nsnull) {
//     return NS_ERROR_FAILURE;
//   }
//   else {
//     mWindowCallback = aCallback;
//   }
//   return NS_OK;
// }

// nsresult
// nsWidget::XWindowCreated(Window aWindow) {
//   if (mWindowCallback) {
//     mWindowCallback->WindowCreated(aWindow);
//   }
//   return NS_OK;
// }

// nsresult
// nsWidget::XWindowDestroyed(Window aWindow) {
//   if (mWindowCallback) {
//     mWindowCallback->WindowDestroyed(aWindow);
//   }
//   return NS_OK;
// }

void
nsWidget::SetUpWMHints(void) {
  // check to see if we need to get the atoms for the protocols
  if (WMProtocolsInitialized == PR_FALSE) {
    WMDeleteWindow = XInternAtom(mDisplay, "WM_DELETE_WINDOW", True);
    WMTakeFocus = XInternAtom(mDisplay, "WM_TAKE_FOCUS", True);
    WMSaveYourself = XInternAtom(mDisplay, "WM_SAVE_YOURSELF", True);
    WMProtocolsInitialized = PR_TRUE;
  }
  Atom WMProtocols[2];
  WMProtocols[0] = WMDeleteWindow;
  WMProtocols[1] = WMTakeFocus;
  // note that we only set two of the above protocols
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Setting up wm hints for window 0x%lx\n",
                                       mBaseWindow));
  XSetWMProtocols(mDisplay, mBaseWindow, WMProtocols, 2);
}

NS_METHOD nsWidget::SetBounds(const nsRect &aRect)
{
  mRequestedSize = aRect;
  return nsBaseWidget::SetBounds(aRect);
}

NS_METHOD nsWidget::GetRequestedBounds(nsRect &aRect)
{
  aRect = mRequestedSize;
  return NS_OK;
}

NS_IMETHODIMP
nsWidget::SetTitle(const nsString& title)
{

  return NS_OK;
}

NS_IMETHODIMP nsWidget::CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent)
{

  return NS_OK;
}
