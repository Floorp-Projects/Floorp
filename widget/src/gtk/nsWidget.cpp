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
#include "nsIDeviceContext.h"
#include "nsIAppShell.h"
#include "nsGfxCIID.h"
#include "nsRepository.h"
#include <gdk/gdkx.h>

nsWidget::nsWidget()
{
    NS_INIT_REFCNT();
    mWidget = nsnull;
// we should probibly init more stuff to null
}

nsWidget::~nsWidget()
{
}

static NS_DEFINE_IID(kWidgetIID, NS_IWIDGET_IID);
NS_IMPL_QUERY_INTERFACE(nsWidget, kWidgetIID)
NS_IMPL_ADDREF(nsWidget)
NS_IMPL_RELEASE(nsWidget)

NS_METHOD nsWidget::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[])
{
    NS_NOTYETIMPLEMENTED("nsWidget::SetTooltips");
    return NS_OK;
}

NS_METHOD nsWidget::UpdateTooltips(nsRect* aNewTips[])
{
    NS_NOTYETIMPLEMENTED("nsWidget::UpdateTooltips");
    return NS_OK;
}

NS_METHOD nsWidget::RemoveTooltips()
{
    NS_NOTYETIMPLEMENTED("nsWidget::RemoveTooltips");
    return NS_OK;
}

NS_METHOD nsWidget::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
    NS_NOTYETIMPLEMENTED("nsWidget::WidgetToScreen");
    return NS_OK;
}

NS_METHOD nsWidget::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
    NS_NOTYETIMPLEMENTED("nsWidget::ScreenToWidget");
    return NS_OK;
}

NS_IMETHODIMP nsWidget::GetClientData(void*& aClientData)
{
  aClientData = mClientData;
  return NS_OK;
}

NS_IMETHODIMP nsWidget::SetClientData(void* aClientData)
{
  mClientData = aClientData;
  return NS_OK;
}

nsIWidget *nsWidget::GetParent(void)
{
    return mParent;
}

nsIEnumerator* nsWidget::GetChildren()
{
    NS_NOTYETIMPLEMENTED("nsWidget::GetChildren");
    return nsnull;
}

void nsWidget::AddChild(nsIWidget* aChild)
{
    NS_NOTYETIMPLEMENTED("nsWidget::AddChild");
}

void nsWidget::RemoveChild(nsIWidget* aChild)
{
    NS_NOTYETIMPLEMENTED("nsWidget::RemoveChild");
}

NS_METHOD nsWidget::Show(PRBool bState)
{
    if (bState) {
	if (mWidget) {
	    gtk_widget_show(mWidget);
	} else {
#ifdef DEBUG_shaver
	    g_print("showing a NULL-be-widgeted widget @ %p\n", this);
#endif
	    return NS_ERROR_NULL_POINTER;
	}
    } else {
	if (mWidget) {
	    gtk_widget_hide(mWidget);
	} else {
#ifdef DEBUG_shaver
	    g_print("hiding a NULL-be-widgeted widget @ %p\n", this);
#endif
	    return NS_ERROR_NULL_POINTER;
	}
    }
    mShown = bState;
    return NS_OK;
}

NS_METHOD nsWidget::IsVisible(PRBool &aState)
{
    aState = mShown;
    return NS_OK;
}

NS_METHOD nsWidget::Move(PRUint32 aX, PRUint32 aY)
{
    NS_NOTYETIMPLEMENTED("nsWidget::Move");
    return NS_OK;
}

NS_METHOD nsWidget::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
    NS_NOTYETIMPLEMENTED("nsWidget::Resize");
    return NS_OK;
}

NS_METHOD nsWidget::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth,
			   PRUint32 aHeight, PRBool aRepaint)
{
    NS_NOTYETIMPLEMENTED("nsWidget::Resize");
    return NS_OK;
}

NS_METHOD nsWidget::Enable(PRBool bState)
{
    gtk_widget_set_sensitive(mWidget, bState);
    return NS_OK;
}

NS_METHOD nsWidget::SetFocus(void)
{
    gtk_widget_grab_focus(mWidget);
    return NS_OK;
}

NS_METHOD nsWidget::GetBounds(nsRect &aRect)
{
    aRect = mBounds;
    return NS_OK;
}

nscolor nsWidget::GetForegroundColor(void)
{
    /* can we safely cache this? */
    return mForeground;
}

NS_METHOD nsWidget::SetForegroundColor(const nscolor &aColor)
{
    mForeground = aColor;
    NS_NOTYETIMPLEMENTED("nsWidget::SetForegroundColor");
    return NS_OK;
}

nscolor nsWidget::GetBackgroundColor(void)
{
    /* can we safely cache this? */
    return mBackground;
}

NS_METHOD nsWidget::SetBackgroundColor(const nscolor &aColor)
{
    mBackground = aColor;
    NS_NOTYETIMPLEMENTED("nsWidget::SetBackgroundColor");
    return NS_OK;
}

nsIFontMetrics *nsWidget::GetFont(void)
{
    NS_NOTYETIMPLEMENTED("nsWidget::GetFont");
    return nsnull;
}

NS_METHOD nsWidget::SetFont(const nsFont &aFont)
{
    NS_NOTYETIMPLEMENTED("nsWidget::SetFont");
    return NS_OK;
}

nsCursor nsWidget::GetCursor(void)
{
    NS_NOTYETIMPLEMENTED("nsWidget::GetCursor");
    return eCursor_standard;
}

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
	    gdk_window_set_cursor(mWidget->window, newCursor);
	}
    }
    return NS_OK;
}

NS_METHOD nsWidget::Invalidate(PRBool aIsSynchronous)
{
    NS_NOTYETIMPLEMENTED("nsWidget::Invalidate");
    return NS_OK;
}

NS_METHOD nsWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
    NS_NOTYETIMPLEMENTED("nsWidget::Invalidate");
    return NS_OK;
}

NS_METHOD nsWidget::Update(void)
{
    NS_NOTYETIMPLEMENTED("nsWidget::Update");
    return NS_OK;
}

void *nsWidget::GetNativeData(PRUint32 aDataType)
{
    switch(aDataType) {
      case NS_NATIVE_WINDOW:
	return (void *)mWidget->window;
      case NS_NATIVE_DISPLAY:
	return (void *)GDK_DISPLAY();
      case NS_NATIVE_WIDGET:
	return (void *)mWidget;
      case NS_NATIVE_GRAPHIC:
	  {
	      void *res;
	      if (mGC) {
		  res = mGC;
	      } else {
		  NS_ASSERTION(mToolkit, "unable to return NS_NATIVE_GRAPHIC");
		  res = (void *)mToolkit->GetSharedGC();
	      }
	      NS_ASSERTION(res, "unable to return NS_NATIVE_GRAPHIC");
	      return res;
	  }
      default:
	break;
    }
    return nsnull;
}

nsIToolkit *nsWidget::GetToolkit(void)
{
    return (nsIToolkit *)mToolkit;
}

NS_METHOD nsWidget::SetColorMap(nsColorMap *aColorMap)
{
    return NS_OK;
}

nsIDeviceContext* nsWidget::GetDeviceContext(void)
{
    NS_IF_ADDREF(mDeviceContext);
    return mDeviceContext;
}

nsIAppShell* nsWidget::GetAppShell(void)
{
    NS_IF_ADDREF(mAppShell);
    return mAppShell;
}

NS_METHOD nsWidget::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
    NS_NOTYETIMPLEMENTED("nsWidget::Scroll");
    return NS_OK;
}

NS_METHOD nsWidget::SetBorderStyle(nsBorderStyle aBorderStyle)
{
    NS_NOTYETIMPLEMENTED("nsWidget::SetBorderStyle");
    return NS_OK;
}

NS_METHOD nsWidget::SetTitle(const nsString& aTitle)
{
    NS_NOTYETIMPLEMENTED("nsWidget::SetTitle");
    return NS_OK;
}

NS_METHOD nsWidget::AddMouseListener(nsIMouseListener *aListener)
{
    NS_NOTYETIMPLEMENTED("nsWidget::AddMouseListener");
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

NS_METHOD nsWidget::GetBorderSize(PRInt32 &aWidth, PRInt32 &aHeight)
{
    nsRect rectWin;
    nsRect rectClient;
    GetBounds(rectWin);
    GetClientBounds(rectClient);

    aWidth  = rectWin.width - rectClient.width;
    aHeight = rectWin.height - rectClient.height;

    return NS_OK;
}

NS_METHOD nsWidget::GetClientBounds(nsRect &aRect)
{
    return GetBounds(aRect);
}

NS_METHOD nsWidget::SetMenuBar(nsIMenuBar * aMenuBar)
{
    NS_NOTYETIMPLEMENTED("nsWidget::SetMenuBar");
    return NS_OK;
}

void nsWidget::InitToolkit(nsIToolkit *aToolkit,
                           nsIWidget  *aWidgetParent)
{
  if (nsnull == mToolkit) {
    if (nsnull != aToolkit) {
      mToolkit = (nsToolkit*)aToolkit;
      mToolkit->AddRef();
    }
    else {
      if (nsnull != aWidgetParent) {
        mToolkit = (nsToolkit*)(aWidgetParent->GetToolkit()); // the call AddRef's, we don't have to
      }
      // it's some top level window with no toolkit passed in.
      // Create a default toolkit with the current thread
      else {
        mToolkit = new nsToolkit();
        mToolkit->AddRef();
        mToolkit->Init(PR_GetCurrentThread());

        // Create a shared GC for all widgets
        ((nsToolkit *)mToolkit)->SetSharedGC((GdkGC*)GetNativeData(NS_NATIVE_GRAPHIC));
      }
    }
  }
}

void nsWidget::InitDeviceContext(nsIDeviceContext *aContext,
                                 GtkWidget *aParentWidget)
{
  // keep a reference to the toolkit object
  if (aContext) {
    mDeviceContext = aContext;
    mDeviceContext->AddRef();
  }
  else {
    nsresult  res;

    static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);
    static NS_DEFINE_IID(kDeviceContextIID, NS_IDEVICE_CONTEXT_IID);

    //res = !NS_OK;
    res = nsRepository::CreateInstance(kDeviceContextCID,
                                       nsnull,
                                       kDeviceContextIID,
                                       (void **)&mDeviceContext);
    if (NS_OK == res) {
      mDeviceContext->Init(aParentWidget);
    }
  }
}

void nsWidget::InitCallbacks(char *aName)
{
    NS_NOTYETIMPLEMENTED("nsWidget::InitCallbacks");
}

nsIRenderingContext* nsWidget::GetRenderingContext()
{
    nsIRenderingContext * ctx = nsnull;

    if (GetNativeData(NS_NATIVE_WIDGET)) {

	nsresult  res;

	static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
	static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

	res = nsRepository::CreateInstance(kRenderingContextCID, nsnull,
					   kRenderingContextIID,
					   (void **)&ctx);

	if (NS_OK == res)
	    ctx->Init(mDeviceContext, this);

	NS_ASSERTION(NULL != ctx, "Null rendering context");
    }

    return ctx;
}

void nsWidget::ConvertToDeviceCoordinates(nscoord &aX, nscoord &aY)
{

}

NS_IMETHODIMP nsWidget::DispatchEvent(nsGUIEvent *event,
				      nsEventStatus &aStatus)
{
    return NS_OK;
}

NS_IMETHODIMP nsWidget::DispatchMouseEvent(nsMouseEvent& aEvent)
{
    return NS_OK;
}
