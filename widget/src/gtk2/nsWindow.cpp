/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#include "nsWindow.h"
#include "nsToolkit.h"

#include <gtk/gtkwindow.h>
#include <gdk/gdkx.h>

nsWindow::nsWindow()
{
  mContainer       = nsnull;
  mDrawingarea     = nsnull;
  mIsTopLevel      = PR_FALSE;
  mWindowType      = eWindowType_child;
}

nsWindow::~nsWindow()
{
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
  return CommonCreate(aParent, nsnull, aRect, aHandleEventFunction,
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
  return CommonCreate(nsnull, aParent, aRect, aHandleEventFunction,
		      aContext, aAppShell, aToolkit, aInitData);
}

NS_IMETHODIMP
nsWindow::Destroy(void)
{
  return NS_ERROR_NOT_IMPLEMENTED; 
}

nsIWidget*
nsWindow::GetParent(void)
{
  nsIWidget *retval;
  retval = mParent;
  NS_IF_ADDREF(retval);
  return retval;
}

NS_IMETHODIMP
nsWindow::Show(PRBool aState)
{
  if (mIsTopLevel) {
    moz_drawingarea_set_visibility(mDrawingarea, aState);
    gtk_widget_show(GTK_WIDGET(mContainer));
    gtk_widget_show(mShell);
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
    gtk_widget_set_uposition(mShell, aX, aY);
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

  if (mIsTopLevel) {
    gtk_window_set_default_size(GTK_WINDOW(mShell), aWidth, aHeight);
    gtk_widget_set_size_request(GTK_WIDGET(mContainer), aWidth, aHeight);
  }

  moz_drawingarea_resize (mDrawingarea, aWidth, aHeight);

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

  if (mIsTopLevel) {
    gtk_window_set_default_size(GTK_WINDOW(mShell), aWidth, aHeight);
    gtk_widget_set_uposition(mShell, aX, aY);
    gtk_widget_set_size_request(GTK_WIDGET(mContainer), aWidth, aHeight);
    moz_drawingarea_resize(mDrawingarea, aWidth, aHeight);
  }
  else {
    moz_drawingarea_move_resize(mDrawingarea, aX, aY, aWidth, aHeight);
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
  return NS_ERROR_NOT_IMPLEMENTED;
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
nsWindow::DispatchEvent(nsGUIEvent*    event,
			nsEventStatus &aStatus)
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

nsresult
nsWindow::CommonCreate(nsIWidget        *aParent,
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

  // save a ref to our parent
  mParent = aParent;

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

  return NS_OK;
}

// nsChildWindow class

nsChildWindow::nsChildWindow()
{
}

nsChildWindow::~nsChildWindow()
{
}
