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
#include "nsIRegion.h"

#include "nsGtkKeyUtils.h"

#include <gtk/gtkwindow.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

/* utility functions */
static nsWindow *get_window_for_gtk_widget(GtkWidget *widget);
static nsWindow *get_window_for_gdk_window(GdkWindow *window);
static nsWindow *get_owning_window_for_gdk_window(GdkWindow *window);

/* callbacks from widgets */
static gboolean expose_event_cb           (GtkWidget *widget,
					   GdkEventExpose *event);
static gboolean configure_event_cb        (GtkWidget *widget,
					   GdkEventConfigure *event);
static void     size_allocate_cb          (GtkWidget *widget,
					   GtkAllocation *allocation);
static gboolean delete_event_cb           (GtkWidget *widget,
					   GdkEventAny *event);
static gboolean enter_notify_event_cb     (GtkWidget *widget,
					   GdkEventCrossing *event);
static gboolean leave_notify_event_cb     (GtkWidget *widget,
					   GdkEventCrossing *event);
static gboolean motion_notify_event_cb    (GtkWidget *widget,
					   GdkEventMotion *event);
static gboolean button_press_event_cb     (GtkWidget *widget,
					   GdkEventButton *event);
static gboolean button_release_event_cb   (GtkWidget *widget,
					   GdkEventButton *event);
static gboolean focus_in_event_cb         (GtkWidget *widget,
					   GdkEventFocus *event);
static gboolean focus_out_event_cb        (GtkWidget *widget,
					   GdkEventFocus *event);
static gboolean key_press_event_cb        (GtkWidget *widget,
					   GdkEventKey *event);
static gboolean key_release_event_cb      (GtkWidget *widget,
					   GdkEventKey *event);
static gboolean scroll_event_cb           (GtkWidget *widget,
					   GdkEventScroll *event);

nsWindow::nsWindow()
{
  mContainer           = nsnull;
  mDrawingarea         = nsnull;
  mShell               = nsnull;
  mWindowType          = eWindowType_child;
  mContainerGotFocus   = PR_FALSE;
  mContainerLostFocus  = PR_FALSE;
  mContainerBlockFocus = PR_FALSE;
  mHasFocus            = PR_FALSE;
  mFocusChild          = nsnull;
  mInKeyRepeat         = PR_FALSE;
}

nsWindow::~nsWindow()
{
  printf("nsWindow::~nsWindow() [%p]\n", (void *)this);
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

  printf("nsWindow::Destroy [%p]\n", (void *)this);
  mIsDestroyed = PR_TRUE;

  NativeShow(PR_FALSE);

  // walk the list of children and call destroy on them.
  nsCOMPtr<nsIEnumerator> children = dont_AddRef(GetChildren());
  if (children) {
    nsCOMPtr<nsISupports> isupp;
    nsCOMPtr<nsIWidget> child;
    while (NS_SUCCEEDED(children->CurrentItem(getter_AddRefs(isupp))
			&& isupp)) {
      child = do_QueryInterface(isupp);
      if (child) {
	child->Destroy();
      }

      if (NS_FAILED(children->Next()))
        break;
    }
  }

  // make sure that we remove ourself as the focus window
  if (mHasFocus) {
    printf("automatically losing focus...\n");
    mHasFocus = PR_FALSE;
    // get the owning gtk widget and the nsWindow for that widget and
    // remove ourselves as the focus widget tracked in that window
    nsWindow *owningWindow =
      get_owning_window_for_gdk_window(mDrawingarea->inner_window);
    owningWindow->mFocusChild = nsnull;
  }

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
nsWindow::ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
  if (aX == mBounds.x && aY == mBounds.y)
    return NS_OK;

  printf("nsWindow::Move [%p] %d %d\n", (void *)this,
	 aX, aY);

  mBounds.x = aX;
  mBounds.y = aY;

  if (mIsTopLevel) {
    if (mParent && mWindowType == eWindowType_popup) {
      nsRect oldrect, newrect;
      oldrect.x = aX;
      oldrect.y = aY;
      mParent->WidgetToScreen(oldrect, newrect);
      gtk_window_move(GTK_WINDOW(mShell), newrect.x, newrect.y);
    }
    else {
      gtk_window_move(GTK_WINDOW(mShell), aX, aY);
    }
  }
  else {
    moz_drawingarea_move(mDrawingarea, aX, aY);
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
  // Make sure that our owning widget has focus.  If it doesn't try to
  // grab it.  Note that we don't set our focus flag in this case.
  
  printf("SetFocus [%p]\n", (void *)this);
  gpointer user_data = NULL;
  gdk_window_get_user_data(mDrawingarea->inner_window, &user_data);
  if (!user_data)
    return NS_ERROR_FAILURE;

  GtkWidget *owningWidget = GTK_WIDGET(user_data);
  nsWindow  *owningWindow = get_window_for_gtk_widget(owningWidget);
  if (!owningWindow)
    return NS_ERROR_FAILURE;

  if (!GTK_WIDGET_HAS_FOCUS(owningWidget)) {
    owningWindow->mContainerBlockFocus = PR_TRUE;
    gtk_widget_grab_focus(owningWidget);
    owningWindow->mContainerBlockFocus = PR_FALSE;
    DispatchGotFocusEvent();
  }

  // Raise the window if someone passed in PR_TRUE and the prefs are
  // set properly.
  // XXX do this

  // If this is the widget that already has focus, return.
  if (mHasFocus) {
    printf("already have focus...\n");
    return NS_OK;
  }
  
  // If there is already a focued child window, dispatch a LOSTFOCUS
  // event from that widget and unset its got focus flag.
  if (owningWindow->mFocusChild) {
    printf("removing focus child %p\n", (void *)owningWindow->mFocusChild);
    owningWindow->mFocusChild->LoseFocus();
  }

  // Set this window to be the focused child window, update our has
  // focus flag and dispatch a GOTFOCUS event.
  owningWindow->mFocusChild = this;
  mHasFocus = PR_TRUE;
  DispatchGotFocusEvent();

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::GetScreenBounds(nsRect &aRect)
{
  nsRect origin(0, 0, mBounds.width, mBounds.height);
  WidgetToScreen(origin, aRect);
  printf("GetScreenBounds %d %d | %d %d | %d %d\n",
	 aRect.x, aRect.y,
	 mBounds.width, mBounds.height,
	 aRect.width, aRect.height);
  return NS_OK;
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
  // Get the update for this window and, well, just drop it on the
  // floor.
  GdkRegion *region = gdk_window_get_update_area(mDrawingarea->inner_window);

  if (region)
    gdk_region_destroy(region);

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(PRBool aIsSynchronous)
{
  GdkRectangle rect;

  rect.x = mBounds.x;
  rect.y = mBounds.y;
  rect.width = mBounds.width;
  rect.height = mBounds.height;

  gdk_window_invalidate_rect(mDrawingarea->inner_window,
			     &rect, TRUE);
  if (aIsSynchronous)
    gdk_window_process_updates(mDrawingarea->inner_window, TRUE);
  
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Invalidate(const nsRect &aRect,
		     PRBool        aIsSynchronous)
{
  GdkRectangle rect;

  rect.x = aRect.x;
  rect.y = aRect.y;
  rect.width = aRect.width;
  rect.height = aRect.height;

  gdk_window_invalidate_rect(mDrawingarea->inner_window,
			     &rect, TRUE);
  if (aIsSynchronous)
    gdk_window_process_updates(mDrawingarea->inner_window, TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::InvalidateRegion(const nsIRegion* aRegion,
			   PRBool           aIsSynchronous)
{
  GdkRegion *region = nsnull;
  aRegion->GetNativeRegion((void *)region);

  if (region)
    gdk_window_invalidate_region(mDrawingarea->inner_window,
				 region, TRUE);
  
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Update()
{
  gdk_window_process_updates(mDrawingarea->inner_window, TRUE);
  return NS_OK;
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
  moz_drawingarea_scroll(mDrawingarea, aDx, aDy);

  // Update bounds on our child windows
  nsCOMPtr<nsIEnumerator> children = dont_AddRef(GetChildren());
  if (children) {
    nsCOMPtr<nsISupports> isupp;
    nsCOMPtr<nsIWidget> child;
    while (NS_SUCCEEDED(children->CurrentItem(getter_AddRefs(isupp))
			&& isupp)) {
      child = do_QueryInterface(isupp);
      if (child) {
        nsRect bounds;
        child->GetBounds(bounds);
        bounds.x += aDx;
        bounds.y += aDy;
        NS_STATIC_CAST(nsBaseWidget*, (nsIWidget*)child)->SetBounds(bounds);
      }

      if (NS_FAILED(children->Next()))
        break;
    }
  }

  // Process all updates so that everything is drawn.
  gdk_window_process_all_updates();
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::ScrollWidgets(PRInt32 aDx,
			PRInt32 aDy)
{
  moz_drawingarea_scroll(mDrawingarea, aDx, aDy);
  return NS_OK;
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
    NS_WARNING("nsWindow::GetNativeData plugin port not supported yet");
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
    NS_WARNING("nsWindow::GetNativeData called with bad value");
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
  gint x, y = 0;

  if (mContainer) {
    gdk_window_get_root_origin(GTK_WIDGET(mContainer)->window,
			       &x, &y);
    printf("WidgetToScreen (container) %d %d\n", x, y);
  }
  else {
    gdk_window_get_origin(mDrawingarea->inner_window, &x, &y);
    printf("WidgetToScreen (drawing) %d %d\n", x, y);
  }

  aNewRect.x = x + aOldRect.x;
  aNewRect.y = y + aOldRect.y;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;

  return NS_OK;
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

void
nsWindow::LoseFocus(void)
{
  // we don't have focus
  mHasFocus = PR_FALSE;

  // make sure that we reset our repeat counter so the next keypress
  // for this widget will get the down event
  mInKeyRepeat = PR_FALSE;

  // Dispatch a lostfocus event
  DispatchLostFocusEvent();
}

gboolean
nsWindow::OnExposeEvent(GtkWidget *aWidget, GdkEventExpose *aEvent)
{
  if (mIsDestroyed) {
    printf("Expose event on destroyed window [%p] window %p\n",
	   (void *)this, (void *)aEvent->window);
    return NS_OK;
  }

  // handle exposes for the inner window only
  if (aEvent->window != mDrawingarea->inner_window)
    return FALSE;

  printf("sending expose event [%p] %p 0x%lx\n\t%d %d %d %d\n",
	 (void *)this,
	 (void *)aEvent->window,
	 GDK_WINDOW_XWINDOW(aEvent->window),
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
  printf("configure event [%p] %d %d %d %d\n", (void *)this,
	 aEvent->x, aEvent->y, aEvent->width, aEvent->height);

  // can we shortcut?
  if (mBounds.x == aEvent->x &&
      mBounds.y == aEvent->y)
    return FALSE;

  nsGUIEvent event;
  InitGUIEvent(event, NS_MOVE);

  event.point.x = aEvent->x;
  event.point.y = aEvent->y;

  // XXX mozilla will invalidate the entire window after this move
  // complete.  wtf?
  nsEventStatus status;
  DispatchEvent(&event, status);

  return FALSE;
}

void
nsWindow::OnSizeAllocate(GtkWidget *aWidget, GtkAllocation *aAllocation)
{
  printf("size_allocate [%p] %d %d %d %d\n",
	 (void *)this, aAllocation->x, aAllocation->y,
	 aAllocation->width, aAllocation->height);
  
  nsRect rect(aAllocation->x, aAllocation->y,
	      aAllocation->width, aAllocation->height);

  mBounds.width = rect.width;
  mBounds.height = rect.height;

  moz_drawingarea_resize (mDrawingarea, rect.width, rect.height);

  nsEventStatus status;
  DispatchResizeEvent (rect, status);
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
nsWindow::OnEnterNotifyEvent(GtkWidget *aWidget, GdkEventCrossing *aEvent)
{
  nsMouseEvent event;
  InitMouseEvent(event, NS_MOUSE_ENTER);

  event.point.x = nscoord(aEvent->x);
  event.point.y = nscoord(aEvent->y);

  nsEventStatus status;
  DispatchEvent(&event, status);
}

void
nsWindow::OnLeaveNotifyEvent(GtkWidget *aWidget, GdkEventCrossing *aEvent)
{
  nsMouseEvent event;
  InitMouseEvent(event, NS_MOUSE_EXIT);

  event.point.x = nscoord(aEvent->x);
  event.point.y = nscoord(aEvent->y);

  nsEventStatus status;
  DispatchEvent(&event, status);
}

void
nsWindow::OnMotionNotifyEvent(GtkWidget *aWidget, GdkEventMotion *aEvent)
{

  // see if we can compress this event
  XEvent xevent;
  PRPackedBool synthEvent = PR_FALSE;
  while (XCheckWindowEvent(GDK_WINDOW_XDISPLAY(aEvent->window),
			   GDK_WINDOW_XWINDOW(aEvent->window),
			   ButtonMotionMask, &xevent)) {
    synthEvent = PR_TRUE;
  }

  nsMouseEvent event;
  InitMouseEvent(event, NS_MOUSE_MOVE);
 
  if (synthEvent) {
    event.point.x = nscoord(xevent.xmotion.x);
    event.point.y = nscoord(xevent.xmotion.y);

    event.isShift   = (xevent.xmotion.state & GDK_SHIFT_MASK)
      ? PR_TRUE : PR_FALSE;
    event.isControl = (xevent.xmotion.state & GDK_CONTROL_MASK)
      ? PR_TRUE : PR_FALSE;
    event.isAlt     = (xevent.xmotion.state & GDK_MOD1_MASK)
      ? PR_TRUE : PR_FALSE;
  }
  else {
    event.point.x = nscoord(aEvent->x);
    event.point.y = nscoord(aEvent->y);

    event.isShift   = (aEvent->state & GDK_SHIFT_MASK)
      ? PR_TRUE : PR_FALSE;
    event.isControl = (aEvent->state & GDK_CONTROL_MASK)
      ? PR_TRUE : PR_FALSE;
    event.isAlt     = (aEvent->state & GDK_MOD1_MASK)
      ? PR_TRUE : PR_FALSE;
  }

  nsEventStatus status;
  DispatchEvent(&event, status);
}

void
nsWindow::OnButtonPressEvent(GtkWidget *aWidget, GdkEventButton *aEvent)
{
  nsMouseEvent  event;
  PRUint32      eventType;
  nsEventStatus status;

  switch (aEvent->button) {
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

  InitButtonEvent(event, eventType, aEvent);
  
  DispatchEvent(&event, status);

  // right menu click on linux should also pop up a context menu
  if (eventType == NS_MOUSE_RIGHT_BUTTON_DOWN) {
    InitButtonEvent(event, NS_CONTEXTMENU, aEvent);
    DispatchEvent(&event, status);
  }
}

void
nsWindow::OnButtonReleaseEvent(GtkWidget *aWidget, GdkEventButton *aEvent)
{
  nsMouseEvent  event;
  PRUint32      eventType;

  switch (aEvent->button) {
  case 2:
    eventType = NS_MOUSE_MIDDLE_BUTTON_UP;
    break;
  case 3:
    eventType = NS_MOUSE_RIGHT_BUTTON_UP;
    break;
    // don't send events for these types
  case 4:
  case 5:
    return;
    break;
    // default including button 1 is left button up
  default:
    eventType = NS_MOUSE_LEFT_BUTTON_UP;
    break;
  }

  InitButtonEvent(event, eventType, aEvent);

  nsEventStatus status;
  DispatchEvent(&event, status);
}

void
nsWindow::OnContainerFocusInEvent(GtkWidget *aWidget, GdkEventFocus *aEvent)
{
  // Return if someone has blocked events for this widget.  This will
  // happen if someone has called gtk_widget_grab_focus() from
  // nsWindow::SetFocus() and will prevent recursion.
  if (mContainerBlockFocus)
    return;

  // dispatch a got focus event
  DispatchGotFocusEvent();

  // dispatch an ACTIVATE event
  DispatchActivateEvent();
}

void
nsWindow::OnContainerFocusOutEvent(GtkWidget *aWidget, GdkEventFocus *aEvent)
{
  // send a lost focus event for the child window
  if (mFocusChild) {
    mFocusChild->LoseFocus();
    mFocusChild->DispatchDeactivateEvent();
    mFocusChild = nsnull;
  }
}

gboolean
nsWindow::OnKeyPressEvent(GtkWidget *aWidget, GdkEventKey *aEvent)
{
  // work around for annoying things.
  if (aEvent->keyval == GDK_Tab)
    if (aEvent->state & GDK_CONTROL_MASK)
      if (aEvent->state & GDK_MOD1_MASK)
        return FALSE;

  // Don't pass shift, control and alt as key press events
  if (aEvent->keyval == GDK_Shift_L
      || aEvent->keyval == GDK_Shift_R
      || aEvent->keyval == GDK_Control_L
      || aEvent->keyval == GDK_Control_R
      || aEvent->keyval == GDK_Alt_L
      || aEvent->keyval == GDK_Alt_R)
    return TRUE;


  // If the key repeat flag isn't set then set it so we don't send
  // another key down event on the next key press -- DOM events are
  // key down, key press and key up.  X only has key press and key
  // release.  gtk2 already filters the extra key release events for
  // us.
  nsEventStatus status;
  nsKeyEvent event;

  if (!mInKeyRepeat) {
    mInKeyRepeat = PR_TRUE;
    // send the key down event
    InitKeyEvent(event, aEvent, NS_KEY_DOWN);
    DispatchEvent(&event, status);
  }
  
  InitKeyEvent(event, aEvent, NS_KEY_PRESS);
  event.charCode = nsConvertCharCodeToUnicode(aEvent);
  if (event.charCode) {
    event.keyCode = 0;
    // if the control, meta, or alt key is down, then we should leave
    // the isShift flag alone (probably not a printable character)
    // if none of the other modifier keys are pressed then we need to
    // clear isShift so the character can be inserted in the editor
    if (!event.isControl && !event.isAlt && !event.isMeta)
      event.isShift = PR_FALSE;
  }

  // send the key press event
  DispatchEvent(&event, status);
  return TRUE;
}

gboolean
nsWindow::OnKeyReleaseEvent(GtkWidget *aWidget, GdkEventKey *aEvent)
{
  // unset the repeat flag
  mInKeyRepeat = PR_FALSE;

  // send the key event as a key up event
  // Don't pass shift, control and alt as key press events
  if (aEvent->keyval == GDK_Shift_L
      || aEvent->keyval == GDK_Shift_R
      || aEvent->keyval == GDK_Control_L
      || aEvent->keyval == GDK_Control_R
      || aEvent->keyval == GDK_Alt_L
      || aEvent->keyval == GDK_Alt_R)
    return TRUE;

  nsKeyEvent event;
  InitKeyEvent(event, aEvent, NS_KEY_UP);

  nsEventStatus status;
  DispatchEvent(&event, status);

  return TRUE;
}

void
nsWindow::OnScrollEvent(GtkWidget *aWidget, GdkEventScroll *aEvent)
{
  nsMouseScrollEvent event;
  InitMouseScrollEvent(event, aEvent, NS_MOUSE_SCROLL);

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
  CommonCreate(aParent, aNativeParent);

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
      parentWindow = GDK_WINDOW(aParent->GetNativeData(NS_NATIVE_WINDOW));
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

      // make sure this is the focus widget in the container
      gtk_window_set_focus(GTK_WINDOW(mShell), GTK_WIDGET(mContainer));

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
    g_signal_connect_after(G_OBJECT(mContainer), "size_allocate",
			   G_CALLBACK(size_allocate_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "expose_event",
		     G_CALLBACK(expose_event_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "enter_notify_event",
		     G_CALLBACK(enter_notify_event_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "leave_notify_event",
		     G_CALLBACK(leave_notify_event_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "motion_notify_event",
		     G_CALLBACK(motion_notify_event_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "button_press_event",
		     G_CALLBACK(button_press_event_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "button_release_event",
		     G_CALLBACK(button_release_event_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "focus_in_event",
		     G_CALLBACK(focus_in_event_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "focus_out_event",
		     G_CALLBACK(focus_out_event_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "key_press_event",
		     G_CALLBACK(key_press_event_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "key_release_event",
		     G_CALLBACK(key_release_event_cb), NULL);
    g_signal_connect(G_OBJECT(mContainer), "scroll_event",
		     G_CALLBACK(scroll_event_cb), NULL);
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

  // resize so that everything is set to the right dimensions
  Resize(mBounds.width, mBounds.height, PR_FALSE);

  return NS_OK;
}

void
nsWindow::NativeResize(PRInt32 aWidth, PRInt32 aHeight, PRBool  aRepaint)
{
  printf("nsWindow::NativeResize [%p] %d %d\n", (void *)this,
	 aWidth, aHeight);

  // clear our resize flag
  mNeedsResize = PR_FALSE;

  if (mIsTopLevel)
    gtk_window_resize(GTK_WINDOW(mShell), aWidth, aHeight);
  
  moz_drawingarea_resize (mDrawingarea, aWidth, aHeight);
}

void
nsWindow::NativeResize(PRInt32 aX, PRInt32 aY,
		       PRInt32 aWidth, PRInt32 aHeight,
		       PRBool  aRepaint)
{
  mNeedsResize = PR_FALSE;
  
  printf("nsWindow::NativeResize [%p] %d %d %d %d\n", (void *)this,
	 aX, aY, aWidth, aHeight);
  
  if (mIsTopLevel) {
    if (mParent && mWindowType == eWindowType_popup) {
      nsRect oldrect, newrect;
      oldrect.x = aX;
      oldrect.y = aY;
      mParent->WidgetToScreen(oldrect, newrect);
      moz_drawingarea_resize(mDrawingarea, aWidth, aHeight);
      gtk_window_move(GTK_WINDOW(mShell), newrect.x, newrect.y);
      gtk_window_resize(GTK_WINDOW(mShell), aWidth, aHeight);
    }
    else {
      gtk_window_move(GTK_WINDOW(mShell), aX, aY);
      gtk_window_resize(GTK_WINDOW(mShell), aWidth, aHeight);
      moz_drawingarea_resize(mDrawingarea, aWidth, aHeight);
    }
  }
  else {
    moz_drawingarea_move_resize(mDrawingarea, aX, aY, aWidth, aHeight);
  }
}

void
nsWindow::NativeShow (PRBool  aAction)
{
  if (aAction) {
    // unset our flag now that our window has been shown
    mNeedsShow = PR_FALSE;
    
    if (mIsTopLevel) {
      moz_drawingarea_set_visibility(mDrawingarea, aAction);
      gtk_widget_show(GTK_WIDGET(mContainer));
      gtk_widget_show(mShell);
      
    }
    else {
      moz_drawingarea_set_visibility(mDrawingarea, aAction);
    }
  }
  else {
    if (mIsTopLevel) {
      gtk_widget_hide(GTK_WIDGET(mShell));
      gtk_widget_hide(GTK_WIDGET(mContainer));
    }
    moz_drawingarea_set_visibility(mDrawingarea, aAction);
  }
}

/* static */
nsWindow *
get_window_for_gtk_widget(GtkWidget *widget)
{
  gpointer user_data;
  user_data = g_object_get_data(G_OBJECT(widget), "nsWindow");

  if (!user_data)
    return nsnull;

  return NS_STATIC_CAST(nsWindow *, user_data);
}

/* static */
nsWindow *
get_window_for_gdk_window(GdkWindow *window)
{
  gpointer user_data;
  user_data = g_object_get_data(G_OBJECT(window), "nsWindow");

  if (!user_data)
    return nsnull;
  
  return NS_STATIC_CAST(nsWindow *, user_data);
}

/* static */
nsWindow *
get_owning_window_for_gdk_window(GdkWindow *window)
{
  gpointer user_data;
  gdk_window_get_user_data(window, &user_data);

  if (!user_data)
    return nsnull;

  GtkWidget *owningWidget = GTK_WIDGET(user_data);
  
  if (!owningWidget)
    return nsnull;

  user_data = NULL;
  user_data = g_object_get_data(G_OBJECT(owningWidget), "nsWindow");
  
  if (!user_data)
    return nsnull;

  return (nsWindow *)user_data;
}

// gtk callbacks

/* static */
gboolean
expose_event_cb(GtkWidget *widget, GdkEventExpose *event)
{
  nsWindow *window = get_window_for_gdk_window(event->window);
  if (!window)
    return FALSE;

  // XXX We are so getting lucky here.  We are doing all of
  // mozilla's painting and then allowing default processing to occur.
  // This means that Mozilla paints in all of it's stuff and then
  // NO_WINDOW widgets (like scrollbars, for example) are painted by
  // Gtk on top of what we painted.

  // This return window->OnExposeEvent(widget, event); */

  window->OnExposeEvent(widget, event);
  return FALSE;
}

/* static */
gboolean
configure_event_cb(GtkWidget *widget,
		   GdkEventConfigure *event)
{
  nsWindow *window = get_window_for_gtk_widget(widget);
  if (!window)
    return FALSE;
  
  return window->OnConfigureEvent(widget, event);
}

/* static */
void
size_allocate_cb (GtkWidget *widget, GtkAllocation *allocation)
{
  nsWindow *window = get_window_for_gtk_widget(widget);
  if (!window)
    return;

  window->OnSizeAllocate(widget, allocation);
}

/* static */
gboolean
delete_event_cb(GtkWidget *widget, GdkEventAny *event)
{
  nsWindow *window = get_window_for_gtk_widget(widget);
  if (!window)
    return FALSE;

  window->OnDeleteEvent(widget, event);

  return TRUE;
}

/* static */
gboolean
enter_notify_event_cb (GtkWidget *widget,
				GdkEventCrossing *event)
{
  nsWindow *window = get_window_for_gdk_window(event->window);
  if (!window)
    return TRUE;
  
  window->OnEnterNotifyEvent(widget, event);

  return TRUE;
}

/* static */
gboolean
leave_notify_event_cb (GtkWidget *widget,
		       GdkEventCrossing *event)
{
  nsWindow *window = get_window_for_gdk_window(event->window);
  if (!window)
    return TRUE;
  
  window->OnLeaveNotifyEvent(widget, event);

  return TRUE;
}

/* static */
gboolean
motion_notify_event_cb (GtkWidget *widget, GdkEventMotion *event)
{
  nsWindow *window = get_window_for_gdk_window(event->window);
  if (!window)
    return TRUE;

  window->OnMotionNotifyEvent(widget, event);

  return TRUE;
}

/* static */
gboolean
button_press_event_cb   (GtkWidget *widget, GdkEventButton *event)
{
  printf("button_press_event_cb\n");
  nsWindow *window = get_window_for_gdk_window(event->window);
  if (!window)
    return TRUE;

  window->OnButtonPressEvent(widget, event);

  return TRUE;
}

/* static */
gboolean
button_release_event_cb (GtkWidget *widget, GdkEventButton *event)
{
  nsWindow *window = get_window_for_gdk_window(event->window);
  if (!window)
    return TRUE;

  window->OnButtonReleaseEvent(widget, event);

  return TRUE;
}

/* static */
gboolean
focus_in_event_cb (GtkWidget *widget, GdkEventFocus *event)
{
  nsWindow *window = get_window_for_gtk_widget(widget);
  if (!window)
    return FALSE;

  window->OnContainerFocusInEvent(widget, event);

  return FALSE;
}

/* static */
gboolean
focus_out_event_cb (GtkWidget *widget, GdkEventFocus *event)
{
  nsWindow *window = get_window_for_gtk_widget(widget);
  if (!window)
    return FALSE;

  window->OnContainerFocusOutEvent(widget, event);

  return FALSE;
}

/* static */
gboolean
key_press_event_cb (GtkWidget *widget, GdkEventKey *event)
{
  printf("key_press_event_cb\n");
  // find the window with focus and dispatch this event to that widget
  nsWindow *window = get_window_for_gtk_widget(widget);
  if (!window)
    return FALSE;

  nsWindow *focusWindow = window->mFocusChild;
  if (!focusWindow)
    focusWindow = window;

  return focusWindow->OnKeyPressEvent(widget, event);
}

gboolean
key_release_event_cb (GtkWidget *widget, GdkEventKey *event)
{
  printf("key_release_event_cb\n");
  // find the window with focus and dispatch this event to that widget
  nsWindow *window = get_window_for_gtk_widget(widget);
  if (!window)
    return FALSE;

  nsWindow *focusWindow = window->mFocusChild;
  if (!focusWindow)
    focusWindow = window;

  return focusWindow->OnKeyReleaseEvent(widget, event);
}

/* static */
gboolean
scroll_event_cb (GtkWidget *widget, GdkEventScroll *event)
{
  nsWindow *window = get_window_for_gdk_window(event->window);
  if (!window)
    return FALSE;

  window->OnScrollEvent(widget, event);

  return TRUE;
}

// nsChildWindow class

nsChildWindow::nsChildWindow()
{
}

nsChildWindow::~nsChildWindow()
{
}
