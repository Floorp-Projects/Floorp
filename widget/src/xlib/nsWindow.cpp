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

#include "nsWindow.h"

#include "xlibrgb.h"

nsWindow::nsWindow() : nsWidget()
{
  NS_INIT_REFCNT();
  mName = "nsWindow";
  mBackground = NS_RGB(255, 255, 255);
  mBackgroundPixel = xlib_rgb_xpixel_from_rgb(mBackground);
  mBorderRGB = NS_RGB(255,255,255);
  mBorderPixel = xlib_rgb_xpixel_from_rgb(mBorderRGB);
}

nsWindow::~nsWindow()
{
}

void
nsWindow::DestroyNative(void)
{
  if (mGC)
    XFreeGC(mDisplay, mGC);
  if (mBaseWindow) {
    XDestroyWindow(mDisplay, mBaseWindow);
    DeleteWindowCallback(mBaseWindow);
  }
}

#if 0
void nsWindow::CreateNative(Window aParent, nsRect aRect)
{
  XSetWindowAttributes attr;
  unsigned long attr_mask;

  // on a window resize, we don't want to window contents to
  // be discarded...
  attr.bit_gravity = NorthWestGravity;
  // make sure that we listen for events
  attr.event_mask = StructureNotifyMask | ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask | KeyReleaseMask | FocusChangeMask | VisibilityChangeMask;
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
#endif

/* virtual */ long
nsWindow::GetEventMask()
{
	long event_mask;

	event_mask = 
    ButtonMotionMask |
    ButtonPressMask | 
    ButtonReleaseMask | 
    EnterWindowMask |
    ExposureMask | 
    FocusChangeMask |
    KeyPressMask | 
    KeyReleaseMask | 
    LeaveWindowMask |
    PointerMotionMask |
    StructureNotifyMask | 
    VisibilityChangeMask;
  return event_mask;
}


NS_IMETHODIMP nsWindow::Invalidate(PRBool aIsSynchronous)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWindow::Invalidate(sync)\n"));
  nsPaintEvent pevent;
  pevent.message = NS_PAINT;
  pevent.widget = this;
  pevent.eventStructType = NS_PAINT_EVENT;
  pevent.rect = new nsRect (mBounds.x, mBounds.y,
                            mBounds.width, mBounds.height);
  // XXX fix this
  pevent.time = 0;
  AddRef();
  OnPaint(pevent);
  Release();
  delete pevent.rect;
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWindow::Invalidate(rect, sync)\n"));

  nsPaintEvent pevent;
  pevent.message = NS_PAINT;
  pevent.widget = this;
  pevent.eventStructType = NS_PAINT_EVENT;
  pevent.rect = new nsRect(aRect);
  // XXX fix this
  pevent.time = 0;
  AddRef();
  OnPaint(pevent);
  Release();
  // XXX will this leak?
  //delete pevent.rect;
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Update()
{
  //PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("nsWindow::Update()\n"));

  nsPaintEvent pevent;
  pevent.message = NS_PAINT;
  pevent.widget = this;
  pevent.eventStructType = NS_PAINT_EVENT;
  pevent.rect = new nsRect (mBounds.x, mBounds.y,
                            mBounds.height, mBounds.width);
  // XXX fix this
  pevent.time = 0;
  AddRef();
  OnPaint(pevent);
  Release();
  delete pevent.rect;
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  PR_LOG(XlibScrollingLM, PR_LOG_DEBUG, ("nsWindow::Scroll()\n"));
  
  //--------
  // Scroll the children
  nsCOMPtr<nsIEnumerator> children ( getter_AddRefs(GetChildren()) );
  if (children)
    {
      children->First();
      do
        {
          nsISupports* child;
          if (NS_SUCCEEDED(children->CurrentItem(&child)))
            {
              nsWindow* childWindow = static_cast<nsWindow*>(child);
              NS_RELEASE(child);
              
              nsRect bounds;
              childWindow->GetBounds(bounds);
              bounds.x += aDx;
              bounds.y += aDy;
              PR_LOG(XlibScrollingLM, PR_LOG_DEBUG, ("nsWindow::Scroll moving child to %d, %d\n", bounds.x, bounds.y));
              childWindow->Move(bounds.x, bounds.y);
            }
        } while (NS_SUCCEEDED(children->Next()));                       
    }
  
  if (aClipRect)
    Invalidate(*aClipRect, PR_TRUE);
  else 
    Invalidate(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetTitle(const nsString& aTitle)
{
  if(!mBaseWindow)
    return NS_ERROR_FAILURE;

  const char *text = aTitle.ToNewCString();
  XStoreName(mDisplay, mBaseWindow, text);
  delete [] text;
  return NS_OK;
}

ChildWindow::ChildWindow(): nsWindow()
{
  mName = "nsChildWindow";
}
