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
 * The Original Code mozilla.org code.
 *
 * The Initial Developer of the Original Code Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by the Initial Developer
 * are Copyright (C) 2001 the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
#include "nsToolkit.h"
#include "nsIRenderingContext.h"

#include <gtk/gtkwindow.h>
#include <gdk/gdkx.h>

static gboolean expose_event_cb   (GtkWidget *widget,
				   GdkEventExpose *event);
static gboolean configure_event_cb(GtkWidget *widget,
				   GdkEventConfigure *event);
static gboolean delete_event_cb   (GtkWidget *widget,
				   GdkEventAny *event);

nsWindow::nsWindow()
{
  mContainer       = nsnull;
  mDrawingarea     = nsnull;
  mShell           = nsnull;
  mIsTopLevel      = PR_FALSE;
  mIsDestroyed     = PR_FALSE;
  mWindowType      = eWindowType_child;
}

nsWindow::~nsWindow()
{
  Destroy();
}

NS_IMETHODIMP
nsWindow::Create(nsIWidget        *aParent,
		 const nsRect     &aRect,
		 EVENT_CALLBACK   aHandleEventFunction,
		 nsIDeviceContext *aContext,
		 nsIAppShell      *aAppShell,
		 nsIToolkit       *aToolkit,
		 nsWidgetInitData *aInitData)
{
  return NativeCreate(aParent, nsnull, aRect, aHandleEventFunction,
		      aContext, aAppShell, aToolkit, aInitData);
}

NS_IMETHODIMP
nsWindow::Create(nsNativeWidget aParent,
		 const nsRect     &aRect,
		 EVENT_CALLBACK   aHandleEventFunction,
		 nsIDeviceContext *aContext,
		 nsIAppShell      *aAppShell,
		 nsIToolkit       *aToolkit,
		 nsWidgetInitData *aInitData)
{
  return NativeCreate(nsnull, aParent, aRect, aHandleEventFunction,
		      aContext, aAppShell, aToolkit, aInitData);
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
  if (mIsDestroyed)
    return NS_OK;

  mIsDestroyed = PR_TRUE;

  if (mShell) {
    gtk_widget_destroy(mShell);
    mShell = nsnull;
  }

  mContainer = nsnull;

  if (mDrawingarea) {
    g_object_unref(mDrawingarea);
    mDrawingarea = nsnull;
  }

  OnDestroy();

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Show(PRBool aState)
{
  if (mIsTopLevel) {
    moz_drawingarea_set_visibility(mDrawingarea, aState);
    if (aState) {
      gtk_widget_show(GTK_WIDGET(mContainer));
      gtk_widget_show(mShell);
    }
    else {
      gtk_widget_hide(GTK_WIDGET(mShell));
      gtk_widget_hide(GTK_WIDGET(mContainer));
    }
  }
  else {
    moz_drawingarea_set_visibility(mDrawingarea, aState);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetModal(PRBool aModal)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsWindow::IsVisible(PRBool & aState)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsWindow::ConstrainPosition(PRInt32 *aX,
			    PRInt32 *aY)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsWindow::Move(PRInt32 aX,
	       PRInt32 aY)
{
  if ((aX == mBounds.x) && (aY == mBounds.y) && !mIsTopLevel)
    return NS_OK;

  mBounds.x = aX;
  mBounds.y = aY;

  // XXX do we need to handle the popup crud here?
  if (mIsTopLevel)
    gtk_window_move(GTK_WINDOW(mShell), aX, aY);
  else
    moz_drawingarea_move(mDrawingarea, aX, aY);
  
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(PRInt32 aWidth,
		 PRInt32 aHeight,
		 PRBool  aRepaint)
{
  mBounds.width = aWidth;
  mBounds.height = aHeight;

  printf("nsWindow::Resize [%p] %d %d\n", (void *)this,
	 aWidth, aHeight);

  if (mIsTopLevel)
    gtk_window_resize(GTK_WINDOW(mShell), aWidth, aHeight);

  moz_drawingarea_resize (mDrawingarea, aWidth, aHeight);

  // synthesize a resize event if this isn't a toplevel
  if (!mIsTopLevel) {
    nsRect rect(mBounds.x, mBounds.y, aWidth, aHeight);
    nsEventStatus status;
    SendResizeEvent(rect, status);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(PRInt32 aX,
		 PRInt32 aY,
		 PRInt32 aWidth,
		 PRInt32 aHeight,
		 PRBool  aRepaint)
{
  mBounds.x = aX;
  mBounds.y = aY;
  mBounds.width = aWidth;
  mBounds.height = aHeight;

  printf("nsWindow::Resize [%p] %d %d %d %d\n", (void *)this,
	 aX, aY, aWidth, aHeight);

  if (mIsTopLevel) {
    gtk_window_move(GTK_WINDOW(mShell), aX, aY);
    gtk_window_resize(GTK_WINDOW(mShell), aWidth, aHeight);
    moz_drawingarea_resize(mDrawingarea, aWidth, aHeight);
  }
  else {
    moz_drawingarea_move_resize(mDrawingarea, aX, aY, aWidth, aHeight);
    // synthesize a resize event
    nsRect rect(aX, aY, aWidth, aHeight);
    nsEventStatus status;
    SendResizeEvent(rect, status);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::PlaceBehind(nsIWidget *aWidget,
		      PRBool     aActivate)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsWindow::Enable(PRBool aState)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetFocus(PRBool aRaise)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(nsRect &aRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetForegroundColor(const nscolor &aColor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetBackgroundColor(const nscolor &aColor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

nsIFontMetrics*
nsWindow::GetFont(void)
{
  return nsnull;
}

NS_IMETHODIMP
nsWindow::SetFont(const nsFont &aFont)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetCursor(nsCursor aCursor)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::Validate()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::Invalidate(PRBool aIsSynchronous)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsRect &aRect,
		     PRBool        aIsSynchronous)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::InvalidateRegion(const nsIRegion* aRegion,
			   PRBool           aIsSynchronous)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::Update()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetColorMap(nsColorMap *aColorMap)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::Scroll(PRInt32  aDx,
		 PRInt32  aDy,
		 nsRect  *aClipRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::ScrollWidgets(PRInt32 aDx,
			PRInt32 aDy)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::ScrollRect(nsRect  &aSrcRect,
		     PRInt32  aDx,
		     PRInt32  aDy)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

void*
nsWindow::GetNativeData(PRUint32 aDataType)
{
  switch (aDataType) {
  case NS_NATIVE_WINDOW:
  case NS_NATIVE_WIDGET:
    return mDrawingarea->inner_window;
    break;

  case NS_NATIVE_PLUGIN_PORT:
    NS_WARNING("nsWindow::GetNativeData plugin port not supported yet\n");
    return nsnull;
    break;

  case NS_NATIVE_DISPLAY:
    return GDK_DISPLAY();
    break;

  case NS_NATIVE_GRAPHIC:
    NS_ASSERTION(nsnull != mToolkit, "NULL toolkit, unable to get a GC");
    return (void *)NS_STATIC_CAST(nsToolkit *, mToolkit)->GetSharedGC();
    break;

  default:
    NS_WARNING("nsWindow::GetNativeData called with bad value\n");
    return nsnull;
  }
}

NS_IMETHODIMP
nsWindow::SetBorderStyle(nsBorderStyle aBorderStyle)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetTitle(const nsString& aTitle)
{
  if (!mShell)
    return NS_OK;

  // convert the string into utf8 and set the title.
  NS_ConvertUCS2toUTF8 utf8title(aTitle);
  gtk_window_set_title(GTK_WINDOW(mShell), (const char *)utf8title.get());

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetIcon(const nsAReadableString& anIconSpec)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetMenuBar(nsIMenuBar * aMenuBar)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::ShowMenuBar(PRBool aShow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::BeginResizingChildren(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::EndResizingChildren(void)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::GetPreferredSize(PRInt32 &aWidth,
			   PRInt32 &aHeight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::SetPreferredSize(PRInt32 aWidth,
			   PRInt32 aHeight)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::Paint(nsIRenderingContext &aRenderingContext,
		const nsRect        &aDirtyRect)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::EnableDragDrop(PRBool aEnable)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
   
void
nsWindow::ConvertToDeviceCoordinates(nscoord &aX,
				     nscoord &aY)
{
}

NS_IMETHODIMP
nsWindow::PreCreateWidget(nsWidgetInitData *aWidgetInitData)
{
  if (nsnull != aWidgetInitData) {
    mWindowType = aWidgetInitData->mWindowType;
    mBorderStyle = aWidgetInitData->mBorderStyle;
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}
  
NS_IMETHODIMP
nsWindow::CaptureMouse(PRBool aCapture)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::CaptureRollupEvents(nsIRollupListener *aListener,
			      PRBool           aDoCapture,
			      PRBool           aConsumeRollupEvent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::ModalEventFilter(PRBool  aRealEvent,
			   void   *aEvent,
			   PRBool *aForWindow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsWindow::GetAttention()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

gboolean
nsWindow::OnExposeEvent(GtkWidget *aWidget, GdkEventExpose *aEvent)
{
  // handle exposes for the inner window only
  if (aEvent->window != mDrawingarea->inner_window)
    return FALSE;

  printf("sending expose event [%p] %d %d %d %d\n",
	 (void *)this,
	 aEvent->area.x, aEvent->area.y,
	 aEvent->area.width, aEvent->area.height);

  // ok, send out the paint event
  // XXX figure out the region/rect stuff!
  nsRect rect(aEvent->area.x, aEvent->area.y,
	      aEvent->area.width, aEvent->area.height);
  nsPaintEvent event;

  InitPaintEvent(event);

  event.point.x = aEvent->area.x;
  event.point.y = aEvent->area.y;
  event.rect = &rect;
  // XXX fix this!
  event.region = nsnull;
  // XXX fix this!
  event.renderingContext = GetRenderingContext();

  nsEventStatus status;
  DispatchEvent(&event, status);

  NS_RELEASE(event.renderingContext);

  // check the return value!
  return TRUE;
}

gboolean
nsWindow::OnConfigureEvent(GtkWidget *aWidget, GdkEventConfigure *aEvent)
{
  printf("configure event [%p] %p %d %d %d %d\n", (void *)this,
	 (void *)aEvent->window,
	 aEvent->x, aEvent->y, aEvent->width, aEvent->height);

  nsRect rect(aEvent->x, aEvent->y, aEvent->width, aEvent->height);

  // update the size of the drawing area
  moz_drawingarea_resize (mDrawingarea, aEvent->width, aEvent->height);

  nsEventStatus status;
  SendResizeEvent(rect, status);

  return FALSE;
}

void
nsWindow::OnDeleteEvent(GtkWidget *aWidget, GdkEventAny *aEvent)
{
  nsGUIEvent event;

  InitGUIEvent(event, NS_XUL_CLOSE);

  event.point.x = 0;
  event.point.y = 0;

  nsEventStatus status;
  DispatchEvent(&event, status);
}

void
nsWindow::SendResizeEvent(nsRect &aRect, nsEventStatus &aStatus)
{
  nsSizeEvent event;
  InitSizeEvent(event);

  event.windowSize = &aRect;
  event.point.x = aRect.x;
  event.point.y = aRect.y;
  event.mWinWidth = aRect.width;
  event.mWinHeight = aRect.height;

  nsEventStatus status;
  DispatchEvent(&event, status); 
}

nsresult
nsWindow::NativeCreate(nsIWidget        *aParent,
		       nsNativeWidget    aNativeParent,
		       const nsRect     &aRect,
		       EVENT_CALLBACK    aHandleEventFunction,
		       nsIDeviceContext *aContext,
		       nsIAppShell      *aAppShell,
		       nsIToolkit       *aToolkit,
		       nsWidgetInitData *aInitData)
{
  // only set the base parent if we're going to be a dialog or a
  // toplevel
  nsIWidget *baseParent = aInitData &&
    (aInitData->mWindowType == eWindowType_dialog ||
     aInitData->mWindowType == eWindowType_toplevel) ?
    nsnull : aParent;

  // initialize all the common bits of this class
  BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
	     aAppShell, aToolkit, aInitData);
  
  // and do our common creation
  CommonCreate(aParent);

  // save our bounds
  mBounds = aRect;

  // figure out our parent window
  MozDrawingarea *parentArea = nsnull;
  MozContainer   *parentContainer = nsnull;
  GtkWindow      *topLevelParent = nsnull;
  if (aParent || aNativeParent) {
    GdkWindow *parentWindow;
    // get the drawing area and the container from the parent
    if (aParent)
      parentWindow = (GdkWindow *)aParent->GetNativeData(NS_NATIVE_WINDOW);
    else
      parentWindow = GDK_WINDOW(aNativeParent);

    // find the mozarea on that window
    gpointer user_data = nsnull;
    user_data = g_object_get_data(G_OBJECT(parentWindow), "mozdrawingarea");
    parentArea = MOZ_DRAWINGAREA(user_data);

    NS_ASSERTION(parentArea, "no drawingarea for parent widget!\n");
    if (!parentArea)
      return NS_ERROR_FAILURE;

    // get the user data for the widget - it should be a container
    user_data = nsnull;
    gdk_window_get_user_data(parentArea->inner_window, &user_data);
    NS_ASSERTION(user_data, "no user data for parentArea\n");
    if (!user_data)
      return NS_ERROR_FAILURE;

    // XXX support generic containers here for embedding!
    parentContainer = MOZ_CONTAINER(user_data);
    NS_ASSERTION(parentContainer, "owning widget is not a mozcontainer!\n");
    if (!parentContainer)
      return NS_ERROR_FAILURE;

    // get the toplevel window just in case someone needs to use it
    // for setting transients or whatever.
    topLevelParent =
      GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(parentContainer)));
  }

  // ok, create our windows
  switch (mWindowType) {
  case eWindowType_dialog:
  case eWindowType_popup:
  case eWindowType_toplevel:
    {
      mIsTopLevel = PR_TRUE;
      if (mWindowType == eWindowType_dialog) {
	mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_type_hint(GTK_WINDOW(mShell),
				 GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_transient_for(GTK_WINDOW(mShell), topLevelParent);
      }
      else if (mWindowType == eWindowType_popup) {
	mShell = gtk_window_new(GTK_WINDOW_POPUP);
	gtk_window_set_transient_for(GTK_WINDOW(mShell), topLevelParent);
      }
      else { // must be eWindowType_toplevel
	mShell = gtk_window_new(GTK_WINDOW_TOPLEVEL);
      }

      // create our container
      mContainer = MOZ_CONTAINER(moz_container_new());
      gtk_container_add(GTK_CONTAINER(mShell), GTK_WIDGET(mContainer));
      gtk_widget_realize(GTK_WIDGET(mContainer));
      
      // and the drawing area
      mDrawingarea = moz_drawingarea_new(nsnull, mContainer);
    }
    break;
  case eWindowType_child:
    {
      mDrawingarea = moz_drawingarea_new(parentArea, parentContainer);
    }
    break;
  default:
    break;
  }

  // label the drawing area with this object so we can find our way
  // home
  g_object_set_data(G_OBJECT(mDrawingarea->clip_window), "nsWindow",
		    this);
  g_object_set_data(G_OBJECT(mDrawingarea->inner_window), "nsWindow",
		    this);

  g_object_set_data(G_OBJECT(mDrawingarea->clip_window), "mozdrawingarea",
		    mDrawingarea);
  g_object_set_data(G_OBJECT(mDrawingarea->inner_window), "mozdrawingarea",
		    mDrawingarea);

  if (mContainer)
    g_object_set_data(G_OBJECT(mContainer), "nsWindow", this);

  if (mShell)
    g_object_set_data(G_OBJECT(mShell), "nsWindow", this);

  // attach listeners for events
  if (mShell) {
    g_signal_connect(G_OBJECT(mShell), "configure_event",
		     G_CALLBACK(configure_event_cb), NULL);
    g_signal_connect(G_OBJECT(mShell), "delete_event",
		     G_CALLBACK(delete_event_cb), NULL);
  }
  if (mContainer) {
    g_signal_connect(G_OBJECT(mContainer), "expose_event",
		     G_CALLBACK(expose_event_cb), NULL);

  }

  printf("nsWindow [%p]\n", (void *)this);
  if (mShell) 
    printf("\tmShell %p %p %lx\n", (void *)mShell, (void *)mShell->window,
	   GDK_WINDOW_XWINDOW(mShell->window));
  if (mContainer)
    printf("\tmContainer %p %p %lx\n", (void *)mContainer,
	   (void *)GTK_WIDGET(mContainer)->window,
	   GDK_WINDOW_XWINDOW(GTK_WIDGET(mContainer)->window));
  if (mDrawingarea)
    printf("\tmDrawingarea %p %p %p %lx %lx\n", (void *)mDrawingarea,
	   (void *)mDrawingarea->clip_window,
	   (void *)mDrawingarea->inner_window,
	   GDK_WINDOW_XWINDOW(mDrawingarea->clip_window),
	   GDK_WINDOW_XWINDOW(mDrawingarea->inner_window));

  return NS_OK;
}

// gtk callbacks

/* static */
gboolean expose_event_cb(GtkWidget *widget, GdkEventExpose *event)
{
  gpointer user_data;
  user_data = g_object_get_data(G_OBJECT(event->window), "nsWindow");

  if (!user_data)
    return FALSE;

  nsWindow *window = NS_STATIC_CAST(nsWindow *, user_data);

  return window->OnExposeEvent(widget, event);
}

/* static */
gboolean
configure_event_cb(GtkWidget *widget,
		   GdkEventConfigure *event)
{
  gpointer user_data;
  user_data = g_object_get_data(G_OBJECT(widget), "nsWindow");

  if (!user_data)
    return FALSE;

  nsWindow *window = NS_STATIC_CAST(nsWindow *, user_data);
  
  return window->OnConfigureEvent(widget, event);
}

/* static */
gboolean
delete_event_cb(GtkWidget *widget, GdkEventAny *event)
{
  gpointer user_data;
  user_data = g_object_get_data(G_OBJECT(widget), "nsWindow");
  
  if (!user_data)
    return FALSE;

  nsWindow *window = NS_STATIC_CAST(nsWindow *, user_data);

  window->OnDeleteEvent(widget, event);

  return TRUE;
}

// nsChildWindow class

nsChildWindow::nsChildWindow()
{
}

nsChildWindow::~nsChildWindow()
{
}
