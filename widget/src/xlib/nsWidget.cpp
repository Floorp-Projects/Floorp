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

Display         *gDisplay;
Screen          *gScreen;
int              gScreenNum;
int              gDepth;
Visual          *gVisual;
XVisualInfo     *gVisualInfo;

PRUint32  gRedZeroMask;     //red color mask in zero position
PRUint32  gGreenZeroMask;   //green color mask in zero position
PRUint32  gBlueZeroMask;    //blue color mask in zero position
PRUint32  gAlphaZeroMask;   //alpha data mask in zero position
PRUint32  gRedMask;         //red color mask
PRUint32  gGreenMask;       //green color mask
PRUint32  gBlueMask;        //blue color mask
PRUint32  gAlphaMask;       //alpha data mask
PRUint8   gRedCount;        //number of red color bits
PRUint8   gGreenCount;      //number of green color bits
PRUint8   gBlueCount;       //number of blue color bits
PRUint8   gAlphaCount;      //number of alpha data bits
PRUint8   gRedShift;        //number to shift value into red position
PRUint8   gGreenShift;      //number to shift value into green position
PRUint8   gBlueShift;       //number to shift value into blue position
PRUint8   gAlphaShift;      //number to shift value into alpha position

nsWidget::nsWidget() : nsBaseWidget()
{
  mPreferredWidth = 0;
  mPreferredHeight = 0;
  mWindow = 0;
}

nsWidget::~nsWidget()
{
}

NS_IMETHODIMP nsWidget::Create(nsIWidget *aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               nsWidgetInitData *aInitData)
{
  return(StandardWindowCreate(aParent, aRect, aHandleEventFunction,
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
  return(StandardWindowCreate(nsnull, aRect, aHandleEventFunction,
                              aContext, aAppShell, aToolkit, aInitData,
                              aParent));
}

nsresult
nsWidget::StandardWindowCreate(nsIWidget *aParent,
                               const nsRect &aRect,
                               EVENT_CALLBACK aHandleEventFunction,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               nsWidgetInitData *aInitData,
                               nsNativeWidget aNativeParent)
{

  XSetWindowAttributes attr;
  unsigned long        attr_mask;
  Window parent;
  
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
    parent = RootWindow(gDisplay, gScreenNum);
  }
  // on a window resize, we don't want to window contents to
  // be discarded...
  attr.bit_gravity = NorthWestGravity;
  // make sure that we listen for events
  attr.event_mask = SubstructureNotifyMask | StructureNotifyMask;
  // here's what's in the struct
  attr_mask = CWBitGravity | CWEventMask;
  
  mWindow = XCreateWindow(gDisplay,
                          parent,
                          aRect.x, aRect.y,
                          aRect.width, aRect.height,
                          0, // border width
                          gDepth,
                          InputOutput,    // class
                          CopyFromParent, // visual
                          attr_mask,
                          &attr);
  // map this window and flush the connection.  we want to see this
  // thing now.
  XMapWindow(gDisplay,
             mWindow);
  XFlush(gDisplay);
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Destroy()
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Move(PRUint32 aX, PRUint32 aY)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Resize(PRUint32 aWidth,
                               PRUint32 aHeight,
                               PRBool   aRepaint)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Resize(PRUint32 aX,
                               PRUint32 aY,
                               PRUint32 aWidth,
                               PRUint32 aHeight,
                               PRBool   aRepaint)
{
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
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
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

void * nsWidget::GetNativeData(PRUint32 aDataType)
{
  switch (aDataType) {
  case NS_NATIVE_WIDGET:
  case NS_NATIVE_WINDOW:
    return (void *)mWindow;
    break;
  case NS_NATIVE_DISPLAY:
    return (void *)gDisplay;
    break;
  case NS_NATIVE_GRAPHIC:
    // XXX implement this...
    return NULL;
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

NS_IMETHODIMP nsWidget::GetBounds(nsRect &aRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Show(PRBool bState)
{
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
  return NS_OK;
}

NS_IMETHODIMP nsWidget::DispatchEvent(nsGUIEvent* event,
                                      nsEventStatus & aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::GetClientBounds(nsRect &aRect)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetBackgroundColor(const nscolor &aColor)
{
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
