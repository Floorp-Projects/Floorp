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
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   B.J. Rossiter <bj@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
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

#include "nsWindow.h"
#include "xlibrgb.h"

#include "nsIRenderingContext.h"

/* for window title unicode->locale conversion */
#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"
#include "nsIServiceManager.h"

// Variables for grabbing
PRBool   nsWindow::sIsGrabbing = PR_FALSE;
nsWindow *nsWindow::sGrabWindow = nsnull;

// Routines implementing an update queue.
// We keep a single queue for all widgets because it is 
// (most likely) more efficient and better looking to handle
// all the updates in one shot. Actually, this queue should
// be at most per-toplevel. FIXME.
//

nsListItem::nsListItem(void *aData, nsListItem *aPrev)
{
  next = nsnull;
  prev = aPrev;
  data = aData;
}

nsList::nsList()
{
  head = nsnull;
  tail = nsnull;
}

nsList::~nsList()
{
  reset();
}

void nsList::add(void *aData)
{
  if (head == nsnull) {           // We have an empty list, create the head
    head = new nsListItem(aData, nsnull);
    tail = head;
  } else {                        // Append to the end of the list
    tail->setNext(new nsListItem(aData, tail));
    tail = tail->getNext();       // Reset the tail
    tail->setNext(nsnull);
  }
}

void nsList::remove(void *aData)
{
  if (head == nsnull) {           // Removing from a null list
    return;
  } else {                        // find the data
    nsListItem *aItem = head;
    while ((aItem != nsnull) && (aItem->getData() != aData)) {
      aItem = aItem->getNext();
    }
    if (aItem == nsnull) {        // we didn't find it
      return;
    } else
    if (aItem == head) {          // we have to remove the head
      head = aItem->getNext();
      delete aItem;
      if (head == nsnull)         // we have emptied the list
        tail = nsnull;
      else
        head->setPrev(nsnull);
    } else
    if (aItem == tail) {          // we have to remove the tail
      tail = aItem->getPrev();
      delete aItem;
      if (tail == nsnull)         // we have emptied the list
        head = nsnull;
      else
        tail->setNext(nsnull);
    } else {                      // we remove from the middle
      nsListItem *prev = aItem->getPrev();
      nsListItem *next = aItem->getNext();
      delete aItem;
      prev->setNext(next);
      next->setPrev(prev);
    }
  }
}

void nsList::reset()
{
  while (head != nsnull) {
    void *aData = head->getData();
    remove(aData);
  }
}

static nsList *update_queue = nsnull;

void 
nsWindow::UpdateIdle (void *data)
{
  if (update_queue != nsnull) {
    nsList     *old_queue;
    nsListItem *tmp_list;

    old_queue    = update_queue;
    update_queue = nsnull;
    
    for( tmp_list = old_queue->getHead() ; tmp_list != nsnull ; tmp_list = tmp_list->getNext() )
    {
      nsWindow *window = NS_STATIC_CAST(nsWindow*,(tmp_list->getData()));       

      window->mIsUpdating = PR_FALSE;
    }
    
    for( tmp_list = old_queue->getHead() ; tmp_list != nsnull ; tmp_list = tmp_list->getNext() )
    {
      nsWindow *window = NS_STATIC_CAST(nsWindow*,(tmp_list->getData()));
       
      window->Update();
    }    

    delete old_queue;
  }
}

// Just raises the window.
// There should probably be checks on this.
// FIXME KenF
NS_IMETHODIMP nsWindow::GetAttention(PRInt32 aCycleCount)
{
  XRaiseWindow(mDisplay, mBaseWindow);
  return NS_OK;
}

void
nsWindow::QueueDraw ()
{
  if (!mIsUpdating)
  {
    if (update_queue == nsnull)
      update_queue = new nsList();
    update_queue->add((void *)this);
    mIsUpdating = PR_TRUE;
  }
}

void
nsWindow::UnqueueDraw ()
{
  if (mIsUpdating)
  {
    if (update_queue != nsnull)
      update_queue->remove((void *)this);
    mIsUpdating = PR_FALSE;
  }
}

///////////////////////////////////////////////////////////////////
NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsWidget)
///////////////////////////////////////////////////////////////////

nsWindow::nsWindow() : nsWidget()
{
  mName.Assign(NS_LITERAL_STRING("nsWindow"));
  mBackground = NS_RGB(255, 255, 255);
  mBackgroundPixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, mBackground);
  mBorderRGB = NS_RGB(255,255,255);
  mBorderPixel = xxlib_rgb_xpixel_from_rgb(mXlibRgbHandle, mBorderRGB);

  // FIXME KenF
  mIsUpdating = PR_FALSE;
  mBlockFocusEvents = PR_FALSE;
  mLastGrabFailed = PR_TRUE;
  mIsTooSmall = PR_FALSE;

  // FIXME New on M17 merge.
  mWindowType = eWindowType_child;
  mBorderStyle = eBorderStyle_default;
  mIsToplevel = PR_FALSE;
  
  mScrollGC = nsnull;
}


nsWindow::~nsWindow()
{
  if (mScrollGC)
    XFreeGC(mDisplay, mScrollGC);
  
  // Release grab
  if (sGrabWindow == this)
  {
    sIsGrabbing = PR_FALSE;
    sGrabWindow = nsnull;
  }
 
  // Should get called from ~nsWidget() anyway. KenF
  // if (mBaseWindow)
  //Destroy();
  if (mIsUpdating)
    UnqueueDraw();
}

PRBool nsWindow::OnExpose(nsPaintEvent &event)
{
  nsresult result = PR_TRUE;

  // call the event callback
  if (mEventCallback) 
  {
    event.renderingContext = nsnull;

    //    printf("nsWindow::OnExpose\n");

    // expose.. we didn't get an Invalidate, so we should up the count here
    //    mBounds.UnionRect(mBounds, event.rect);

    //    SendExposeEvent();
  }

  return result;
}

NS_IMETHODIMP nsWindow::Show(PRBool bState)
{
  // don't show if we are too small
  if (mIsTooSmall)
    return NS_OK;

  if (bState) {
    if (mIsToplevel) {
      PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Someone just used the show method on the toplevel window.\n"));
    }

    if (mParentWidget) {
      ((nsWidget *)mParentWidget)->WidgetShow(this);
      // Fix Popups appearing behind mozilla window. TonyT
      XRaiseWindow(mDisplay, mBaseWindow);
    } else {
      if (mBaseWindow) {
        PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Mapping window 0x%lx...\n", mBaseWindow));
        Map();
      }
    }

    mIsShown = PR_TRUE;
    if (sGrabWindow == this && mLastGrabFailed) {
      /* re-grab things like popup menus - the window isn't mapped when
       * the first grab occurs */
      NativeGrab(PR_TRUE);
    }
  } else {
    if (mBaseWindow) {
      PR_LOG(XlibWidgetsLM, PR_LOG_DEBUG, ("Unmapping window 0x%lx...\n", mBaseWindow));
      Unmap();
    }
    mIsShown = PR_FALSE;
  }
  return NS_OK;
}

// Function that does native grab.
void nsWindow::NativeGrab(PRBool aGrab)
{
  mLastGrabFailed = PR_FALSE;

  if (aGrab)
  {
    int retval;

    Cursor newCursor = XCreateFontCursor(mDisplay, XC_right_ptr);
    retval = XGrabPointer(mDisplay, mBaseWindow, PR_TRUE, (ButtonPressMask |
                          ButtonReleaseMask | EnterWindowMask | LeaveWindowMask 
                          | PointerMotionMask), GrabModeAsync, GrabModeAsync, 
                          (Window)0, newCursor, CurrentTime);
    XFreeCursor(mDisplay, newCursor);

    if (retval != GrabSuccess)
      mLastGrabFailed = PR_TRUE;

    retval = XGrabKeyboard(mDisplay, mBaseWindow, PR_TRUE, GrabModeAsync,
                           GrabModeAsync, CurrentTime);

    if (retval != GrabSuccess)
      mLastGrabFailed = PR_TRUE;

  } else {
    XUngrabPointer(mDisplay, CurrentTime);
    XUngrabKeyboard(mDisplay, CurrentTime);
  }
}

/* This called when a popup is generated so that if a mouse down event occurs,
 * the passed listener can be informed (see nsWidget::HandlePopup). It is also
 * called with aDoCapture == PR_FALSE when a popup is no longer visible
 */
NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener,
                                            PRBool aDoCapture,
                                            PRBool aConsumeRollupEvent)
  {
  if (aDoCapture) {
    NativeGrab(PR_TRUE);

    sIsGrabbing = PR_TRUE;
    sGrabWindow = this;

    gRollupConsumeRollupEvent = PR_TRUE;
    gRollupListener = aListener;
    gRollupWidget = do_GetWeakReference(NS_STATIC_CAST(nsIWidget*, this));
  }else{
    // Release Grab
    if (sGrabWindow == this)
      sGrabWindow = nsnull;

    sIsGrabbing = PR_FALSE;

    NativeGrab(PR_FALSE);

    gRollupListener = nsnull;
    gRollupWidget = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP nsWindow::InvalidateRegion(const nsIRegion* aRegion, PRBool aIsSynchronous)
{
  mUpdateArea->Union(*aRegion);

  if (aIsSynchronous)
    Update();
  else
    QueueDraw();
  
  return NS_OK;
}


NS_IMETHODIMP nsWindow::SetFocus(PRBool aRaise)
{
  nsEventStatus status;
  nsFocusEvent event(NS_GOTFOCUS, this);
  //  nsGUIEvent eventActivate;

  if (mBaseWindow)
    mFocusWindow = mBaseWindow;
   
  if (mBlockFocusEvents)
    return NS_OK;
   
  mBlockFocusEvents = PR_TRUE;
 
  AddRef();
  DispatchEvent(&event, status);
  Release();
  
  nsGUIEvent actEvent(NS_ACTIVATE, this);
  
  AddRef();
  DispatchWindowEvent(actEvent);
  Release();
  
  mBlockFocusEvents = PR_FALSE;
  
  return NS_OK;
}

/* NOTE: Originally, nsWindow just uses Resize from nsWidget. 
 * Changed so it will first use the nsWidget resizing routine, then
 * send out a NS_SIZE event. This makes sure that the resizing is known
 * by all parts that NEED to know about it.
 * event bit is common for both resizes, yes, could make it a function,
 * but haven't yet....
 */
NS_IMETHODIMP nsWindow::Resize(PRInt32 aWidth,
                               PRInt32 aHeight,
                               PRBool   aRepaint)
{
  //printf("nsWindow::Resize aWidth=%i aHeight=%i\n", aWidth,aHeight);
  PRBool NeedToShow = PR_FALSE;
  /* PRInt32 sizeHeight = aHeight;
  PRInt32 sizeWidth = aWidth; */

  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  // code to keep the window from showing before it has been moved or resized

  // if we are resized to 1x1 or less, we will hide the window.  Show(TRUE) will be ignored until a
  // larger resize has happened
  if (aWidth <= 1 || aHeight <= 1)
  {
    aWidth = 1;
    aHeight = 1;
    mIsTooSmall = PR_TRUE;
    Show(PR_FALSE);
  }
  else
  {
    if (mIsTooSmall)
    {
      // if we are not shown, we don't want to force a show here, so check and see if Show(TRUE) has been called
      NeedToShow = mIsShown;
      mIsTooSmall = PR_FALSE;
    }
  }
  nsWidget::Resize(aWidth, aHeight, aRepaint);

  nsSizeEvent sevent(NS_SIZE, this);
  nsRect sevent_windowSize(0, 0, aWidth, aHeight);
  sevent.windowSize = &sevent_windowSize;
  sevent.mWinWidth = aWidth;
  sevent.mWinHeight = aHeight;
  AddRef();
  OnResize(sevent);
  Release();

  if (NeedToShow)
    Show(PR_TRUE);

  if (aRepaint)
    Invalidate(PR_FALSE);

  return NS_OK;
}

/* NOTE: Originally, nsWindow just uses Resize from nsWidget. 
 * Changed so it will first use the nsWidget resizing routine, then
 * send out a NS_SIZE event. This makes sure that the resizing is known
 * by all parts that NEED to know about it.
 */
NS_IMETHODIMP nsWindow::Resize(PRInt32 aX,
                               PRInt32 aY,
                               PRInt32 aWidth,
                               PRInt32 aHeight,
                               PRBool   aRepaint)
{
  //printf("nsWindow::Resize aX=%i, aY=%i, aWidth=%i, aHeight=%i\n", aX,aY,aWidth,aHeight);

  nsWidget::Resize(aX, aY, aWidth, aHeight, aRepaint);

  nsSizeEvent sevent(NS_SIZE, this);
  nsRect sevent_windowSize(0, 0, aWidth, aHeight);
  sevent.windowSize = &sevent_windowSize;
  sevent.mWinWidth = aWidth;
  sevent.mWinHeight = aHeight;
  AddRef();
  OnResize(sevent);
  Release();
  return NS_OK;
}

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

#undef TRACE_INVALIDATE

#ifdef TRACE_INVALIDATE
static PRInt32 sInvalidatePrintCount = 0;
#endif

NS_IMETHODIMP nsWindow::Invalidate(PRBool aIsSynchronous)
{
  mUpdateArea->SetTo(mBounds.x, mBounds.y, mBounds.width, mBounds.height);

  if (aIsSynchronous)
    Update();
  else
    QueueDraw();

  return NS_OK;
}

NS_IMETHODIMP nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  mUpdateArea->Union(aRect.x, aRect.y, aRect.width, aRect.height);
  if (aIsSynchronous)
    Update();
  else
    QueueDraw();
  
  return NS_OK;
}

void 
nsWindow::DoPaint (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                   nsIRegion *aClipRegion)
{
  if (mEventCallback) {
    nsPaintEvent event(NS_PAINT, this);
    nsRect rect(aX, aY, aWidth, aHeight);
    event.point.x = aX;
    event.point.y = aY; 
    event.time = PR_Now(); /* No time in EXPOSE events */
    event.rect = &rect;
    
    event.renderingContext = GetRenderingContext();
    if (event.renderingContext) {
      DispatchWindowEvent(event);
      NS_RELEASE(event.renderingContext);
    }
  }
}

NS_IMETHODIMP nsWindow::Update(void)
{
  if (mIsUpdating)
    UnqueueDraw();

  if (!mUpdateArea->IsEmpty()) {
    PRUint32 numRects;
    mUpdateArea->GetNumRects(&numRects);

    /* We paint the rects by themselves if we have 2 to 15 rects,
     * otherwise we will just paint the bounding box. */
    if (numRects != 1 && numRects < 16) {
      nsRegionRectSet *regionRectSet = nsnull;

      if (NS_FAILED(mUpdateArea->GetRects(&regionRectSet)))
        return NS_ERROR_FAILURE;

      PRUint32 len;
      PRUint32 i;
      
      len = regionRectSet->mRectsLen;

      for (i=0;i<len;++i) {
        nsRegionRect *r = &(regionRectSet->mRects[i]);
        DoPaint (r->x, r->y, r->width, r->height, mUpdateArea);
      }
      
      mUpdateArea->FreeRects(regionRectSet);
      
      mUpdateArea->SetTo(0, 0, 0, 0);
      return NS_OK;
    } else {
      PRInt32 x, y, w, h;
      mUpdateArea->GetBoundingBox(&x, &y, &w, &h);
      DoPaint (x, y, w, h, mUpdateArea);
      mUpdateArea->SetTo(0, 0, 0, 0);
    }
  } 

  // The view manager also expects us to force our
  // children to update too!

  for (nsIWidget* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
    kid->Update();
  }

  // While I'd think you should NS_RELEASE(aPaintEvent.widget) here,
  // if you do, it is a NULL pointer.  Not sure where it is getting
  // released.
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  PR_LOG(XlibScrollingLM, PR_LOG_DEBUG, ("nsWindow::Scroll()\n"));
  
  if (mIsUpdating)
    UnqueueDraw();

  PRInt32 srcX = 0, srcY = 0, destX = 0, destY = 0, width = 0, height = 0;
  nsRect aRect;
 
  /* only create GC for scolling once ... */
  if (!mScrollGC)
    mScrollGC = XCreateGC(mDisplay, mBaseWindow, 0, nsnull);   
 
  if (aDx < 0 || aDy < 0)
  {
    srcX   = mBounds.x + PR_ABS(aDx);
    srcY   = mBounds.y + PR_ABS(aDy);
    destX  = mBounds.x;
    destY  = mBounds.y;
    width  = mBounds.width  - PR_ABS(aDx);
    height = mBounds.height - PR_ABS(aDy);
  } 
  else if (aDx > 0 || aDy > 0)
  {
    srcX   = mBounds.x;
    srcY   = mBounds.y;
    destX  = mBounds.x + PR_ABS(aDx);
    destY  = mBounds.y + PR_ABS(aDy);
    width  = mBounds.width  - PR_ABS(aDx);
    height = mBounds.height - PR_ABS(aDy);
  }

  XCopyArea(mDisplay, mBaseWindow, mBaseWindow, mScrollGC,
            srcX, srcY, width, height, destX, destY);

  width = mBounds.width;
  height = mBounds.height;

  if (aDx != 0 || aDy != 0) {
    if (aDx < 0) {
      aRect.SetRect(width + aDx, 0,
                    -aDx, height);
    }
    else if (aDx > 0) {
      aRect.SetRect(0,0, aDx, height);
    }

    if (aDy < 0) {
      aRect.SetRect(0, height + aDy,
                    width, -aDy);
    }
    else if (aDy > 0) {
      aRect.SetRect(0,0, width, aDy);
    }

    mUpdateArea->Offset(aDx, aDy);
    Invalidate(aRect, PR_TRUE);
  }

  //--------
  // Scroll the children
  for (nsIWidget* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
    nsWindow* childWindow = NS_STATIC_CAST(nsWindow*, kid);
    nsRect bounds;
    childWindow->GetRequestedBounds(bounds);
    childWindow->Move(bounds.x + aDx, bounds.y + aDy);
    Invalidate(bounds, PR_TRUE);
  }

  // If we are obscurred by another window we have to update those areas
  // which were not copied with the XCopyArea function.

  if (mVisibility == VisibilityPartiallyObscured)
  {
    XEvent event;
    PRBool needToUpdate = PR_FALSE;
    mUpdateArea->SetTo(0,0,0,0);
    while(XCheckWindowEvent(mDisplay, mBaseWindow, 0xffffffff, &event))
    {
      if (event.type == GraphicsExpose) {
        nsRect rect;
        rect.SetRect(event.xgraphicsexpose.x,
                     event.xgraphicsexpose.y,
                     event.xgraphicsexpose.width,
                     event.xgraphicsexpose.height);
        mUpdateArea->Union(rect.x, rect.y, rect.width, rect.height);
        needToUpdate = PR_TRUE;
        if (event.xgraphicsexpose.count == 0) {
          continue;
        }
      }
    }
    if (needToUpdate)
      Update();
  }
  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollWidgets(PRInt32 aDx, PRInt32 aDy)
{
  return Scroll(aDx, aDy, nsnull);
}

NS_IMETHODIMP nsWindow::ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy)
{
  return Scroll(aDx, aDy, nsnull);
}

NS_IMETHODIMP nsWindow::SetTitle(const nsAString& aTitle)
{
  if(!mBaseWindow)
    return NS_ERROR_FAILURE;

  nsresult rv;
  char *platformText;
  PRInt32 platformLen;

  nsCOMPtr<nsIUnicodeEncoder> encoder;
  /* get the charset */
  nsCAutoString platformCharset;
  nsCOMPtr <nsIPlatformCharset> platformCharsetService = do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = platformCharsetService->GetCharset(kPlatformCharsetSel_Menu, platformCharset);
  
  if (NS_FAILED(rv))
    platformCharset.Assign(NS_LITERAL_CSTRING("ISO-8859-1"));

  /* get the encoder */
  nsCOMPtr<nsICharsetConverterManager> ccm = 
           do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);  
  rv = ccm->GetUnicodeEncoderRaw(platformCharset.get(), getter_AddRefs(encoder));
  NS_ASSERTION(NS_SUCCEEDED(rv), "GetUnicodeEncoderRaw failed.");
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  /* Estimate out length and allocate the buffer based on a worst-case estimate, then do
     the conversion. */
  nsAString::const_iterator begin;
  const PRUnichar *title = aTitle.BeginReading(begin).get();
  PRInt32 len = (PRInt32)aTitle.Length();
  encoder->GetMaxLength(title, len, &platformLen);
  if (platformLen) {
    platformText = NS_REINTERPRET_CAST(char*, nsMemory::Alloc(platformLen + sizeof(char)));
    if (platformText) {
      rv = encoder->Convert(title, &len, platformText, &platformLen);
      (platformText)[platformLen] = '\0';  // null terminate. Convert() doesn't do it for us
    }
  } // if valid length

  if (platformLen > 0) {
    int status = 0;
    XTextProperty prop;

    /* Use XStdICCTextStyle for 41786(a.k.a TWM sucks) and 43108(JA text title) */
    prop.value = 0;
    status = XmbTextListToTextProperty(mDisplay, &platformText, 1,
        XStdICCTextStyle, &prop);

    if (status == Success) {
      XSetWMProperties(mDisplay, mBaseWindow,
                       &prop, &prop, nsnull, 0, nsnull, nsnull, nsnull);
      if (prop.value)
        XFree(prop.value);

      nsMemory::Free(platformText);
      return NS_OK;
    } else {                    // status != Success
      if (prop.value)
        XFree(prop.value);
      nsMemory::Free(platformText);
    }
  }

  /* if the stuff above failed, replace multibyte with .... */
  XStoreName(mDisplay, mBaseWindow, NS_LossyConvertUCS2toASCII(aTitle).get());

  return NS_OK;
}

ChildWindow::ChildWindow(): nsWindow()
{
  mName.Assign(NS_LITERAL_STRING("nsChildWindow"));
}


