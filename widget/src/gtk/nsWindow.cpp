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
#include <gdk/gdkx.h>
#include <gtk/gtkprivate.h>

#include "nsWindow.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"
#include "nsMenuBar.h"

#include "nsGtkEventHandler.h"
#include "nsAppShell.h"

#include "stdio.h"

//#define DBG 0

static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow()
{
  NS_INIT_REFCNT();
  strcpy(gInstanceClassName, "nsWindow");
  mFontMetrics = nsnull;
  mVBox = nsnull;
  mResized = PR_FALSE;
  mVisible = PR_FALSE;
  mDisplayed = PR_FALSE;
  mLowerLeft = PR_FALSE;
  mFont = nsnull;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  if (mWidget)
  {
    if (GTK_IS_WIDGET(mWidget))
      gtk_widget_destroy(mWidget);
    mWidget = nsnull;
  }
  if (mGC) {
    gdk_gc_destroy(mGC);
    mGC = nsnull;
  }
}

//-------------------------------------------------------------------------
void nsWindow::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{

}

//-------------------------------------------------------------------------
//
// Setup initial tooltip rectangles
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[])
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Update all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::UpdateTooltips(nsRect* aNewTips[])
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::RemoveTooltips()
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create the native widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CreateNative(GtkWidget *parentWidget)
{
  GtkWidget *mainWindow;

  mWidget = gtk_layout_new(PR_FALSE, PR_FALSE);
  GTK_WIDGET_SET_FLAGS(mWidget, GTK_CAN_FOCUS);

  gtk_widget_set_events (mWidget,
                         GDK_BUTTON_PRESS_MASK |
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_ENTER_NOTIFY_MASK |
                         GDK_EXPOSURE_MASK |
                         GDK_FOCUS_CHANGE_MASK |
                         GDK_KEY_PRESS_MASK |
                         GDK_KEY_RELEASE_MASK |
                         GDK_LEAVE_NOTIFY_MASK |
                         GDK_POINTER_MOTION_MASK);

  if (!parentWidget) {

    mainWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_widget_show (mainWindow);

// VBox for the menu, etc.
    mVBox = gtk_vbox_new(PR_FALSE, 0);
    gtk_widget_show (mVBox);
    gtk_container_add(GTK_CONTAINER(mainWindow), mVBox);
    gtk_box_pack_start(GTK_BOX(mVBox), mWidget, PR_TRUE, PR_TRUE, 0);
  }

  // Force cursor to default setting
  gtk_widget_set_name(mWidget, "nsWindow");
  mCursor = eCursor_select;
  SetCursor(eCursor_standard);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Initialize all the Callbacks
//
//-------------------------------------------------------------------------
void nsWindow::InitCallbacks(char * aName)
{
  gtk_signal_connect_after(GTK_OBJECT(mWidget),
                     "size_allocate",
                     GTK_SIGNAL_FUNC(handle_size_allocate),
                     this);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "button_press_event",
		     GTK_SIGNAL_FUNC(handle_button_press_event),
		     this);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "button_release_event",
		     GTK_SIGNAL_FUNC(handle_button_release_event),
		     this);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "motion_notify_event",
		     GTK_SIGNAL_FUNC(handle_motion_notify_event),
		     this);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "enter_notify_event",
		     GTK_SIGNAL_FUNC(handle_enter_notify_event),
		     this);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "leave_notify_event",
		     GTK_SIGNAL_FUNC(handle_leave_notify_event),
		     this);
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
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "focus_in_event",
		     GTK_SIGNAL_FUNC(handle_focus_in_event),
		     this);
  gtk_signal_connect(GTK_OBJECT(mWidget),
                     "focus_out_event",
		     GTK_SIGNAL_FUNC(handle_focus_out_event),
		     this);
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
  if (mWidget == nsnull) {
    return NS_ERROR_FAILURE;
  }

  if (!GTK_IS_WIDGET (mWidget)) {
    return NS_ERROR_FAILURE;
  }

  if (!GTK_WIDGET_REALIZED (GTK_WIDGET(mWidget))) {
    return NS_ERROR_FAILURE;
  }

  GdkEventExpose event;

  event.type = GDK_EXPOSE;
  event.send_event = PR_TRUE;
  event.window = GTK_WIDGET(mWidget)->window;
  event.area.width = mBounds.width;
  event.area.height = mBounds.height;
  event.area.x = 0;
  event.area.y = 0;

  event.count = 0;

  gdk_window_ref (event.window);
  gtk_widget_event (GTK_WIDGET(mWidget), (GdkEvent*) &event);
  gdk_window_unref (event.window);

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  if (mWidget == nsnull) {
    return NS_ERROR_FAILURE;
  }

  if (!GTK_IS_WIDGET (mWidget)) {
    return NS_ERROR_FAILURE;
  }

  if (!GTK_WIDGET_REALIZED (GTK_WIDGET(mWidget))) {
    return NS_ERROR_FAILURE;
  }

  GdkEventExpose event;

  event.type = GDK_EXPOSE;
  event.send_event = PR_TRUE;
  event.window = GTK_WIDGET(mWidget)->window;

  event.area.width = aRect.width;
  event.area.height = aRect.height;
  event.area.x = aRect.x;
  event.area.y = aRect.y;

  event.count = 0;

  gdk_window_ref (event.window);
  gtk_widget_event (GTK_WIDGET(mWidget), (GdkEvent*) &event);
  gdk_window_unref (event.window);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Force a synchronous repaint of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Update()
{
  return NS_OK;
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
	return (void *)GTK_LAYOUT(mWidget)->bin_window;
      case NS_NATIVE_DISPLAY:
	return (void *)GDK_DISPLAY();
      case NS_NATIVE_WIDGET:
	return (void *)mWidget;
      case NS_NATIVE_GRAPHIC:
	if (mGC) return (void *)mGC;
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
  if (mVBox) // Top level widget (has correct parent)
  {
    char * titleStr = aTitle.ToNewCString();
    gtk_window_set_title(GTK_WINDOW(mVBox->parent), titleStr);
    delete[] titleStr;
  }
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

    nsRect rr ;

    /*
     * Maybe  ... some day ... somone will pull the invalid rect
     * out of the paint message rather than drawing the whole thing...
     */
    GetBounds(rr);

    rr.x = 0;
    rr.y = 0;

    event.rect = &rr;

    event.renderingContext = nsnull;
    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
    if (NS_OK == nsRepository::CreateInstance(kRenderingContextCID,
					      nsnull,
					      kRenderingContextIID,
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
  }
  return result;
}


NS_METHOD nsWindow::BeginResizingChildren(void)
{
  gtk_layout_freeze(GTK_LAYOUT(mWidget));
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  gtk_layout_thaw(GTK_LAYOUT(mWidget));
  return NS_OK;
}


PRBool nsWindow::OnResize(nsSizeEvent &aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}

PRBool nsWindow::OnKey(nsKeyEvent &aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}


PRBool nsWindow::DispatchFocus(nsGUIEvent &aEvent)
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

NS_METHOD nsWindow::SetMenuBar(nsIMenuBar * aMenuBar)
{
  GtkWidget *menubar;
  void *voidData;

  aMenuBar->GetNativeData(voidData);
  menubar = GTK_WIDGET(voidData);

  gtk_menu_bar_set_shadow_type (GTK_MENU_BAR(menubar), GTK_SHADOW_NONE);

  gtk_box_pack_start(GTK_BOX(mVBox), menubar, PR_FALSE, PR_FALSE, 0);
  gtk_box_reorder_child(GTK_BOX(mVBox), menubar, 0);
  return NS_OK;
}
