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

#include "nsGtkEventHandler.h"
#include "nsIAppShell.h"
#include "nsIComponentManager.h"
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsILookAndFeel.h"
#include "nsToolkit.h"
#include "nsWidgetsCID.h"

#include <gdk/gdkx.h>


#ifdef USE_XIM
#include "nsIServiceManager.h"
#include "nsIPref.h"
#endif


static NS_DEFINE_CID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);


nsILookAndFeel *nsWidget::sLookAndFeel = nsnull;
PRUint32 nsWidget::sWidgetCount = 0;

//
// Keep track of the last widget being "dragged"
//
nsWidget *nsWidget::sButtonMotionTarget = NULL;
gint nsWidget::sButtonMotionRootX = -1;
gint nsWidget::sButtonMotionRootY = -1;
gint nsWidget::sButtonMotionWidgetX = -1;
gint nsWidget::sButtonMotionWidgetY = -1;

// Drag & Drop stuff.
enum {
  TARGET_STRING,
  TARGET_ROOTWIN
};

static GtkTargetEntry target_table[] = {
  { "STRING",     0, TARGET_STRING },
  { "text/plain", 0, TARGET_STRING },
  { "application/x-rootwin-drop", 0, TARGET_ROOTWIN }
};

static guint n_targets = sizeof(target_table) / sizeof(target_table[0]);


//#undef DEBUG_pavlov

nsWidget::nsWidget()
{
  // XXX Shouldn't this be done in nsBaseWidget?
  NS_INIT_REFCNT();

  if (!sLookAndFeel) {
    if (NS_OK != nsComponentManager::CreateInstance(kLookAndFeelCID,
                                                    nsnull,
                                                    NS_GET_IID(nsILookAndFeel),
                                                    (void**)&sLookAndFeel))
      sLookAndFeel = nsnull;
  }

  if (sLookAndFeel)
    sLookAndFeel->GetColor(nsILookAndFeel::eColor_WindowBackground,
                           mBackground);

  mGrabTime = 0;
  mWidget = nsnull;
  mParent = nsnull;
  mPreferredWidth  = 0;
  mPreferredHeight = 0;
  mShown = PR_FALSE;
  mBounds.x = 0;
  mBounds.y = 0;
  mBounds.width = 0;
  mBounds.height = 0;
  mIsDestroying = PR_FALSE;
  mIsDragDest = PR_FALSE;
  mOnDestroyCalled = PR_FALSE;
  mIsToplevel = PR_FALSE;
  mUpdateArea.SetRect(0, 0, 0, 0);
  sWidgetCount++;



#ifdef        USE_XIM
  mIMEEnable = PR_TRUE;
  mIC = nsnull;
  mIMECompositionUniString = nsnull;
  mIMECompositionUniStringSize = 0;
#endif

}

nsWidget::~nsWidget()
{
#ifdef NOISY_DESTROY
  IndentByDepth(stdout);
  printf("nsWidget::~nsWidget:%p\n", this);
#endif
  mIsDestroying = PR_TRUE;
  if (nsnull != mWidget) {
    Destroy();
  }
  if (!sWidgetCount--) {
    NS_IF_RELEASE(sLookAndFeel);
  }
}

NS_IMETHODIMP nsWidget::GetAbsoluteBounds(nsRect &aRect)
{
  gint x;
  gint y;

#ifdef DEBUG_pavlov
  g_print("nsWidget::GetAbsoluteBounds\n");
#endif
  if (mWidget)
  {
    if (mWidget->window)
    {
      gdk_window_get_origin(mWidget->window, &x, &y);
      aRect.x = x;
      aRect.y = y;
#ifdef DEBUG_pavlov
      g_print("  x = %i, y = %i\n", x, y);
#endif
    }
    else
      return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP nsWidget::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  gint x;
  gint y;

#ifdef DEBUG_pavlov
  g_print("nsWidget::WidgetToScreen\n");
#endif

  if (mWidget)
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

NS_IMETHODIMP nsWidget::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
#ifdef DEBUG_pavlov
    g_print("nsWidget::ScreenToWidget\n");
#endif
    NS_NOTYETIMPLEMENTED("nsWidget::ScreenToWidget");
    return NS_OK;
}

#ifdef DEBUG
void
nsWidget::IndentByDepth(FILE* out)
{
  PRInt32 depth = 0;
  nsWidget* parent = (nsWidget*)mParent;
  while (parent) {
    parent = (nsWidget*) parent->mParent;
    depth++;
  }
  while (--depth >= 0) fprintf(out, "  ");
}
#endif

//-------------------------------------------------------------------------
//
// Close this nsWidget
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::Destroy(void)
{
#ifdef NOISY_DESTROY
  IndentByDepth(stdout);
  printf("nsWidget::Destroy:%p: isDestroying=%s widget=%p parent=%p\n",
         this, mIsDestroying ? "yes" : "no", mWidget, mParent);
#endif
  GtkAllocation *old_size = NULL;
  if (!mIsDestroying) {
    nsBaseWidget::Destroy();
    NS_IF_RELEASE(mParent);
  }
  if (mWidget) {
    // see if we need to destroy the old size information
    old_size = (GtkAllocation *) gtk_object_get_data(GTK_OBJECT(mWidget), "mozilla.old_size");
    if (old_size) {
      g_free(old_size);
    }
    // prevent the widget from causing additional events
    mEventCallback = nsnull;
    ::gtk_widget_destroy(mWidget);
    mWidget = nsnull;
    if (PR_FALSE == mOnDestroyCalled)
      OnDestroy();
  }
  return NS_OK;
}

// make sure that we clean up here

void nsWidget::OnDestroy()
{
  mOnDestroyCalled = PR_TRUE;
  // release references to children, device context, toolkit + app shell
  nsBaseWidget::OnDestroy();
  // dispatch the event
  if (!mIsDestroying) {
    // dispatching of the event may cause the reference count to drop
    // to 0 and result in this object being destroyed. To avoid that,
    // add a reference and then release it after dispatching the event
    nsrefcnt old = mRefCnt;
    mRefCnt = 99;
    DispatchStandardEvent(NS_DESTROY);
    mRefCnt = old;
  }
}

gint
nsWidget::DestroySignal(GtkWidget* aGtkWidget, nsWidget* aWidget)
{
  aWidget->OnDestroySignal(aGtkWidget);
  return PR_TRUE;
}

void
nsWidget::OnDestroySignal(GtkWidget* aGtkWidget)
{
  if (aGtkWidget == mWidget) {
    mWidget = nsnull;
  }
}

//-------------------------------------------------------------------------
//
// Get this nsWidget parent
//
//-------------------------------------------------------------------------

nsIWidget* nsWidget::GetParent(void)
{
  if (nsnull != mParent) {
    NS_ADDREF(mParent);
  }
  return mParent;
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::Show(PRBool bState)
{
  if (!mWidget)
    return NS_OK; // Will be null durring printing

  if (bState)
    gtk_widget_show(mWidget);
  else
    gtk_widget_hide(mWidget);

  mShown = bState;

  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetModal(void)
{
	GtkWindow *toplevel;

	if (!mWidget)
		return NS_ERROR_FAILURE;

	toplevel = GTK_WINDOW(gtk_widget_get_toplevel(mWidget));

	if (!toplevel)
		return NS_ERROR_FAILURE;
	
  gtk_window_set_modal(toplevel, PR_TRUE);

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// grab mouse events for this widget
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWidget::CaptureMouse(PRBool aCapture)
{
  if (aCapture)
  {
    mGrabTime = gdk_time_get();

    gdk_pointer_grab (mWidget->window, PR_TRUE,(GdkEventMask)
                      (GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                      GDK_ENTER_NOTIFY_MASK | GDK_LEAVE_NOTIFY_MASK |
                      GDK_POINTER_MOTION_MASK),
                      mWidget->window, NULL, GDK_CURRENT_TIME);
    //    gtk_grab_add(mWidget);
  }
  else
  {
    gdk_pointer_ungrab(mGrabTime);
    //    gtk_grab_remove(mWidget);
  }

  return NS_OK;
}


NS_IMETHODIMP nsWidget::IsVisible(PRBool &aState)
{
  if (mWidget)
    aState = GTK_WIDGET_VISIBLE(mWidget);
  else
    aState = PR_FALSE;

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::Move(PRInt32 aX, PRInt32 aY)
{
  if (mWidget) 
  {
    GtkWidget *    layout = mWidget->parent;

    GtkAdjustment* ha = gtk_layout_get_hadjustment(GTK_LAYOUT(layout));
    GtkAdjustment* va = gtk_layout_get_vadjustment(GTK_LAYOUT(layout));

    // This correction is needed because the view manager code in
    // gecko assumes that the implementation of scrolling happens
    // only in one window (as is the case in win32 and mac).  The
    // GtkLayout widget uses 2 windows to do arbitrarily long scrolling
    // (beyond the 16 bit dimension hard limit of X windows)
    //
    // The first window is the base.
    // 
    // The second window is a clip window (called the bin_window).
    //
    // The position of the bin_window is controlled by 2 GtkAdjustment
    // data structures.
    // 
    // What happens is that the view manager computes offsets for 
    // widgets from the viewport's origin.  
    //
    // The GtkLayout widget scrolls the bin_window (which is the true
    // parent window of the widgets we are trying to Move) from
    // its own origin.
    // 
    // So, the widgets end up being positioned off by the amount of
    // offset between the viewport's origin, and the position of
    // the GtkLayout's clip window and hence the correction...
    //
    // Simple...
    PRInt32        x_correction = (PRInt32) ha->value;
    PRInt32        y_correction = (PRInt32) va->value;
    
    gtk_layout_move(GTK_LAYOUT(layout), 
                    mWidget, 
                    aX + x_correction, 
                    aY + y_correction);
  }

  return NS_OK;
}

NS_IMETHODIMP nsWidget::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
#if 0
  printf("nsWidget::Resize %s (%p) to %d %d\n",
         mWidget ? gtk_widget_get_name(mWidget) : "(no-widget)", this,
         aWidth, aHeight);
#endif
  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  if (mWidget)
    gtk_widget_set_usize(mWidget, aWidth, aHeight);

  return NS_OK;
}

NS_IMETHODIMP nsWidget::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth,
                           PRInt32 aHeight, PRBool aRepaint)
{
  Move(aX, aY);
  Resize(aWidth,aHeight,aRepaint);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWidget::OnResize(nsSizeEvent event)
{
  return DispatchWindowEvent(&event);
}


PRBool nsWidget::OnResize(nsRect &aRect)
{
  nsSizeEvent event;

  InitEvent(event, NS_SIZE);
  event.eventStructType = NS_SIZE_EVENT;

  nsRect *foo = new nsRect(0, 0, aRect.width, aRect.height);
  event.windowSize = foo;

  event.point.x = 0;
  event.point.y = 0;
  event.mWinWidth = aRect.width;
  event.mWinHeight = aRect.height;
  
  NS_ADDREF_THIS();
  PRBool result = OnResize(event);
  NS_RELEASE_THIS();

  return result;
}


//------
// Move
//------
PRBool nsWidget::OnMove(PRInt32 aX, PRInt32 aY)
{
    nsGUIEvent event;
#if 0
    printf("nsWidget::OnMove %s (%p)\n",
           mWidget ? gtk_widget_get_name(mWidget) : "(no-widget)",
           this);
#endif
    InitEvent(event, NS_MOVE);
    event.point.x = aX;
    event.point.y = aY;
    event.eventStructType = NS_GUI_EVENT;
    PRBool result = DispatchWindowEvent(&event);
    return result;
}

//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWidget::Enable(PRBool bState)
{
  if (mWidget)
   gtk_widget_set_sensitive(mWidget, bState);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWidget::SetFocus(void)
{
  if (mWidget)
    gtk_widget_grab_focus(mWidget);

  return NS_OK;
}

NS_IMETHODIMP nsWidget::GetBounds(nsRect &aRect)
{
  aRect = mBounds;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics *nsWidget::GetFont(void)
{
  NS_NOTYETIMPLEMENTED("nsWidget::GetFont");
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWidget::SetFont(const nsFont &aFont)
{
    nsIFontMetrics* mFontMetrics;
    mContext->GetMetricsFor(aFont, mFontMetrics);

    if (mFontMetrics) {
        nsFontHandle  fontHandle;
        mFontMetrics->GetFontHandle(fontHandle);
        // FIXME avoid fontset problems....
        if (((GdkFont*)fontHandle)->type == GDK_FONT_FONTSET)
        {
            g_print("nsWidget:SetFont - got a FontSet.. ignoring\n");
            NS_RELEASE(mFontMetrics);
            return NS_ERROR_FAILURE;
        }

        if (mWidget) 
          SetFontNative((GdkFont *)fontHandle);
    }
    NS_RELEASE(mFontMetrics);
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::SetBackgroundColor(const nscolor &aColor)
{
  nsBaseWidget::SetBackgroundColor(aColor);

  if (nsnull != mWidget) {
    GdkColor color_nor, color_bri, color_dark;

    NSCOLOR_TO_GDKCOLOR(aColor, color_nor);
    NSCOLOR_TO_GDKCOLOR(NS_BrightenColor(aColor), color_bri);
    NSCOLOR_TO_GDKCOLOR(NS_DarkenColor(aColor), color_dark);

    //    gdk_color.red = 256 * NS_GET_R(aColor);
    // gdk_color.green = 256 * NS_GET_G(aColor);
    // gdk_color.blue = 256 * NS_GET_B(aColor);
    // gdk_color.pixel ?

    // calls virtual native set color
    SetBackgroundColorNative(&color_nor, &color_bri, &color_dark);

#if 0
    GtkStyle *style = gtk_style_copy(mWidget->style);
  
    style->bg[GTK_STATE_NORMAL]=gdk_color;
    // other states too? (GTK_STATE_ACTIVE, GTK_STATE_PRELIGHT,
    //               GTK_STATE_SELECTED, GTK_STATE_INSENSITIVE)
    gtk_widget_set_style(mWidget, style);
    gtk_style_unref(style);
#endif
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWidget::SetCursor(nsCursor aCursor)
{
  if (!mWidget || !mWidget->window)
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
      ::gdk_window_set_cursor(mWidget->window, newCursor);
      ::gdk_cursor_destroy(newCursor);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Invalidate(PRBool aIsSynchronous)
{
  if (mWidget == nsnull)
    return NS_OK; // mWidget will be null during printing. 

  if (!GTK_IS_WIDGET (mWidget))
    return NS_ERROR_FAILURE;

  if (!GTK_WIDGET_REALIZED (GTK_WIDGET(mWidget)))
    return NS_ERROR_FAILURE;

  if (aIsSynchronous) {
    ::gtk_widget_draw(mWidget, NULL);
    mUpdateArea.SetRect(0, 0, 0, 0);
  } else {
    ::gtk_widget_queue_draw(mWidget);
    mUpdateArea.SetRect(0, 0, mBounds.width, mBounds.height);
  }

  return NS_OK;
}

NS_IMETHODIMP nsWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  if (mWidget)
    return NS_OK;  // mWidget is null during printing

  if (!GTK_IS_WIDGET (mWidget))
    return NS_ERROR_FAILURE;

  if (!GTK_WIDGET_REALIZED (GTK_WIDGET(mWidget)))
    return NS_ERROR_FAILURE;


  if (aIsSynchronous)
  {
    GdkRectangle nRect;
    NSRECT_TO_GDKRECT(aRect, nRect);
    gtk_widget_draw(mWidget, &nRect);
  }
  else
  {
    mUpdateArea.UnionRect(mUpdateArea, aRect);
    gtk_widget_queue_draw_area(mWidget,
                               aRect.x, aRect.y,
                               aRect.width, aRect.height);
  }

  return NS_OK;
}

NS_IMETHODIMP nsWidget::Update(void)
{
  if (! mWidget)
    return NS_OK;

  if (mUpdateArea.width && mUpdateArea.height) {
    if (!mIsDestroying) {
      Invalidate(mUpdateArea, PR_TRUE);

      mUpdateArea.SetRect(0, 0, 0, 0);
      return NS_OK;
    }
    else {
      return NS_ERROR_FAILURE;
    }
  }
  else {
    //  g_print("nsWidget::Update(this=%p): avoided update of empty area\n", this);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void *nsWidget::GetNativeData(PRUint32 aDataType)
{
  switch(aDataType) {
  case NS_NATIVE_WINDOW:
    if (mWidget) {
      return (void *)mWidget->window;
    }
    break;

  case NS_NATIVE_DISPLAY:
    return (void *)GDK_DISPLAY();

  case NS_NATIVE_WIDGET:
  case NS_NATIVE_PLUGIN_PORT:
    if (mWidget) {
      return (void *)mWidget;
    }
    break;

  case NS_NATIVE_GRAPHIC:
    /* GetSharedGC ups the ref count on the GdkGC so make sure you release
     * it afterwards. */
    return (void *)((nsToolkit *)mToolkit)->GetSharedGC();

  default:
    g_print("nsWidget::GetNativeData(%i) - weird value\n", aDataType);
    break;
  }
  return nsnull;
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWidget::SetColorMap(nsColorMap *aColorMap)
{
  return NS_OK;
}

NS_IMETHODIMP nsWidget::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  NS_NOTYETIMPLEMENTED("nsWidget::Scroll");
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

NS_IMETHODIMP nsWidget::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth  = mPreferredWidth;
  aHeight = mPreferredHeight;
  return (mPreferredWidth != 0 && mPreferredHeight != 0)?NS_OK:NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWidget::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetMenuBar(nsIMenuBar * aMenuBar)
{
  g_print("bleh\n");
  NS_NOTYETIMPLEMENTED("nsWidget::SetMenuBar");
  return NS_OK;
}

NS_IMETHODIMP nsWidget::ShowMenuBar(PRBool aShow)
{
  g_print("bleh\n");
  NS_NOTYETIMPLEMENTED("nsWidget::ShowMenuBar");
  return NS_OK;
}

nsresult nsWidget::CreateWidget(nsIWidget *aParent,
                                const nsRect &aRect,
                                EVENT_CALLBACK aHandleEventFunction,
                                nsIDeviceContext *aContext,
                                nsIAppShell *aAppShell,
                                nsIToolkit *aToolkit,
                                nsWidgetInitData *aInitData,
                                nsNativeWidget aNativeParent)
{
  GtkWidget *parentWidget = nsnull;

#ifdef NOISY_DESTROY
  if (aParent)
    g_print("nsWidget::CreateWidget (%p) nsIWidget parent\n",
            this);
  else if (aNativeParent)
    g_print("nsWidget::CreateWidget (%p) native parent\n",
            this);
  else if(aAppShell)
    g_print("nsWidget::CreateWidget (%p) nsAppShell parent\n",
            this);
#endif

  gtk_widget_push_colormap(gdk_rgb_get_cmap());
  gtk_widget_push_visual(gdk_rgb_get_visual());

  nsIWidget *baseParent = aInitData &&
    (aInitData->mWindowType == eWindowType_dialog ||
     aInitData->mWindowType == eWindowType_toplevel) ?
    nsnull : aParent;
  BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);
  mParent = aParent;
  NS_IF_ADDREF(mParent);

  if (aNativeParent) {
    parentWidget = GTK_WIDGET(aNativeParent);
  } else if (aParent) {
    // this ups the refcount of the gtk widget, we must unref later.
    parentWidget = GTK_WIDGET(aParent->GetNativeData(NS_NATIVE_WIDGET));
  } else if(aAppShell) {
    nsNativeWidget shellWidget = aAppShell->GetNativeData(NS_NATIVE_SHELL);
    if (shellWidget)
      parentWidget = GTK_WIDGET(shellWidget);
  }

  mBounds = aRect;
  CreateNative (parentWidget);

  Resize(aRect.width, aRect.height, PR_FALSE);

  /* place the widget in its parent if it isn't a toplevel window*/
  if (mIsToplevel)
  {
    if (parentWidget)
    {
      // set transient properties
    }
  }
  else
  {
    if (parentWidget)
    {
      gtk_layout_put(GTK_LAYOUT(parentWidget), mWidget, aRect.x, aRect.y);
    }
  }

  gtk_widget_pop_colormap();
  gtk_widget_pop_visual();


  InstallButtonPressSignal(mWidget);
  InstallButtonReleaseSignal(mWidget);

  InstallMotionNotifySignal(mWidget);

  InstallEnterNotifySignal(mWidget);
  InstallLeaveNotifySignal(mWidget);

  // Initialize this window instance as a drag target.
  gtk_drag_dest_set (mWidget,
                     GTK_DEST_DEFAULT_ALL,
                     target_table, n_targets - 1, /* no rootwin */
                     GdkDragAction(GDK_ACTION_COPY | GDK_ACTION_MOVE));


  // Drag & Drop events.
  InstallDragBeginSignal(mWidget);
  InstallDragLeaveSignal(mWidget);
  InstallDragMotionSignal(mWidget);
  InstallDragDropSignal(mWidget);


  // Focus
  InstallFocusInSignal(mWidget);
  InstallFocusOutSignal(mWidget);



  DispatchStandardEvent(NS_CREATE);
  InitCallbacks();

  // Add in destroy callback
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "destroy",
                     GTK_SIGNAL_FUNC(DestroySignal),
                     this);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// create with nsIWidget parent
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::Create(nsIWidget *aParent,
                           const nsRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
  return CreateWidget(aParent, aRect, aHandleEventFunction,
                      aContext, aAppShell, aToolkit, aInitData,
                      nsnull);
}

//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWidget::Create(nsNativeWidget aParent,
                           const nsRect &aRect,
                           EVENT_CALLBACK aHandleEventFunction,
                           nsIDeviceContext *aContext,
                           nsIAppShell *aAppShell,
                           nsIToolkit *aToolkit,
                           nsWidgetInitData *aInitData)
{
  return CreateWidget(nsnull, aRect, aHandleEventFunction,
                      aContext, aAppShell, aToolkit, aInitData,
                      aParent);
}

//-------------------------------------------------------------------------
//
// Initialize all the Callbacks
//
//-------------------------------------------------------------------------
void nsWidget::InitCallbacks(char *aName)
{
}

void nsWidget::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{

}

void nsWidget::InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint)
{
  event.widget = this;

  GdkEventConfigure *ge;
  ge = (GdkEventConfigure*)gtk_get_current_event();

  if (aPoint == nsnull) {     // use the point from the event
    // get the message position in client coordinates and in twips

    if (ge != nsnull) {
      //       ::ScreenToClient(mWnd, &cpos);
      event.point.x = PRInt32(ge->x);
      event.point.y = PRInt32(ge->y);
    } else { 
      event.point.x = 0;
      event.point.y = 0;
    }  
  }    
  else {                      // use the point override if provided
    event.point.x = aPoint->x;
    event.point.y = aPoint->y;
  }

  event.time = gdk_event_get_time((GdkEvent*)ge);
  event.message = aEventType;

  //    mLastPoint.x = event.point.x;
  //    mLastPoint.y = event.point.y;
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
  return PR_FALSE;
}

PRBool nsWidget::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWidget::DispatchStandardEvent(PRUint32 aMsg)
{
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  InitEvent(event, aMsg);
  PRBool result = DispatchWindowEvent(&event);
  return result;
}

PRBool nsWidget::DispatchFocus(nsGUIEvent &aEvent)
{
  if (mEventCallback)
    return DispatchWindowEvent(&aEvent);

  return PR_FALSE;
}

//////////////////////////////////////////////////////////////////
//
// OnSomething handlers
//
//////////////////////////////////////////////////////////////////

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

#ifdef DEBUG_pavlov
#define EVENT_SPAM
#endif

#if defined(EVENT_SPAM)
#define TRACE_EVENTS 1
#define TRACE_EVENTS_MOTION 1
#define TRACE_EVENTS_PAINT 1
#define TRACE_EVENTS_CROSSING 1
#endif

#ifdef DEBUG
void
nsWidget::DebugPrintEvent(nsGUIEvent &   aEvent,
                          GtkWidget *    aGtkWidget)
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

  printf("%4d %-26s(this=%-8p , name=%-12s",
         sPrintCount++,
         (const char *) nsAutoCString(GuiEventToString(aEvent)),
         this,
         aGtkWidget ? gtk_widget_get_name(aGtkWidget) : "null");
         
  Window win = 0;
  
  if (aGtkWidget && GTK_WIDGET_REALIZED(aGtkWidget))
  {
    win = GDK_WINDOW_XWINDOW(aGtkWidget->window);
  }
  
  printf(" , xid=%-8p",(void *) win);
  
  printf(" , x=%-3d, y=%d)",aEvent.point.x,aEvent.point.y);

  printf("\n");
}
#endif // DEBUG
//////////////////////////////////////////////////////////////////

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::DispatchEvent(nsGUIEvent *event,
                                      nsEventStatus &aStatus)
{
  NS_ADDREF(event->widget);

#ifdef TRACE_EVENTS
  DebugPrintEvent(*event,mWidget);
#endif

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


//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWidget::DispatchMouseEvent(nsMouseEvent& aEvent)
{
  PRBool result = PR_FALSE;
  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  // call the event callback
  if (nsnull != mEventCallback) {
    result = DispatchWindowEvent(&aEvent);

    return result;
  }

  if (nsnull != mMouseListener) {
    switch (aEvent.message) {
      case NS_MOUSE_MOVE: {
//         result = ConvertStatus(mMouseListener->MouseMoved(aEvent));
//         nsRect rect;
//         GetBounds(rect);
//         if (rect.Contains(event.point.x, event.point.y)) {
//           if (mCurrentWindow == NULL || mCurrentWindow != this) {
//             printf("Mouse enter");
//             mCurrentWindow = this;
//           }
//         } else {
//           printf("Mouse exit");
//         }

      } break;

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

    case NS_DRAGDROP_DROP:
      printf("nsWidget::DispatchMouseEvent, NS_DRAGDROP_DROP\n");
      break;

    default:
      break;

    } // switch
  }
  return result;
}

//////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////
//
// GTK signal installers
//
//////////////////////////////////////////////////////////////////
void
nsWidget::AddToEventMask(GtkWidget * aWidget,
                         gint        aEventMask)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( 0 != aEventMask, "mask is 0");

  gtk_widget_add_events(aWidget,aEventMask);
}
//////////////////////////////////////////////////////////////////
void 
nsWidget::InstallMotionNotifySignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
				"motion_notify_event",
				GTK_SIGNAL_FUNC(nsWidget::MotionNotifySignal));
}

void 
nsWidget::InstallDragLeaveSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
                "drag_leave",
                GTK_SIGNAL_FUNC(nsWidget::DragLeaveSignal));
}

void 
nsWidget::InstallDragMotionSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
                "drag_motion",
                GTK_SIGNAL_FUNC(nsWidget::DragMotionSignal));
}

void 
nsWidget::InstallDragBeginSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
                "drag_begin",
                GTK_SIGNAL_FUNC(nsWidget::DragBeginSignal));
}

void 
nsWidget::InstallDragDropSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
                "drag_drop",
                GTK_SIGNAL_FUNC(nsWidget::DragDropSignal));
}

//////////////////////////////////////////////////////////////////
void 
nsWidget::InstallEnterNotifySignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
				"enter_notify_event",
				GTK_SIGNAL_FUNC(nsWidget::EnterNotifySignal));
}
//////////////////////////////////////////////////////////////////
void 
nsWidget::InstallLeaveNotifySignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
				"leave_notify_event",
				GTK_SIGNAL_FUNC(nsWidget::LeaveNotifySignal));
}
//////////////////////////////////////////////////////////////////
void 
nsWidget::InstallButtonPressSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
				"button_press_event",
				GTK_SIGNAL_FUNC(nsWidget::ButtonPressSignal));
}
//////////////////////////////////////////////////////////////////
void 
nsWidget::InstallButtonReleaseSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
				"button_release_event",
				GTK_SIGNAL_FUNC(nsWidget::ButtonReleaseSignal));
}
//////////////////////////////////////////////////////////////////
void 
nsWidget::InstallFocusInSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
				"focus_in_event",
				GTK_SIGNAL_FUNC(nsWidget::FocusInSignal));
}
//////////////////////////////////////////////////////////////////
void 
nsWidget::InstallFocusOutSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");

  InstallSignal(aWidget,
				"focus_out_event",
				GTK_SIGNAL_FUNC(nsWidget::FocusOutSignal));
}
//////////////////////////////////////////////////////////////////
void 
nsWidget::InstallRealizeSignal(GtkWidget * aWidget)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");
  
  InstallSignal(aWidget,
				"realize",
				GTK_SIGNAL_FUNC(nsWidget::RealizeSignal));
}
//////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////
/* virtual */ void 
nsWidget::OnMotionNotifySignal(GdkEventMotion * aGdkMotionEvent)
{
  nsMouseEvent event;

  event.message = NS_MOUSE_MOVE;
  event.eventStructType = NS_MOUSE_EVENT;

  // If there is a button motion target, use that instead of the
  // current widget

  // XXX pav
  // i'm confused as to wtf this sButtonMoetionTarget thing is for.
  // so i'm not going to use it.

  // XXX ramiro
  // 
  // Because of dynamic widget creation and destruction, this could
  // potentially be a dangerious thing to do.  
  //
  // If the sButtonMotionTarget is destroyed between the time when
  // it got set and now, we should end up sending an event to 
  // a junk nsWidget.
  //
  // The way to solve this would be to add a destroy signal to
  // the GtkWidget corresponding to the sButtonMotionTarget and
  // marking if nsnull in there.
  //
  gint x, y;

  if (aGdkMotionEvent)
  {
    x = (gint) aGdkMotionEvent->x;
    y = (gint) aGdkMotionEvent->y;
 
    gdk_window_get_pointer(aGdkMotionEvent->window, &x, &y, nsnull);

    event.point.x = nscoord(x);
    event.point.y = nscoord(y);

    event.widget = this;
  }

#if 0
  if (nsnull != sButtonMotionTarget)
  {
    gint diffX;
    gint diffY;

    if (aGdkMotionEvent != NULL) 
    {
      // Compute the difference between the original root coordinates
      diffX = (gint) aGdkMotionEvent->x_root - sButtonMotionRootX;
      diffY = (gint) aGdkMotionEvent->y_root - sButtonMotionRootY;
      
      event.widget = sButtonMotionTarget;
      
      // The event coords will be the initial *widget* coords plus the 
      // root difference computed above.
      event.point.x = nscoord(sButtonMotionWidgetX + diffX);
      event.point.y = nscoord(sButtonMotionWidgetY + diffY);
    }
  }
  else
  {
    event.widget = this;

    if (aGdkMotionEvent != NULL) 
    {
      event.point.x = nscoord(aGdkMotionEvent->x);
      event.point.y = nscoord(aGdkMotionEvent->y);
    }
  }

  if (aGdkMotionEvent != NULL) 
  {
    event.time = aGdkMotionEvent->time;
  }
#endif

  AddRef();

  DispatchMouseEvent(event);

  Release();
}

/* virtual */ void 
nsWidget::OnDragMotionSignal(GdkDragContext *aGdkDragContext,
                             gint            x,
                             gint            y,
                             guint           time)
{
  if (!mIsDragDest)
  {
    // this will happen on the first motion event, so we will generate an ENTER event
    OnDragEnterSignal(aGdkDragContext, x, y, time);
  }


  GtkWidget *source_widget;
  source_widget = gtk_drag_get_source_widget (aGdkDragContext);
  g_print("motion, source %s\n", source_widget ?
	    gtk_type_name (GTK_OBJECT (source_widget)->klass->type) :
	    "unknown");

  gdk_drag_status (aGdkDragContext, aGdkDragContext->suggested_action, time);



  nsMouseEvent event;

  event.message = NS_DRAGDROP_OVER;
  event.eventStructType = NS_DRAGDROP_EVENT;

  event.widget = this;

  event.point.x = x;
  event.point.y = y;

  AddRef();

  DispatchMouseEvent(event);

  Release();
}

/* not a real signal.. called from OnDragMotionSignal */
/* virtual */ void 
nsWidget::OnDragEnterSignal(GdkDragContext *aGdkDragContext,
                             gint            x,
                             gint            y,
                             guint           time)
{
  // we are a drag dest.. cool huh?
  mIsDragDest = PR_TRUE;

  nsMouseEvent event;

  event.message = NS_DRAGDROP_ENTER;
  event.eventStructType = NS_DRAGDROP_EVENT;

  event.widget = this;

  event.point.x = x;
  event.point.y = y;

  AddRef();

  DispatchMouseEvent(event);

  Release();
}

/* virtual */ void 
nsWidget::OnDragLeaveSignal(GdkDragContext   *context,
                            guint             time)
{
  mIsDragDest = PR_FALSE;


  nsMouseEvent event;

  event.message = NS_DRAGDROP_EXIT;
  event.eventStructType = NS_DRAGDROP_EVENT;

  event.widget = this;

  //  GdkEvent *current_event;
  //  current_event = gtk_get_current_event();

  //  g_print("current event's x_root = %i , y_root = %i\n", current_event->dnd.x_root, current_event->dnd.y_root);

  // FIXME
  event.point.x = 0;
  event.point.y = 0;

  AddRef();

  DispatchMouseEvent(event);

  Release();
}

/* virtual */ void 
nsWidget::OnDragBeginSignal(GdkDragContext * aGdkDragContext)
{
  nsMouseEvent event;

  event.message = NS_MOUSE_MOVE;
  event.eventStructType = NS_MOUSE_EVENT;
  
  AddRef();

  DispatchMouseEvent(event);

  Release();
}


/* virtual */ void 
nsWidget::OnDragDropSignal(GdkDragContext *aDragContext,
                           gint            x,
                           gint            y,
                           guint           time)
{
  nsMouseEvent    event;

  event.message = NS_DRAGDROP_DROP;
  event.eventStructType = NS_DRAGDROP_EVENT;
  event.widget = this;
  
  // Test coordinates.
  event.point.x = 17;
  event.point.y = 19;

  AddRef();

  DispatchWindowEvent(&event);

  Release();
}


//////////////////////////////////////////////////////////////////
/* virtual */ void
nsWidget::OnEnterNotifySignal(GdkEventCrossing * aGdkCrossingEvent)
{
  // If there is a button motion target, then we can ignore this
  // event since what the gecko event system expects is for
  // only motion events to be sent to that widget, even if the
  // pointer is crossing on other widgets.
  //
  // XXX ramiro - Same as above.
  //
  if (nsnull != sButtonMotionTarget)
  {
    return;
  }

  nsMouseEvent event;

  event.message = NS_MOUSE_ENTER;
  event.widget  = this;
  event.eventStructType = NS_MOUSE_EVENT;

  if (aGdkCrossingEvent != NULL) 
  {
    event.point.x = nscoord(aGdkCrossingEvent->x);
    event.point.y = nscoord(aGdkCrossingEvent->y);
    event.time = aGdkCrossingEvent->time;
  }

  AddRef();

  DispatchMouseEvent(event);

  Release();
}
//////////////////////////////////////////////////////////////////////
/* virtual */ void
nsWidget::OnLeaveNotifySignal(GdkEventCrossing * aGdkCrossingEvent)
{
  // If there is a button motion target, then we can ignore this
  // event since what the gecko event system expects is for
  // only motion events to be sent to that widget, even if the
  // pointer is crossing on other widgets.
  //
  // XXX ramiro - Same as above.
  //
  if (nsnull != sButtonMotionTarget)
  {
    return;
  }

  nsMouseEvent event;

  event.message = NS_MOUSE_EXIT;
  event.widget  = this;
  event.eventStructType = NS_MOUSE_EVENT;

  if (aGdkCrossingEvent != NULL) 
  {
    event.point.x = nscoord(aGdkCrossingEvent->x);
    event.point.y = nscoord(aGdkCrossingEvent->y);
    event.time = aGdkCrossingEvent->time;
  }

  AddRef();

  DispatchMouseEvent(event);

  Release();
}
//////////////////////////////////////////////////////////////////////
/* virtual */ void
nsWidget::OnButtonPressSignal(GdkEventButton * aGdkButtonEvent)
{
  nsMouseEvent event;
  PRUint32 eventType = 0;

  // Switch on single, double, triple click.
  switch (aGdkButtonEvent->type) 
  {
    // Single click.
  case GDK_BUTTON_PRESS:

    switch (aGdkButtonEvent->button)  // Which button?
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

      // Single-click default.
    default:
      eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
      break;
    }
    break;

    // Double click.
  case GDK_2BUTTON_PRESS:

    switch (aGdkButtonEvent->button)  // Which button?
    {
    case 1:
      eventType = NS_MOUSE_LEFT_DOUBLECLICK;
      break;
      
    case 2:
      eventType = NS_MOUSE_MIDDLE_DOUBLECLICK;
      break;

    case 3:
      eventType = NS_MOUSE_RIGHT_DOUBLECLICK;
      break;

    default:
      // Double-click default.
      eventType = NS_MOUSE_LEFT_DOUBLECLICK;
      break;
    }
    break;

    // Triple click.
  case GDK_3BUTTON_PRESS:
    // Unhandled triple click.
    break;
	
  default:
    break;
  }

  InitMouseEvent(aGdkButtonEvent, event, eventType);

  // Set the button motion target and remeber the widget and root coords
  sButtonMotionTarget = this;

  sButtonMotionRootX = (gint) aGdkButtonEvent->x_root;
  sButtonMotionRootY = (gint) aGdkButtonEvent->y_root;

  sButtonMotionWidgetX = (gint) aGdkButtonEvent->x;
  sButtonMotionWidgetY = (gint) aGdkButtonEvent->y;
  
  AddRef();

  DispatchMouseEvent(event);

  Release();

}
//////////////////////////////////////////////////////////////////////
/* virtual */ void
nsWidget::OnButtonReleaseSignal(GdkEventButton * aGdkButtonEvent)
{
  nsMouseEvent event;
  PRUint32 eventType = 0;

  switch (aGdkButtonEvent->button)
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

  InitMouseEvent(aGdkButtonEvent, event, eventType);

  if (sButtonMotionTarget)
  {
    sButtonMotionTarget = nsnull;

    sButtonMotionRootX = -1;
    sButtonMotionRootY = -1;
  }

  AddRef();

  DispatchMouseEvent(event);

  Release();
}
//////////////////////////////////////////////////////////////////////
/* virtual */ void
nsWidget::OnFocusInSignal(GdkEventFocus * aGdkFocusEvent)
{
  if (mIsDestroying)
    return;

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


#ifdef USE_XIM
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

#endif // USE_XIM

}
//////////////////////////////////////////////////////////////////////
/* virtual */ void
nsWidget::OnFocusOutSignal(GdkEventFocus * aGdkFocusEvent)
{
  if (mIsDestroying)
    return;

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


#ifdef USE_XIM

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

#endif

}
//////////////////////////////////////////////////////////////////////
/* virtual */ void
nsWidget::OnRealize()
{
  //  printf("nsWidget::OnRealize(%p)\n",this);
}
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//
// GTK event support methods
//
//////////////////////////////////////////////////////////////////
void 
nsWidget::InstallSignal(GtkWidget *   aWidget,
                        gchar *       aSignalName,
                        GtkSignalFunc aSignalFunction)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( aSignalName, "signal name is null");
  NS_ASSERTION( aSignalFunction, "signal function is null");

  gtk_signal_connect(GTK_OBJECT(aWidget),
                     aSignalName,
                     GTK_SIGNAL_FUNC(aSignalFunction),
                     (gpointer) this);
}
//////////////////////////////////////////////////////////////////
void 
nsWidget::InitMouseEvent(GdkEventButton * aGdkButtonEvent,
						 nsMouseEvent &anEvent,
						 PRUint32   aEventType)
{
  anEvent.message = aEventType;
  anEvent.widget  = this;

  anEvent.eventStructType = NS_MOUSE_EVENT;

  if (aGdkButtonEvent != NULL) {
    anEvent.point.x = nscoord(aGdkButtonEvent->x);
    anEvent.point.y = nscoord(aGdkButtonEvent->y);

    anEvent.isShift = (aGdkButtonEvent->state & GDK_SHIFT_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isControl = (aGdkButtonEvent->state & GDK_CONTROL_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.isAlt = (aGdkButtonEvent->state & GDK_MOD1_MASK) ? PR_TRUE : PR_FALSE;
    anEvent.time = aGdkButtonEvent->time;

    switch(aGdkButtonEvent->type)
      {
      case GDK_BUTTON_PRESS:
        anEvent.clickCount = 1;
        break;
      case GDK_2BUTTON_PRESS:
        anEvent.clickCount = 2;
        break;
      case GDK_3BUTTON_PRESS:  /* Clamp to double-click */
        anEvent.clickCount = 2;
        break;
      default:
        anEvent.clickCount = 1;
    }

  }
}
//////////////////////////////////////////////////////////////////
PRBool
nsWidget::DropEvent(GtkWidget * aWidget, 
                    GdkWindow * aEventWindow) 
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( nsnull != aEventWindow, "event window is null");

#if 0
  static int count = 0;

  if (GTK_IS_LAYOUT(aWidget))
  {
    GtkLayout * layout = GTK_LAYOUT(aWidget);

    printf("%4d DropEvent(this=%p,widget=%p,event_win=%p,wid_win=%p,bin_win=%p)\n",
           count++,
           this,
           aWidget,
           aEventWindow,
           aWidget->window,
           layout->bin_window);
  }
  else
  {
    printf("%4d DropEvent(this=%p,widget=%p,event_win=%p,wid_win=%p)\n",
           count++,
           this,
           aWidget,
           aEventWindow,
           aWidget->window);
  }
#endif


  // For gtklayout widgets, we dont want to handle events
  // that occur in the sub windows.  Check the window member
  // of the GdkEvent, if it is not the gtklayout's bin_window,
  // drop the event.
  if (GTK_IS_LAYOUT(aWidget))
  {
    GtkLayout * layout = GTK_LAYOUT(aWidget);

    if (aEventWindow != layout->bin_window)
    {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//
// GTK widget signals
//
//////////////////////////////////////////////////////////////////
/* static */ gint
nsWidget::MotionNotifySignal(GtkWidget *      aWidget,
							 GdkEventMotion * aGdkMotionEvent,
							 gpointer         aData)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( nsnull != aGdkMotionEvent, "event is null");

  nsWidget * widget = (nsWidget *) aData;

  NS_ASSERTION( nsnull != widget, "instance pointer is null");

  if (widget->DropEvent(aWidget, aGdkMotionEvent->window))
  {
    return PR_TRUE;
  }

  widget->OnMotionNotifySignal(aGdkMotionEvent);

  return PR_TRUE;
}

/* static */ gint
nsWidget::DragMotionSignal(GtkWidget *      aWidget,
                           GdkDragContext   *aDragContext,
                           gint             x,
                           gint             y,
                           guint            time,
                           void             *aData)
{
  nsWidget * widget = (nsWidget *) aData;
  NS_ASSERTION( nsnull != widget, "instance pointer is null");

  widget->OnDragMotionSignal(aDragContext, x, y, time);

  return PR_TRUE;
}

/* static */ void
nsWidget::DragLeaveSignal(GtkWidget *      aWidget,
                          GdkDragContext   *aDragContext,
                          guint            time,
                          void             *aData)
{
  nsWidget * widget = (nsWidget *) aData;
  NS_ASSERTION( nsnull != widget, "instance pointer is null");

  widget->OnDragLeaveSignal(aDragContext, time);
}

/* static */ gint
nsWidget::DragBeginSignal(GtkWidget *      aWidget,
                          GdkDragContext   *aDragContext,
                          gint             x,
                          gint             y,
                          guint            time,
                          void             *aData)
{
  printf("nsWidget::DragBeginSignal\n");
  fflush(stdout);

  return PR_TRUE;
}

/* static */ gint
nsWidget::DragDropSignal(GtkWidget *      aWidget,
                         GdkDragContext   *aDragContext,
                         gint             x,
                         gint             y,
                         guint            time,
                         void             *aData)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( nsnull != aDragContext, "dragcontext is null");

  nsWidget * widget = (nsWidget *) aData;
  NS_ASSERTION( nsnull != widget, "instance pointer is null");

#if 0
  if (widget->DropEvent(aWidget, aDragContext->source_window)) {
    return PR_TRUE;
  }
#endif

  widget->OnDragDropSignal(aDragContext, x, y, time);

  return PR_TRUE;
}




//////////////////////////////////////////////////////////////////
/* static */ gint 
nsWidget::EnterNotifySignal(GtkWidget *        aWidget, 
							GdkEventCrossing * aGdkCrossingEvent, 
							gpointer           aData)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( nsnull != aGdkCrossingEvent, "event is null");

  nsWidget * widget = (nsWidget *) aData;

  NS_ASSERTION( nsnull != widget, "instance pointer is null");

  if (widget->DropEvent(aWidget, aGdkCrossingEvent->window))
  {
	return PR_TRUE;
  }

  widget->OnEnterNotifySignal(aGdkCrossingEvent);

  return PR_TRUE;
}
//////////////////////////////////////////////////////////////////////
/* static */ gint 
nsWidget::LeaveNotifySignal(GtkWidget *        aWidget, 
							GdkEventCrossing * aGdkCrossingEvent, 
							gpointer           aData)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( nsnull != aGdkCrossingEvent, "event is null");

  nsWidget * widget = (nsWidget *) aData;

  NS_ASSERTION( nsnull != widget, "instance pointer is null");

  if (widget->DropEvent(aWidget, aGdkCrossingEvent->window))
  {
    return PR_TRUE;
  }

  widget->OnLeaveNotifySignal(aGdkCrossingEvent);

  return PR_TRUE;
}
//////////////////////////////////////////////////////////////////////
/* static */ gint 
nsWidget::ButtonPressSignal(GtkWidget *      aWidget, 
							GdkEventButton * aGdkButtonEvent, 
							gpointer         aData)
{
  //  printf("nsWidget::ButtonPressSignal(%p)\n",aData);

  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( nsnull != aGdkButtonEvent, "event is null");

  nsWidget * widget = (nsWidget *) aData;

  NS_ASSERTION( nsnull != widget, "instance pointer is null");

  if (widget->DropEvent(aWidget, aGdkButtonEvent->window))
  {
    return PR_TRUE;
  }

  widget->OnButtonPressSignal(aGdkButtonEvent);

  return PR_TRUE;
}
//////////////////////////////////////////////////////////////////////
/* static */ gint 
nsWidget::ButtonReleaseSignal(GtkWidget *      aWidget, 
							GdkEventButton * aGdkButtonEvent, 
							gpointer         aData)
{
  //  printf("nsWidget::ButtonReleaseSignal(%p)\n",aData);

  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( nsnull != aGdkButtonEvent, "event is null");

  nsWidget * widget = (nsWidget *) aData;

  NS_ASSERTION( nsnull != widget, "instance pointer is null");

  if (widget->DropEvent(aWidget, aGdkButtonEvent->window))
  {
    return PR_TRUE;
  }

  widget->OnButtonReleaseSignal(aGdkButtonEvent);

  return PR_TRUE;
}
//////////////////////////////////////////////////////////////////////
/* static */ gint 
nsWidget::RealizeSignal(GtkWidget *      aWidget,
                        gpointer         aData)
{
  NS_ASSERTION( nsnull != aWidget, "widget is null");
  
  nsWidget * widget = (nsWidget *) aData;

  NS_ASSERTION( nsnull != widget, "instance pointer is null");
  
  widget->OnRealize();

  return PR_TRUE;
}
//////////////////////////////////////////////////////////////////////
/* static */ gint
nsWidget::FocusInSignal(GtkWidget *      aWidget, 
                        GdkEventFocus *  aGdkFocusEvent, 
                        gpointer         aData)
{
  //  printf("nsWidget::ButtonReleaseSignal(%p)\n",aData);

  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( nsnull != aGdkFocusEvent, "event is null");

  nsWidget * widget = (nsWidget *) aData;

  NS_ASSERTION( nsnull != widget, "instance pointer is null");

//   if (widget->DropEvent(aWidget, aGdkFocusEvent->window))
//   {
// 	return PR_TRUE;
//   }

  widget->OnFocusInSignal(aGdkFocusEvent);

  if (GTK_IS_WINDOW(aWidget))
    gtk_signal_emit_stop_by_name(GTK_OBJECT(aWidget), "focus_in_event");
  
  return PR_TRUE;
}
//////////////////////////////////////////////////////////////////////
/* static */ gint
nsWidget::FocusOutSignal(GtkWidget *      aWidget, 
                        GdkEventFocus *  aGdkFocusEvent, 
                        gpointer         aData)
{
  //  printf("nsWidget::ButtonReleaseSignal(%p)\n",aData);

  NS_ASSERTION( nsnull != aWidget, "widget is null");
  NS_ASSERTION( nsnull != aGdkFocusEvent, "event is null");

  nsWidget * widget = (nsWidget *) aData;

  NS_ASSERTION( nsnull != widget, "instance pointer is null");

//   if (widget->DropEvent(aWidget, aGdkFocusEvent->window))
//   {
// 	return PR_TRUE;
//   }

  widget->OnFocusOutSignal(aGdkFocusEvent);

  if (GTK_IS_WINDOW(aWidget))
    gtk_signal_emit_stop_by_name(GTK_OBJECT(aWidget), "focus_out_event");
  
  return PR_TRUE;
}
//////////////////////////////////////////////////////////////////////

/* virtual */ GdkWindow *
nsWidget::GetWindowForSetBackground()
{
  GdkWindow * gdk_window = nsnull;

  if (mWidget)
  {
	gdk_window = mWidget->window;
  }

  return gdk_window;
}


//////////////////////////////////////////////////////////////////////
// default setfont for most widgets
/*virtual*/
void nsWidget::SetFontNative(GdkFont *aFont)
{
  GtkStyle *style = gtk_style_copy(mWidget->style);
  // gtk_style_copy ups the ref count of the font
  gdk_font_unref (style->font);
  
  style->font = aFont;
  gdk_font_ref(style->font);
  
  gtk_widget_set_style(mWidget, style);
  
  gtk_style_unref(style);
}

//////////////////////////////////////////////////////////////////////
// default SetBackgroundColor for most widgets
/*virtual*/
void nsWidget::SetBackgroundColorNative(GdkColor *aColorNor,
                                        GdkColor *aColorBri,
                                        GdkColor *aColorDark)
{
  // use same style copy as SetFont
  GtkStyle *style = gtk_style_copy(mWidget->style);
  
  style->bg[GTK_STATE_NORMAL]=*aColorNor;
  
  // Mouse over button
  style->bg[GTK_STATE_PRELIGHT]=*aColorBri;

  // Button is down
  style->bg[GTK_STATE_ACTIVE]=*aColorDark;

  // other states too? (GTK_STATE_ACTIVE, GTK_STATE_PRELIGHT,
  //               GTK_STATE_SELECTED, GTK_STATE_INSENSITIVE)
  gtk_widget_set_style(mWidget, style);
  gtk_style_unref(style);
}





























//////////////////////////////////////////////////////////////////////

#ifdef USE_XIM

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

#define PREF_XIM_PREEDIT "xim.preedit"
#define PREF_XIM_STATUS "xim.status"
#define SUPPORTED_PREEDIT (GDK_IM_PREEDIT_AREA |        \
                         GDK_IM_PREEDIT_POSITION |      \
                         GDK_IM_PREEDIT_NOTHING |       \
                         GDK_IM_PREEDIT_NONE)
//                     GDK_IM_PREEDIT_CALLBACKS

#define SUPPORTED_STATUS (GDK_IM_STATUS_NOTHING |       \
                        GDK_IM_STATUS_NONE)
//                    GDK_IM_STATUS_AREA
//                    GDK_IM_STATUS_CALLBACKS

void
nsWidget::SetXIC(GdkICPrivate *aIC)
{
  if(mIMEEnable == PR_FALSE) {
     return;
  }
  mIC = aIC;
  return;
}

GdkICPrivate*
nsWidget::GetXIC()
{
  if(mIMEEnable == PR_FALSE)
     return nsnull;

  if (mIC) return mIC;          // mIC is already set

  // IC-per-shell, we share a single IC among all widgets of
  // a single toplevel widget
  nsIWidget *widget = this;
  nsIWidget *root = this;
  while (widget) {
    root = widget;
    widget = widget->GetParent();
  }
  nsWidget *root_win = (nsWidget*)root; // this is a toplevel window
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

      GdkIMStyle supported_style = (GdkIMStyle) (SUPPORTED_PREEDIT | SUPPORTED_STATUS);
      style = gdk_im_decide_style(supported_style);

      NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv);
      if (!NS_FAILED(rv) && (prefs)) {
        rv = prefs->GetIntPref(PREF_XIM_PREEDIT, &ivalue);
        if (SUPPORTED_PREEDIT & ivalue) {
            style = (GdkIMStyle) ((style & GDK_IM_STATUS_MASK) | ivalue);
        }
        rv = prefs->GetIntPref(PREF_XIM_STATUS, &ivalue);
        if (SUPPORTED_STATUS & ivalue) {
            style = (GdkIMStyle) ((style & GDK_IM_PREEDIT_MASK) | ivalue);
        }
      }

      attr->style = style;
      attr->client_window = gdkWindow;

      attrmask = (GdkICAttributesType) (attrmask | GDK_IC_PREEDIT_COLORMAP);
      attr->preedit_colormap = gdkWindow_private->colormap;

      switch (style & GDK_IM_PREEDIT_MASK)
      {
      case GDK_IM_PREEDIT_POSITION:
      default:
        attrmask = (GdkICAttributesType) (attrmask | GDK_IC_PREEDIT_POSITION_REQ);
        gdk_window_get_size (gdkWindow, &width, &height);

        /* need to know how to get spot location */
        attr->spot_location.x = 0;
        attr->spot_location.y = 0; // height;

        attr->preedit_area.x = 0;
        attr->preedit_area.y = 0;
        attr->preedit_area.width = width;
        attr->preedit_area.height = height;
        attrmask = (GdkICAttributesType) (attrmask | GDK_IC_PREEDIT_AREA);

        attr->preedit_fontset = gfontset;
        attrmask = (GdkICAttributesType) (attrmask | GDK_IC_PREEDIT_FONTSET);
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
nsWidget::GetXYFromPosition(unsigned long *aX,
                            unsigned long *aY)
{
  if(mIMEEnable == PR_FALSE)
    return;

  if (!mIC)
    return;

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
nsWidget::SetXICSpotLocation(nsPoint aPoint)
{
  if(mIMEEnable == PR_FALSE)
  {
    return;
  }
  if (!mIC) GetXIC();
  if (mIC)
  {
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
