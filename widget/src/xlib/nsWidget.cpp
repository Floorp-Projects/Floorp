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

#include "nsWidget.h"
#include "nsGfxCIID.h"
// set up our static members here.
nsHashtable *nsWidget::window_list = nsnull;

// this is a class for generating keys for
// the list of windows managed by mozilla.

class nsWindowKey : public nsHashKey {
protected:
  Window mKey;

public:
  nsWindowKey(Window key) {
    mKey = key;
  }
  ~nsWindowKey(void) {
  }
  PRUint32 HashValue(void) const {
    return (PRUint32)mKey;
  }

  PRBool Equals(const nsHashKey *aKey) const {
    return (mKey == ((nsWindowKey *)aKey)->mKey);
  }

  nsHashKey *Clone(void) const {
    return new nsWindowKey(mKey);
  }
};

nsWidget::nsWidget() : nsBaseWidget()
{
  mPreferredWidth = 0;
  mPreferredHeight = 0;

  mDisplay = 0;
  mScreen = 0;
  mVisual = 0;

  mBaseWindow = 0;
  mBackground = NS_RGB(192, 192, 192);
  mBackgroundPixel = xlib_rgb_xpixel_from_rgb(mBackground);
  mBackground = NS_RGB(192, 192, 192);
  mBorderPixel = xlib_rgb_xpixel_from_rgb(mBorderRGB);
  mGC = 0;
  mParentWidget = nsnull;
  mName = "unnamed";
}

nsWidget::~nsWidget()
{
  DestroyNative();
}

void
nsWidget::DestroyNative(void)
{
  if (mGC)
    XFreeGC(mDisplay, mGC);

  if (mBaseWindow) {
    XDestroyWindow(mDisplay, mBaseWindow);
    DeleteWindowCallback(mBaseWindow);
  }
}

NS_IMETHODIMP nsWidget::Create(nsIWidget *aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               nsWidgetInitData *aInitData)
{
  mParentWidget = aParent;

  return(StandardWidgetCreate(aParent, aRect, aHandleEventFunction,
                              aContext, aAppShell, aToolkit, aInitData,
                              nsnull));
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
  
  Window parent;

  mDisplay = gDisplay;
  mScreen = gScreen;
  mVisual = gVisual;

  // set up the BaseWidget parts.
  BaseCreate(aParent, aRect, aHandleEventFunction, aContext, 
             aAppShell, aToolkit, aInitData);
  // check to see if the parent is there...
  if (nsnull != aParent) {
    parent = ((aParent) ? (Window)aParent->GetNativeData(NS_NATIVE_WINDOW) : nsnull);
  } else {
    parent = (Window)aNativeParent;
  }
  // if there's no parent, make the parent the root window.
  if (parent == 0) {
    parent = RootWindowOfScreen(mScreen);
  }
  // set the bounds
  mBounds = aRect;
  // call the native create function
  CreateNative(parent, mBounds);
  XSync(mDisplay, False);
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Destroy()
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Move(PRUint32 aX, PRUint32 aY)
{
  printf("nsWidget::Move(x, y)\n");
  if (aX < 0) {
    printf("*** x is %d, fixing.\n", aX);
    aX = 0;
  }
  if (aY < 0) {
    printf("*** y is %d, fixing.\n", aY);
    aY = 0;
  }
  printf("Moving window 0x%lx to %d, %d\n", mBaseWindow, aX, aY);
  XMoveWindow(mDisplay, mBaseWindow, aX, aY);
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Resize(PRUint32 aWidth,
                               PRUint32 aHeight,
                               PRBool   aRepaint)
{
  printf("nsWidget::Resize(width, height)\n");
  if (aWidth <= 0) {
    printf("*** width is %d, fixing.\n", aWidth);
    aWidth = 1;
  }
  if (aHeight <= 0) {
    printf("*** height is %d, fixing.\n", aHeight);
    aHeight = 1;
  }
  printf("Resizing window 0x%lx to %d, %d\n", mBaseWindow, aWidth, aHeight);
  mBounds.width = aWidth;
  mBounds.height = aHeight;
  XResizeWindow(mDisplay, mBaseWindow, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Resize(PRUint32 aX,
                               PRUint32 aY,
                               PRUint32 aWidth,
                               PRUint32 aHeight,
                               PRBool   aRepaint)
{
  printf("nsWidget::Resize(x, y, width, height)\n");
  if (aWidth <= 0) {
    printf("*** width is %d, fixing.\n", aWidth);
    aWidth = 1;
  }
  if (aHeight <= 0) {
    printf("*** height is %d, fixing.\n", aHeight);
    aHeight = 1;
  }
  if (aX < 0) {
    printf("*** x is %d, fixing.\n", aX);
    aX = 0;
  }
  if (aY < 0) {
    printf("*** y is %d, fixing.\n", aY);
    aY = 0;
  }
  printf("Resizing window 0x%lx to %d, %d\n", mBaseWindow, aWidth, aHeight);
  printf("Moving window 0x%lx to %d, %d\n", mBaseWindow, aX, aY);
  mBounds.width = aWidth;
  mBounds.height = aHeight;
  XMoveResizeWindow(mDisplay, mBaseWindow, aX, aY, aWidth, aHeight);
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Enable(PRBool bState)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetFocus(void)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Invalidate(PRBool aIsSynchronous)
{
  printf("nsWidget::Invalidate(sync)\n");
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  printf("nsWidget::Invalidate(rect, sync)\n");
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

NS_IMETHODIMP nsWidget::IsMenuBarVisible(PRBool *aVisible)
{
  return NS_OK;
}

void * nsWidget::GetNativeData(PRUint32 aDataType)
{
  switch (aDataType) {
  case NS_NATIVE_WIDGET:
  case NS_NATIVE_WINDOW:
    return (void *)mBaseWindow;
    break;
  case NS_NATIVE_DISPLAY:
    return (void *)mDisplay;
    break;
  case NS_NATIVE_GRAPHIC:
    // XXX implement this...
    return (void *)mGC;
    break;
  default:
    fprintf(stderr, "nsWidget::GetNativeData(%d) called with crap value.\n",
            aDataType);
    return NULL;
    break;
  }
}

NS_IMETHODIMP nsWidget::SetTitle(const nsString& aTitle)
{
  return NS_OK;
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
  if (mBaseWindow) {
    XMapWindow(mDisplay, mBaseWindow);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWidget::IsVisible(PRBool &aState)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Update()
{
  printf("nsWidget::Update()\n");
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetBackgroundColor(const nscolor &aColor)
{
  printf("nsWidget::SetBackgroundColor()\n");
  nsBaseWidget::SetBackgroundColor(aColor);
  mBackgroundPixel = xlib_rgb_xpixel_from_rgb(mBackground);
  // set the window attrib
  XSetWindowBackground(mDisplay, mBaseWindow, mBackgroundPixel);
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::WidgetToScreen(const nsRect& aOldRect,
                                       nsRect& aNewRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::ScreenToWidget(const nsRect& aOldRect,
                                       nsRect& aNewRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetCursor(nsCursor aCursor)
{
  return NS_OK;
}

nsIWidget *nsWidget::GetParent(void)
{
  return nsnull;
}

void nsWidget::CreateNative(Window aParent, nsRect aRect)
{
  XSetWindowAttributes attr;
  unsigned long attr_mask;

  // on a window resize, we don't want to window contents to
  // be discarded...
  attr.bit_gravity = NorthWestGravity;
  // make sure that we listen for events
  attr.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
  // set the default background color and border to that awful gray
  attr.background_pixel = mBackgroundPixel;
  attr.border_pixel = mBorderPixel;
  // set the colormap
  attr.colormap = xlib_rgb_get_cmap();
  // here's what's in the struct
  attr_mask = CWBitGravity | CWEventMask | CWBackPixel | CWBorderPixel;
  // check to see if there was actually a colormap.
  if (attr.colormap)
    attr_mask |= CWColormap;

  CreateNativeWindow(aParent, mBounds, attr, attr_mask);
  CreateGC();

}
                            

void nsWidget::CreateNativeWindow(Window aParent, nsRect aRect,
                                  XSetWindowAttributes aAttr, unsigned long aMask)
{
  int width;
  int height;

  printf("*** Warning: nsWidget::CreateNative falling back to sane default for widget type \"%s\"\n", 
         (const char *) nsAutoCString(mName));

  if (mName == "unnamed") {
    printf("What freaking widget is this, anyway?\n");
  }
  printf("Creating XWindow: x %d y %d w %d h %d\n",
         aRect.x, aRect.y, aRect.width, aRect.height);
  if (aRect.width <= 0) {
    printf("*** Fixing width...\n");
    width = 1;
  }
  else {
    width = aRect.width;
  }
  if (aRect.height <= 0) {
    printf("*** Fixing height...\n");
    height = 1;
  }
  else {
    height = aRect.height;
  }
  
  mBaseWindow = XCreateWindow(mDisplay,
                              aParent,
                              aRect.x, aRect.y,
                              width, height,
                              0,                // border width
                              gDepth,
                              InputOutput,      // class
                              mVisual,          // visual
                              aMask,
                              &aAttr);

  printf("nsWidget: Created window 0x%lx with parent 0x%lx\n",
         mBaseWindow, aParent);

  // XXX when we stop getting lame values for this remove it.
  // sometimes the dimensions have been corrected by the code above.
  mBounds.height = height;
  mBounds.width = width;
  // add the callback for this
  AddWindowCallback(mBaseWindow, this);
}

nsWidget *
nsWidget::GetWidgetForWindow(Window aWindow)
{
  if (window_list == nsnull) {
    return nsnull;
  }
  nsWindowKey *window_key = new nsWindowKey(aWindow);
  nsWidget *retval = (nsWidget *)window_list->Get(window_key);
  return retval;
}

void
nsWidget::AddWindowCallback(Window aWindow, nsWidget *aWidget)
{
  // make sure that the list has been initialized
  if (window_list == nsnull) {
    window_list = new nsHashtable();
  }
  nsWindowKey *window_key = new nsWindowKey(aWindow);
  window_list->Put(window_key, aWidget);
  // add a new ref to this widget
  NS_ADDREF(aWidget);
  delete window_key;
}

void
nsWidget::DeleteWindowCallback(Window aWindow)
{
  nsWindowKey *window_key = new nsWindowKey(aWindow);
  nsWidget *widget = (nsWidget *)window_list->Get(window_key);
  NS_RELEASE(widget);
  window_list->Remove(window_key);
  delete window_key;
}

PRBool
nsWidget::OnPaint(nsPaintEvent &event)
{
  nsresult result = PR_FALSE;
  if (mEventCallback) {
    event.renderingContext = nsnull;
    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
    if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID,
                                                    nsnull,
                                                    kRenderingContextIID,
                                                    (void **)&event.renderingContext)) {
      event.renderingContext->Init(mContext, this);
      result = DispatchWindowEvent(&event);
      NS_RELEASE(event.renderingContext);
    }
    else {
      result = PR_FALSE;
    }
  }
  return result;
}

PRBool nsWidget::DispatchMouseEvent(nsMouseEvent& aEvent) 
{
  PRBool result = PR_FALSE;
  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  if (nsnull != mEventCallback) {
    result = DispatchWindowEvent(&aEvent);
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
  nsresult result = PR_FALSE;
  if (mEventCallback) {
      result = DispatchWindowEvent(&event);
  }
  return result;
}

PRBool nsWidget::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}


NS_IMETHODIMP nsWidget::DispatchEvent(nsGUIEvent *event,
                                      nsEventStatus &aStatus)
{
  NS_ADDREF(event->widget);

  if (nsnull != mMenuListener) {
    if (NS_MENU_EVENT == event->eventStructType)
      aStatus = mMenuListener->MenuSelected(NS_STATIC_CAST(nsMenuEvent&, *event));
  }

  aStatus = nsEventStatus_eIgnore;
  if (nsnull != mEventCallback) {
    aStatus = (*mEventCallback)(event);
  }

  // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
    aStatus = mEventListener->ProcessEvent(*event);
  }
  NS_RELEASE(event->widget);

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

void nsWidget::CreateGC(void)
{
  mGC = XCreateGC(mDisplay, mBaseWindow, 0, NULL);
}
