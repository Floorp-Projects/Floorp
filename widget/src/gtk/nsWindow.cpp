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
#include "nsIMenuBar.h"
#include "nsToolkit.h"
#include "nsGtkEventHandler.h"
#include "nsIAppShell.h"
#include "nsClipboard.h"
#include "nsIRollupListener.h"

#include "nsGtkUtils.h" // for nsGtkUtils::gdk_window_flash()

gint handle_toplevel_focus_in(
    GtkWidget *      aWidget, 
    GdkEventFocus *  aGdkFocusEvent, 
    gpointer         aData);
    
gint handle_toplevel_focus_out(
    GtkWidget *      aWidget, 
    GdkEventFocus *  aGdkFocusEvent, 
    gpointer         aData);

// are we grabbing?
PRBool      nsWindow::mIsGrabbing = PR_FALSE;
nsWindow   *nsWindow::mGrabWindow = NULL;

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() 
{
  NS_INIT_REFCNT();
  mFontMetrics = nsnull;
  mShell = nsnull;
  mResized = PR_FALSE;
  mVisible = PR_FALSE;
  mDisplayed = PR_FALSE;
  mLowerLeft = PR_FALSE;
  mWindowType = eWindowType_child;
  mBorderStyle = eBorderStyle_default;
  mFont = nsnull;
  mSuperWin = 0;
  mMozArea = 0;
  mMozAreaClosestParent = 0;
  mScrollExposeCounter = 0;
  mIsTooSmall = PR_FALSE;
  mIsUpdating = PR_FALSE;
  mBlockFocusEvents = PR_FALSE;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{

  // make sure that we release the grab indicator here
  if (mGrabWindow == this) {
    mIsGrabbing = PR_FALSE;
    mGrabWindow = NULL;
  }
  // make sure to release our focus window
  if (mHasFocus == PR_TRUE) {
    focusWindow = NULL;
  }

  // always call destroy.  if it's already been called, there's no harm
  // since it keeps track of when it's already been called.

  Destroy();

  if (mIsUpdating)
    UnqueueDraw();
}

PRBool nsWindow::IsChild() const
{
  return PR_FALSE;
}

NS_IMETHODIMP nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  gint x;
  gint y;

  if (mIsToplevel && mShell)
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
  // destroy all of the children that are nsWindow() classes
  // preempting the gdk destroy system.
  DestroyNativeChildren();

  if (mShell) {
    gtk_widget_destroy(mShell);
    mShell = nsnull;
    // the moz area and superwin will have been destroyed when we destroyed the shell
    mMozArea = nsnull;
    mSuperWin = nsnull;
  }
  else if(mSuperWin) {
    gdk_superwin_destroy(mSuperWin);
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
  nsCOMPtr <nsIEnumerator> children (getter_AddRefs(GetChildren()));
  
  if (children) {
    children->First();
    do {
      nsISupports *child;
      if (NS_SUCCEEDED(children->CurrentItem(&child))) {
        nsIWidget *childWindow = NS_STATIC_CAST(nsIWidget *, child);
        NS_RELEASE(child);
        childWindow->Destroy();
      }
    } while(NS_SUCCEEDED(children->Next()));
  }
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

NS_IMETHODIMP nsWindow::GetAbsoluteBounds(nsRect &aRect)
{
  gint x;
  gint y;

  if (mSuperWin)
  {
    gdk_window_get_origin(mSuperWin->shell_window, &x, &y);
    aRect.x = x;
    aRect.y = y;
  }
  else
    return NS_ERROR_FAILURE;
  return NS_OK;
}

void 
nsWindow::DoPaint (PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                   nsIRegion *aClipRegion)
{
  if (mEventCallback) 
  {
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
    if (event.renderingContext)
    {
      PRBool rv;
      
      if (aClipRegion != nsnull)
        event.renderingContext->SetClipRegion(NS_STATIC_CAST(const nsIRegion &, *aClipRegion),
                                              nsClipCombine_kReplace, rv);
      else
      {
        nsRect clipRect (aX, aY, aWidth, aHeight);
        event.renderingContext->SetClipRect(clipRect,
                                            nsClipCombine_kReplace, rv);
      }
      
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
    nsRegionRectSet *regionRectSet = nsnull;

    if (NS_FAILED(mUpdateArea->GetRects(&regionRectSet)))
      return NS_ERROR_FAILURE;

    PRUint32 len;
    PRUint32 i;

    len = regionRectSet->mRectsLen;

    for (i=0;i<len;++i)
    {
      nsRegionRect *r = &(regionRectSet->mRects[i]);
      DoPaint (r->x, r->y, r->width, r->height, mUpdateArea);
    }

    mUpdateArea->FreeRects(regionRectSet);

    mUpdateArea->SetTo(0, 0, 0, 0);
    return NS_OK;
  }
  else {
    //  g_print("nsWidget::Update(this=%p): avoided update of empty area\n", this);
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
#ifdef DEBUG_pavlov
  printf("nsWindow::CaptureRollupEvents() this = %p , doCapture = %i\n", this, aDoCapture);
#endif
  GtkWidget *grabWidget;

  grabWidget = mWidget;
  // XXX we need a visible widget!!

  if (aDoCapture)
  {
#ifdef DEBUG_pavlov
    printf("grabbing widget\n");
#endif
    GdkCursor *cursor = gdk_cursor_new (GDK_ARROW);
    if (!mSuperWin) {
#ifdef DEBUG_pavlov
      printf("no superWin for this widget");
#endif
    }
    else
      {
        mIsGrabbing = PR_TRUE;
        mGrabWindow = this;
#ifdef DEBUG_pavlov
        int ret = 
#endif
        gdk_pointer_grab (GDK_SUPERWIN(mSuperWin)->bin_window, PR_TRUE,(GdkEventMask)
                                    (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                                     GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                                     GDK_POINTER_MOTION_MASK),
                                    (GdkWindow*)NULL, cursor, GDK_CURRENT_TIME);
#ifdef DEBUG_pavlov
      printf("pointer grab returned %i\n", ret);
#endif
      gdk_cursor_destroy(cursor);
    }
  }
  else
    {
#ifdef DEBUG_pavlov
    printf("ungrabbing widget\n");
#endif
    // make sure that the grab window is marked as released
    if (mGrabWindow == this) {
      mGrabWindow = NULL;
    }
    mIsGrabbing = PR_FALSE;
    gdk_pointer_ungrab(GDK_CURRENT_TIME);
    //    gtk_grab_remove(grabWidget);
  }
  
  if (aDoCapture) {
    //    gtk_grab_add(mWidget);
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    gRollupConsumeRollupEvent = PR_TRUE;
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);
  } else {
    //    gtk_grab_remove(mWidget);
    NS_IF_RELEASE(gRollupListener);
    //gRollupListener = nsnull;
    NS_IF_RELEASE(gRollupWidget);
  }
  
  return NS_OK;
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
    return NS_ERROR_FAILURE;

  // Only change cursor if it's changing
  if (aCursor != mCursor) {
    GdkCursor *newCursor = 0;

    switch(aCursor) {
      case eCursor_select:
        newCursor = gdk_cursor_new(GDK_XTERM);
        break;

      case eCursor_wait:
        newCursor = gdk_cursor_new(GDK_WATCH);
        break;

      case eCursor_hyperlink:
        newCursor = gdk_cursor_new(GDK_HAND2);
        break;

      case eCursor_standard:
        newCursor = gdk_cursor_new(GDK_LEFT_PTR);
        break;

      case eCursor_sizeWE:
      case eCursor_sizeNS:
        newCursor = gdk_cursor_new(GDK_TCROSS);
        break;

      case eCursor_arrow_south:
      case eCursor_arrow_south_plus:
        newCursor = gdk_cursor_new(GDK_BOTTOM_SIDE);
        break;

      case eCursor_arrow_north:
      case eCursor_arrow_north_plus:
        newCursor = gdk_cursor_new(GDK_TOP_SIDE);
        break;

      case eCursor_arrow_east:
      case eCursor_arrow_east_plus:
        newCursor = gdk_cursor_new(GDK_RIGHT_SIDE);
        break;

      case eCursor_arrow_west:
      case eCursor_arrow_west_plus:
        newCursor = gdk_cursor_new(GDK_LEFT_SIDE);
        break;

      default:
        NS_ASSERTION(PR_FALSE, "Invalid cursor type");
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

  GtkWidget *top_mozarea = GetMozArea();
  
  if (top_mozarea)
  {
    if (!GTK_WIDGET_HAS_FOCUS(top_mozarea))
      gtk_widget_grab_focus(top_mozarea);
  }
  
  // check to see if we need to send a focus out event for the old window
  if (focusWindow)
  {
    if(mIMEEnable == PR_FALSE)
    {
#ifdef NOISY_XIM
      printf("  IME is not usable on this window\n");
#endif
    }
    if (mIC)
    {
      GdkWindow *gdkWindow = (GdkWindow*) focusWindow->GetNativeData(NS_NATIVE_WINDOW);
      focusWindow->KillICSpotTimer();
      if (gdkWindow)
      {
        gdk_im_end();
      }
      else
      {
#ifdef NOISY_XIM
        printf("gdkWindow is not usable\n");
#endif
      }
    }
    else
    {
#ifdef NOISY_XIM
      printf("mIC isn't created yet\n");
#endif
    }

    // let the current window loose its focus
    focusWindow->LooseFocus();
  }

  // set the focus window to this window

  focusWindow = this;
  mHasFocus = PR_TRUE;

  // don't recurse
  if (mBlockFocusEvents)
  {
    return NS_OK;
  }
  
  mBlockFocusEvents = PR_TRUE;
  
  nsGUIEvent event;
  
  event.message = NS_GOTFOCUS;
  event.widget  = this;

  event.eventStructType = NS_GUI_EVENT;

  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;

  AddRef();
  
  DispatchFocus(event);
  
  Release();

  mBlockFocusEvents = PR_FALSE;

  if(mIMEEnable == PR_FALSE)
  {
#ifdef NOISY_XIM
    printf("  IME is not usable on this window\n");
#endif
    return NS_OK;
  }

  if (!mIC)
    GetXIC();

  if (mIC)
  {
    GdkWindow *gdkWindow = (GdkWindow*) GetNativeData(NS_NATIVE_WINDOW);
    if (gdkWindow)
    {
      gdk_im_begin ((GdkIC*)mIC, gdkWindow);
      PrimeICSpotTimer();
    }
    else
    {
#ifdef NOISY_XIM
      printf("gdkWindow is not usable\n");
#endif
    }
  }
  else
  {
#ifdef NOISY_XIM
    printf("mIC can't created yet\n");
#endif
  }

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////
/* virtual */ void
nsWindow::OnFocusInSignal(GdkEventFocus * aGdkFocusEvent)
{
  
  GTK_WIDGET_SET_FLAGS(mMozArea, GTK_HAS_FOCUS);

  nsGUIEvent event;
  
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


  if(mIMEEnable == PR_FALSE)
  {
#ifdef NOISY_XIM
    printf("  IME is not usable on this window\n");
#endif
    return;
  }

  if (!mIC)
    GetXIC();

  if (mIC)
  {
    GdkWindow *gdkWindow = (GdkWindow*) GetNativeData(NS_NATIVE_WINDOW);
    if (gdkWindow)
    {
      gdk_im_begin ((GdkIC*)mIC, gdkWindow);
      PrimeICSpotTimer();
    }
    else
    {
#ifdef NOISY_XIM
      printf("gdkWindow is not usable\n");
#endif
    }
  }
  else
  {
#ifdef NOISY_XIM
    printf("mIC can't created yet\n");
#endif
  }


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



  if(mIMEEnable == PR_FALSE)
  {
#ifdef NOISY_XIM
    printf("  IME is not usable on this window\n");
#endif
    return;
  }
  if (mIC)
  {
    GdkWindow *gdkWindow = (GdkWindow*) GetNativeData(NS_NATIVE_WINDOW);
    KillICSpotTimer();
    if (gdkWindow)
    {
      gdk_im_end();
    }
    else
    {
#ifdef NOISY_XIM
      printf("gdkWindow is not usable\n");
#endif
    }
  }
  else
  {
#ifdef NOISY_XIM
    printf("mIC isn't created yet\n");
#endif
  }
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
  switch (event->any.type)
    {
    case GDK_MOTION_NOTIFY:
      OnMotionNotifySignal (&event->motion);
      break;
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
      OnButtonPressSignal (&event->button);
      break;
    case GDK_BUTTON_RELEASE:
      OnButtonReleaseSignal (&event->button);
      break;
    default:
      break;
    }
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
  GdkSuperWin *superwin = 0;
  GdkEventMask mask;

  if (parentWidget) {
    if (GDK_IS_SUPERWIN(parentWidget))
      superwin = GDK_SUPERWIN(parentWidget);
    else
      g_print("warning: attempted to CreateNative() width a non-superwin parent\n");
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
    gtk_signal_connect(GTK_OBJECT(mShell),
                       "delete_event",
                       GTK_SIGNAL_FUNC(handle_delete_event),
                       this);
    break;

  case eWindowType_popup:
    mIsToplevel = PR_TRUE;
    mShell = gtk_window_new(GTK_WINDOW_POPUP);
    //    gtk_widget_set_app_paintable(mShell, PR_TRUE);
    // create the mozarea.  this will be the single child of the
    // toplevel window
    mMozArea = gtk_mozarea_new();
    gtk_container_add(GTK_CONTAINER(mShell), mMozArea);
    gtk_widget_realize(GTK_WIDGET(mMozArea));
    mSuperWin = GTK_MOZAREA(mMozArea)->superwin;
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
    if (superwin) {
      mSuperWin = gdk_superwin_new(superwin->bin_window,
                                   mBounds.x, mBounds.y,
                                   mBounds.width, mBounds.height);
    }
    else
      g_print("warning: attempted to CreateNative() without a superwin parent\n");
    break;

  default:
    break;
  }

  mask = (GdkEventMask)(GDK_BUTTON_PRESS_MASK |
                        GDK_BUTTON_RELEASE_MASK |
                        GDK_ENTER_NOTIFY_MASK |
                        GDK_LEAVE_NOTIFY_MASK |
                        GDK_EXPOSURE_MASK |
                        GDK_FOCUS_CHANGE_MASK |
                        GDK_KEY_PRESS_MASK |
                        GDK_KEY_RELEASE_MASK |
                        GDK_POINTER_MOTION_MASK |
                        GDK_POINTER_MOTION_HINT_MASK);

  gdk_window_set_events(mSuperWin->bin_window, 
                        mask);

  // set our object data so that we can find the class for this window
  gtk_object_set_data (GTK_OBJECT (mSuperWin), "nsWindow", this);
  // set user data on the bin_window so we can find the superwin for it.
  gdk_window_set_user_data (mSuperWin->bin_window, (gpointer)mSuperWin);

  // set our background color to make people happy.

  SetBackgroundColor(NS_RGB(192,192,192));
  //gdk_window_set_back_pixmap(mSuperWin->bin_window, NULL, 0);

  // track focus in and focus out events for the shell
  if (mShell) {
    gtk_signal_connect(GTK_OBJECT(mShell),
                       "focus_in_event",
                       GTK_SIGNAL_FUNC(handle_toplevel_focus_in),
                       this);
    gtk_signal_connect(GTK_OBJECT(mShell),
                       "focus_out_event",
                       GTK_SIGNAL_FUNC(handle_toplevel_focus_out),
                       this);

  }

  // XXX fix this later...
  if (mIsToplevel)
  {
    if (parentWidget && GTK_IS_WIDGET(parentWidget))
    {
      GtkWidget *tlw = gtk_widget_get_toplevel(GTK_WIDGET(parentWidget));
      if (GTK_IS_WINDOW(tlw))
      {
        gtk_window_set_transient_for(GTK_WINDOW(mShell), GTK_WINDOW(tlw));
      }
    }
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
  gdk_superwin_set_event_funcs(mSuperWin, handle_xlib_shell_event,
                               handle_xlib_bin_event, this, NULL);
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

#ifdef DEBUG_pavlov
#define OH_I_LOVE_SCROLLING_SMOOTHLY
struct GtkLayoutChild {
  GtkWidget *widget;
  gint x;
  gint y;
};

#define IS_ONSCREEN(x,y) ((x >= G_MINSHORT) && (x <= G_MAXSHORT) && \
                          (y >= G_MINSHORT) && (y <= G_MAXSHORT))

#endif

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  UnqueueDraw();
  mUpdateArea->Offset(aDx, aDy);
  //  printf("mScrollExposeCounter++ = %i\n", mScrollExposeCounter);
  mScrollExposeCounter++;

  if (mSuperWin) {
    gdk_superwin_scroll(mSuperWin, aDx, aDy);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollRect(nsRect &aSrcRect, PRInt32 aDx, PRInt32 aDy)
{
#ifdef OH_I_LOVE_SCROLLING_SMOOTHLY
  // copy our off screen pixmap onto the window.
  GdkWindow *window = nsnull;
  GdkGC *gc = nsnull;

  window = GTK_LAYOUT(mWidget)->bin_window;

  gc = gdk_gc_new(window);

  printf("nsWindow::Scroll(%i, %i\n", aDx, aDy);

  if (aDx > 0) {                        /* moving left */
    if (abs(aDx) < aSrcRect.width) { /* only copy if we arn't moving further than our width */
      gdk_window_copy_area(window, gc,
                           aDx, aDy,                              // source coords
                           window,                                // source window
                           aSrcRect.x, aSrcRect.y,                // dest coords
                           aSrcRect.width - aDx,                  // width
                           aSrcRect.height - aDy);                // height

      nsRect rect(0, 0, aDx, aSrcRect.height);
      Invalidate(rect, PR_TRUE);
    } else {
      Invalidate(aSrcRect, PR_TRUE); /* redraw the area of the widget if we are jumping more than our width */
    }
  } else if (aDx < 0) {                 /* moving right */
    if (abs(aDx) < aSrcRect.width) { /* only copy if we arn't moving further than our width */
      gdk_window_copy_area(window, gc,
                           aSrcRect.x, aSrcRect.y,                // source coords
                           window,                                // source window
                           -aDx, -aDy,                            // dest coords
                           aSrcRect.width + aDx,                  // width
                           aSrcRect.height + aDy);                // height
      nsRect rect(aSrcRect.width + aDx, 0, -aDx, aSrcRect.height);
      Invalidate(rect, PR_TRUE);
    } else {
      Invalidate(aSrcRect, PR_TRUE); /* redraw the area of the widget if we are jumping more than our width */
    }
  }
  if (aDy > 0) {                        /* moving up */
    if (abs(aDy) < aSrcRect.height) { /* only copy if we arn't moving further than our height */
      gdk_window_copy_area(window, gc,
                           aDx, aDy,                              // source coords
                           window,                                // source window
                           aSrcRect.x, aSrcRect.y,                // dest coords
                           aSrcRect.width - aDx,                  // width
                           aSrcRect.height - aDy);                // height
      nsRect rect(0, 0, aSrcRect.width, aDy);
      Invalidate(rect, PR_TRUE);
    } else {
      Invalidate(aSrcRect, PR_TRUE); /* redraw the area of the widget if we are jumping more than our height */
    }
  } else if (aDy < 0) {                 /* moving down */
    if (abs(aDy) < aSrcRect.height) { /* only copy if we arn't moving further than our height */
      gdk_window_copy_area(window, gc,
                           aSrcRect.x, aSrcRect.y,                // source coords
                           window,                                // source window
                           -aDx, -aDy,                            // dest coords
                           aSrcRect.width + aDx,                  // width
                           aSrcRect.height + aDy);                // height
      nsRect rect(0, aSrcRect.height + aDy, aSrcRect.width, -aDy);
      Invalidate(rect, PR_TRUE);
    } else {
      Invalidate(aSrcRect, PR_TRUE); /* redraw the area of the widget if we are jumping more than our height */
    }
  }

  gdk_gc_destroy(gc);

#else

  printf("uhh, you don't use good scrolling and you want to scroll a rect?  too bad.\n");

#endif

  return NS_OK;
}



NS_IMETHODIMP nsWindow::SetTitle(const nsString& aTitle)
{
  if (!mShell)
    return NS_ERROR_FAILURE;

  gtk_window_set_title(GTK_WINDOW(mShell), nsAutoCString(aTitle));

  return NS_OK;
}

// Just give the window a default icon, Mozilla.
#include "mozicon50.xpm"
nsresult nsWindow::SetIcon()
{
  static GdkPixmap *w_pixmap = nsnull;
  static GdkBitmap *w_mask   = nsnull;
  GtkStyle         *w_style;

  w_style = gtk_widget_get_style (mShell);

  if (!w_pixmap) {
    w_pixmap =
      gdk_pixmap_create_from_xpm_d (mShell->window,
				    &w_mask,
				    &w_style->bg[GTK_STATE_NORMAL],
				    mozicon50_xpm);
  }
  
  return SetIcon(w_pixmap, w_mask);
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

#define CAPS_LOCK_IS_ON \
(nsGtkUtils::gdk_keyboard_get_modifiers() & GDK_LOCK_MASK)

#define WANT_PAINT_FLASHING \
(debug_WantPaintFlashing() && CAPS_LOCK_IS_ON)

/**
 * Processes an Expose Event
 *
 **/
PRBool nsWindow::OnExpose(nsPaintEvent &event)
{
  nsresult result;

  // call the event callback
  if (mEventCallback) 
  {
    event.renderingContext = nsnull;

    // expose.. we didn't get an Invalidate, so we should up the count here
    mUpdateArea->Union(event.rect->x, event.rect->y, event.rect->width, event.rect->height);

    mUpdateArea->Intersect(0, 0, mBounds.width, mBounds.height);

    //    NS_ADDREF(mUpdateArea);
    //    event.region = mUpdateArea;
    if (mScrollExposeCounter > 1) {
      //printf("mScrollExposeCounter-- = %i\n", mScrollExposeCounter);
      mScrollExposeCounter--;
      return NS_OK;
    }
    //    printf("mScrollExposeCounter   = 0\n");
    mScrollExposeCounter = 0;

    //    printf("\n\n");
    PRInt32 x, y, w, h;
    mUpdateArea->GetBoundingBox(&x,&y,&w,&h);

    //    printf("\n\n");
    event.rect->x = x;
    event.rect->y = y;
    event.rect->width = w;
    event.rect->height = h;

    if (event.rect->width == 0 || event.rect->height == 0)
    {
      //      printf("********\n****** got an expose for 0x0 window?? - ignoring paint for 0x0\n");
      return NS_OK;
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
    if (event.renderingContext)
    {
      PRBool rv;

      event.renderingContext->SetClipRegion(NS_STATIC_CAST(const nsIRegion &, *mUpdateArea),
                                            nsClipCombine_kReplace, rv);

      result = DispatchWindowEvent(&event);
      NS_RELEASE(event.renderingContext);
      //      NS_RELEASE(mUpdateArea);
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
    if (event.renderingContext)
    {
      PRBool rv;

      event.renderingContext->SetClipRegion(NS_STATIC_CAST(const nsIRegion &, *mUpdateArea),
                                            nsClipCombine_kReplace, rv);

      result = DispatchWindowEvent(&event);
      NS_RELEASE(event.renderingContext);
      //      NS_RELEASE(mUpdateArea);
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

PRBool nsWindow::OnKey(nsKeyEvent &aEvent)
{

  PRBool    releaseWidget = PR_FALSE;
  nsWidget *widget = NULL;

  // rewrite the key event to the window with 'de focus
  if (focusWindow) {
    widget = focusWindow;
    NS_ADDREF(widget);
    aEvent.widget = focusWindow;
    releaseWidget = PR_TRUE;
  }
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }

  if (releaseWidget)
    NS_RELEASE(widget);

  return PR_FALSE;
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

    // are we a toplevel window?
    if (mIsToplevel && mShell)
    {
#if 0
      printf("nsWindow::Show %s (%p) bState = %i, mWindowType = %i\n",
             (const char *) debug_GetName(mWidget),
             this,
             bState, mWindowType);
#endif
      gtk_widget_show(mMozArea);
      gtk_widget_show(mShell);
    }
  }
  // hide
  else
  {
    gdk_window_hide(mSuperWin->bin_window);
    gdk_window_hide(mSuperWin->shell_window);
    // hide toplevel first so that things don't disapear from the screen one by one

    // are we a toplevel window?
    if (mIsToplevel && mShell)
    {
      gtk_widget_hide(mShell);
      gtk_widget_hide(mMozArea);
      //gtk_widget_unmap(mShell);
    } 

    // For some strange reason, gtk_widget_hide() does not seem to
    // unmap the window.
    //    gtk_widget_unmap(mWidget);
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
    printf("grabbing widget\n");
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

NS_IMETHODIMP nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
  if (mIsToplevel && mShell)
  {
    // do it the way it should be done period.
    if (!mParent)
    {
      // XXX don't move the window if it is toplevel window.. this keeps us from moving the
      // window's title bar off the screen in some Window managers
      if (mWindowType != eWindowType_toplevel)
        gtk_widget_set_uposition(mShell, aX, aY);
    }
    else
    {
      // *VERY* stupid hack to make gfx combo boxes work
      nsRect oldrect, newrect;
      oldrect.x = aX;
      oldrect.y = aY;
      mParent->WidgetToScreen(oldrect, newrect);
      gtk_widget_set_uposition(mShell, newrect.x, newrect.y);
    }
  }
  else if (mSuperWin)
  {
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

  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  // code to keep the window from showing before it has been moved or resized

  // if we are resized to 1x1 or less, we will hide the window.  Show(TRUE) will be ignored until a
  // larger resize has happened
  if (aWidth <= 1 || aHeight <= 1)
  {
    if (mIsToplevel && mShell)
    {
      aWidth = 1;
      aHeight = 1;
      mIsTooSmall = PR_TRUE;
      if (GTK_WIDGET_VISIBLE(mShell))
      {
        gtk_widget_hide(mMozArea);
        gtk_widget_hide(mShell);
        gtk_widget_unmap(mShell);
      }
    }
    else
    {
      aWidth = 1;
      aHeight = 1;
      mIsTooSmall = PR_TRUE;
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
      if (GTK_WIDGET_VISIBLE(mShell) && GTK_WIDGET_REALIZED(mShell))  // set_default_size won't make a window smaller after it is visible
        gdk_window_resize(mShell->window, aWidth, aHeight);

      gtk_window_set_default_size(GTK_WINDOW(mShell), aWidth, aHeight);
    }
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
#if 0
  // XXX pav
  // call the size allocation handler directly to avoid code duplication
  // note, this could be a problem as this will make layout think that it
  // got the size it requested which could be wrong.
  // but, we don't use many native widgets anymore, so this shouldn't be a problem
  // layout's will size to the size you tell them to, which are the only native widgets
  // we still use after all the xp widgets land
  GtkAllocation alloc;
  alloc.width = aWidth;
  alloc.height = aHeight;
  alloc.x = 0;
  alloc.y = 0;
  handle_size_allocate(mWidget, &alloc, this);
#endif

  if (nNeedToShow)
    Show(PR_TRUE);

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

/* virtual */ void
nsWindow::OnRealize(GtkWidget *aWidget)
{
  if (aWidget == mShell)
  {
    SetIcon();

    // we were just realized, so we better have a window, but we will make sure...
    if (mShell->window)
    {
      // XXX bug 8002
      //    gdk_window_raise(mShell->window);

      gint wmd = ConvertBorderStyles(mBorderStyle);
      if (wmd != -1)
        gdk_window_set_decorations(mShell->window, (GdkWMDecoration)wmd);
    }
  }
}

gint handle_toplevel_focus_in(GtkWidget *      aWidget, 
                              GdkEventFocus *  aGdkFocusEvent, 
                              gpointer         aData)
{

  if(!aWidget) {
    return PR_TRUE;
  }

  if(!aGdkFocusEvent) {
    return PR_TRUE;
  }

  nsWidget * widget = (nsWidget *) aData;

  if(!widget) {
    return PR_TRUE;
  }

  nsGUIEvent event;
  
  event.message = NS_ACTIVATE;
  event.widget  = widget;

  event.eventStructType = NS_GUI_EVENT;

  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;
 
  NS_ADDREF(widget);
 
  nsEventStatus status;
  widget->DispatchEvent(&event, status);
  
  NS_RELEASE(widget);
  
  return PR_TRUE;
}

gint handle_toplevel_focus_out(GtkWidget *      aWidget, 
                               GdkEventFocus *  aGdkFocusEvent, 
                               gpointer         aData)
{
  if(!aWidget) {
    return PR_TRUE;
  }
  
  if(!aGdkFocusEvent) {
    return PR_TRUE;
  }

  nsWidget * widget = (nsWidget *) aData;

  if(!widget) {
    return PR_TRUE;
  }

  // Dispatch NS_DEACTIVATE
  nsGUIEvent event;
  
  event.message = NS_DEACTIVATE;
  event.widget  = widget;

  event.eventStructType = NS_GUI_EVENT;

  event.time = 0;
  event.point.x = 0;
  event.point.y = 0;

  NS_ADDREF(widget);

  nsEventStatus status;
  widget->DispatchEvent(&event, status);
  
  NS_RELEASE(widget);
  
  return PR_TRUE;
}

void
nsWindow::HandleXlibExposeEvent(XEvent *event)
{
  nsPaintEvent pevent;

  pevent.rect = new nsRect(event->xexpose.x, event->xexpose.y,
                           event->xexpose.width, event->xexpose.height);

  if (event->xexpose.count != 0) {
    XEvent       extra_event;
    do {
      XWindowEvent(event->xany.display, event->xany.window, ExposureMask, (XEvent *)&extra_event);
      pevent.rect->UnionRect(*pevent.rect, nsRect(extra_event.xexpose.x, extra_event.xexpose.y,
                                                  extra_event.xexpose.width, extra_event.xexpose.height));
      if (mScrollExposeCounter > 0) {
        int delta = MIN(mScrollExposeCounter, extra_event.xexpose.count);
        //printf("delta = %i\n", delta);
        mScrollExposeCounter -= delta;
      }
    } while (extra_event.xexpose.count > 0);
  }
  
  pevent.message = NS_PAINT;
  pevent.widget = this;
  pevent.eventStructType = NS_PAINT_EVENT;
  pevent.point.x = event->xexpose.x;
  pevent.point.y = event->xexpose.y;
  /* XXX fix this */
  pevent.time = 0;
  AddRef();
  OnExpose(pevent);
  Release();
  delete pevent.rect;
}
 
void
nsWindow::HandleXlibButtonEvent(XButtonEvent * aButtonEvent)
{
  nsMouseEvent event;
  
  PRUint32 eventType = 0;
  
  if (aButtonEvent->type == ButtonPress)
    {
      switch(aButtonEvent->button)
        {
        case 1:
          eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
          break;
          
        case 2:
          eventType = NS_MOUSE_MIDDLE_BUTTON_DOWN;
          break;
          
        case 3:
          eventType = NS_MOUSE_RIGHT_BUTTON_DOWN;
          break;
          
        default:
          eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
          break;
        }
    }
  else if (aButtonEvent->type == ButtonRelease)
    {
      switch(aButtonEvent->button)
        {
        case 1:
          eventType = NS_MOUSE_LEFT_BUTTON_UP;
          break;
          
        case 2:
          eventType = NS_MOUSE_MIDDLE_BUTTON_UP;
          break;
          
        case 3:
          eventType = NS_MOUSE_RIGHT_BUTTON_UP;
          break;
          
        default:
          eventType = NS_MOUSE_LEFT_BUTTON_UP;
          break;
        }
    }
  
  event.message = eventType;
  event.widget  = this;
  event.eventStructType = NS_MOUSE_EVENT;
  
  event.point.x = nscoord(aButtonEvent->x);
  event.point.y = nscoord(aButtonEvent->y);
  
  event.isShift = aButtonEvent->state & ShiftMask;
  event.isControl = aButtonEvent->state & ControlMask;
  event.isAlt = aButtonEvent->state & Mod1Mask;
  event.time = aButtonEvent->time;
  event.clickCount = 1;
  
  AddRef();
  DispatchMouseEvent(event);
  Release();
}

void
nsWindow::HandleXlibMotionNotifyEvent(XMotionEvent * aMotionEvent)
{
  nsMouseEvent event;
  
  event.message = NS_MOUSE_MOVE;
  event.eventStructType = NS_MOUSE_EVENT;
  
  event.point.x = nscoord(aMotionEvent->x);
  event.point.y = nscoord(aMotionEvent->y);
  
  event.widget = this;
  
  AddRef();
  
  DispatchMouseEvent(event);
  
  Release();
}

void
nsWindow::HandleXlibCrossingEvent(XCrossingEvent * aCrossingEvent)
{
  nsMouseEvent event;
  
  if (aCrossingEvent->type == EnterNotify)
    event.message = NS_MOUSE_ENTER;
  
  else
    event.message = NS_MOUSE_EXIT;
  
  event.widget  = this;
  event.eventStructType = NS_MOUSE_EVENT;
  
  event.point.x = nscoord(aCrossingEvent->x);
  event.point.y = nscoord(aCrossingEvent->y);
  event.time = aCrossingEvent->time;
  
  AddRef();
  
  DispatchMouseEvent(event);
  
  Release();
}


void
nsWindow::HandleXlibConfigureNotifyEvent(XEvent *event)
{
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

  gdk_superwin_clear_translate_queue(mSuperWin, event->xany.serial);

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
  GdkWindow *parent = mSuperWin->shell_window;
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
  return nsWindow::mIsGrabbing;
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

