/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsIDeviceContext.h"
#include "nsIAppShell.h"
#include "nsGfxCIID.h"
#include "nsIComponentManager.h"
#include "nsGtkEventHandler.h"
#include "nsIFontMetrics.h"
#include <gdk/gdkx.h>

// BGR, not RGB
#define NSCOLOR_TO_GDKCOLOR(g,n) \
  g.red=NS_GET_B(n); \
  g.green=NS_GET_G(n); \
  g.blue=NS_GET_R(n);

static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);
static NS_DEFINE_IID(kLookAndFeelCID, NS_LOOKANDFEEL_CID);

//#define DBG 1

nsWidget::nsWidget()
{
  // XXX Shouldn't this be done in nsBaseWidget?
  NS_INIT_REFCNT();

  // get the proper color from the look and feel code
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
    lookAndFeel->GetColor(nsILookAndFeel::eColor_WindowBackground, mBackground);
  }
  NS_RELEASE(lookAndFeel);
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
  mOnDestroyCalled = PR_FALSE;
  mIsToplevel = PR_FALSE;
  mUpdateArea.SetRect(0, 0, 0, 0);
}

nsWidget::~nsWidget()
{
  mIsDestroying = PR_TRUE;
  if (nsnull != mWidget) {
    Destroy();
  }
}

NS_METHOD nsWidget::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
    g_print("nsWidget::WidgetToScreen\n");
    // FIXME gdk_window_get_origin()   might do what we want.... ???
    NS_NOTYETIMPLEMENTED("nsWidget::WidgetToScreen");
    return NS_OK;
}

NS_METHOD nsWidget::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
    g_print("nsWidget::ScreenToWidget\n");
    NS_NOTYETIMPLEMENTED("nsWidget::ScreenToWidget");
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Close this nsWidget
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWidget::Destroy(void)
{
  GtkAllocation *old_size = NULL;
  if (!mIsDestroying) {
    nsBaseWidget::Destroy();
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
    // dispatching of the event may cause the reference count to drop to 0
    // and result in this object being destroyed. To avoid that, add a reference
    // and then release it after dispatching the event
    AddRef();
    DispatchStandardEvent(NS_DESTROY);
    Release();
  }
}

//-------------------------------------------------------------------------
//
// Get this nsWidget parent
//
//-------------------------------------------------------------------------

nsIWidget *nsWidget::GetParent(void)
{
//  NS_NOTYETIMPLEMENTED("nsWidget::GetParent");
  return mParent;
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Show(PRBool bState)
{
  if (!mWidget)
    return NS_OK; // Will be null durring printing

  if (bState)
    ::gtk_widget_show(mWidget);
  else
    ::gtk_widget_hide(mWidget);

  mShown = bState;

  return NS_OK;
}

NS_METHOD nsWidget::IsVisible(PRBool &aState)
{
    if (mWidget) {
      gint RealVis = GTK_WIDGET_VISIBLE(mWidget);
      aState = mShown;
      g_return_val_if_fail(RealVis == mShown, NS_ERROR_FAILURE);
    }
    else
      aState = PR_TRUE;

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Move(PRUint32 aX, PRUint32 aY)
{
  mBounds.x = aX;
  mBounds.y = aY;
  if (mWidget)
    ::gtk_layout_move(GTK_LAYOUT(mWidget->parent), mWidget, aX, aY);
  return NS_OK;
}

NS_METHOD nsWidget::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
#if 0
  printf("nsWidget::Resize %s (%p) to %d %d\n",
         gtk_widget_get_name(mWidget), this,
         aWidth, aHeight);
#endif
  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  if (mWidget) {
    ::gtk_widget_set_usize(mWidget, aWidth, aHeight);

    if (aRepaint)
      if (GTK_WIDGET_VISIBLE (mWidget))
        ::gtk_widget_queue_draw (mWidget);
  }

  return NS_OK;
}

NS_METHOD nsWidget::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth,
                           PRUint32 aHeight, PRBool aRepaint)
{
  Resize(aWidth,aHeight,aRepaint);
  Move(aX,aY);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWidget::OnResize(nsRect &aRect)
{
  // call the event callback
#if 0
  printf("nsWidget::OnResize %s (%p)\n",
         gtk_widget_get_name(mWidget),
         this);
#endif
  if (mEventCallback) {
    nsSizeEvent event;
    InitEvent(event, NS_SIZE);
    event.windowSize = &aRect;
    event.eventStructType = NS_SIZE_EVENT;
    if (mWidget) {
      event.mWinWidth = mWidget->allocation.width;
      event.mWinHeight = mWidget->allocation.height;
    } else {
      event.mWinWidth = 0;
      event.mWinHeight = 0;
    }
    event.point.x = mWidget->allocation.x;
    event.point.y = mWidget->allocation.y;
    event.time = 0;
    PRBool result = DispatchWindowEvent(&event);
    // XXX why does this always crash?  maybe we need to add 
    // a ref in the dispatch code?  check the windows
    // code for a reference
    //NS_RELEASE(event.widget);
    return result;
  }
return PR_FALSE;
}

//------
// Move
//------
PRBool nsWidget::OnMove(PRInt32 aX, PRInt32 aY)
{
  nsGUIEvent event;
#if 0
  printf("nsWidget::OnMove %s (%p)\n",
         gtk_widget_get_name(mWidget),
         this);
#endif
  InitEvent(event, NS_MOVE);
  event.point.x = aX;
  event.point.y = aY;
  event.eventStructType = NS_GUI_EVENT;
  PRBool result = DispatchWindowEvent(&event);
  // NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Enable(PRBool bState)
{
    if (mWidget)
      ::gtk_widget_set_sensitive(mWidget, bState);
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetFocus(void)
{
    if (mWidget)
      ::gtk_widget_grab_focus(mWidget);
    return NS_OK;
}

NS_METHOD nsWidget::GetBounds(nsRect &aRect)
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
NS_METHOD nsWidget::SetFont(const nsFont &aFont)
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

    gtk_widget_ensure_style(mWidget);

    GtkStyle *style = gtk_style_copy(mWidget->style);
    // gtk_style_copy ups the ref count of the font
    gdk_font_unref (style->font);

    GdkFont *font = (GdkFont *)fontHandle;
    style->font = font;
    gdk_font_ref(style->font);

    gtk_widget_set_style(mWidget, style);

    gtk_style_unref(style);

  }
  NS_RELEASE(mFontMetrics);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetCursor(nsCursor aCursor)
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

NS_METHOD nsWidget::Invalidate(PRBool aIsSynchronous)
{
  if (mWidget == nsnull) {
    return NS_OK; // mWidget will be null during printing. 
  }

  if (!GTK_IS_WIDGET (mWidget)) {
    return NS_ERROR_FAILURE;
  }

  if (!GTK_WIDGET_REALIZED (GTK_WIDGET(mWidget))) {
    return NS_ERROR_FAILURE;
  }

  if (aIsSynchronous) {
    ::gtk_widget_draw(mWidget, NULL);
    mUpdateArea.SetRect(0, 0, 0, 0);
  } else {
    ::gtk_widget_queue_draw(mWidget);
    mUpdateArea.SetRect(0, 0, mBounds.width, mBounds.height);
  }

#ifdef DEBUG_pavlov
  g_print("nsWidget::Invalidate(this=%p, %i)\n", this, aIsSynchronous);
#endif
  return NS_OK;
}

NS_METHOD nsWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  if (mWidget == nsnull) {
    return NS_OK;  // mWidget is null during printing
  }

  if (!GTK_IS_WIDGET (mWidget)) {
    return NS_ERROR_FAILURE;
  }

  if (!GTK_WIDGET_REALIZED (GTK_WIDGET(mWidget))) {
    return NS_ERROR_FAILURE;
  }


  if ( aIsSynchronous) {
      GdkRectangle nRect;
      nRect.width = aRect.width;
      nRect.height = aRect.height;
      nRect.x = aRect.x;
      nRect.y = aRect.y;
      ::gtk_widget_draw(mWidget, &nRect);
  } else {
      mUpdateArea.UnionRect(mUpdateArea, aRect);
      ::gtk_widget_queue_draw_area(mWidget,
                                   aRect.x, aRect.y,
                                   aRect.width, aRect.height);
  }

#ifdef DEBUG_pavlov
  g_print("nsWidget::Invalidate(this=%p, {x=%i,y=%i,w=%i,h=%i}, %i)\n",
          this, aRect.x, aRect.y, aRect.width, aRect.height, aIsSynchronous);
#endif

  return NS_OK;
}

NS_METHOD nsWidget::Update(void)
{
  if (! mWidget)
    return NS_OK;

  if (mUpdateArea.width && mUpdateArea.height) {
    if (!mIsDestroying) {
      GdkRectangle nRect;
      nRect.width = mUpdateArea.width;
      nRect.height = mUpdateArea.height;
      nRect.x = mUpdateArea.x;
      nRect.y = mUpdateArea.y;
#ifdef DEBUG_pavlov
      g_print("nsWidget::Update(this=%p): update {%i,%i,%i,%i}\n",
              this, mUpdateArea.x, mUpdateArea.y,
              mUpdateArea.width, mUpdateArea.height);
#endif
      ::gtk_widget_draw(mWidget, &nRect);

      mUpdateArea.SetRect(0, 0, 0, 0);
      return NS_OK;
    }
    else {
      return NS_ERROR_FAILURE;
    }
  }
  else {
#ifdef DEBUG_pavlov
  g_print("nsWidget::Update(this=%p): avoided update of empty area\n", this);
#endif
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
#ifdef NS_GTK_REF
      return (void *)gdk_window_ref(mWidget->window);
#else
      return (void *)mWidget->window;
#endif
    case NS_NATIVE_DISPLAY:
      return (void *)GDK_DISPLAY();
    case NS_NATIVE_WIDGET:
#ifdef NS_GTK_REF
      gtk_widget_ref(mWidget);
#endif
      return (void *)mWidget;
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

#ifdef NS_GTK_REF
void nsWidget::ReleaseNativeData(PRUint32 aDataType)
{
  switch(aDataType) {
    case NS_NATIVE_WINDOW:
      gdk_window_unref(mWidget->window);
      break;
    case NS_NATIVE_DISPLAY:
      break;
    case NS_NATIVE_WIDGET:
      gtk_widget_unref(mWidget);
      break;
    case NS_NATIVE_GRAPHIC:
      gdk_gc_unref(((nsToolkit *)mToolkit)->GetSharedGC());
      break;
    default:
      g_print("nsWidget::ReleaseNativeData(%i) - weird value\n", aDataType);
      break;
  }
}
#endif

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::SetColorMap(nsColorMap *aColorMap)
{
    return NS_OK;
}

NS_METHOD nsWidget::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
    NS_NOTYETIMPLEMENTED("nsWidget::Scroll");
    return NS_OK;
}

NS_METHOD nsWidget::BeginResizingChildren(void)
{
  return NS_OK;
}

NS_METHOD nsWidget::EndResizingChildren(void)
{
  return NS_OK;
}

NS_METHOD nsWidget::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth  = mPreferredWidth;
  aHeight = mPreferredHeight;
  return (mPreferredWidth != 0 && mPreferredHeight != 0)?NS_OK:NS_ERROR_FAILURE;
}

NS_METHOD nsWidget::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
    mPreferredWidth  = aWidth;
    mPreferredHeight = aHeight;
    return NS_OK;
}

NS_METHOD nsWidget::SetMenuBar(nsIMenuBar * aMenuBar)
{
  g_print("bleh\n");
    NS_NOTYETIMPLEMENTED("nsWidget::SetMenuBar");
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

#if 0
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

  BaseCreate(aParent, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);

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
  /* place the widget in its parent */
  if (parentWidget)
    gtk_layout_put(GTK_LAYOUT(parentWidget), mWidget, aRect.x, aRect.y);

  gtk_widget_pop_colormap();
  gtk_widget_pop_visual();

  DispatchStandardEvent(NS_CREATE);
  InitCallbacks();

#ifdef NS_GTK_REF
  if (aNativeParent) {
    gtk_widget_unref(GTK_WIDGET(aNativeParent));
  } else if (aParent) {
    aParent->ReleaseNativeData(NS_NATIVE_WIDGET);
  }
#endif

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// create with nsIWidget parent
//
//-------------------------------------------------------------------------

NS_METHOD nsWidget::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    return(CreateWidget(aParent, aRect, aHandleEventFunction,
                        aContext, aAppShell, aToolkit, aInitData,
                        nsnull));
}

//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------
NS_METHOD nsWidget::Create(nsNativeWidget aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
    return(CreateWidget(nsnull, aRect, aHandleEventFunction,
                        aContext, aAppShell, aToolkit, aInitData,
                        aParent));
}

//-------------------------------------------------------------------------
//
// Initialize all the Callbacks
//
//-------------------------------------------------------------------------
void nsWidget::InitCallbacks(char *aName)
{
/* basically we are keeping the parent from getting the childs signals by
 * doing this. */
  gtk_signal_connect_after(GTK_OBJECT(mWidget),
                           "button_press_event",
                           GTK_SIGNAL_FUNC(gtk_true),
                           NULL);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "button_release_event",
                     GTK_SIGNAL_FUNC(gtk_true),
                     NULL);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "motion_notify_event",
                     GTK_SIGNAL_FUNC(gtk_true),
                     NULL);
  /*
    gtk_signal_connect(GTK_OBJECT(mWidget),
    "enter_notify_event",
    GTK_SIGNAL_FUNC(gtk_true),
    NULL);
    gtk_signal_connect(GTK_OBJECT(mWidget),
    "leave_notify_event",
    GTK_SIGNAL_FUNC(gtk_true),
    NULL);
    
    gtk_signal_connect(GTK_OBJECT(mWidget),
    "draw",
    GTK_SIGNAL_FUNC(gtk_false),
    NULL);
    gtk_signal_connect(GTK_OBJECT(mWidget),
    "expose_event",
    GTK_SIGNAL_FUNC(gtk_true),
    NULL);
    gtk_signal_connect(GTK_OBJECT(mWidget),
    "key_press_event",
    GTK_SIGNAL_FUNC(gtk_true),
    NULL);
    gtk_signal_connect(GTK_OBJECT(mWidget),
    "key_release_event",
    GTK_SIGNAL_FUNC(gtk_true),
    NULL);
    gtk_signal_connect(GTK_OBJECT(mWidget),
    "focus_in_event",
    GTK_SIGNAL_FUNC(gtk_true),
    NULL);
    gtk_signal_connect(GTK_OBJECT(mWidget),
    "focus_out_event",
    GTK_SIGNAL_FUNC(gtk_true),
    NULL);
  */
}

void nsWidget::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{

}

void nsWidget::InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint)
{
    event.widget = this;
    NS_IF_ADDREF(event.widget);

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
  return(PR_FALSE);
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
  NS_IF_RELEASE(event.widget);
  return result;
}


//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

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
        /*result = ConvertStatus(mMouseListener->MouseMoved(event));
        nsRect rect;
        GetBounds(rect);
        if (rect.Contains(event.point.x, event.point.y)) {
          if (mCurrentWindow == NULL || mCurrentWindow != this) {
            //printf("Mouse enter");
            mCurrentWindow = this;
          }
        } else {
          //printf("Mouse exit");
        }*/

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
    } // switch
  }
  return result;
}
