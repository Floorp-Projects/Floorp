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
 *'
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include <stdio.h>

#include <gtk/gtk.h>

#include <gdk/gdkx.h>
#include <gtk/gtkprivate.h>
// XXX FTSO nsWebShell
#include <gdk/gdkprivate.h>

#include <X11/Xatom.h>   // For XA_STRING

#include "nsWindow.h"
#include "nsWidgetsCID.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"
#include "nsGtkEventHandler.h"
#include "nsIAppShell.h"
#include "nsClipboard.h"
#include "nsIRollupListener.h"

#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"
#include "nsIServiceManager.h"

#include "nsGtkUtils.h" // for nsGtkUtils::gdk_window_flash()

#include "nsSpecialSystemDirectory.h"
#include "nsGtkMozRemoteHelper.h"

#include <unistd.h>

#ifdef NEED_USLEEP_PROTOTYPE
extern "C" int usleep(unsigned int);
#endif
#if defined(__QNX__)
#define usleep(s)	sleep(s)
#endif

#undef DEBUG_DND_XLATE
#undef DEBUG_FOCUS
#undef DEBUG_GRAB
#define MODAL_TIMERS_BROKEN

#define CAPS_LOCK_IS_ON \
(nsWidget::sDebugFeedback && (nsGtkUtils::gdk_keyboard_get_modifiers() & GDK_LOCK_MASK))

#define WANT_PAINT_FLASHING \
(CAPS_LOCK_IS_ON && debug_WantPaintFlashing())

#define kWindowPositionSlop 10

gint handle_toplevel_focus_in (
    GtkWidget *      aWidget, 
    GdkEventFocus *  aGdkFocusEvent, 
    gpointer         aData);

gint handle_toplevel_focus_out (
    GtkWidget *      aWidget, 
    GdkEventFocus *  aGdkFocusEvent, 
    gpointer         aData);

gint handle_mozarea_focus_in (
    GtkWidget *      aWidget, 
    GdkEventFocus *  aGdkFocusEvent, 
    gpointer         aData);
    
gint handle_mozarea_focus_out (
    GtkWidget *      aWidget, 
    GdkEventFocus *  aGdkFocusEvent, 
    gpointer         aData);

gboolean handle_toplevel_property_change (
    GtkWidget *widget,
    GdkEventProperty *event,
    gpointer aData);

// are we grabbing?
PRBool      nsWindow::sIsGrabbing = PR_FALSE;
nsWindow   *nsWindow::sGrabWindow = NULL;

// this is a hash table that contains a list of the
// shell_window -> nsWindow * lookups
GHashTable *nsWindow::mWindowLookupTable = NULL;

// this is the last window that had a drag event happen on it.
nsWindow *nsWindow::mLastDragMotionWindow = NULL;
// we get our drop after the leave.
nsWindow *nsWindow::mLastLeaveWindow = NULL;

PRBool gJustGotActivate = PR_FALSE;
PRBool gJustGotDeactivate = PR_TRUE;

static void printDepth(int depth) {
  int i;
  for (i=0; i < depth; i++)
  {
    g_print(" ");
  }
}

NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, nsWidget)

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
  mShell = nsnull;
  mResized = PR_FALSE;
  mVisible = PR_FALSE;
  mDisplayed = PR_FALSE;
  mLowerLeft = PR_FALSE;
  mWindowType = eWindowType_child;
  mBorderStyle = eBorderStyle_default;
  mSuperWin = 0;
  mMozArea = 0;
  mMozAreaClosestParent = 0;
  mIsTooSmall = PR_FALSE;
  mIsUpdating = PR_FALSE;
  // init the hash table if it hasn't happened already
  if (mWindowLookupTable == NULL) {
    mWindowLookupTable = g_hash_table_new(g_direct_hash, g_direct_equal);
  }
  if (mLastLeaveWindow == this)
    mLastLeaveWindow = NULL;
  if (mLastDragMotionWindow == this)
    mLastDragMotionWindow = NULL;
  mBlockMozAreaFocusIn = PR_FALSE;
  mLastGrabFailed = PR_TRUE;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  //  printf("%p nsWindow::~nsWindow\n", this);
  // make sure that we release the grab indicator here
  if (sGrabWindow == this) {
    sIsGrabbing = PR_FALSE;
    sGrabWindow = NULL;
  }
  // make sure that we unset the lastDragMotionWindow if
  // we are it.
  if (mLastDragMotionWindow == this) {
    mLastDragMotionWindow = NULL;
  }
  // make sure to release our focus window
  if (mHasFocus == PR_TRUE) {
    sFocusWindow = NULL;
  }

  // always call destroy.  if it's already been called, there's no harm
  // since it keeps track of when it's already been called.

  Destroy();

  if (mIsUpdating)
    UnqueueDraw();
}

NS_IMETHODIMP nsWindow::Destroy(void)
{
  // remove our pointer from the object so that we event handlers don't send us events
  // after we are gone or in the process of going away

  if (mSuperWin)
    gtk_object_remove_data(GTK_OBJECT(mSuperWin), "nsWindow");
  if (mShell)
    gtk_object_remove_data(GTK_OBJECT(mShell), "nsWindow");
  if (mMozArea)
    gtk_object_remove_data(GTK_OBJECT(mMozArea), "nsWindow");

  return nsWidget::Destroy();
}


PRBool nsWindow::IsChild() const
{
  return PR_FALSE;
}

NS_IMETHODIMP nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  gint x;
  gint y;

  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  if (mMozArea)
  {
    if (mMozArea->window)
    {
      gdk_window_get_origin(mMozArea->window, &x, &y);
      aNewRect.x = x + aOldRect.x;
      aNewRect.y = y + aOldRect.y;
    }
    else
      return NS_ERROR_FAILURE;
  }
  else if (mSuperWin)
  {
    if (mSuperWin->bin_window)
    {
      gdk_window_get_origin(mSuperWin->bin_window, &x, &y);
      aNewRect.x = x + aOldRect.x;
      aNewRect.y = y + aOldRect.y;
    }

    else
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

// this is the function that will destroy the native windows for this widget.
 
/* virtual */
void
nsWindow::DestroyNative(void)
{
#ifdef USE_XIM
  IMEDestroyIC();
#endif // USE_XIM 

  // destroy all of the children that are nsWindow() classes
  // preempting the gdk destroy system.
  DestroyNativeChildren();

  if (mSuperWin) {
    // remove the key from the hash table for the shell_window
    g_hash_table_remove(mWindowLookupTable, mSuperWin->shell_window);
  }

  if (mShell) {
    gtk_widget_destroy(mShell);
    mShell = nsnull;
    // the moz area and superwin will have been destroyed when we destroyed the shell
    mMozArea = nsnull;
    mSuperWin = nsnull;
  }
  else if(mMozArea) {
    // We will get here if the widget was created as the child of a
    // GtkContainer.
    gtk_widget_destroy(mMozArea);
    mMozArea = nsnull;
    mSuperWin = nsnull;
  }
  else if(mSuperWin) {
    gtk_object_unref(GTK_OBJECT(mSuperWin));
    mSuperWin = NULL;
  }
}

// this function will walk the list of children and destroy them.
// the reason why this is here is that because of the superwin code
// it's very likely that we will never get notification of the
// the destruction of the child windows.  so, we have to beat the
// layout engine to the punch.  CB 

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

  if (mSuperWin)
  {
    display = GDK_DISPLAY();
    window = GDK_WINDOW_XWINDOW(mSuperWin->bin_window);
    if (window && !((GdkWindowPrivate *)mSuperWin->bin_window)->destroyed)
    {
      //DumpWindowTree();
      // get a list of children for this window
      XQueryTree(display, window, &root_return, &parent_return,
                 &children_return, &nchildren_return);
      // walk the list of children
      for (i=0; i < nchildren_return; i++)
      {
        Window child_window = children_return[i];
        nsWindow *thisWindow = GetnsWindowFromXWindow(child_window);
        if (thisWindow)
        {
          thisWindow->Destroy();
        }
      }      
    }
  }

  // free up this struct
  if (children_return)
    XFree(children_return);
}

void
nsWindow::ShowCrossAtLocation(guint x, guint y)
{
  g_print("ShowCrossAtLocation %d, %d\n", x, y);
  if (mSuperWin) {
    GdkGC *gc = 0;
    GdkColor white;
    int i;
    gc = gdk_gc_new(GDK_ROOT_PARENT());
    white.pixel = WhitePixel(gdk_display, DefaultScreen(gdk_display));
    gdk_gc_set_foreground(gc,&white);
    gdk_gc_set_function(gc,GDK_XOR);
    gdk_gc_set_subwindow(gc,GDK_INCLUDE_INFERIORS);
    gdk_gc_set_line_attributes(gc, 4, GDK_LINE_SOLID, GDK_CAP_ROUND, GDK_JOIN_ROUND);
    for (i=0; i < 2; i++) {
      gdk_draw_line(mSuperWin->bin_window, gc, x - 20 , y, x + 20, y);
      XSync(gdk_display, False);
      usleep(200);
      gdk_draw_line(mSuperWin->bin_window, gc, x, y - 20, x, y + 20);
    }
    gdk_gc_destroy(gc);
  }
}

// This function will try to take a given native X window and try 
// to find the nsWindow * class that has it corresponds to.

/* static */
nsWindow *
nsWindow::GetnsWindowFromXWindow(Window aWindow)
{
  GdkWindow *thisWindow = NULL;

  thisWindow = gdk_window_lookup(aWindow);

  if (!thisWindow)
  {
    return NULL;
  }
  gpointer data = NULL;
  // see if this is a real widget
  gdk_window_get_user_data(thisWindow, &data);
  if (data)
  {
    if (GTK_IS_OBJECT(data))
    {
      return (nsWindow *)gtk_object_get_data(GTK_OBJECT(data), "nsWindow");
    }
    else
    {
      return NULL;
    }
  }
  else
  {
    // ok, see if it's a shell window
    nsWindow *childWindow = (nsWindow *)g_hash_table_lookup(nsWindow::mWindowLookupTable,
                                                            thisWindow);
    if (childWindow)
    {
      return childWindow;
    }
  }
  // shouldn't ever get here but just to make the compiler happy...
  return NULL;
}

// given an origin window and inner window ( can be the same ) 
// this function will find the innermost window in the 
// window tree that fits inside of the coordinates
// the depth is how deep we are in the tree and really
// is for debugging...

/* static */
Window
nsWindow::GetInnerMostWindow(Window aOriginWindow,
                             Window aWindow,
                             nscoord x, nscoord y,
                             nscoord *retx, nscoord *rety,
                             int depth)
{

  Display     *display;
  Window       window;
  Window       root_return;
  Window       parent_return;
  Window      *children_return = NULL;
  unsigned int nchildren_return;
  unsigned int i;
  Window       returnWindow = None;
  
  display = GDK_DISPLAY();
  window = aWindow;

#ifdef DEBUG_DND_XLATE
  printDepth(depth);
  g_print("Finding children for 0x%lx\n", aWindow);
#endif

  // get a list of children for this window
  XQueryTree(display, window, &root_return, &parent_return,
             &children_return, &nchildren_return);
  
#ifdef DEBUG_DND_XLATE
  printDepth(depth);
  g_print("Found %d children\n", nchildren_return);
#endif

  // walk the list looking for someone who matches the coords

  for (i=0; i < nchildren_return; i++)
  {
    Window src_w = aOriginWindow;
    Window dest_w = children_return[i];
    int  src_x = x;
    int  src_y = y;
    int  dest_x_return, dest_y_return;
    Window child_return;
    
#ifdef DEBUG_DND_XLATE
    printDepth(depth);
    g_print("Checking window 0x%lx with coords %d %d\n", dest_w,
            src_x, src_y);
#endif
    if (XTranslateCoordinates(display, src_w, dest_w,
                              src_x, src_y,
                              &dest_x_return, &dest_y_return,
                              &child_return))
    {
      int x_return, y_return;
      unsigned int width_return, height_return;
      unsigned int border_width_return;
      unsigned int depth_return;

      // get the parent window's geometry
      XGetGeometry(display, src_w, &root_return, &x_return, &y_return,
                   &width_return, &height_return, &border_width_return,
                   &depth_return);

#ifdef DEBUG_DND_XLATE
      printDepth(depth);
      g_print("parent has geo %d %d %d %d\n",
              x_return, y_return, width_return, height_return);
#endif

      // get the child window's geometry
      XGetGeometry(display, dest_w, &root_return, &x_return, &y_return,
                   &width_return, &height_return, &border_width_return,
                   &depth_return);

#ifdef DEBUG_DND_XLATE
      printDepth(depth);
      g_print("child has geo %d %d %d %d\n",
              x_return, y_return, width_return, height_return);
      printDepth(depth);
      g_print("coords are %d %d in child window's geo\n",
              dest_x_return, dest_y_return);
#endif

      int x_offset = width_return;
      int y_offset = height_return;
      x_offset -= dest_x_return;
      y_offset -= dest_y_return;
#ifdef DEBUG_DND_XLATE
      printDepth(depth);
      g_print("offsets are %d %d\n", x_offset, y_offset);
#endif
      // does this child exist within the x,y coords?
      if ((dest_x_return > 0) && (dest_y_return > 0) &&
          (y_offset > 0) && (x_offset > 0))
      {
        // set our return window to this dest
        returnWindow = dest_w;
        // set the coords that we are going to return to the coords that we got above.
        *retx = dest_x_return;
        *rety = dest_y_return;
        // check to see if there's a more inner window that is
        // also within these coords
        Window tempWindow = None;
        tempWindow = GetInnerMostWindow(aOriginWindow, dest_w, x, y, retx, rety, (depth + 1));
        if (tempWindow != None)
          returnWindow = tempWindow;
        goto finishedWalk;
      }
      
    }
    else
    {
      g_assert("XTranslateCoordinates failed!\n");
    }
  }
  
 finishedWalk:

  // free up the list of children
  if (children_return)
    XFree(children_return);

  return returnWindow;
}

// Routines implementing an update queue.
// We keep a single queue for all widgets because it is 
// (most likely) more efficient and better looking to handle
// all the updates in one shot. Actually, this queue should
// be at most per-toplevel. FIXME.
//

static GSList *update_queue = NULL;
static guint update_idle = 0;

gboolean 
nsWindow::UpdateIdle (gpointer data)
{
  GSList *old_queue = update_queue;
  GSList *tmp_list = old_queue;
  
  update_idle = 0;
  update_queue = nsnull;
  
  while (tmp_list)
  {
    nsWindow *window = (nsWindow *)tmp_list->data;
    
    window->mIsUpdating = PR_FALSE;
    window->Update();
    
    tmp_list = tmp_list->next;
  }
  
  g_slist_free (old_queue);
  
  return PR_FALSE;
}

void
nsWindow::QueueDraw ()
{
  if (!mIsUpdating)
  {
    update_queue = g_slist_prepend (update_queue, (gpointer)this);
    if (!update_idle)
      update_idle = g_idle_add_full (G_PRIORITY_HIGH_IDLE, 
                                     UpdateIdle,
                                     NULL, (GDestroyNotify)NULL);
    mIsUpdating = PR_TRUE;
  }
}

void
nsWindow::UnqueueDraw ()
{
  if (mIsUpdating)
  {
    update_queue = g_slist_remove (update_queue, (gpointer)this);
    mIsUpdating = PR_FALSE;
  }
}

void 
nsWindow::DoPaint (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                   nsIRegion *aClipRegion)
{

#ifdef NS_DEBUG
  if (this == debugWidget) {
    g_print("nsWindow::DoPaint %d %d %d %d\n",
            aX, aY, aWidth, aHeight);
  }
#endif // NS_DEBUG
  if (mEventCallback) {

    nsPaintEvent event;
    nsRect rect(aX, aY, aWidth, aHeight);
 
    event.message = NS_PAINT;
    event.widget = (nsWidget *)this;
    event.eventStructType = NS_PAINT_EVENT;
    event.point.x = aX;
    event.point.y = aY; 
    event.time = GDK_CURRENT_TIME; // No time in EXPOSE events
    
    event.rect = &rect;
    event.region = nsnull;
    
    event.renderingContext = GetRenderingContext();
    if (event.renderingContext) {

#ifdef NS_DEBUG
      if (WANT_PAINT_FLASHING)
      {
        GdkWindow *gw = GetRenderWindow(GTK_OBJECT(mSuperWin));
        if (gw)
        {
          GdkRectangle   ar;
          GdkRectangle * area = (GdkRectangle*) NULL;
        
          if (event.rect)
          {
            ar.x = event.rect->x;
            ar.y = event.rect->y;
          
            ar.width = event.rect->width;
            ar.height = event.rect->height;
          
            area = &ar;
          }
        
          nsGtkUtils::gdk_window_flash(gw,1,100000,area);
        }
      }
#endif // NS_DEBUG
      
      DispatchWindowEvent(&event);
      NS_RELEASE(event.renderingContext);
    }

  }
}

NS_IMETHODIMP nsWindow::Update(void)
{
  if (!mSuperWin)               // XXX ???
    return NS_OK;

  if (mIsUpdating)
    UnqueueDraw();

  if (!mUpdateArea->IsEmpty()) {

    PRUint32 numRects;
    mUpdateArea->GetNumRects(&numRects);

    // if we have 1 or more than 10 rects, just paint the bounding box otherwise
    // lets paint each rect by itself

    if (numRects != 1 && numRects < 10) {
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

  } else {
    //  g_print("nsWidget::Update(this=%p): avoided update of empty area\n", this);
  }

  // The view manager also expects us to force our
  // children to update too!

  nsCOMPtr<nsIEnumerator> children;

  children = dont_AddRef(GetChildren());

  if (children) {
    nsCOMPtr<nsISupports> isupp;

    nsCOMPtr<nsIWidget> child;
    while (NS_SUCCEEDED(children->CurrentItem(getter_AddRefs(isupp))) && isupp) {

      child = do_QueryInterface(isupp);

      if (child) {
        child->Update();
      }

      if (NS_FAILED(children->Next())) {
        break;
      }
    }
  }

  // While I'd think you should NS_RELEASE(aPaintEvent.widget) here,
  // if you do, it is a NULL pointer.  Not sure where it is getting
  // released.
  return NS_OK;
}

NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener,
                                            PRBool aDoCapture,
                                            PRBool aConsumeRollupEvent)
{
  GtkWidget *grabWidget;

  grabWidget = mWidget;
  // XXX we need a visible widget!!

  if (aDoCapture) {

    if (mSuperWin) {

      NativeGrab(PR_TRUE);

      sIsGrabbing = PR_TRUE;
      sGrabWindow = this;

      SuppressModality(PR_TRUE);
    }
    gRollupConsumeRollupEvent = PR_TRUE;

    gRollupListener = aListener;
    gRollupWidget = getter_AddRefs(NS_GetWeakReference(NS_STATIC_CAST(nsIWidget*,this)));
  } else {
    // make sure that the grab window is marked as released
    if (sGrabWindow == this) {
      sGrabWindow = NULL;
    }
    sIsGrabbing = PR_FALSE;

    NativeGrab(PR_FALSE);

    gRollupListener = nsnull;
    gRollupWidget = nsnull;
    SuppressModality(PR_FALSE);
  }
  
  return NS_OK;
}

// this function is the function that actually does the native window
// grab passing in PR_TRUE will activate the grab, PR_FALSE will
// release the grab

void nsWindow::NativeGrab(PRBool aGrab)
{
  // unconditionally reset our state
  mLastGrabFailed = PR_FALSE;

  if (aGrab) {
    GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);
    
    gint retval;
    retval = gdk_pointer_grab (GDK_SUPERWIN(mSuperWin)->bin_window, PR_TRUE,(GdkEventMask)
                               (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                                GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                                GDK_POINTER_MOTION_MASK),
                               (GdkWindow*)NULL, cursor, GDK_CURRENT_TIME);
#ifdef DEBUG_GRAB
    printf("nsWindow::NativeGrab %p pointer_grab %d\n", this, retval);
#endif
    // check and set our flag if the grab failed
    if (retval != 0)
      mLastGrabFailed = PR_TRUE;

    retval = gdk_keyboard_grab(mSuperWin->bin_window, PR_TRUE, GDK_CURRENT_TIME);
#ifdef DEBUG_GRAB
    printf("nsWindow::NativeGrab %p keyboard_grab %d\n", this, retval);
#endif
    // check and set our flag if the grab failed
    if (retval != 0)
      mLastGrabFailed = PR_TRUE;

    gdk_cursor_destroy(cursor);
  } else {
#ifdef DEBUG_GRAB
    printf("nsWindow::NativeGrab %p ungrab\n", this);
#endif
    gdk_keyboard_ungrab(GDK_CURRENT_TIME);
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
  }
}

NS_IMETHODIMP nsWindow::Invalidate(PRBool aIsSynchronous)
{

  if (!mSuperWin)
    return NS_OK;
  
  mUpdateArea->SetTo(mBounds.x, mBounds.y, mBounds.width, mBounds.height);
  
  if (aIsSynchronous)
    Update();
  else
    QueueDraw();
  
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{

  if (!mSuperWin)
    return NS_OK;

  mUpdateArea->Union(aRect.x, aRect.y, aRect.width, aRect.height);

  if (aIsSynchronous)
    Update();
  else
    QueueDraw();
  
  return NS_OK;
}

NS_IMETHODIMP nsWindow::InvalidateRegion(const nsIRegion* aRegion, PRBool aIsSynchronous)
{

  if (!mSuperWin)
    return NS_OK;
  
  mUpdateArea->Union(*aRegion);

  if (aIsSynchronous)
    Update();
  else
    QueueDraw();
  
  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetBackgroundColor(const nscolor &aColor)
{
  nsBaseWidget::SetBackgroundColor(aColor);

  if (nsnull != mSuperWin) {
    GdkColor back_color;

    back_color.pixel = ::gdk_rgb_xpixel_from_rgb(NS_TO_GDK_RGB(aColor));

    gdk_window_set_background(mSuperWin->bin_window, &back_color); 
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::SetCursor(nsCursor aCursor)
{
  if (!mSuperWin) 
    /* These cases should agree with enum nsCursor in nsIWidget.h
     * We're limited to those cursors available with XCreateFontCursor()
     * If you change these, change them in gtk/nsWidget, too. */
    return NS_ERROR_FAILURE;

  // Only change cursor if it's changing
  if (aCursor != mCursor) {
    GdkCursor *newCursor = 0;

    switch(aCursor) {
      case eCursor_standard:
        newCursor = gdk_cursor_new(GDK_LEFT_PTR);
        break;

      case eCursor_wait:
        newCursor = gdk_cursor_new(GDK_WATCH);
        break;

      case eCursor_select:
        newCursor = gdk_cursor_new(GDK_XTERM);
        break;

      case eCursor_hyperlink:
        newCursor = gdk_cursor_new(GDK_HAND2);
        break;

      case eCursor_sizeWE:
        /* GDK_SB_H_DOUBLE_ARROW <==>.  The ideal choice is: =>||<= */
        newCursor = gdk_cursor_new(GDK_SB_H_DOUBLE_ARROW);
        break;

      case eCursor_sizeNS:
        /* Again, should be =>||<= rotated 90 degrees. */
        newCursor = gdk_cursor_new(GDK_SB_V_DOUBLE_ARROW);
        break;
      
      case eCursor_sizeNW:
        newCursor = gdk_cursor_new(GDK_TOP_LEFT_CORNER);
        break;

      case eCursor_sizeSE:
        newCursor = gdk_cursor_new(GDK_BOTTOM_RIGHT_CORNER);
        break;

      case eCursor_sizeNE:
        newCursor = gdk_cursor_new(GDK_TOP_RIGHT_CORNER);
        break;

      case eCursor_sizeSW:
        newCursor = gdk_cursor_new(GDK_BOTTOM_LEFT_CORNER);
        break;

      case eCursor_arrow_north:
      case eCursor_arrow_north_plus:
        newCursor = gdk_cursor_new(GDK_TOP_SIDE);
        break;

      case eCursor_arrow_south:
      case eCursor_arrow_south_plus:
        newCursor = gdk_cursor_new(GDK_BOTTOM_SIDE);
        break;

      case eCursor_arrow_west:
      case eCursor_arrow_west_plus:
        newCursor = gdk_cursor_new(GDK_LEFT_SIDE);
        break;

       case eCursor_arrow_east:
       case eCursor_arrow_east_plus:
         newCursor = gdk_cursor_new(GDK_RIGHT_SIDE);
         break;
 
       case eCursor_crosshair:
         newCursor = gdk_cursor_new(GDK_CROSSHAIR);
         break;
 
       case eCursor_move:
         newCursor = gdk_cursor_new(GDK_FLEUR);
         break;
 
       case eCursor_help:
         newCursor = gdk_cursor_new(GDK_QUESTION_ARROW);
         break;
 
       case eCursor_copy: // CSS3
       case eCursor_alias:
       case eCursor_context_menu:
         // XXX: these CSS3 cursors need to be implemented
         // For CSS3 Cursor Definitions, See:
         // www.w3.org/TR/css3-userint
         break;
 
       case eCursor_cell:
         newCursor = gdk_cursor_new(GDK_PLUS);
         break;
 
       case eCursor_grab:
       case eCursor_grabbing:
         newCursor = gdk_cursor_new(GDK_HAND1);
         break;
 
       case eCursor_spinning:
         newCursor = gdk_cursor_new(GDK_EXCHANGE);
         break;
 
       case eCursor_count_up:
       case eCursor_count_down:
       case eCursor_count_up_down:
         // XXX: these CSS3 cursors need to be implemented
         break;

      default:
        NS_ASSERTION(aCursor, "Invalid cursor type");
        break;
    }

    if (nsnull != newCursor) {
      mCursor = aCursor;
      ::gdk_window_set_cursor(mSuperWin->shell_window, newCursor);
      ::gdk_cursor_destroy(newCursor);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetFocus(void)
{
#ifdef DEBUG_FOCUS
  printf("nsWindow::SetFocus %p\n", this);
#endif /* DEBUG_FOCUS */

  if (mHasFocus)
  {
#ifdef DEBUG_FOCUS
    printf("Already have focus.\n");
#endif /* DEBUG_FOCUS */
    return NS_OK;
  }

  GtkWidget *top_mozarea = GetMozArea();
  
  if (top_mozarea)
  {
    if (!GTK_WIDGET_HAS_FOCUS(top_mozarea))
    {
      nsWindow *mozAreaWindow = (nsWindow *)gtk_object_get_data(GTK_OBJECT(top_mozarea), "nsWindow");
      mozAreaWindow->mBlockMozAreaFocusIn = PR_TRUE;
#ifdef DEBUG_FOCUS
      printf("mozarea grabbing focus!\n");
#endif
      gtk_widget_grab_focus(top_mozarea);
      // this will show the window if it's minimized and bring it to
      // the front of the stacking order.
      GetAttention();
      mozAreaWindow->mBlockMozAreaFocusIn = PR_FALSE;
    }
  }
  
  // check to see if we need to send a focus out event for the old window
  if (sFocusWindow)
  {
#ifdef USE_XIM
    sFocusWindow->IMEUnsetFocusWidget();
#endif // USE_XIM 
    // let the current window loose its focus
    sFocusWindow->DispatchLostFocusEvent();
    sFocusWindow->LoseFocus();
  }

  // set the focus window to this window

  sFocusWindow = this;
  mHasFocus = PR_TRUE;

#ifdef USE_XIM
  if (sFocusWindow) sFocusWindow->IMESetFocusWidget();
#endif // USE_XIM 

  DispatchSetFocusEvent();
  if (/*gJustGotActivate &&*/ gJustGotDeactivate) {
    DispatchActivateEvent();
    gJustGotActivate = PR_FALSE;
  }

#ifdef USE_XIM
    IMEActivateWidget();
#endif // USE_XIM 

  return NS_OK;
}

void nsWindow::DispatchSetFocusEvent(void)
{
#ifdef DEBUG_FOCUS
  printf("nsWindow::DispatchSetFocusEvent %p\n", this);
#endif /* DEBUG_FOCUS */

  nsGUIEvent event;
  event.message = NS_GOTFOCUS;
  event.widget  = this;

  event.eventStructType = NS_GUI_EVENT;

  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;

  NS_ADDREF_THIS();
  DispatchFocus(event);
  NS_RELEASE_THIS();
}

void nsWindow::DispatchLostFocusEvent(void)
{

#ifdef DEBUG_FOCUS
  printf("nsWindow::DispatchLostFocusEvent %p\n", this);
#endif /* DEBUG_FOCUS */

  nsGUIEvent event;
  event.message = NS_LOSTFOCUS;
  event.widget  = this;

  event.eventStructType = NS_GUI_EVENT;

  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;

  NS_ADDREF_THIS();
  
  DispatchFocus(event);
  
  NS_RELEASE_THIS();
}

void nsWindow::DispatchActivateEvent(void)
{
  printf("nsWindow::DispatchActivateEvent %p\n", this);

  if(!gJustGotDeactivate)
    return;

  gJustGotDeactivate = PR_FALSE;

  nsGUIEvent event;
  event.message = NS_ACTIVATE;
  event.widget  = this;

  event.eventStructType = NS_GUI_EVENT;

  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;

  NS_ADDREF_THIS();  
  DispatchFocus(event);
  NS_RELEASE_THIS();
}

void nsWindow::DispatchDeactivateEvent(void)
{
  printf("nsWindow::DispatchDeactivateEvent %p\n", this);

  gJustGotDeactivate = PR_TRUE;

  nsGUIEvent event;
  event.message = NS_DEACTIVATE;
  event.widget  = this;

  event.eventStructType = NS_GUI_EVENT;

  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;

  NS_ADDREF_THIS();
  DispatchFocus(event);
  NS_RELEASE_THIS();
}

// this function is called whenever there's a focus in event on the
// mozarea.

void nsWindow::HandleMozAreaFocusIn(void)
{
  // If there's a shell window then we aren't embedded so all the
  // functionality is done in the focus handlers for the toplevel
  // window.  Do nothing.
  if (mShell)
    return;
  // If we're getting this focus in as a result of a child superwin
  // getting called with SetFocus() this flag will be set.  We don't
  // want to generate extra focus in events so just return.
  if (mBlockMozAreaFocusIn)
    return;
  // otherwise, dispatch our focus events
#ifdef DEBUG_FOCUS
  printf("nsWindow::HandleMozAreaFocusIn %p\n", this);
#endif /* DEBUG_FOCUS */
  gJustGotActivate = PR_TRUE;
  DispatchSetFocusEvent();
}

// this function is called whenever there's a focus out event on the
// mozarea.

void nsWindow::HandleMozAreaFocusOut(void)
{
  // If there's a shell window then we aren't embedded so all the
  // functionality is done in the focus handlers for the toplevel
  // window.  Do nothing.
  if (mShell)
    return;
  
  // otherwise handle our focus out here.
#ifdef DEBUG_FOCUS
  printf("nsWindow::HandleMozAreaFocusOut %p\n", this);
#endif /* DEBUG_FOCUS */
  // if there's a window with focus, send a focus out event for that
  // window.
  if (sFocusWindow)
  {
    nsWidget *focusWidget = sFocusWindow;

    NS_ADDREF(focusWidget);
   
    focusWidget->DispatchLostFocusEvent();
    focusWidget->DispatchDeactivateEvent();
    focusWidget->LoseFocus();

    NS_RELEASE(focusWidget);

  }

}

//////////////////////////////////////////////////////////////////////
/* virtual */ void
nsWindow::OnFocusInSignal(GdkEventFocus * aGdkFocusEvent)
{
  
  GTK_WIDGET_SET_FLAGS(mMozArea, GTK_HAS_FOCUS);

  nsGUIEvent event;
  
  printf("send NS_GOTFOCUS from nsWindow::OnFocusInSignal\n");
  event.message = NS_GOTFOCUS;
  event.widget  = this;

  event.eventStructType = NS_GUI_EVENT;

//  event.time = aGdkFocusEvent->time;;
//  event.time = PR_Now();
  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;

  AddRef();
  
  DispatchFocus(event);
  
  Release();
}
//////////////////////////////////////////////////////////////////////
/* virtual */ void
nsWindow::OnFocusOutSignal(GdkEventFocus * aGdkFocusEvent)
{

  GTK_WIDGET_UNSET_FLAGS(mMozArea, GTK_HAS_FOCUS);

  nsGUIEvent event;
  
  event.message = NS_LOSTFOCUS;
  event.widget  = this;

  event.eventStructType = NS_GUI_EVENT;

//  event.time = aGdkFocusEvent->time;;
//  event.time = PR_Now();
  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;

  AddRef();
  
  DispatchFocus(event);
  
  Release();
}

//////////////////////////////////////////////////////////////////
void 
nsWindow::InstallFocusInSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
				(gchar *)"focus_in_event",
				GTK_SIGNAL_FUNC(nsWindow::FocusInSignal));
}
//////////////////////////////////////////////////////////////////
void 
nsWindow::InstallFocusOutSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
				(gchar *)"focus_out_event",
				GTK_SIGNAL_FUNC(nsWindow::FocusOutSignal));
}

void 
nsWindow::HandleGDKEvent(GdkEvent *event)
{
  if (mIsDestroying)
    return;

  switch (event->any.type)
  {
  case GDK_MOTION_NOTIFY:
  {
    XEvent xevent;
    GdkEvent gdk_event;
    PRBool synthEvent = PR_FALSE;
    while (XCheckWindowEvent(GDK_DISPLAY(),
                             GDK_WINDOW_XWINDOW(mSuperWin->bin_window),
                             ButtonMotionMask, &xevent)) {
      synthEvent = PR_TRUE;
    }
    if (synthEvent) {
      gdk_event.type = GDK_MOTION_NOTIFY;
      gdk_event.motion.window = event->motion.window;
      gdk_event.motion.send_event = event->motion.send_event;
      gdk_event.motion.time = xevent.xmotion.time;
      gdk_event.motion.x = xevent.xmotion.x;
      gdk_event.motion.y = xevent.xmotion.y;
      gdk_event.motion.pressure = event->motion.pressure;
      gdk_event.motion.xtilt = event->motion.xtilt;
      gdk_event.motion.ytilt = event->motion.ytilt;
      gdk_event.motion.state = event->motion.state;
      gdk_event.motion.is_hint = xevent.xmotion.is_hint;
      gdk_event.motion.source = event->motion.source;
      gdk_event.motion.deviceid = event->motion.deviceid;
      gdk_event.motion.x_root = xevent.xmotion.x_root;
      gdk_event.motion.y_root = xevent.xmotion.y_root;
      OnMotionNotifySignal (&gdk_event.motion);
    }
    else {
      OnMotionNotifySignal (&event->motion);
    }
  }
  break;
  case GDK_BUTTON_PRESS:
  case GDK_2BUTTON_PRESS:
  case GDK_3BUTTON_PRESS:
    OnButtonPressSignal (&event->button);
    break;
  case GDK_BUTTON_RELEASE:
    OnButtonReleaseSignal (&event->button);
    break;
  case GDK_LEAVE_NOTIFY:
    OnLeaveNotifySignal (&event->crossing);
    break;
  case GDK_ENTER_NOTIFY:
    OnEnterNotifySignal ( &event->crossing );
    break;
  default:
    break;
  }
}

void
nsWindow::InstallToplevelDragDataReceivedSignal(void)
{
  NS_ASSERTION( nsnull != mShell, "mShell is null");
  
  InstallSignal(mShell,
                (gchar *)"drag_data_received",
                GTK_SIGNAL_FUNC(nsWindow::ToplevelDragDataReceivedSignal));
}

/* static */
void nsWindow::ToplevelDragDataReceivedSignal(GtkWidget         *aWidget,
                                              GdkDragContext    *aDragContext,
                                              gint               x,
                                              gint               y,
                                              GtkSelectionData  *aSelectionData,
                                              guint              aInfo,
                                              guint32            aTime,
                                              gpointer           aData)
{
   nsWindow *widget = (nsWindow *)aData;
   NS_ASSERTION(nsnull != widget, "instance pointer is null");

   widget->OnDragDataReceivedSignal(aWidget, aDragContext, x, y, aSelectionData, aInfo, aTime);
}

void
nsWindow::InstallToplevelDragBeginSignal(void)
{
  NS_ASSERTION( nsnull != mShell, "mShell is null");
  
  InstallSignal(mShell,
                (gchar *)"drag_begin",
                GTK_SIGNAL_FUNC(nsWindow::ToplevelDragBeginSignal));
}

/* static */
gint
nsWindow::ToplevelDragBeginSignal(GtkWidget *aWidget,
                                  GdkDragContext   *aDragContext,
                                  gint x,
                                  gint y,
                                  guint time,
                                  void *aData)
{
  g_print("nsWindow::ToplevelDragBeginSignal\n");
  return PR_FALSE;
}

/* static */
gint
nsWindow::ToplevelDragLeaveSignal(GtkWidget *      aWidget,
                                  GdkDragContext   *aDragContext,
                                  guint            aTime,
                                  void             *aData)
{
  if (mLastDragMotionWindow) {
    mLastDragMotionWindow->OnDragLeaveSignal(aDragContext, aTime);
    mLastLeaveWindow = mLastDragMotionWindow;
    mLastDragMotionWindow = NULL;
  }
  else {
    g_print("Warning: no motion window set\n");
  }
  
  return PR_TRUE;
}

void
nsWindow::InstallToplevelDragMotionSignal(void)
{
  NS_ASSERTION( nsnull != mShell, "mShell is null");
  
  InstallSignal(mShell,
                (gchar *)"drag_motion",
                GTK_SIGNAL_FUNC(nsWindow::ToplevelDragMotionSignal));
  
}

/* static */
gint
nsWindow::ToplevelDragMotionSignal(GtkWidget *      aWidget,
                                   GdkDragContext   *aDragContext,
                                   gint             x,
                                   gint             y,
                                   guint            time,
                                   void             *aData)
{
  nsWindow *widget = (nsWindow *)aData;

  NS_ASSERTION(nsnull != widget, "instance pointer is null");

  widget->OnToplevelDragMotion(aWidget, aDragContext, x, y, time);

  return TRUE;
}

#ifdef NS_DEBUG

/* static */
void
nsWindow::dumpWindowChildren(Window aWindow, unsigned int depth)
{
  Display     *display;
  Window       window;
  Window       root_return;
  Window       parent_return;
  Window      *children_return;
  unsigned int nchildren_return;
  unsigned int i;
  
  display = GDK_DISPLAY();
  window = aWindow;
  XQueryTree(display, window, &root_return, &parent_return,
             &children_return, &nchildren_return);

  printDepth(depth);

  g_print("Window 0x%lx ", window);

  GdkWindow *thisWindow = NULL;

  thisWindow = gdk_window_lookup(window);

  if (!thisWindow)
  {
    g_print("(none)\n");
  }
  else
  {
    gpointer data;
    // see if this is a real widget
    gdk_window_get_user_data(thisWindow, &data);
    if (data)
    {
      if (GTK_IS_WIDGET(data))
      {
        g_print("(%s)\n", gtk_widget_get_name(GTK_WIDGET(data)));
      }
      else if (GDK_IS_SUPERWIN(data))
      {
        g_print("(bin_window for nsWindow %p)\n", gtk_object_get_data(GTK_OBJECT(data), "nsWindow"));
      }
      else
      {
        g_print("(invalid GtkWidget)\n");
      }
    }
    else
    {
      // ok, see if it's a shell window
      nsWindow *childWindow = (nsWindow *)g_hash_table_lookup(nsWindow::mWindowLookupTable,
                                                              thisWindow);
      if (childWindow)
      {
        g_print("(shell_window for nsWindow %p)\n", childWindow);
      }
    }
  }

  for (i=0; i < nchildren_return; i++)
  {
    dumpWindowChildren(children_return[i], depth + 1);
  }
  
  // free the list of children
  if (children_return)
    XFree(children_return);

}

void
nsWindow::DumpWindowTree(void)
{
  if (mShell || mSuperWin)
  {
    GdkWindow *startWindow = NULL;
    // see where we are starting
    if (mShell)
    {
      g_print("dumping from shell for %p.\n", this);
      startWindow = mShell->window;
    }
    else 
    {
      g_print("dumping from superwin for %p.\n", this);
      startWindow = mSuperWin->shell_window;
    }
    Window window;
    window = GDK_WINDOW_XWINDOW(startWindow);
    dumpWindowChildren(window, 0);
  }
  else
  {
    g_print("no windows for %p.\n", this);
  }
    
}

#endif /* NS_DEBUG */

void
nsWindow::OnToplevelDragMotion     (GtkWidget      *aWidget,
                                    GdkDragContext *aDragContext,
                                    gint            x,
                                    gint            y,
                                    guint           aTime)
{

#ifdef DEBUG_DND_XLATE
  DumpWindowTree();
#endif

  nscoord retx = 0;
  nscoord rety = 0;

  Window thisWindow = GDK_WINDOW_XWINDOW(aWidget->window);
  Window returnWindow = None;
  returnWindow = GetInnerMostWindow(thisWindow, thisWindow, x, y, &retx, &rety, 0);

  nsWindow *innerMostWidget = NULL;
  innerMostWidget = GetnsWindowFromXWindow(returnWindow);

  if (!innerMostWidget)
    innerMostWidget = this;

#ifdef DEBUG_DND_XLATE
  g_print("innerMostWidget is %p\n", innerMostWidget);
#endif

  // check to see if there was a drag motion window already in place
  if (mLastDragMotionWindow) {
    // if it wasn't this
    if (mLastDragMotionWindow != innerMostWidget) {
      // send a drag event to the last window that got a motion event
      mLastDragMotionWindow->OnDragLeaveSignal(aDragContext, aTime);
    }
  }

  // set the last window to this 
  mLastDragMotionWindow = innerMostWidget;

  if (!innerMostWidget->mIsDragDest)
  {
    // this will happen on the first motion event, so we will generate an ENTER event
    innerMostWidget->OnDragEnterSignal(aDragContext, x, y, aTime);
  }

  // notify the drag service that we are starting a drag motion.
  StartDragMotion(aWidget, aDragContext, aTime);

  nsMouseEvent event;

  event.message = NS_DRAGDROP_OVER;
  event.eventStructType = NS_DRAGDROP_EVENT;

  event.widget = innerMostWidget;

  event.point.x = retx;
  event.point.y = rety;

  innerMostWidget->AddRef();

  innerMostWidget->DispatchMouseEvent(event);

  innerMostWidget->Release();

  // we're done with the drag motion event.  notify the drag service.
  EndDragMotion(aWidget, aDragContext, aTime);
}

void
nsWindow::OnToplevelDragDrop       (GtkWidget      *aWidget,
                                    GdkDragContext *aDragContext,
                                    gint            x,
                                    gint            y,
                                    guint           aTime)
{

  g_print("OnToplevelDragDrop\n");
#ifdef DEBUG_DND_XLATE
  DumpWindowTree();
#endif
  nscoord retx = 0;
  nscoord rety = 0;

  Window thisWindow = GDK_WINDOW_XWINDOW(aWidget->window);
  Window returnWindow = None;
  returnWindow = GetInnerMostWindow(thisWindow, thisWindow, x, y, &retx, &rety, 0);

  nsWindow *innerMostWidget = NULL;
  innerMostWidget = GetnsWindowFromXWindow(returnWindow);

  if (!innerMostWidget)
    innerMostWidget = this;

#ifdef DEBUG_DND_XLATE
  g_print("innerMostWidget is %p\n", innerMostWidget);
#endif

  // check to see if there was a drag motion window already in place
  if (mLastDragMotionWindow) {
    // if it wasn't this
    if (mLastDragMotionWindow != innerMostWidget) {
      // send a drag event to the last window that got a motion event
      mLastDragMotionWindow->OnDragLeaveSignal(aDragContext, aTime);
    }
  }

  // set the last window to this 
  mLastDragMotionWindow = innerMostWidget;

  if (!innerMostWidget->mIsDragDest)
  {
    // this will happen on the first motion event, so we will generate an ENTER event
    innerMostWidget->OnDragEnterSignal(aDragContext, x, y, aTime);
  }

  UpdateDragContext(aWidget, aDragContext, aTime);

  nsMouseEvent event;

  event.message = NS_DRAGDROP_DROP;
  event.eventStructType = NS_DRAGDROP_EVENT;

  event.widget = innerMostWidget;

  event.point.x = retx;
  event.point.y = rety;

  innerMostWidget->AddRef();

  innerMostWidget->DispatchMouseEvent(event);

  innerMostWidget->Release();

  // before we unset the context we need to do a drop_finish

  gdk_drop_finish(aDragContext, TRUE, aTime);

  // after a drop takes place we need to make sure that the drag
  // service doesn't think that it still has a context.  if the other
  // way ( besides the drop ) to end a drag event is during the leave
  // event and and that case is handled in that handler.
  UpdateDragContext(NULL, NULL, 0);

}

void
nsWindow::InstallToplevelDragDropSignal(void)
{
  NS_ASSERTION( nsnull != mShell, "mShell is null");

  InstallSignal(mShell,
                (gchar *)"drag_drop",
                GTK_SIGNAL_FUNC(nsWindow::ToplevelDragDropSignal));
}

/* static */
gint
nsWindow::ToplevelDragDropSignal(GtkWidget *      aWidget,
                                 GdkDragContext   *aDragContext,
                                 gint             x,
                                 gint             y,
                                 guint            aTime,
                                 void             *aData)
{
  nsWindow *widget = (nsWindow *)aData;

  NS_ASSERTION(nsnull != widget, "instance pointer is null");

  widget->OnToplevelDragDrop(aWidget, aDragContext, x, y, aTime);

  return PR_TRUE;
}

void
nsWindow::OnDestroySignal(GtkWidget* aGtkWidget)
{
  nsWidget::OnDestroySignal(aGtkWidget);
  if (aGtkWidget == mShell) {
    mShell = nsnull;
  }
}

gint handle_delete_event(GtkWidget *w, GdkEventAny *e, nsWindow *win)
{
  NS_ADDREF(win);

  // dispatch an "onclose" event. to delete immediately, call win->Destroy()
  nsGUIEvent event;
  nsEventStatus status;
  
  event.message = NS_XUL_CLOSE;
  event.widget  = win;
  event.eventStructType = NS_GUI_EVENT;

  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;
 
  win->DispatchEvent(&event, status);

  NS_RELEASE(win);
  return TRUE;
}



NS_IMETHODIMP nsWindow::PreCreateWidget(nsWidgetInitData *aInitData)
{
  if (nsnull != aInitData) {
    SetWindowType(aInitData->mWindowType);
    SetBorderStyle(aInitData->mBorderStyle);

    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}


gint nsWindow::ConvertBorderStyles(nsBorderStyle bs)
{
  gint w = 0;

  if (bs == eBorderStyle_default)
    return -1;

  if (bs & eBorderStyle_all)
    w |= GDK_DECOR_ALL;
  if (bs & eBorderStyle_border)
    w |= GDK_DECOR_BORDER;
  if (bs & eBorderStyle_resizeh)
    w |= GDK_DECOR_RESIZEH;
  if (bs & eBorderStyle_title)
    w |= GDK_DECOR_TITLE;
  if (bs & eBorderStyle_menu)
    w |= GDK_DECOR_MENU;
  if (bs & eBorderStyle_minimize)
    w |= GDK_DECOR_MINIMIZE;
  if (bs & eBorderStyle_maximize)
    w |= GDK_DECOR_MAXIMIZE;
  if (bs & eBorderStyle_close)
    printf("we don't handle eBorderStyle_close yet... please fix me\n");

  return w;
}


//-------------------------------------------------------------------------
//
// Create the native widget
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::CreateNative(GtkObject *parentWidget)
{
  GdkSuperWin  *superwin = 0;
  GdkEventMask  mask;
  PRBool        parentIsContainer = PR_FALSE;
  GtkContainer *parentContainer = NULL;

  if (parentWidget) {
    if (GDK_IS_SUPERWIN(parentWidget))
      superwin = GDK_SUPERWIN(parentWidget);
    else if (GTK_IS_CONTAINER(parentWidget)) {
      parentContainer = GTK_CONTAINER(parentWidget);
      parentIsContainer = PR_TRUE;
    }
    else
      g_print("warning: attempted to CreateNative() width a non-superwin and non gtk container parent\n");
  }

  switch(mWindowType)
  {
  case eWindowType_dialog:
    mIsToplevel = PR_TRUE;
    mShell = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_window_set_policy(GTK_WINDOW(mShell), PR_TRUE, PR_TRUE, PR_FALSE);
    //    gtk_widget_set_app_paintable(mShell, PR_TRUE);
    InstallRealizeSignal(mShell);

    // create the mozarea.  this will be the single child of the
    // toplevel window
    mMozArea = gtk_mozarea_new();
    gtk_container_add(GTK_CONTAINER(mShell), mMozArea);
    gtk_widget_realize(GTK_WIDGET(mMozArea));
    mSuperWin = GTK_MOZAREA(mMozArea)->superwin;
    // set the back pixmap to None so that we don't get a flash of
    // black
    gdk_window_set_back_pixmap(mShell->window, NULL, FALSE);

    gtk_signal_connect(GTK_OBJECT(mShell),
                       "delete_event",
                       GTK_SIGNAL_FUNC(handle_delete_event),
                       this);
    break;

  case eWindowType_popup:
    mIsToplevel = PR_TRUE;
    mShell = gtk_window_new(GTK_WINDOW_POPUP);
    // create the mozarea.  this will be the single child of the
    // toplevel window
    mMozArea = gtk_mozarea_new();
    gtk_container_add(GTK_CONTAINER(mShell), mMozArea);
    gtk_widget_realize(GTK_WIDGET(mMozArea));
    mSuperWin = GTK_MOZAREA(mMozArea)->superwin;
    // set the back pixmap to None so that we don't get a flash of
    // black
    gdk_window_set_back_pixmap(mShell->window, NULL, FALSE);
    break;

  case eWindowType_toplevel:
    mIsToplevel = PR_TRUE;
    mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    //    gtk_widget_set_app_paintable(mShell, PR_TRUE);
    gtk_window_set_policy(GTK_WINDOW(mShell), PR_TRUE, PR_TRUE, PR_FALSE);
    InstallRealizeSignal(mShell);
    // create the mozarea.  this will be the single child of the
    // toplevel window
    mMozArea = gtk_mozarea_new();
    gtk_container_add(GTK_CONTAINER(mShell), mMozArea);
    gtk_widget_realize(GTK_WIDGET(mMozArea));
    mSuperWin = GTK_MOZAREA(mMozArea)->superwin;
    // set the back pixmap to None so that we don't get a flash of
    // black
    gdk_window_set_back_pixmap(mShell->window, NULL, FALSE);

    gtk_signal_connect(GTK_OBJECT(mShell),
                       "delete_event",
                       GTK_SIGNAL_FUNC(handle_delete_event),
                       this);
    gtk_signal_connect_after(GTK_OBJECT(mShell),
                             "size_allocate",
                             GTK_SIGNAL_FUNC(handle_size_allocate),
                             this);
    break;

  case eWindowType_child:
    // check to see if we need to create a mozarea for this widget
    
    if (parentIsContainer) {
      // check to make sure that the widget is realized
      if (!GTK_WIDGET_REALIZED(parentContainer))
        g_print("Warning: The parent container of this widget is not realized.  I'm going to crash very, very soon.\n");
      else {
        //  create a containg mozarea for the superwin since we've got
        //  play nice with the gtk widget system.
        mMozArea = gtk_mozarea_new();
        gtk_container_add(parentContainer, mMozArea);
        gtk_widget_realize(GTK_WIDGET(mMozArea));
        mSuperWin = GTK_MOZAREA(mMozArea)->superwin;
      }
    }
    else {
      if (superwin)
        mSuperWin = gdk_superwin_new(superwin->bin_window,
                                     mBounds.x, mBounds.y,
                                     mBounds.width, mBounds.height);
    }
    break;

  default:
    break;
  }

  // set up all the focus handling

  if (mShell) {
    GdkEventMask shellMask;
    // Track focus in event for the shell.  We only need the focus in
    // event so that we can dispatch it to the mMozArea
    gtk_signal_connect(GTK_OBJECT(mShell),
                       "focus_in_event",
                       GTK_SIGNAL_FUNC(handle_toplevel_focus_in),
                       this);
    gtk_signal_connect(GTK_OBJECT(mShell),
                       "focus_out_event",
                       GTK_SIGNAL_FUNC(handle_toplevel_focus_out),
                       this);
    gtk_signal_connect(GTK_OBJECT(mShell),
                       "property_notify_event",
                       GTK_SIGNAL_FUNC(handle_toplevel_property_change),
                       this);
    shellMask = gdk_window_get_events(mShell->window);
    shellMask = (GdkEventMask)(shellMask | GDK_PROPERTY_CHANGE_MASK);
    gdk_window_set_events(mShell->window, shellMask);
    // set up the version information
    nsGtkMozRemoteHelper::SetupVersion(mShell->window);
  }

  if (mMozArea) {
    // track focus events for the moz area, too
    gtk_signal_connect(GTK_OBJECT(mMozArea),
                       "focus_in_event",
                       GTK_SIGNAL_FUNC(handle_mozarea_focus_in),
                       this);
    gtk_signal_connect(GTK_OBJECT(mMozArea),
                       "focus_out_event",
                       GTK_SIGNAL_FUNC(handle_mozarea_focus_out),
                       this);
  }

  // end of settup up focus handling

  mask = (GdkEventMask)(GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK |
                        GDK_ENTER_NOTIFY_MASK |
                        GDK_LEAVE_NOTIFY_MASK |
                        GDK_EXPOSURE_MASK |
                        GDK_FOCUS_CHANGE_MASK |
                        GDK_KEY_PRESS_MASK |
                        GDK_KEY_RELEASE_MASK |
                        GDK_POINTER_MOTION_MASK);


  NS_ASSERTION(mSuperWin,"no super window!");
  if (!mSuperWin) return NS_ERROR_FAILURE;

  gdk_window_set_events(mSuperWin->bin_window, 
                        mask);

  // set our object data so that we can find the class for this window
  gtk_object_set_data (GTK_OBJECT (mSuperWin), "nsWindow", this);
  // we want to set this on our moz area and shell too so we can
  // always find the nsWindow given a specific GtkWidget *
  if (mShell)
    gtk_object_set_data(GTK_OBJECT(mShell), "nsWindow", this);
  if (mMozArea) {
    gtk_object_set_data(GTK_OBJECT(mMozArea), "nsWindow", this);
    // make sure that the mozarea widget can take the focus
    GTK_WIDGET_SET_FLAGS(mMozArea, GTK_CAN_FOCUS);
  }
  // set user data on the bin_window so we can find the superwin for it.
  gdk_window_set_user_data (mSuperWin->bin_window, (gpointer)mSuperWin);

  // unset the back pixmap on this window.
  gdk_window_set_back_pixmap(mSuperWin->bin_window, NULL, 0);

  if (mShell) {
    if (parentWidget) {
      GdkWindow *topLevelParent = nsnull;
      if (GTK_IS_WIDGET(parentWidget)) {
        topLevelParent = gdk_window_get_toplevel(GTK_WIDGET(parentWidget)->window);
      }
      else if (GDK_IS_SUPERWIN(parentWidget)) {
        topLevelParent = gdk_window_get_toplevel(GDK_SUPERWIN(parentWidget)->shell_window);
      }
      if (topLevelParent)
        gdk_window_set_transient_for(mShell->window, topLevelParent);
    }

    // install the drag and drop signals for the toplevel window.
    InstallToplevelDragBeginSignal();
    InstallToplevelDragMotionSignal();
    InstallToplevelDragDropSignal();
    InstallToplevelDragDataReceivedSignal();

    // set the shell window as a drop target
    gtk_drag_dest_set (mShell,
                       (GtkDestDefaults)0,
                       NULL,
                       0,
                       (GdkDragAction)0);
  }

  if (mMozArea) {
    AddToEventMask(mMozArea, GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
    gtk_signal_connect(GTK_OBJECT(mMozArea),
                       "key_press_event",
                       GTK_SIGNAL_FUNC(handle_key_press_event),
                       this);
    gtk_signal_connect(GTK_OBJECT(mMozArea),
                       "key_release_event",
                       GTK_SIGNAL_FUNC(handle_key_release_event),
                       this);
  }

  if (mSuperWin) {
    // add the shell_window for this window to the table lookup
    // this is so that as part of destruction we can find the superwin
    // associated with the window.
    g_hash_table_insert(mWindowLookupTable, mSuperWin->shell_window, this);
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Initialize all the Callbacks
//
//-------------------------------------------------------------------------
void nsWindow::InitCallbacks(char * aName)
{
  NS_ASSERTION(mSuperWin,"no superwin, can't init callbacks");
  if (mSuperWin) {
    gdk_superwin_set_event_funcs(mSuperWin,
                                 handle_xlib_shell_event,
                                 handle_superwin_paint,
                                 handle_superwin_flush,
                                 this, NULL);
  }
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void * nsWindow::GetNativeData(PRUint32 aDataType)
{

  if (aDataType == NS_NATIVE_WINDOW)
  {
    if (mSuperWin) {
      GdkWindowPrivate *private_window = (GdkWindowPrivate *)mSuperWin->bin_window;
      if (private_window->destroyed == PR_TRUE) {
        return NULL;
      }
      return (void *)mSuperWin->bin_window;
    }
  }
  else if (aDataType == NS_NATIVE_WIDGET) {
    if (mSuperWin) {
      GdkWindowPrivate *private_window = (GdkWindowPrivate *)mSuperWin->bin_window;
      if (private_window->destroyed == PR_TRUE) {
        return NULL;
      }
    }
    return (void *)mSuperWin;
  }
  else if (aDataType == NS_NATIVE_PLUGIN_PORT) {
    if (mSuperWin) {
      GdkWindowPrivate *private_window = (GdkWindowPrivate *)mSuperWin->bin_window;
      if (private_window->destroyed == PR_TRUE) {
        return NULL;
      }
    }
    return (void *)mSuperWin;
  }

  return nsWidget::GetNativeData(aDataType);
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  NS_ASSERTION(mIsDestroying != PR_TRUE, "Trying to scroll a destroyed widget\n");
  UnqueueDraw();
  mUpdateArea->Offset(aDx, aDy);

  if (mSuperWin) {
    // scroll baby, scroll!
    gdk_superwin_scroll(mSuperWin, aDx, aDy);
  }
  return NS_OK;
}
//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::ScrollWidgets(PRInt32 aDx, PRInt32 aDy)
{
  UnqueueDraw();
  mUpdateArea->Offset(aDx, aDy);

  if (mSuperWin) {
    // scroll baby, scroll!
    gdk_superwin_scroll(mSuperWin, aDx, aDy);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy)
{
  return NS_OK;
}

NS_IMETHODIMP nsWindow::SetTitle(const nsString& aTitle)
{
  if (!mShell)
    return NS_ERROR_FAILURE;


  nsresult rv;
  char *platformText;
  PRInt32 platformLen;

  nsCOMPtr<nsIUnicodeEncoder> encoder;
  // get the charset
  nsAutoString platformCharset;
  nsCOMPtr <nsIPlatformCharset> platformCharsetService = do_GetService(NS_PLATFORMCHARSET_PROGID, &rv);
  if (NS_SUCCEEDED(rv))
    rv = platformCharsetService->GetCharset(kPlatformCharsetSel_Menu, platformCharset);
  if (NS_FAILED(rv))
    platformCharset.AssignWithConversion("ISO-8859-1");

  // get the encoder
  NS_WITH_SERVICE(nsICharsetConverterManager, ccm, NS_CHARSETCONVERTERMANAGER_PROGID, &rv);  
  rv = ccm->GetUnicodeEncoder(&platformCharset, getter_AddRefs(encoder));

  // Estimate out length and allocate the buffer based on a worst-case estimate, then do
  // the conversion.
  PRInt32 len = (PRInt32)aTitle.Length();
  encoder->GetMaxLength(aTitle.GetUnicode(), len, &platformLen);
  if (platformLen) {
    platformText = NS_REINTERPRET_CAST(char*, nsMemory::Alloc(platformLen + sizeof(char)));
    if (platformText) {
      rv = encoder->Convert(aTitle.GetUnicode(), &len, platformText, &platformLen);
      (platformText)[platformLen] = '\0';  // null terminate. Convert() doesn't do it for us
    }
  } // if valid length

  if (platformLen > 0) {
    int status = 0;
    XTextProperty prop;

#ifdef DEBUG_TITLE
    g_print("\nConverted text from unicode to platform locale\n");
    g_print("platformText is %s\n", platformText);
    g_print("platformLen is %d\n", platformLen);
#endif
    

    // maybe using XTextStyle would work as we want... i doubt it though.
    status = XmbTextListToTextProperty(GDK_DISPLAY(), &platformText, 1, XCompoundTextStyle,
                                       &prop);

    if (status == Success) {
#ifdef DEBUG_TITLE
      g_print("\nXmbTextListToTextProperty succeeded\n  text is %s\n  length is %d\n", prop.value,
              prop.nitems);
#endif
      XSetWMProperties(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(mShell->window),
                       &prop, &prop, NULL, 0, NULL, NULL, NULL);

      // TWM sucks and doesn't support compound text.. argh
      XTextProperty tmpProp;
      // XGetWMName is weird - returns nonzero for success
      status = XGetWMName(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(mShell->window),
                          &tmpProp);
      if (status) {
        if (prop.value) // free from the previous attempt
          XFree(prop.value);
        int xret = XmbTextListToTextProperty(GDK_DISPLAY(), &platformText, 1, XStringStyle,
                                             &prop);
        if (xret == Success) {
          XSetWMProperties(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(mShell->window),
                           &prop, &prop, NULL, 0, NULL, NULL, NULL);
        } else {
          // we're fucked, set it however we can.
          gtk_window_set_title(GTK_WINDOW(mShell), nsAutoCString(aTitle));
        }
        if (tmpProp.value)
          XFree(tmpProp.value);
      }

      if (prop.value)
        XFree(prop.value);

      nsMemory::Free(platformText);
      // free properties list?
      return NS_OK;
    }
  }

  // fallback to use bad conversion
  gtk_window_set_title(GTK_WINDOW(mShell), nsAutoCString(aTitle));
  return NS_OK;
}

// Just give the window a default icon, Mozilla.
nsresult nsWindow::SetIcon()
{
  static GdkPixmap *w_pixmap = nsnull;
  static GdkBitmap *w_mask   = nsnull;
  static GdkPixmap *w_minipixmap = nsnull;
  static GdkBitmap *w_minimask = nsnull;
  static nsSpecialSystemDirectory sysDir(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);

  GtkStyle         *w_style;

  w_style = gtk_widget_get_style (mShell);

  if (!w_pixmap) {
    nsFileSpec bigIcon = sysDir + "/icons/" + "mozicon50.xpm";
    if (bigIcon.Exists()) {
      w_pixmap = gdk_pixmap_create_from_xpm(mShell->window,
                                            &w_mask,
                                            &w_style->bg[GTK_STATE_NORMAL],
                                            bigIcon.GetNativePathCString());
    }
  }

  if (!w_minipixmap) {
    nsFileSpec miniIcon = sysDir + "/icons/" + "mozicon16.xpm";
    if (miniIcon.Exists()) {
      w_minipixmap = gdk_pixmap_create_from_xpm(mShell->window,
                                                &w_minimask,
                                                &w_style->bg[GTK_STATE_NORMAL],
                                                miniIcon.GetNativePathCString());
    }
  }

  if (SetIcon(w_pixmap, w_mask) != NS_OK)
        return NS_ERROR_FAILURE;

  /* Now set the mini icon */
  return SetMiniIcon (w_minipixmap, w_minimask);
}

nsresult nsWindow::SetMiniIcon(GdkPixmap *pixmap,
                               GdkBitmap *mask)
{
   GdkAtom icon_atom;
   glong data[2];

   if (!mShell)
      return NS_ERROR_FAILURE;
   
   data[0] = ((GdkPixmapPrivate *)pixmap)->xwindow;
   data[1] = ((GdkPixmapPrivate *)mask)->xwindow;

   icon_atom = gdk_atom_intern ("KWM_WIN_ICON", FALSE);
   gdk_property_change (mShell->window, icon_atom, icon_atom,
                        32, GDK_PROP_MODE_REPLACE,
                        (guchar *)data, 2);
   return NS_OK;
}

// Set the iconify icon for the window.
nsresult nsWindow::SetIcon(GdkPixmap *pixmap, 
                           GdkBitmap *mask)
{
  if (!mShell)
    return NS_ERROR_FAILURE;

  gdk_window_set_icon(mShell->window, (GdkWindow*)nsnull, pixmap, mask);

  return NS_OK;
}



void nsWindow::SendExposeEvent()
{
  mUpdateArea->Intersect(0, 0, mBounds.width, mBounds.height);

  nsPaintEvent event;

  event.rect = new nsRect();

  event.message = NS_PAINT;
  event.widget = this;
  event.eventStructType = NS_PAINT_EVENT;
  //  event.point.x = event->xexpose.x;
  //  event.point.y = event->xexpose.y;
  /* XXX fix this */
  event.time = 0;

  PRInt32 x, y, w, h;
  mUpdateArea->GetBoundingBox(&x,&y,&w,&h);

  event.rect->x = x;
  event.rect->y = y;
  event.rect->width = w;
  event.rect->height = h;
  
  if (event.rect->width == 0 || event.rect->height == 0) {
    delete event.rect;
    return;
  }

  // print out stuff here incase the event got dropped on the floor above
#ifdef NS_DEBUG
  if (CAPS_LOCK_IS_ON)
  {
    debug_DumpPaintEvent(stdout,
                         this,
                         &event,
                         debug_GetName(GTK_OBJECT(mSuperWin)),
                         (PRInt32) debug_GetRenderXID(GTK_OBJECT(mSuperWin)));
  }
#endif // NS_DEBUG

  event.renderingContext = GetRenderingContext();
  if (event.renderingContext) {
    DispatchWindowEvent(&event);
    NS_RELEASE(event.renderingContext);
  }


  mUpdateArea->Subtract(event.rect->x, event.rect->y, event.rect->width, event.rect->height);

#ifdef NS_DEBUG
  if (WANT_PAINT_FLASHING)
  {
    GdkWindow *gw = GetRenderWindow(GTK_OBJECT(mSuperWin));
    if (gw)
    {
      GdkRectangle   ar;
      GdkRectangle * area = (GdkRectangle*) NULL;
        
      if (event.rect)
      {
        ar.x = event.rect->x;
        ar.y = event.rect->y;
          
        ar.width = event.rect->width;
        ar.height = event.rect->height;
        
        area = &ar;
      }
        
      nsGtkUtils::gdk_window_flash(gw,1,100000,area);
    }
  }
#endif // NS_DEBUG

  delete event.rect;
}


/**
 * Processes an Expose Event
 *
 **/
PRBool nsWindow::OnExpose(nsPaintEvent &event)
{
  nsresult result = PR_TRUE;

  // call the event callback
  if (mEventCallback) 
  {
    event.renderingContext = nsnull;

    //    printf("nsWindow::OnExpose\n");

    // expose.. we didn't get an Invalidate, so we should up the count here
    mUpdateArea->Union(event.rect->x, event.rect->y, event.rect->width, event.rect->height);

    SendExposeEvent();
  }

  return result;
}

/**
 * Processes an Draw Event
 *
 **/
PRBool nsWindow::OnDraw(nsPaintEvent &event)
{
  nsresult result ;

  // call the event callback
  if (mEventCallback) 
  {
    event.renderingContext = nsnull;

    // XXX we SHOULD get an expose and not a draw for things, but we don't always with gtk <= 1.2.5
    //    mUpdateArea->Union(event.rect->x, event.rect->y, event.rect->width, event.rect->height);

#ifdef NS_DEBUG
    if (CAPS_LOCK_IS_ON)
    {
      debug_DumpPaintEvent(stdout,
                           this,
                           &event,
                           debug_GetName(GTK_OBJECT(mSuperWin)),
                           (PRInt32) debug_GetRenderXID(GTK_OBJECT(mSuperWin)));
    }
#endif // NS_DEBUG


    //    NS_ADDREF(mUpdateArea);
    //    event.region = mUpdateArea;

    //    printf("\n\n");
    PRInt32 x, y, w, h;
    mUpdateArea->GetBoundingBox(&x,&y,&w,&h);
    //    printf("should be painting x = %i , y = %i , w = %i , h = %i\n", x, y, w, h);
    //    printf("\n\n");
    event.rect->x = x;
    event.rect->y = y;
    event.rect->width = w;
    event.rect->height = h;

    if (event.rect->width == 0 || event.rect->height == 0)
    {
      //      printf("ignoring paint for 0x0\n");
      return NS_OK;
    }


    event.renderingContext = GetRenderingContext();
    if (event.renderingContext) {
      result = DispatchWindowEvent(&event);
      NS_RELEASE(event.renderingContext);
    }


    mUpdateArea->Subtract(event.rect->x, event.rect->y, event.rect->width, event.rect->height);

#ifdef NS_DEBUG
    if (WANT_PAINT_FLASHING)
    {
      GdkWindow *    gw = GetRenderWindow(GTK_OBJECT(mSuperWin));
      if (gw)
      {
        GdkRectangle   ar;
        GdkRectangle * area = (GdkRectangle*) NULL;
        
        if (event.rect)
        {
          ar.x = event.rect->x;
          ar.y = event.rect->y;
          
          ar.width = event.rect->width;
          ar.height = event.rect->height;
          
          area = &ar;
        }
        
        nsGtkUtils::gdk_window_flash(gw,1,100000,area);
      }
    }
#endif // NS_DEBUG
  }
  return result;
}


NS_IMETHODIMP nsWindow::BeginResizingChildren(void)
{
  //  gtk_layout_freeze(GTK_LAYOUT(mWidget));
  return NS_OK;
}

NS_IMETHODIMP nsWindow::EndResizingChildren(void)
{
  //  gtk_layout_thaw(GTK_LAYOUT(mWidget));
  return NS_OK;
}

NS_IMETHODIMP nsWindow::GetScreenBounds(nsRect &aRect)
{
  WidgetToScreen(mBounds, aRect);
  return NS_OK;
}


PRBool nsWindow::OnScroll(nsScrollbarEvent &aEvent, PRUint32 cPos)
{
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::Show(PRBool bState)
{
  if (!mSuperWin)
    return NS_OK; // Will be null durring printing

  mShown = bState;


  // don't show if we are too small
  if (mIsTooSmall)
    return NS_OK;

  // show
  if (bState)
  {
    // show mSuperWin
    gdk_window_show(mSuperWin->bin_window);
    gdk_window_show(mSuperWin->shell_window);

    if (mMozArea)
    {
      gtk_widget_show(mMozArea);
      // if we're a toplevel window, show that too
      if (mShell)
        gtk_widget_show(mShell);
    }
    // and if we've been grabbed, grab for good measure.
    if (sGrabWindow == this && mLastGrabFailed)
      NativeGrab(PR_TRUE);
  }
  // hide
  else
  {
    gdk_window_hide(mSuperWin->bin_window);
    gdk_window_hide(mSuperWin->shell_window);
    // hide toplevel first so that things don't disapear from the screen one by one

    // are we a toplevel window?
    if (mMozArea)
    {
      // if we're a toplevel window hide that too
      if (mShell)
        gtk_widget_hide(mShell);
      gtk_widget_hide(mMozArea);
    } 

  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// grab mouse events for this widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::CaptureMouse(PRBool aCapture)
{
  GtkWidget *grabWidget;

  if (mIsToplevel && mShell)
    grabWidget = mShell;
  else
    grabWidget = mWidget;

  if (aCapture)
  {
    GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);
    gdk_pointer_grab (GTK_WIDGET(grabWidget)->window, PR_TRUE,(GdkEventMask)
                      (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                       GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                       GDK_POINTER_MOTION_MASK),
                      (GdkWindow*) NULL, cursor, GDK_CURRENT_TIME);
    gdk_cursor_destroy(cursor);
    gtk_grab_add(grabWidget);
  }
  else
  {
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    gtk_grab_remove(grabWidget);
  }

  return NS_OK;
}

NS_IMETHODIMP nsWindow::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
  if (mIsToplevel && mShell)
  {
    PRInt32 screenWidth = gdk_screen_width();
    PRInt32 screenHeight = gdk_screen_height();
    if (*aX < kWindowPositionSlop - mBounds.width)
      *aX = kWindowPositionSlop - mBounds.width;
    if (*aX > screenWidth - kWindowPositionSlop)
      *aX = screenWidth - kWindowPositionSlop;
    if (*aY < kWindowPositionSlop - mBounds.height)
      *aY = kWindowPositionSlop - mBounds.height;
    if (*aY > screenHeight - kWindowPositionSlop)
      *aY = screenHeight - kWindowPositionSlop;
  }
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
  // check if we are at right place already
  if((aX == mBounds.x) && (aY == mBounds.y) && !mIsToplevel) {
     return NS_OK;
  }

  mBounds.x = aX;
  mBounds.y = aY;

  if (mIsToplevel && mShell)
  {
#ifdef DEBUG
    // complain if a window is moved offscreen (legal, but potentially worrisome)
    PRInt32 screenWidth = gdk_screen_width();
    PRInt32 screenHeight = gdk_screen_height();
    // no annoying assertions. just mention the issue.
    if (aX < 0 || aX >= screenWidth || aY < 0 || aY >= screenHeight)
      printf("window moved to offscreen position\n");
#endif

    // do it the way it should be done period.
    if (mParent && mWindowType == eWindowType_popup) {
      // *VERY* stupid hack to make gfx combo boxes work
      nsRect oldrect, newrect;
      oldrect.x = aX;
      oldrect.y = aY;
      mParent->WidgetToScreen(oldrect, newrect);
      gtk_widget_set_uposition(mShell, newrect.x, newrect.y);
    } else {
      gtk_widget_set_uposition(mShell, aX, aY);
    }
  }
  else if (mSuperWin) {
    gdk_window_move(mSuperWin->shell_window, aX, aY);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  PRBool nNeedToShow = PR_FALSE;

#if 0
  printf("nsWindow::Resize %s (%p) to %d %d\n",
         (const char *) debug_GetName(mWidget),
         this,
         aWidth, aHeight);
#endif
  
  PRInt32 screenWidth = gdk_screen_width();
  PRInt32 screenHeight = gdk_screen_height();
    
  if(aWidth > screenWidth)
    aWidth = screenWidth;
    
  if(aHeight > screenHeight)
    aHeight = screenHeight;

  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  // code to keep the window from showing before it has been moved or resized

  // if we are resized to 1x1 or less, we will hide the window.  Show(TRUE) will be ignored until a
  // larger resize has happened
  if (aWidth <= 1 || aHeight <= 1)
  {
    if (mMozArea)
    {
      aWidth = 1;
      aHeight = 1;
      mIsTooSmall = PR_TRUE;
      if (mShell)
      {
        if (GTK_WIDGET_VISIBLE(mShell))
        {
          gtk_widget_hide(mMozArea);
          gtk_widget_hide(mShell);
          gtk_widget_unmap(mShell);
        }
      }
      else
      {
        gtk_widget_hide(mMozArea);
      }
    }
    else
    {
      aWidth = 1;
      aHeight = 1;
      mIsTooSmall = PR_TRUE;

      NS_ASSERTION(mSuperWin,"no super window!");
      if (!mSuperWin) return NS_ERROR_FAILURE;

      gdk_window_hide(mSuperWin->bin_window);
      gdk_window_hide(mSuperWin->shell_window);
    }
  }
  else
  {
    if (mIsTooSmall)
    {
      // if we are not shown, we don't want to force a show here, so check and see if Show(TRUE) has been called
      nNeedToShow = mShown;
      mIsTooSmall = PR_FALSE;
    }
  }


  if (mSuperWin) {
    // toplevel window?  if so, we should resize it as well.
    if (mIsToplevel && mShell)
    {
      // set_default_size won't make a window smaller after it is visible
      if (GTK_WIDGET_VISIBLE(mShell) && GTK_WIDGET_REALIZED(mShell))  
        gdk_window_resize(mShell->window, aWidth, aHeight);

      gtk_window_set_default_size(GTK_WINDOW(mShell), aWidth, aHeight);
    }
    // we could be a mozarea resizing.
    else if (mMozArea)
      gdk_window_resize(mMozArea->window, aWidth, aHeight);
    // in any case we always resize our mSuperWin window.
    gdk_superwin_resize(mSuperWin, aWidth, aHeight);
  }
  if (mIsToplevel || mListenForResizes) {
    //g_print("sending resize event\n");
    nsSizeEvent sevent;
    sevent.message = NS_SIZE;
    sevent.widget = this;
    sevent.eventStructType = NS_SIZE_EVENT;
    sevent.windowSize = new nsRect (0, 0, aWidth, aHeight);
    sevent.point.x = 0;
    sevent.point.y = 0;
    sevent.mWinWidth = aWidth;
    sevent.mWinHeight = aHeight;
    // XXX fix this
    sevent.time = 0;
    AddRef();
    OnResize(sevent);
    Release();
    delete sevent.windowSize;
  }
  else {
    //g_print("not sending resize event\n");
  }

  if (nNeedToShow)
    Show(PR_TRUE);

  if (aRepaint)
    Invalidate(PR_FALSE);

  return NS_OK;
}

NS_IMETHODIMP nsWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                               PRInt32 aHeight, PRBool aRepaint)
{
  Move(aX,aY);
  // resize can cause a show to happen, so do this last
  Resize(aWidth,aHeight,aRepaint);
  return NS_OK;
}

NS_IMETHODIMP nsWindow::GetAttention(void)
{
  // get the next up moz area
  GtkWidget *top_mozarea = GetMozArea();
  if (top_mozarea) {
    GtkWidget *top_window = gtk_widget_get_toplevel(top_mozarea);
    if (top_window && GTK_WIDGET_VISIBLE(top_window)) {
      // this will raise the toplevel window
      gdk_window_show(top_window->window);
    }
  }
  return NS_OK;
}

/* virtual */ void
nsWindow::OnRealize(GtkWidget *aWidget)
{
  if (aWidget == mShell) {
    SetIcon();
    gint wmd = ConvertBorderStyles(mBorderStyle);
    if (wmd != -1)
      gdk_window_set_decorations(mShell->window, (GdkWMDecoration)wmd);
  }
}

gint handle_toplevel_focus_in(GtkWidget *      aWidget, 
                              GdkEventFocus *  aGdkFocusEvent, 
                              gpointer         aData)
{
  GtkWindow *window = NULL;
  GtkBin    *bin = NULL;

  if (!aWidget)
    return PR_TRUE;

  if (!aGdkFocusEvent)
    return PR_TRUE;

  nsWindow *widget = (nsWindow *)aData;

  if (!widget)
    return PR_TRUE;

#ifdef DEBUG_FOCUS
  printf("handle_toplevel_focus_in %p\n", widget);
#endif /* DEBUG_FOCUS */

  window = GTK_WINDOW(aWidget);
  bin = GTK_BIN(window);
  
  gtk_widget_grab_focus(bin->child);

  gJustGotActivate = PR_TRUE;
  widget->DispatchSetFocusEvent();

  return PR_TRUE;
}

gint handle_toplevel_focus_out(GtkWidget *      aWidget, 
                               GdkEventFocus *  aGdkFocusEvent, 
                               gpointer         aData)
{
  if (!aWidget)
    return PR_TRUE;
  
  if (!aGdkFocusEvent)
    return PR_TRUE;

  nsWindow *widget = (nsWindow *) aData;

  if (!widget)
    return PR_TRUE;

#ifdef DEBUG_FOCUS
  printf("handle_toplevel_focus_out %p\n", widget); 
#endif

#ifdef USE_XIM
  widget->IMEDeactivateWidget();
#endif // USE_XIM 

  if (widget->sFocusWindow)
  {
#ifdef USE_XIM
    widget->sFocusWindow->IMEUnsetFocusWidget();
#endif // USE_XIM 
  }

  // addref the widget here since we might send > 1 event to it.
  NS_ADDREF(widget);
    
  // if there's a window with focus, send a focus out event for that
  // window.
  if (widget->sFocusWindow)
  {
    nsWidget *focusWidget = widget->sFocusWindow;

    NS_ADDREF(focusWidget);
   
    focusWidget->DispatchLostFocusEvent();
    focusWidget->DispatchDeactivateEvent();
    focusWidget->LoseFocus();

    NS_RELEASE(focusWidget);

  }

  NS_RELEASE(widget);
  
  return PR_TRUE;
}

gint handle_mozarea_focus_in(GtkWidget *      aWidget, 
                             GdkEventFocus *  aGdkFocusEvent, 
                             gpointer         aData)
{
  if (!aWidget)
    return PR_TRUE;

  if (!aGdkFocusEvent)
    return PR_TRUE;

  nsWindow *widget = (nsWindow *)aData;

  if (!widget)
    return PR_TRUE;

#ifdef DEBUG_FOCUS
  printf("handle_mozarea_focus_in\n");
#endif

  // make sure that we set our focus flag
  GTK_WIDGET_SET_FLAGS(aWidget, GTK_HAS_FOCUS);

  widget->HandleMozAreaFocusIn();

  return PR_TRUE;
}

gint handle_mozarea_focus_out(GtkWidget *      aWidget, 
                              GdkEventFocus *  aGdkFocusEvent, 
                              gpointer         aData)
{
#ifdef DEBUG_FOCUS
  printf("handle_mozarea_focus_out\n");
#endif

  if (!aWidget) {
    return PR_TRUE;
  }
  
  if (!aGdkFocusEvent) {
    return PR_TRUE;
  }

  nsWindow *widget = (nsWindow *) aData;

  if (!widget) {
    return PR_TRUE;
  }

  // make sure that we unset our focus flag
  GTK_WIDGET_UNSET_FLAGS(aWidget, GTK_HAS_FOCUS);

  widget->HandleMozAreaFocusOut();

  return PR_TRUE;
}


gboolean handle_toplevel_property_change (
    GtkWidget *widget,
    GdkEventProperty *event,
    gpointer aData)
{
  return nsGtkMozRemoteHelper::HandlePropertyChange(widget, event);
}

void
nsWindow::HandleXlibConfigureNotifyEvent(XEvent *event)
{
#if 0
  XEvent    config_event;

  while (XCheckTypedWindowEvent(event->xany.display, 
                                event->xany.window, 
                                ConfigureNotify,
                                &config_event) == True) {
    // make sure that we don't get other types of events.  
    // StructureNotifyMask includes other kinds of events, too.
    // g_print("clearing xlate queue from widget handler, serial is %ld\n", event->xany.serial);
    //    gdk_superwin_clear_translate_queue(mSuperWin, event->xany.serial);
    *event = config_event;
    // make sure that if we remove a configure event from the queue
    // that it gets pulled out of the superwin tranlate queue,
    // too.
#if 0
    g_print("Extra ConfigureNotify event for window 0x%lx %d %d %d %d\n",
            event->xconfigure.window,
            event->xconfigure.x, 
            event->xconfigure.y,
            event->xconfigure.width, 
            event->xconfigure.height);
#endif
  }

  // gdk_superwin_clear_translate_queue(mSuperWin, event->xany.serial);

#endif

  if (mIsToplevel) {
    nsSizeEvent sevent;
    sevent.message = NS_SIZE;
    sevent.widget = this;
    sevent.eventStructType = NS_SIZE_EVENT;
    sevent.windowSize = new nsRect (event->xconfigure.x, event->xconfigure.y,
                                    event->xconfigure.width, event->xconfigure.height);
    sevent.point.x = event->xconfigure.x;
    sevent.point.y = event->xconfigure.y;
    sevent.mWinWidth = event->xconfigure.width;
    sevent.mWinHeight = event->xconfigure.height;
    // XXX fix this
    sevent.time = 0;
    AddRef();
    OnResize(sevent);
    Release();
    delete sevent.windowSize;
  }
}

// Return the GtkMozArea that is the nearest parent of this widget
GtkWidget *
nsWindow::GetMozArea()
{
  GdkWindow *parent = nsnull;
  GtkWidget *widget;

  if (mMozAreaClosestParent)
  {
    return (GtkWidget *)mMozAreaClosestParent;
  }
  if ((mMozAreaClosestParent == nsnull) && mMozArea)
  {
    mMozAreaClosestParent = mMozArea;
    return (GtkWidget *)mMozAreaClosestParent;
  }
  
  if (mSuperWin)
  {
    parent = mSuperWin->shell_window;
  }

  while (parent)
  {
    gdk_window_get_user_data (parent, (void **)&widget);
    if (widget != nsnull && GTK_IS_MOZAREA (widget))
    {
      mMozAreaClosestParent = widget;
      break;
    }
    parent = gdk_window_get_parent (parent);
    parent = gdk_window_get_parent (parent);
  }
  
  return (GtkWidget *)mMozAreaClosestParent;
}

PRBool
nsWindow::GrabInProgress(void)
{
  return nsWindow::sIsGrabbing;
}

/* static */
nsWindow *
nsWindow::GetGrabWindow(void)
{
  if (nsWindow::sIsGrabbing)
  {
    NS_ADDREF(sGrabWindow);
    return sGrabWindow;
  }
  else
    return nsnull;
}

/* virtual */ GdkWindow *
nsWindow::GetRenderWindow(GtkObject * aGtkObject)
{
  GdkWindow * renderWindow = nsnull;

  if (aGtkObject)
  {
    if (GDK_IS_SUPERWIN(aGtkObject))
    {
      renderWindow = GDK_SUPERWIN(aGtkObject)->bin_window;
    }
  }
  return renderWindow;
}

/* virtual */
GtkWindow *nsWindow::GetTopLevelWindow(void)
{
  GtkWidget *moz_area;

  if (!mSuperWin)
    return NULL;
  moz_area = GetMozArea();
  return GTK_WINDOW(gtk_widget_get_toplevel(moz_area));
}

//////////////////////////////////////////////////////////////////////
//
// Draw signal
// 
//////////////////////////////////////////////////////////////////////
void 
nsWindow::InitDrawEvent(GdkRectangle * aArea,
                        nsPaintEvent & aPaintEvent,
                        PRUint32       aEventType)
{
  aPaintEvent.message = aEventType;
  aPaintEvent.widget  = (nsWidget *) this;

  aPaintEvent.eventStructType = NS_PAINT_EVENT;
  //  aPaintEvent.point.x = 0;
  //  aPaintEvent.point.y = 0;
  aPaintEvent.point.x = aArea->x;
  aPaintEvent.point.y = aArea->y; 
  aPaintEvent.time = GDK_CURRENT_TIME;

  if (aArea != NULL) 
  {
    aPaintEvent.rect = new nsRect(aArea->x, 
							  aArea->y, 
							  aArea->width, 
							  aArea->height);
  }
}
//////////////////////////////////////////////////////////////////////
void 
nsWindow::UninitDrawEvent(GdkRectangle * area,
                          nsPaintEvent & aPaintEvent,
                          PRUint32       aEventType)
{
  if (area != NULL) 
  {
    delete aPaintEvent.rect;
  }

  // While I'd think you should NS_RELEASE(aPaintEvent.widget) here,
  // if you do, it is a NULL pointer.  Not sure where it is getting
  // released.
}
//////////////////////////////////////////////////////////////////////
/* static */ gint
nsWindow::DrawSignal(GtkWidget *    /* aWidget */,
					 GdkRectangle * aArea,
					 gpointer       aData)
{
  nsWindow * window = (nsWindow *) aData;

  NS_ASSERTION(nsnull != window,"window is null");

  return window->OnDrawSignal(aArea);
}
//////////////////////////////////////////////////////////////////////
/* virtual */ gint
nsWindow::OnDrawSignal(GdkRectangle * aArea)
{
  //printf("nsWindow::OnDrawSignal()\n");

  nsPaintEvent pevent;

  InitDrawEvent(aArea, pevent, NS_PAINT);

  nsWindow * win = (nsWindow *) this;

  NS_ADDREF(win);

  win->OnDraw(pevent);

  NS_RELEASE(win);

  UninitDrawEvent(aArea, pevent, NS_PAINT);

  return PR_TRUE;
}

// Add an XATOM property to this window.
// Assuming XA_STRING type.
// Borrowed from xfe classic branch.
void nsWindow::StoreProperty(char *property, unsigned char *data)
{
  
  // This needs to happen before properties start working.
  // Not sure if calling this is ? overkill or not.
  gtk_widget_show_all (mShell);

  // GetRenderWindow(mWidget),
  gdk_property_change (mShell->window,
                       gdk_atom_intern (property, FALSE), /* property */
                       XA_STRING, /* type */
                       8, /* *sizeof(GdkAtom) Format. ? */
                       GDK_PROP_MODE_REPLACE, /* mode */
                       (guchar *)data, /* data */
                       (gint)strlen((char *)data)); /* size of data */
}



ChildWindow::ChildWindow()
{
}

ChildWindow::~ChildWindow()
{
#ifdef NOISY_DESTROY
  IndentByDepth(stdout);
  printf("ChildWindow::~ChildWindow:%p\n", this);
#endif
}

PRBool ChildWindow::IsChild() const
{
  return PR_TRUE;
}

