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
#include "nsSelectionMgr.h"

#include "stdio.h"

//#define DBG 0

// for nsISupports
NS_IMPL_ADDREF(nsWindow)
NS_IMPL_RELEASE(nsWindow)

/**
 * Implement the standard QueryInterface for NS_IWIDGET_IID and NS_ISUPPORTS_IID
 * @modify gpk 8/4/98
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 *
*/
nsresult nsWindow::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }

    static NS_DEFINE_IID(kCWindow, NS_WINDOW_CID);
    if (aIID.Equals(kCWindow)) {
        *aInstancePtr = (void*) ((nsWindow*)this);
        AddRef();
        return NS_OK;
    }
    return nsWidget::QueryInterface(aIID,aInstancePtr);
}



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
  mShell = nsnull;
  mVBox = nsnull;
  mResized = PR_FALSE;
  mVisible = PR_FALSE;
  mDisplayed = PR_FALSE;
  mLowerLeft = PR_FALSE;
  mBorderStyle = GTK_WINDOW_TOPLEVEL;
  mIsDestroying = PR_FALSE;
  mOnDestroyCalled = PR_FALSE;
  mFont = nsnull;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  mIsDestroying = PR_TRUE;
  if (mShell)
  {
    if (GTK_IS_WIDGET(mShell))
      gtk_widget_destroy(mShell);
    mShell = nsnull;
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

NS_METHOD nsWindow::Destroy()
{
  // disconnect from the parent


  if (mIsDestroying == PR_TRUE) {
    nsBaseWidget::Destroy();
    if (PR_FALSE == mOnDestroyCalled)
	OnDestroy();
    if (mShell) {
    	if (GTK_IS_WIDGET(mShell))
     		gtk_widget_destroy(mShell);
    	mShell = nsnull;
    }
  }

  return NS_OK;
}

void nsWindow::OnDestroy()
{
    mOnDestroyCalled = PR_TRUE;

    // release references to children, device context, toolkit, and app shell
    nsBaseWidget::OnDestroy();

    // dispatch the event
    if (mIsDestroying == PR_TRUE) {
      // dispatching of the event may cause the reference count to drop to 0
      // and result in this object being destroyed. To avoid that, add a reference
      // and then release it after dispatching the event
      AddRef();
      DispatchStandardEvent(NS_DESTROY);
      Release();
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
    switch(aInitData->mBorderStyle)
    {
      case eBorderStyle_none:
        break;
      case eBorderStyle_dialog:
        mBorderStyle = GTK_WINDOW_DIALOG;
        break;
      case eBorderStyle_window:
        mBorderStyle = GTK_WINDOW_TOPLEVEL;
        break;
      case eBorderStyle_3DChildWindow:
        break;
    }
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
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

//  mainWindow = gtk_window_new(mBorderStyle);
    mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(mShell), 
                                mBounds.width,
                                mBounds.height);
    gtk_widget_show (mShell);

    // Now that the window is up, change the window manager hints
    // associated with it so that its resizable (smaller). Note that
    // the numbers chosen here are arbitrary. The application really
    // needs a hand in this.
    GdkGeometry geom;
    geom.min_width = 50;
    geom.min_height = 50;
    geom.base_width = mBounds.width;
    geom.base_height = mBounds.height;
    geom.width_inc = 1;
    geom.height_inc = 1;
    gtk_window_set_geometry_hints(GTK_WINDOW(mShell), mShell, &geom,
                                  (GdkWindowHints) (GDK_HINT_MIN_SIZE |
                                                    GDK_HINT_RESIZE_INC));

// VBox for the menu, etc.
    mVBox = gtk_vbox_new(PR_FALSE, 0);
    gtk_widget_show (mVBox);
    gtk_container_add(GTK_CONTAINER(mShell), mVBox);
    gtk_box_pack_start(GTK_BOX(mVBox), mWidget, PR_TRUE, PR_TRUE, 0);
    // this is done in CreateWidget now...
    //mIsToplevel = PR_TRUE;
    gtk_signal_connect(GTK_OBJECT(mShell),
                     "delete_event",
                     GTK_SIGNAL_FUNC(handle_delete_event),
                     this);
    nsSelectionMgr::SetTopLevelWidget(mShell);
  }

  // Force cursor to default setting
  gtk_widget_set_name(mWidget, "nsWindow");
  mIsToplevel = PR_TRUE;
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
  gtk_signal_connect_after(GTK_OBJECT(mWidget),
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
                     "draw",
		     GTK_SIGNAL_FUNC(handle_draw_event),
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
  char * titleStr = aTitle.ToNewCString();
  gtk_window_set_title(GTK_WINDOW(mShell), titleStr);
  delete[] titleStr;
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
    static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
    static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
    if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID,
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
    
    //NS_RELEASE(event.widget);
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

#if 0
PRBool nsWindow::OnResize(nsSizeEvent &aEvent)
{
  if (mEventCallback) {
    return DispatchWindowEvent(&aEvent);
  }
  return PR_FALSE;
}
#endif

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
