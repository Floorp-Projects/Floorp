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
 *   Tim Copperfield <timecop@japan.co.jp>
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

#include "nsScrollBar.h"
#include "nsGfxCIID.h"
#include "xlibrgb.h"

#define CLAMP(x, low, high) (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))
#define NS_SCROLLBAR_MIN 8

NS_IMPL_ADDREF_INHERITED(nsScrollbar, nsWidget)
NS_IMPL_RELEASE_INHERITED(nsScrollbar, nsWidget)
NS_IMPL_QUERY_INTERFACE2(nsScrollbar, nsIScrollbar, nsIWidget)

PRLogModuleInfo *XlibScrollbarLM = PR_NewLogModule("XlibScrollbar");

nsScrollbar::nsScrollbar(PRBool aIsVertical) : nsWidget(), nsIScrollbar()
{
  NS_INIT_REFCNT();
  mMaxRange = 0;
  mPosition = 0;
  mThumbSize = 0;
  mXScale = 0.0;
  mYScale = 0.0;
  mLineIncrement = 1;
  mIsVertical = aIsVertical;
  mBackground = NS_RGB(100,100,100);
  mBackgroundPixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, mBackground);
  mBorderPixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, mBackground);
  mBar = 0;
  mBarBounds.x = mBarBounds.y = mBarBounds.width = mBarBounds.height = 0;
};

nsScrollbar::~nsScrollbar()
{
}

/* Create scrollbar widget */
void nsScrollbar::CreateNative(Window aParent, nsRect aRect)
{
  XSetWindowAttributes attr;
  unsigned long attr_mask;

  // on a window resize, we don't want to window contents to
  // be discarded...
  attr.bit_gravity = NorthWestGravity;
  // make sure that we listen for events
  attr.event_mask = StructureNotifyMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask | FocusChangeMask | VisibilityChangeMask;
  // set the default background color and border to that awful gray
  attr.background_pixel = mBackgroundPixel;
  attr.border_pixel = mBorderPixel;
  // set the colormap
  attr.colormap = xxlib_rgb_get_cmap(mXlibRgbHandle);
  // here's what's in the struct
  attr_mask = CWBitGravity | CWEventMask | CWBackPixel | CWBorderPixel;
  // check to see if there was actually a colormap.
  if (attr.colormap)
    attr_mask |= CWColormap;

  CreateNativeWindow(aParent, mBounds, attr, attr_mask);
  // set up the scrolling bar.
  attr.event_mask = Button1MotionMask | ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask | FocusChangeMask | VisibilityChangeMask;
  attr.background_pixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, NS_RGB(192,192,192));
  attr.border_pixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, NS_RGB(100,100,100));
  // set up the size
  CalcBarBounds();
  mBar = XCreateWindow(mDisplay,
                       mBaseWindow,
                       mBarBounds.x, mBarBounds.y,
                       mBarBounds.width, mBarBounds.height,
                       2,  // border width
                       mDepth,
                       InputOutput,
                       mVisual,
                       attr_mask,
                       &attr);
  AddWindowCallback(mBar, this);
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::CreateNative created window 0x%lx with bar 0x%lx\n",
         mBaseWindow, mBar));
}

void nsScrollbar::DestroyNative(void)
{
  // override since we have two native widgets
  if (mBar) {
    DestroyNativeChildren(mDisplay, mBar);

    XDestroyWindow(mDisplay, mBar);
    DeleteWindowCallback(mBar);
    mBar = 0;
  }
  if (mBaseWindow) {
    nsWidget::DestroyNative();
    mBaseWindow = 0;
  }
}

NS_METHOD nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::SetMaxRange()\n"));
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("Max Range set to %d\n", aEndRange));
  mMaxRange = aEndRange;
  CalcBarBounds();
  LayoutBar();
  return NS_OK;
}

PRUint32 nsScrollbar::GetMaxRange(PRUint32& aRange)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::GetMaxRange()\n"));
  aRange = mMaxRange;
  return NS_OK;
}

NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::SetPosition()\n"));
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("Scroll to %d\n", aPos));
  mPosition = CLAMP(aPos, 0, mMaxRange);
  CalcBarBounds();
  LayoutBar();
  return NS_OK;
}

PRUint32 nsScrollbar::GetPosition(PRUint32& aPosition)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::GetPosition()\n"));
  aPosition = mPosition;
  return NS_OK;
}

NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::SetThumbSize()\n"));
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("Thumb size set to %d\n", aSize));

  mThumbSize = aSize;
  CalcBarBounds();
  LayoutBar();
  return NS_OK;
}

NS_METHOD nsScrollbar::GetThumbSize(PRUint32& aSize)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::GetThumbSize()\n"));

  aSize = mThumbSize;
  return NS_OK;
}

NS_METHOD nsScrollbar::SetLineIncrement(PRUint32 aSize)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::SetLineIncrement()\n"));
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("Set Line Increment to %d\n", aSize));

  mLineIncrement = aSize;
  CalcBarBounds();
  LayoutBar();
  return NS_OK;
}

NS_METHOD nsScrollbar::GetLineIncrement(PRUint32& aSize)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::GetLineIncrement()\n"));

  aSize = mLineIncrement;
  return NS_OK;
}

NS_METHOD nsScrollbar::SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                                PRUint32 aPosition, PRUint32 aLineIncrement)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::SetParameters()\n"));
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("MaxRange = %d ThumbSize = %d aPosition = %d LineIncrement = %d\n",
         aMaxRange, aThumbSize, aPosition, aLineIncrement));

  SetMaxRange(aMaxRange);
  SetThumbSize(aThumbSize);
  SetPosition(aPosition);
  SetLineIncrement(aLineIncrement);
  CalcBarBounds();
  LayoutBar();
  return NS_OK;
}

PRBool nsScrollbar::OnScroll(PRUint32 scrollCode, int cPos)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::OnScroll\n"));

  PRBool result = PR_FALSE;
  switch (scrollCode) {
  case NS_SCROLLBAR_PAGE_NEXT:
    result = NextPage();
    break;
  case NS_SCROLLBAR_PAGE_PREV:
    result = PrevPage();
    break;
  case NS_SCROLLBAR_POS:
      if (mIsVertical == PR_TRUE)
        result = SetPosition(cPos);
      else
    result = SetPosition(cPos);
    break;
  default:
    break;
  }
  return result;
}

PRBool nsScrollbar::OnResize(nsSizeEvent &event)
{
  PRBool result;

  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::OnResize\n"));

  nsWidget::OnResize(event);
  CalcBarBounds();
  LayoutBar();
  result = PR_FALSE;
  return result;
}

PRBool nsScrollbar::DispatchMouseEvent(nsMouseEvent &aEvent)
{
  PRInt32 real;

  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::DispatchMouseEvent\n"));

  /* that calculation is kind of screwed, but better than the original */
  switch (aEvent.message) {
  case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    if (mIsVertical == PR_TRUE) {
        real = (PRInt32)((float)(PR_MAX(0, aEvent.point.y - ((PRInt32)PR_MAX((mThumbSize * mYScale), NS_SCROLLBAR_MIN) / 2))) / mYScale);
        OnScroll(NS_SCROLLBAR_POS, real);
      } else {
        real = (PRInt32)((float)(PR_MAX(0, aEvent.point.x - ((PRInt32)PR_MAX((mThumbSize * mXScale), NS_SCROLLBAR_MIN) / 2))) / mXScale);
        OnScroll(NS_SCROLLBAR_POS, real);
    } 
    break;

  case NS_MOUSE_LEFT_BUTTON_DOWN:
    if (mIsVertical == PR_TRUE) {
        if (aEvent.point.y < mBarBounds.y)
        OnScroll(NS_SCROLLBAR_PAGE_PREV, 0);
        else if (aEvent.point.y > (mBarBounds.y + mBarBounds.height))
        OnScroll(NS_SCROLLBAR_PAGE_NEXT, 0);
      } else { /* !vertical */
        if (aEvent.point.x < mBarBounds.x)
        OnScroll(NS_SCROLLBAR_PAGE_PREV, 0);
        else if (aEvent.point.x > (mBarBounds.x + mBarBounds.width))
        OnScroll(NS_SCROLLBAR_PAGE_NEXT, 0);
      }
    break;
  default:
    break;
}
  return SendEvent(NS_SCROLLBAR_POS);
  }

NS_IMETHODIMP nsScrollbar::Show(PRBool bState)
{
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::Show(): %s\n",
                                         (bState == PR_TRUE) ? "true" : "false"));
  nsWidget::Show(bState);
  if (bState) {
    if (mBar) {
      XMapWindow(mDisplay, mBar);
    }
    CalcBarBounds();
    LayoutBar();
  }
  return NS_OK;
}

NS_IMETHODIMP nsScrollbar::Resize(PRInt32 aWidth,
                                  PRInt32 aHeight,
                                  PRBool   aRepaint)
{
  nsWidget::Resize(aWidth, aHeight, aRepaint);
  CalcBarBounds();
  LayoutBar();
  return NS_OK;
}

NS_IMETHODIMP nsScrollbar::Resize(PRInt32 aX,
                                  PRInt32 aY,
                                  PRInt32 aWidth,
                                  PRInt32 aHeight,
                                  PRBool   aRepaint)
{
  nsWidget::Resize(aX, aY, aWidth, aHeight, aRepaint);
  CalcBarBounds();
  LayoutBar();
  return NS_OK;
}

nsresult nsScrollbar::NextPage(void)
{
  PRUint32 max;
  nsresult result = PR_FALSE;

  // change it locally.
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::NextPage(): maxrange is %d thumb is %d position is %d\n", mMaxRange, mThumbSize, mPosition));
  max = mMaxRange - mThumbSize;
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::NextPage(): max is %d\n", max));
  mPosition += mThumbSize;
  if (mPosition > max)
    mPosition = max;
  PR_LOG(XlibScrollbarLM, PR_LOG_DEBUG, ("nsScrollbar::NextPage(): new position is %d\n", mPosition));

  result = SendEvent(NS_SCROLLBAR_POS);
  CalcBarBounds();
  LayoutBar();
  return result;
}

nsresult nsScrollbar::PrevPage(void)
{
  nsresult result = PR_FALSE;

  // check to make sure we don't go backwards
  if (mThumbSize > mPosition) {
    mPosition = 0;
  } else {
    mPosition -= mThumbSize;
  }
  result = SendEvent(NS_SCROLLBAR_POS);
  CalcBarBounds();
  LayoutBar();
  return result;
}

void nsScrollbar::CalcBarBounds(void)
{
  PRUint32 bar_start;
  PRUint32 bar_end;

  if (mMaxRange == 0) {
    bar_start = 0;
    bar_end = 0;
    mXScale = 0.0;
    mYScale = 0.0;
  } else {
    if (mIsVertical == PR_TRUE) {
      mYScale = (float)mRequestedSize.height / (float)mMaxRange;
      bar_start = (PRUint32)((float)mPosition * mYScale);
      bar_end = (PRUint32)((float)mThumbSize * mYScale);
      bar_start = CLAMP(bar_start, 0, (PRUint32)(mRequestedSize.height));
    } else {
      mXScale = (float)mRequestedSize.width / (float)mMaxRange;
      bar_start = (PRUint32)((float)mPosition * mXScale);
      bar_end = (PRUint32)((float)mThumbSize * mXScale);
      bar_start = CLAMP(bar_start, 0, (PRUint32)(mRequestedSize.width));
  }
  }
  bar_end = PR_MAX(bar_end, NS_SCROLLBAR_MIN);
  
  if (mIsVertical == PR_TRUE) {
    mBarBounds.x = 0;
    mBarBounds.y = bar_start;
    mBarBounds.width = mRequestedSize.width - 4;
    mBarBounds.height = bar_end;
  } else {
    mBarBounds.x = bar_start;
    mBarBounds.y = 0;
    mBarBounds.width = bar_end;
    mBarBounds.height = mRequestedSize.height - 4;
  }

  if (mBarBounds.height == 0)
    mBarBounds.height = 1;
  
  if (mBarBounds.width == 0)
    mBarBounds.width = 1;
}

void nsScrollbar::LayoutBar(void)
{
  /* Workaround for bug 77344. I am not sure whether this is mandatory or not. */
  if (mDisplay)
    XMoveResizeWindow(mDisplay, mBar,
                      mBarBounds.x, mBarBounds.y,
                      mBarBounds.width, mBarBounds.height);
}

NS_IMETHODIMP nsScrollbar::Move(PRInt32 aX, PRInt32 aY)
{
  return nsWidget::Move(aX, aY);
}

PRBool nsScrollbar::SendEvent(PRUint32 message)
{
  PRBool result = PR_FALSE;
  // send the event
  if (mEventCallback) {
    nsScrollbarEvent sevent;
    sevent.message = message;
    sevent.widget = (nsWidget *)this;
    sevent.eventStructType = NS_SCROLLBAR_EVENT;
    sevent.position = (mPosition);
    // send the event
    result = ConvertStatus((*mEventCallback) (&sevent));
    // the gtk code indicates that the callback can
    // modify the position.  how odd.
    mPosition = sevent.position;
  }
  return result;
}
