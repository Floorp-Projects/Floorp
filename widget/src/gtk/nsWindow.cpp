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

#include <gtk/gtk.h>
#define USE_XIM
#include <gdk/gdkx.h>
#include <gtk/gtkprivate.h>

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
#ifdef USE_XIM
#include "nsIServiceManager.h"
#include "nsIPref.h"
#endif

#include "stdio.h"

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
  mIsDestroyingWindow = PR_FALSE;
  mOnDestroyCalled = PR_FALSE;
  mFont = nsnull;
  
  mMenuBar = nsnull;
#ifdef        USE_XIM
  mIMEEnable = PR_TRUE;
  mIC = nsnull;
  mIMECompositionUniString = nsnull;
  mIMECompositionUniStringSize = 0;
#endif
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
#ifdef NOISY_DESTROY
  IndentByDepth(stdout);
  printf("nsWindow::~nsWindow:%p\n", this);
#endif
  mIsDestroyingWindow = PR_TRUE;
  if (nsnull != mShell) {
    Destroy();
  }
  NS_IF_RELEASE(mMenuBar);
}

PRBool nsWindow::IsChild() const
{
  return PR_FALSE;
}

NS_METHOD nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  gint x;
  gint y;

#ifdef DEBUG_pavlov
  g_print("nsWindow::WidgetToScreen\n");
#endif
  if (mIsToplevel && mShell)
  {
    if (mShell->window)
    {
      gdk_window_get_origin(mWidget->window, &x, &y);
      aNewRect.x = x + aOldRect.x;
      aNewRect.y = y + aOldRect.y;
#ifdef DEBUG_pavlov
      g_print("  x = %i, y = %i\n", x, y);
#endif
    }
    else
      return NS_ERROR_FAILURE;
  }
  else if (mWidget)
  {
    if (mWidget->window)
    {
      gdk_window_get_origin(mWidget->window, &x, &y);
      aNewRect.x = x + aOldRect.x;
      aNewRect.y = y + aOldRect.y;
#ifdef DEBUG_pavlov
      g_print("  x = %i, y = %i\n", x, y);
#endif
    }
    else
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_METHOD nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
  g_print("nsWidget::ScreenToWidget\n");
  NS_NOTYETIMPLEMENTED("nsWidget::ScreenToWidget");
  return NS_OK;
}


//-------------------------------------------------------------------------
void nsWindow::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{

}

NS_METHOD nsWindow::Destroy()
{
#ifdef NOISY_DESTROY
  IndentByDepth(stdout);
  printf("nsWindow::Destroy:%p: isDestroyingWindow=%s widget=%p shell=%p parent=%p\n",
         this, mIsDestroyingWindow ? "yes" : "no", mWidget, mShell, mParent);
#endif
  NS_IF_RELEASE(mMenuBar);

  // Call base class first... we need to ensure that upper management
  // knows about the close so that if this is the main application
  // window, for example, the application will exit as it should.

  if (mIsDestroyingWindow == PR_TRUE) {
    nsBaseWidget::Destroy();
    if (PR_FALSE == mOnDestroyCalled)
        nsWidget::OnDestroy();

    if (mShell) {
    	if (GTK_IS_WIDGET(mShell))
     		gtk_widget_destroy(mShell);
    	mShell = nsnull;
    }
  }

  return NS_OK;
}

#ifdef        USE_XIM
void
nsWindow::OnFocusInSignal(GdkEventFocus * aGdkFocusEvent)
{
  nsWidget::OnFocusInSignal(aGdkFocusEvent);

  if(mIMEEnable == PR_FALSE){
#ifdef NOISY_XIM
     printf("  IME is not usable on this window\n");
#endif
     return;
  }
  if (!mIC) {
    GetXIC();
  }
  if (mIC){
    GdkWindow *gdkWindow = (GdkWindow*) GetNativeData(NS_NATIVE_WINDOW);
    if (gdkWindow) {
      gdk_im_begin ((GdkIC*)mIC, gdkWindow);
    } else {
#ifdef NOISY_XIM
      printf("gdkWindow is not usable\n");
#endif
    }
  } else {
#ifdef NOISY_XIM
    printf("mIC can't created yet\n");
#endif
  }
}

void
nsWindow::OnFocusOutSignal(GdkEventFocus * aGdkFocusEvent)
{
  nsWidget::OnFocusOutSignal(aGdkFocusEvent);

  if(mIMEEnable == PR_FALSE){
#ifdef NOISY_XIM
     printf("  IME is not usable on this window\n");
#endif
     return;
  }
  if (mIC){
    GdkWindow *gdkWindow = (GdkWindow*) GetNativeData(NS_NATIVE_WINDOW);
    if (gdkWindow) {
      gdk_im_end();
    } else {
#ifdef NOISY_XIM
      printf("gdkWindow is not usable\n");
#endif
    }
  } else {
#ifdef NOISY_XIM
    printf("mIC isn't created yet\n");
#endif
  }
}
#endif /* USE_XIM */
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
  win->SetIsDestroying( PR_TRUE );
  win->Destroy();
  return TRUE;
}



NS_METHOD nsWindow::PreCreateWidget(nsWidgetInitData *aInitData)
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
NS_METHOD nsWindow::CreateNative(GtkWidget *parentWidget)
{
  mWidget = gtk_layout_new(PR_FALSE, PR_FALSE);
  GTK_WIDGET_SET_FLAGS(mWidget, GTK_CAN_FOCUS);
  gtk_widget_set_app_paintable(mWidget, PR_TRUE);

  gtk_widget_set_name(mWidget, "nsWindow");

  AddToEventMask(mWidget,
                 GDK_BUTTON_PRESS_MASK |
                 GDK_BUTTON_RELEASE_MASK |
                 GDK_ENTER_NOTIFY_MASK |
                 GDK_LEAVE_NOTIFY_MASK |
                 GDK_EXPOSURE_MASK |
                 GDK_FOCUS_CHANGE_MASK |
                 GDK_KEY_PRESS_MASK |
                 GDK_KEY_RELEASE_MASK |
                 GDK_POINTER_MOTION_MASK |
                 GDK_POINTER_MOTION_HINT_MASK);

  switch(mWindowType)
  {
  case eWindowType_dialog:
    mIsToplevel = PR_TRUE;

    mShell = gtk_window_new(GTK_WINDOW_DIALOG);
    gtk_window_set_policy(GTK_WINDOW(mShell), PR_TRUE, PR_TRUE, PR_FALSE);
    InstallRealizeSignal(mShell);

    gtk_container_add(GTK_CONTAINER(mShell), mWidget);

    gtk_signal_connect(GTK_OBJECT(mShell),
                       "delete_event",
                       GTK_SIGNAL_FUNC(handle_delete_event),
                       this);
    break;

  case eWindowType_popup:
    mIsToplevel = PR_TRUE;
    mShell = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_container_add(GTK_CONTAINER(mShell), mWidget);
    break;

  case eWindowType_toplevel:
    mIsToplevel = PR_TRUE;
    mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_policy(GTK_WINDOW(mShell), PR_TRUE, PR_TRUE, PR_FALSE);
    InstallRealizeSignal(mShell);
    gtk_container_add(GTK_CONTAINER(mShell), mWidget);

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
    break;

  default:
    break;
  }


  if (mIsToplevel)
  {
    if (parentWidget)
    {
      GtkWidget *tlw = gtk_widget_get_toplevel(parentWidget);
      if (GTK_IS_WINDOW(tlw))
        gtk_window_set_transient_for(GTK_WINDOW(mShell), GTK_WINDOW(tlw));
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
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "draw",
                     GTK_SIGNAL_FUNC(nsWindow::DrawSignal),
                     (gpointer) this);

  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "expose_event",
                     GTK_SIGNAL_FUNC(handle_expose_event),
                     this);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "key_press_event",
                     GTK_SIGNAL_FUNC(handle_key_press_event),
                     this);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "key_release_event",
                     GTK_SIGNAL_FUNC(handle_key_release_event),
                     this);
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void *nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch(aDataType) {
      case NS_NATIVE_WINDOW:
#ifdef NS_GTK_REF
	return (void *)gdk_window_ref(GTK_LAYOUT(mWidget)->bin_window);
#else
	return (void *)GTK_LAYOUT(mWidget)->bin_window;
#endif
      case NS_NATIVE_DISPLAY:
	return (void *)GDK_DISPLAY();
      case NS_NATIVE_WIDGET:
      case NS_NATIVE_PLUGIN_PORT:

#ifdef NS_GTK_REF
	gtk_widget_ref(mWidget);
#endif
	return (void *)mWidget;
      case NS_NATIVE_GRAPHIC:
       /* GetSharedGC ups the ref count on the GdkGC so make sure you release
	* it afterwards. */
        return (void *)((nsToolkit *)mToolkit)->GetSharedGC();
      default:
	break;
    }
    return nsnull;
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  if (GTK_IS_LAYOUT(mWidget)) {
    GtkAdjustment* horiz = gtk_layout_get_hadjustment(GTK_LAYOUT(mWidget));
    GtkAdjustment* vert = gtk_layout_get_vadjustment(GTK_LAYOUT(mWidget));
    horiz->value -= aDx;
    vert->value -= aDy;
    gtk_adjustment_value_changed(horiz);
    gtk_adjustment_value_changed(vert);
  }
  return NS_OK;
}


NS_METHOD nsWindow::SetTitle(const nsString& aTitle)
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

/**
 * Processes an Expose Event
 *
 **/
PRBool nsWindow::OnPaint(nsPaintEvent &event)
{
  nsresult result ;

  // call the event callback
  if (mEventCallback) {

    event.renderingContext = nsnull;
#if 0
    if (event.rect) {
      g_print("nsWindow::OnPaint(this=%p, {%i,%i,%i,%i})\n", this,
              event.rect->x, event.rect->y,
              event.rect->width, event.rect->height);
    }
    else {
      g_print("nsWindow::OnPaint(this=%p, NO RECT)\n", this);
    }
#endif
    static NS_DEFINE_CID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID,
                                                    nsnull,
                                                    NS_GET_IID(nsIRenderingContext),
                                                    (void **)&event.renderingContext))
    {
      event.renderingContext->Init(mContext, this);
      result = DispatchWindowEvent(&event);
      NS_RELEASE(event.renderingContext);
    }
    else
    {
      result = PR_FALSE;
    }
    
    //NS_RELEASE(event.widget);
  }
  return result;
}


NS_METHOD nsWindow::BeginResizingChildren(void)
{
  //  gtk_layout_freeze(GTK_LAYOUT(mWidget));
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  //  gtk_layout_thaw(GTK_LAYOUT(mWidget));
  return NS_OK;
}

PRBool nsWindow::OnKey(nsKeyEvent &aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}

PRBool nsWindow::OnScroll(nsScrollbarEvent &aEvent, PRUint32 cPos)
{
  return PR_FALSE;
}

NS_METHOD nsWindow::SetMenuBar(nsIMenuBar* aMenuBar)
{
  // this is needed for viewer so we need to do something here
  // like add it to a layout widget.. (i.e. mWidget)
#if 0
  if (mMenuBar == aMenuBar) {
    // Ignore duplicate calls
    return NS_OK;
  }

  if (mMenuBar) {
    // Get rid of the old menubar
    GtkWidget* oldMenuBar;
    mMenuBar->GetNativeData((void*&) oldMenuBar);
    if (oldMenuBar) {
      gtk_container_remove(GTK_CONTAINER(mVBox), oldMenuBar);
    }
    NS_RELEASE(mMenuBar);
  }

  mMenuBar = aMenuBar;
  if (aMenuBar) {
    NS_ADDREF(mMenuBar);
  
    GtkWidget *menubar;
    void *voidData;
    aMenuBar->GetNativeData(voidData);
    menubar = GTK_WIDGET(voidData);

    gtk_menu_bar_set_shadow_type (GTK_MENU_BAR(menubar), GTK_SHADOW_NONE);
    gtk_box_pack_start(GTK_BOX(mVBox), menubar, PR_FALSE, PR_FALSE, 0);
    gtk_box_reorder_child(GTK_BOX(mVBox), menubar, 0);
  }
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::Show(PRBool bState)
{
  if (!mWidget)
    return NS_OK; // Will be null durring printing

  mShown = bState;

  // show
  if (bState)
  {
    // show mWidget
    gtk_widget_show(mWidget);

    // are we a toplevel window?
    if (mIsToplevel && mShell)
    {
#if 0
      printf("nsWidget::Show %s (%p) bState = %i, mWindowType = %i\n",
             mWidget ? gtk_widget_get_name(mWidget) : "(no-widget)", this,
             bState, mWindowType);
#endif

      gtk_widget_show(mShell);
    }
  }
  // hide
  else
  {
    // hide toplevel first so that things don't disapear from the screen one by one

    // are we a toplevel window?
    if (mIsToplevel && mShell)
    {
      gtk_widget_hide(mShell);
    } 

    gtk_widget_hide(mWidget);

   
    // For some strange reason, gtk_widget_hide() does not seem to
    // unmap the window.
    //    gtk_widget_unmap(mWidget);
  }

  mShown = bState;

  return NS_OK;
}

NS_METHOD nsWindow::ShowMenuBar(PRBool aShow)
{
  if (!mMenuBar)
    //    return NS_ERROR_FAILURE;
    return NS_OK;

  GtkWidget *menubar;
  void *voidData;
  mMenuBar->GetNativeData(voidData);
  menubar = GTK_WIDGET(voidData);
  
  if (aShow == PR_TRUE)
    gtk_widget_show(menubar);
  else
    gtk_widget_hide(menubar);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// grab mouse events for this widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CaptureMouse(PRBool aCapture)
{
  if (mIsToplevel && mShell)
  {
    if (aCapture)
      gtk_grab_add(mShell);
    else
      gtk_grab_remove(mShell);
  }
  else
  { 
    if (aCapture)
      gtk_grab_add(mWidget);
    else
      gtk_grab_remove(mWidget);
  }

  return NS_OK;
}


NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
#if 0
  printf("nsWindow::Move %s (%p) to %d %d\n",
         mWidget ? gtk_widget_get_name(mWidget) : "(no-widget)", this,
         aX, aY);
#endif
  // not implimented for toplevel windows
  if (mIsToplevel && mShell)
  {
    // do it the way it should be done period.
    if (!mParent)
      gtk_widget_set_uposition(mShell, aX, aY);
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
  else if (mWidget) 
  {
    GtkWidget *    layout = mWidget->parent;

    GtkAdjustment* ha = gtk_layout_get_hadjustment(GTK_LAYOUT(layout));
    GtkAdjustment* va = gtk_layout_get_vadjustment(GTK_LAYOUT(layout));

    // See nsWidget::Move() for comments on this correction thing
    PRInt32        x_correction = (PRInt32) ha->value;
    PRInt32        y_correction = (PRInt32) va->value;
    
    ::gtk_layout_move(GTK_LAYOUT(layout), 
                      mWidget, 
                      aX + x_correction, 
                      aY + y_correction);
  }

  return NS_OK;
}


NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
#if 0
  printf("nsWindow::Resize %s (%p) to %d %d\n",
         mWidget ? gtk_widget_get_name(mWidget) : "(no-widget)", this,
         aWidth, aHeight);
#endif

  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  // ignore resizes smaller than or equal to 1x1 for everything except not-yet-shown toplevel windows
  if (aWidth <= 1 || aHeight <= 1)
  {
    if (mIsToplevel && mShell)
    {
      if (!GTK_WIDGET_VISIBLE(mShell))
      {
        aWidth = 1;
        aHeight = 1;
      }
    }
    else
      return NS_OK;
  }

  if (mWidget) {
    // toplevel window?  if so, we should resize it as well.
    if (mIsToplevel && mShell)
    {
      gtk_window_set_default_size(GTK_WINDOW(mShell), aWidth, aHeight);
    }

    gtk_widget_set_usize(mWidget, aWidth, aHeight);
  }

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

  return NS_OK;
}


NS_METHOD nsWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                           PRInt32 aHeight, PRBool aRepaint)
{
  Resize(aWidth,aHeight,aRepaint);
  Move(aX,aY);
  return NS_OK;
}

NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
#if 0
  g_print("     nsWindow::Invalidate(nr)  (this=%p , aIsSynchronous=%i)\n",
          this, aIsSynchronous);
#endif
  if (mWidget == nsnull) {
    return NS_OK; // mWidget will be null during printing. 
  }

  if (!GTK_IS_WIDGET (mWidget)) {
    return NS_ERROR_FAILURE;
  }

  if (!GTK_WIDGET_REALIZED (GTK_WIDGET(mWidget))) {
    return NS_ERROR_FAILURE;
  }

  if (aIsSynchronous)
  {
    /*
      if (mIsToplevel && mShell)
      gtk_widget_draw(mShell, (GdkRectangle *) NULL);
    */

    gtk_widget_draw(mWidget, (GdkRectangle *) NULL);
    mUpdateArea.SetRect(0, 0, 0, 0);
  }
  else
  {
    /*
      if (mIsToplevel && mShell)
      gtk_widget_queue_draw(mShell);
    */

    gtk_widget_queue_draw(mWidget);
    mUpdateArea.SetRect(0, 0, mBounds.width, mBounds.height);
  }

  return NS_OK;
}

NS_METHOD nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
#if 0
  g_print("     nsWindow::Invalidate(wr)  (this=%p, x=%i , y=%i , width=%i , height = %i , aIsSynchronous=%i)\n",
          this, aRect.x, aRect.y, aRect.width, aRect.height, aIsSynchronous);
#endif
  if (mWidget == nsnull) {
    return NS_OK;  // mWidget is null during printing
  }

  if (!GTK_IS_WIDGET (mWidget)) {
    return NS_ERROR_FAILURE;
  }

  if (!GTK_WIDGET_REALIZED (GTK_WIDGET(mWidget))) {
    return NS_ERROR_FAILURE;
  }


  if (aIsSynchronous) {
      GdkRectangle nRect;
      NSRECT_TO_GDKRECT(aRect, nRect);
      gtk_widget_draw(mWidget, &nRect);
      /*
        if (mIsToplevel && mShell)
        gtk_widget_draw(mShell, &nRect);
      */
  } else {
      mUpdateArea.UnionRect(mUpdateArea, aRect);
      ::gtk_widget_queue_draw_area(mWidget,
                                   aRect.x, aRect.y,
                                   aRect.width, aRect.height);
      /*
        if (mIsToplevel && mShell)
        gtk_widget_queue_draw_area(mShell,
        aRect.x, aRect.y,
        aRect.width, aRect.height);
      */
  }

  return NS_OK;
}

/* virtual */ void
nsWindow::OnRealize()
{
  SetIcon();

  if (!mShell)
    return;

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

  win->OnPaint(pevent);

  NS_RELEASE(win);

  UninitDrawEvent(aArea, pevent, NS_PAINT);

  return PR_TRUE;
}
//////////////////////////////////////////////////////////////////////

#ifdef        USE_XIM

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

#define PREF_XIM_PREEDIT "xim.preedit"
#define PREF_XIM_STATUS "xim.status"
#define SUPPORTED_PREEDIT (GDK_IM_PREEDIT_AREA | \
                         GDK_IM_PREEDIT_POSITION | \
                         GDK_IM_PREEDIT_NOTHING | \
                         GDK_IM_PREEDIT_NONE)
//                     GDK_IM_PREEDIT_CALLBACKS

#define SUPPORTED_STATUS (GDK_IM_STATUS_NOTHING | \
                        GDK_IM_STATUS_NONE)
//                    GDK_IM_STATUS_AREA
//                    GDK_IM_STATUS_CALLBACKS

void
nsWindow::SetXIC(GdkICPrivate *aIC) {
  if(mIMEEnable == PR_FALSE){
     return;
  }
  mIC = aIC;
  return;
}

GdkICPrivate*
nsWindow::GetXIC() {
  if(mIMEEnable == PR_FALSE){
     return nsnull;
  }
  if (mIC) return mIC;          // mIC is already set

  // IC-per-shell, we share a single IC among all widgets of
  // a single toplevel widget
  nsIWidget *widget = this;
  nsIWidget *root = this;
  while (widget) {
    root = widget;
    widget = widget->GetParent();
  }
  nsWindow *root_win = (nsWindow*)root; // this is a toplevel window
  if (!root_win->mIC) {
    // create an XIC as this is a new toplevel window

    // open an XIM
    if (!gdk_xim_ic) {
#ifdef NOISY_XIM
      printf("Try gdk_im_open()\n");
#endif
      if (gdk_im_open() == FALSE){
#ifdef NOISY_XIM
        printf("Can't Open IM\n");
#endif
      }
    } else {
#ifdef NOISY_XIM
      printf("gdk_xim_ic is already created\n");
#endif
    }
    if (gdk_im_ready()) {
      int height, width;
      // fontset is hardcoded, but need to get a fontset at the
      // text insertion point
      GdkFont *gfontset =
gdk_fontset_load("-misc-fixed-medium-r-normal--*-130-*-*-*-*-*-0");
      GdkWindow *gdkWindow = (GdkWindow*) GetNativeData(NS_NATIVE_WINDOW);
      if (!gdkWindow) return nsnull;

      GdkWindowPrivate *gdkWindow_private = (GdkWindowPrivate*) gdkWindow;
      GdkICAttr *attr = gdk_ic_attr_new();
      GdkICAttributesType attrmask = GDK_IC_ALL_REQ;
      GdkIMStyle style;

      PRInt32 ivalue = 0;
      nsresult rv;

      GdkIMStyle supported_style = SUPPORTED_PREEDIT | SUPPORTED_STATUS;
      style = gdk_im_decide_style(supported_style);

      NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
      if (!NS_FAILED(rv) && (prefs)) {
        rv = prefs->GetIntPref(PREF_XIM_PREEDIT, &ivalue);
        if (SUPPORTED_PREEDIT & ivalue) {
            style = (style & GDK_IM_STATUS_MASK) | ivalue;
        }
        rv = prefs->GetIntPref(PREF_XIM_STATUS, &ivalue);
        if (SUPPORTED_STATUS & ivalue) {
            style = (style & GDK_IM_PREEDIT_MASK) | ivalue;
        }
      }

      attr->style = style;
      attr->client_window = gdkWindow;

      attrmask |= GDK_IC_PREEDIT_COLORMAP;
      attr->preedit_colormap = gdkWindow_private->colormap;

      switch (style & GDK_IM_PREEDIT_MASK) {
      case GDK_IM_PREEDIT_POSITION:
      default:
        attrmask |= GDK_IC_PREEDIT_POSITION_REQ;
        gdk_window_get_size (gdkWindow, &width, &height);

        /* need to know how to get spot location */
        attr->spot_location.x = 0;
        attr->spot_location.y = 0; // height;

        attr->preedit_area.x = 0;
        attr->preedit_area.y = 0;
        attr->preedit_area.width = width;
        attr->preedit_area.height = height;
        attrmask |= GDK_IC_PREEDIT_AREA;

        attr->preedit_fontset = gfontset;
        attrmask |= GDK_IC_PREEDIT_FONTSET;
        break;
      }
      GdkICPrivate *IC = (GdkICPrivate*) gdk_ic_new (attr, attrmask);
      gdk_ic_attr_destroy(attr);
      root_win->SetXIC(IC);     // set to toplevel
      SetXIC(IC);               // set to myself
      return IC;
    }
  } else {
    mIC = root_win->mIC;
    return mIC;
  }
  return nsnull;
}

void
nsWindow::GetXYFromPosition(unsigned long *aX,
                            unsigned long *aY) {
  if(mIMEEnable == PR_FALSE){
     return;
  }
  GdkICAttr *attr = gdk_ic_attr_new();
  GdkICAttributesType attrMask = GDK_IC_PREEDIT_FONTSET;
  mIC->mask = GDK_IC_PREEDIT_FONTSET; // hack
  gdk_ic_get_attr((GdkIC*)mIC, attr, attrMask);
  if (attr->preedit_fontset) {
    *aY += attr->preedit_fontset->ascent;
  }
  gdk_ic_attr_destroy(attr);
  return;
}
void
nsWindow::SetXICSpotLocation(nsPoint aPoint) {
  if(mIMEEnable == PR_FALSE){
     return;
  }
  if (!mIC) GetXIC();
  if (mIC) {
    GdkICAttr *attr = gdk_ic_attr_new();
    GdkICAttributesType attrMask = GDK_IC_SPOT_LOCATION;
    unsigned long x, y;
    x = aPoint.x, y = aPoint.y;
    GetXYFromPosition(&x, &y);
    attr->spot_location.x = x;
    attr->spot_location.y = y;
    gdk_ic_set_attr((GdkIC*)mIC, attr, attrMask);
    gdk_ic_attr_destroy(attr);
  }
  return;
}

#endif
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

NS_METHOD ChildWindow::Destroy()
{
#ifdef NOISY_DESTROY
  IndentByDepth(stdout);
  printf("ChildWindow::Destroy:%p  \n", this);
#endif

  // Skip over baseclass Destroy method which doesn't do what we want;
  // instead make sure widget destroy method gets invoked.
  return nsWidget::Destroy();
}
