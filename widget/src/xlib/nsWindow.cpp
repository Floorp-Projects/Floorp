/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Peter Hartshorn <peter@igelaus.com.au>
 *   Ken Faulkner <faulkner@igelaus.com.au>
 *   B.J. Rossiter <bj@igelaus.com.au>
 *   Tony Tsui <tony@igelaus.com.au>
 */

#include "nsWindow.h"

#include "xlibrgb.h"

#include "nsFileSpec.h" // for nsAutoCString

#define ABS(i) ( i<0 ? 0-i : i )

// Variables for grabbing
PRBool   nsWindow::sIsGrabbing = PR_FALSE;
nsWindow *nsWindow::sGrabWindow = NULL;

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
    nsList *old_queue = update_queue;
    nsListItem *tmp_list = old_queue->getHead();

    update_queue = nsnull;
  
    if (tmp_list != nsnull) {
      while (tmp_list != nsnull)
      {
        nsWindow *window = (nsWindow *)tmp_list->getData();
        window->Update();
        window->mIsUpdating = PR_FALSE;

        tmp_list = tmp_list->getNext();
      }
    }
    delete old_queue;
  }
}

// Just raises the window.
// There should probably be checks on this.
// FIXME KenF
NS_IMETHODIMP nsWindow::GetAttention(void)
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
///////////////////////////////////////////////////////////////////

nsWindow::nsWindow() : nsWidget()
{
  NS_INIT_REFCNT();
  mName.AssignWithConversion("nsWindow");
  mBackground = NS_RGB(255, 255, 255);
  mBackgroundPixel = xlib_rgb_xpixel_from_rgb(mBackground);
  mBorderRGB = NS_RGB(255,255,255);
  mBorderPixel = xlib_rgb_xpixel_from_rgb(mBorderRGB);

  // FIXME KenF
  mIsUpdating = PR_FALSE;
  mBlockFocusEvents = PR_FALSE;

  // FIXME New on M17 merge.
  mWindowType = eWindowType_child;
  mBorderStyle = eBorderStyle_default;
  mIsToplevel = PR_FALSE;
}


nsWindow::~nsWindow()
{
  // Release grab
  if (sGrabWindow == this)
  {
    sIsGrabbing = PR_FALSE;
    sGrabWindow = NULL;
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

// Function that does native grab.
void nsWindow::NativeGrab(PRBool aGrab)
{
  Cursor newCursor = XCreateFontCursor(mDisplay, XC_right_ptr);

  if (aGrab)
  {
    int retval;

    retval = XGrabPointer(mDisplay, mBaseWindow, PR_TRUE, (ButtonPressMask |
                          ButtonReleaseMask | EnterWindowMask | LeaveWindowMask 
                          | PointerMotionMask), GrabModeAsync, GrabModeAsync, 
                          (Window*)NULL, newCursor, CurrentTime);

    if (retval != GrabSuccess)
      fprintf(stderr, "Grab pointer failed!\n");

    retval = XGrabKeyboard(mDisplay, mBaseWindow, PR_TRUE, GrabModeAsync,
                           GrabModeAsync, CurrentTime);

    if (retval != GrabSuccess)
      fprintf(stderr, "Grab keyboard failed!\n");

  }
  else
  {
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
  if (aDoCapture)
  {
    NativeGrab(PR_TRUE);

    sIsGrabbing = PR_TRUE;
    sGrabWindow = this;

      //fprintf(stderr, "Received listener from generated popup\n");
      gRollupListener = aListener;

      // Get assertion error:
      // ###!!! ASSERTION: Did you know you were calling |NS_GetWeakReference()| on
      // something that doesn't support weak references?: 'factoryPtr', file
      // nsWeakReference.cpp, line 55
      //gRollupWidget = getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIWidget*,this)));
      gRollupWidget = NS_STATIC_CAST(nsIWidget*,this);

      // GTK does not seem to use this but other window toolkits do
      gRollupConsumeRollupEvent = PR_TRUE;
    //}
  }else{

    // Release Grab
    if (sGrabWindow == this)
      sGrabWindow = NULL;

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


NS_IMETHODIMP nsWindow::SetFocus(void)
{
  nsEventStatus status;
  nsGUIEvent event; 
  //  nsGUIEvent eventActivate;

  if (mBaseWindow)
    mFocusWindow = mBaseWindow;
   
  if (mBlockFocusEvents)
    return NS_OK;
  
  mBlockFocusEvents = PR_TRUE;
 
  event.message = NS_GOTFOCUS;
  event.widget  = this;
  event.eventStructType = NS_GUI_EVENT;
  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;
  
  AddRef();
  DispatchEvent(&event, status);
  Release();
  
  

  event.message = NS_ACTIVATE;
  
  event.widget  = this;
  event.eventStructType = NS_GUI_EVENT;
  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;
  
  AddRef();
  DispatchWindowEvent(event);
  Release();
  
  mBlockFocusEvents = PR_FALSE;
  
  return NS_OK;
}


// Borrowed heavily from GTK. This should go through heirarchy of XWindow
// windows, and destroy the appropriate children. 
// KenF
void
nsWindow::DestroyNativeChildren(void)
{
  Display     *display;
  Window       window;
  Window       root_return;
  Window       parent_return;
  Window      *children_return = NULL;
  unsigned int nchildren_return = 0;
  unsigned int i = 0;
  
  display = mDisplay;
  window = mBaseWindow;
  XQueryTree(display, window, &root_return, &parent_return,
             &children_return, &nchildren_return);
  // walk the list of children
  for (i=0; i < nchildren_return; i++)
    {
      Window child_window = children_return[i];
      nsWindow *thisWindow = (nsWindow*) GetWidgetForWindow(child_window);
      if (thisWindow)
        {
          thisWindow->Destroy();
        }
    }      

  // free up this struct
  if (children_return)
    XFree(children_return);
}

void nsWindow::DestroyNative(void)
{

  // Destroy Children. DOH!!!! KenF
  DestroyNativeChildren();

  // This is handled in nsDrawingSurfaceXlib for now
#if 0
  if (mGC)
    XFreeGC(mDisplay, mGC);
#endif

  if (mBaseWindow) {
      XDestroyWindow(mDisplay, mBaseWindow);
      DeleteWindowCallback(mBaseWindow);
  }
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
  nsWidget::Resize(aWidth, aHeight, aRepaint);

  nsSizeEvent sevent;
  sevent.message = NS_SIZE;
  sevent.widget = this;
  sevent.eventStructType = NS_SIZE_EVENT;
  sevent.windowSize = new nsRect (0, 0, aWidth, aHeight);
  sevent.point.x = 0;
  sevent.point.y = 0;
  sevent.mWinWidth = aWidth;
  sevent.mWinHeight = aHeight;
  sevent.time = 0;
  AddRef();
  OnResize(sevent);
  Release();
  delete sevent.windowSize;
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

  nsWidget::Resize(aX, aY, aWidth, aHeight, aRepaint);

  nsSizeEvent sevent;
  sevent.message = NS_SIZE;
  sevent.widget = this;
  sevent.eventStructType = NS_SIZE_EVENT;
  sevent.windowSize = new nsRect (0, 0, aWidth, aHeight);
  sevent.point.x = 0;
  sevent.point.y = 0;
  sevent.mWinWidth = aWidth;
  sevent.mWinHeight = aHeight;
  sevent.time = 0;
  AddRef();
  OnResize(sevent);
  Release();
  delete sevent.windowSize;
  return NS_OK;
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
  attr.event_mask = StructureNotifyMask |
    ExposureMask |
    ButtonPressMask |
    ButtonReleaseMask |
    PointerMotionMask |
    KeyPressMask |
    KeyReleaseMask |
    VisibilityChangeMask;
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

NS_IMETHODIMP nsWindow::Update()
{
  if (mIsUpdating)
    UnqueueDraw();

  nsPaintEvent pevent;
  pevent.message = NS_PAINT;
  pevent.eventStructType = NS_PAINT_EVENT;
  pevent.rect = new nsRect (0,0,0,0);
  pevent.time = 0;
  pevent.region = mUpdateArea;

  if (!mUpdateArea->IsEmpty()) {
    PRUint32 numRects;
    mUpdateArea->GetNumRects(&numRects);

    if (numRects != 1 && numRects < 10) {
      nsRegionRectSet *regionRectSet = nsnull;

      if (NS_FAILED(mUpdateArea->GetRects(&regionRectSet)))
        return NS_ERROR_FAILURE;

      PRUint32 len;
      PRUint32 i;

      len = regionRectSet->mRectsLen;

      for (i=0; i<len; ++i) {
        nsRegionRect *r = &(regionRectSet->mRects[i]);

        pevent.widget = this;
        pevent.rect->SetRect(r->x, r->y, r->width, r->height);

        pevent.time = PR_Now();    
        AddRef();
        OnPaint(pevent);
        Release();
      }
      mUpdateArea->FreeRects(regionRectSet);
      mUpdateArea->SetTo(0,0,0,0);
      return NS_OK;
    } else {
      PRInt32 x,y,w,h;
      mUpdateArea->GetBoundingBox(&x,&y,&w,&h);
      pevent.widget = this;
      pevent.rect->SetRect(x,y,w,h);
      AddRef();
      OnPaint(pevent);
      Release();
      mUpdateArea->SetTo(0,0,0,0);
    }
  }

  delete pevent.rect;

  // The view manager also expects us to force our
  // children to update too!

  nsCOMPtr<nsIEnumerator> children ( getter_AddRefs(GetChildren()) );
  if (children)
  {
    children->First();
    do
    {
      nsISupports* child;
      if (NS_SUCCEEDED(children->CurrentItem(&child)))
      {
        nsWindow* childWindow = NS_STATIC_CAST(nsWindow*,child);
        NS_RELEASE(child);

        childWindow->Update();
      }
    } while (NS_SUCCEEDED(children->Next()));
  }
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  PR_LOG(XlibScrollingLM, PR_LOG_DEBUG, ("nsWindow::Scroll()\n"));
  
  if (mIsUpdating)
    UnqueueDraw();

  PRInt32 srcX, srcY, destX, destY, width, height;
  nsRect aRect;
  GC gc;

  gc = XCreateGC(mDisplay, mBaseWindow, 0, NULL);

  if (aDx < 0 || aDy < 0)
  {
    srcX = mBounds.x + ABS(aDx);
    srcY = mBounds.y + ABS(aDy);
    destX = mBounds.x;
    destY = mBounds.y;
    width = mBounds.width - ABS(aDx);
    height = mBounds.height - ABS(aDy);
  } else
  if (aDx > 0 || aDy > 0)
  {
    srcX = mBounds.x;
    srcY = mBounds.y;
    destX = mBounds.x + ABS(aDx);
    destY = mBounds.y + ABS(aDy);
    width = mBounds.width - ABS(aDx);
    height = mBounds.height - ABS(aDy);
  }

  XCopyArea(mDisplay, mBaseWindow, mBaseWindow, gc,
            srcX, srcY, width, height, destX, destY);

  XFreeGC(mDisplay, gc);

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
  nsCOMPtr<nsIEnumerator> children ( getter_AddRefs(GetChildren()) );
  if (children)
    {
      children->First();
      do
        {
          nsISupports* child;
          if (NS_SUCCEEDED(children->CurrentItem(&child)))
            {
              nsWindow* childWindow = NS_STATIC_CAST(nsWindow*,child);
              NS_RELEASE(child);

              nsRect bounds;
              childWindow->GetRequestedBounds(bounds);
              childWindow->Move(bounds.x + aDx, bounds.y + aDy);
              Invalidate(bounds, PR_TRUE);
            }
        } while (NS_SUCCEEDED(children->Next()));
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
    if (needToUpdate == PR_TRUE)
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

NS_IMETHODIMP nsWindow::SetTitle(const nsString& aTitle)
{
  if(!mBaseWindow)
    return NS_ERROR_FAILURE;

  XStoreName(mDisplay, mBaseWindow, (const char *) nsAutoCString(aTitle));

  return NS_OK;
}

ChildWindow::ChildWindow(): nsWindow()
{
  mName.AssignWithConversion("nsChildWindow");
}
