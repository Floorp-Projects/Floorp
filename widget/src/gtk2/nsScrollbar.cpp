/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:expandtab:shiftwidth=4:tabstop=4:
 */
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

#include "nsScrollbar.h"
#include "mozcontainer.h"
#include <gdk/gdkx.h>
#include <gtk/gtkhscrollbar.h>
#include <gtk/gtkvscrollbar.h>

/* callbacks from widgets */
static void value_changed_cb (GtkAdjustment *adjustment, gpointer data);

nsScrollbar::nsScrollbar(PRBool aIsVertical)
{
    if (aIsVertical)
        mOrientation = GTK_ORIENTATION_VERTICAL;
    else
        mOrientation = GTK_ORIENTATION_HORIZONTAL;

    mWidget     = nsnull;
    mAdjustment = nsnull;
    mMaxRange   = 0;
    mThumbSize  = 0;
}

nsScrollbar::~nsScrollbar()
{
}

NS_IMPL_ADDREF_INHERITED(nsScrollbar, nsCommonWidget)
NS_IMPL_RELEASE_INHERITED(nsScrollbar, nsCommonWidget)
NS_IMPL_QUERY_INTERFACE2(nsScrollbar, nsIScrollbar, nsIWidget)

// nsIWidget

NS_IMETHODIMP
nsScrollbar::Create(nsIWidget        *aParent,
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
nsScrollbar::Create(nsNativeWidget aParent,
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
nsScrollbar::Destroy(void)
{
    if (mIsDestroyed || !mCreated)
        return NS_OK;

    LOG(("nsScrollbar::Destroy [%p]\n", (void *)this));
    mIsDestroyed = PR_TRUE;
    mCreated = PR_FALSE;

    NativeShow(PR_FALSE);

    if (mWidget) {
        gtk_widget_destroy(mWidget);
        mWidget = NULL;
        // this is just a ref into the widget above
        mAdjustment = NULL;
    }

    OnDestroy();

    return NS_OK;
}

NS_IMETHODIMP
nsScrollbar::IsVisible(PRBool & aState)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::Move(PRInt32 aX, PRInt32 aY)
{
    LOG(("nsScrollbar::Move [%p] %d %d\n", (void *)this,
         aX, aY));

    if (aX == mBounds.x && aY == mBounds.y)
        return NS_OK;

    mBounds.x = aX;
    mBounds.y = aY;

    // If the bounds aren't sane then don't actually move the window.
    // It will be moved to the proper position when the bounds become
    // sane.
    if (AreBoundsSane())
        moz_container_move(MOZ_CONTAINER(gtk_widget_get_parent(mWidget)),
                           mWidget, aX, aY, mBounds.width, mBounds.height);

    return NS_OK;
}

NS_IMETHODIMP
nsScrollbar::Enable(PRBool aState)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::SetFocus(PRBool aRaise)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsIFontMetrics*
nsScrollbar::GetFont(void)
{
    return nsnull;
}

NS_IMETHODIMP
nsScrollbar::SetFont(const nsFont &aFont)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::Invalidate(PRBool aIsSynchronous)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::Update()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::SetColorMap(nsColorMap *aColorMap)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

void*
nsScrollbar::GetNativeData(PRUint32 aDataType)
{
    return nsnull;
}

NS_IMETHODIMP
nsScrollbar::SetTitle(const nsString& aTitle)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::SetMenuBar(nsIMenuBar * aMenuBar)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::ShowMenuBar(PRBool aShow)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::BeginResizingChildren(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::EndResizingChildren(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsScrollbar::CaptureRollupEvents(nsIRollupListener * aListener,
                                 PRBool aDoCapture,
                                 PRBool aConsumeRollupEvent)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


// nsIScrollbar

NS_IMETHODIMP
nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
    mMaxRange = aEndRange;

    UpdateAdjustment();

    return NS_OK;
}

NS_IMETHODIMP
nsScrollbar::GetMaxRange(PRUint32& aMaxRange)
{
    aMaxRange = mMaxRange;
    return NS_OK;
}


NS_IMETHODIMP
nsScrollbar::SetPosition(PRUint32 aPos)
{
    if (mAdjustment && (PRUint32)mAdjustment->value != aPos) {
        mAdjustment->value = (gdouble)aPos;
        UpdateAdjustment();
    }

    return NS_OK;
}


NS_IMETHODIMP
nsScrollbar::GetPosition(PRUint32& aPos)
{
    if (mAdjustment)
        aPos = (PRUint32)mAdjustment->value;
    else
        aPos = 0;

    return NS_OK;
}


NS_IMETHODIMP
nsScrollbar::SetThumbSize(PRUint32 aSize)
{
    mThumbSize = aSize;

    if (mAdjustment) {
        mAdjustment->page_increment = aSize;
        UpdateAdjustment();
    }

    return NS_OK;
}


NS_IMETHODIMP
nsScrollbar::GetThumbSize(PRUint32& aSize)
{
    aSize = mThumbSize;
    return NS_OK;
}


NS_IMETHODIMP
nsScrollbar::SetLineIncrement(PRUint32 aSize)
{
    if (mAdjustment) {
        mAdjustment->step_increment = aSize;
        UpdateAdjustment();
    }

    return NS_OK;
}


NS_IMETHODIMP
nsScrollbar::GetLineIncrement(PRUint32& aSize)
{
    if (mAdjustment)
        aSize = (PRUint32)mAdjustment->step_increment;
    else 
        aSize = 0;

    return NS_OK;
}


NS_IMETHODIMP
nsScrollbar::SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
               PRUint32 aPosition, PRUint32 aLineIncrement)
{
    mMaxRange = aMaxRange;
    mThumbSize = aThumbSize;

    if (mAdjustment) {
        mAdjustment->lower = 0;
        mAdjustment->page_increment = aThumbSize;
        mAdjustment->step_increment = aLineIncrement;

        UpdateAdjustment();
    }

    return NS_OK;
}


// we assume that this will ONLY be called when updating from scrolling
NS_IMETHODIMP
nsScrollbar::SetBounds (const nsRect &aRect)
{
    LOG(("nsScrollbar::SetBounds [%p] %d %d %d %d\n",
         (void *)this, aRect.x, aRect.y,
         aRect.width, aRect.height));

    if (mWidget) {
        LOG(("widget allocation %d %d %d %d\n",
             mWidget->allocation.x,
             mWidget->allocation.y,
             mWidget->allocation.width,
             mWidget->allocation.height));
        nsCommonWidget::SetBounds(aRect);

        // update the x/y with our new sizes
        mWidget->allocation.x = aRect.x;
        mWidget->allocation.y = aRect.y;

        moz_container_scroll_update (MOZ_CONTAINER(gtk_widget_get_parent(mWidget)),
                                     mWidget, aRect.x, aRect.y);
    }
    return NS_OK;
}

nsresult
nsScrollbar::NativeCreate(nsIWidget        *aParent,
                          nsNativeWidget    aNativeParent,
                          const nsRect     &aRect,
                          EVENT_CALLBACK    aHandleEventFunction,
                          nsIDeviceContext *aContext,
                          nsIAppShell      *aAppShell,
                          nsIToolkit       *aToolkit,
                          nsWidgetInitData *aInitData)
{
    // initialize all the common bits of this class
    BaseCreate(aParent, aRect, aHandleEventFunction, aContext,
               aAppShell, aToolkit, aInitData);

    // Do we need to listen for resizes?
    PRBool listenForResizes = PR_FALSE;
    if (aNativeParent || (aInitData && aInitData->mListenForResizes))
        listenForResizes = PR_TRUE;

    // and do our common creation
    CommonCreate(aParent, listenForResizes);

    // save our bounds
    mBounds = aRect;

    // find our parent window
    GdkWindow *parentWindow;
    if (aParent)
        parentWindow = GDK_WINDOW(aParent->GetNativeData(NS_NATIVE_WINDOW));
    else
        parentWindow = GDK_WINDOW(aNativeParent);

    if (!parentWindow)
        return NS_ERROR_FAILURE;

    // use the parent window to find the parent widget
    gpointer user_data;
    gdk_window_get_user_data(parentWindow, &user_data);

    if (!user_data)
        return NS_ERROR_FAILURE;

    // our parent widget
    GtkWidget *parentWidget = GTK_WIDGET(user_data);

    // create the right widget
    if (mOrientation == GTK_ORIENTATION_VERTICAL)
        mWidget = gtk_vscrollbar_new(NULL);
    else
        mWidget = gtk_hscrollbar_new(NULL);

    // set the parent of the scrollbar to be the gdk window of the
    // parent window.
    gtk_widget_set_parent_window(mWidget, parentWindow);

    // add this widget to the parent window, assuming it's a container
    // of course.
    moz_container_put(MOZ_CONTAINER(parentWidget), mWidget,
                      mBounds.x, mBounds.y);

    // and realize the widget
    gtk_widget_realize(mWidget);

    // resize so that everything is set to the right dimensions
    Resize(mBounds.width, mBounds.height, PR_FALSE);

    // get a handle to the adjustment for later use - this doesn't
    // addref
    mAdjustment = gtk_range_get_adjustment(GTK_RANGE(mWidget));

    // add a label so we can get back here if we need to
    g_object_set_data(G_OBJECT(mAdjustment), "nsScrollbar",
                      this);

    // add a callback so we will get value changes
    g_signal_connect(G_OBJECT(mAdjustment), "value_changed",
                     G_CALLBACK(value_changed_cb), this);

    LOG(("nsScrollbar [%p] %s %p %lx\n", (void *)this,
         (mOrientation == GTK_ORIENTATION_VERTICAL)
         ? "vertical" : "horizontal", 
         (void *)mWidget, GDK_WINDOW_XWINDOW(mWidget->window)));
    LOG(("\tparent was %p %lx\n", (void *)parentWindow,
         GDK_WINDOW_XWINDOW(parentWindow)));

    return NS_OK;
}

void
nsScrollbar::NativeResize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
    LOG(("nsScrollbar::NativeResize [%p] %d %d\n", (void *)this,
         aWidth, aHeight));
    // clear our resize flag
    mNeedsResize = PR_FALSE;

    moz_container_move(MOZ_CONTAINER(gtk_widget_get_parent(mWidget)),
                       mWidget, mBounds.x, mBounds.y, aWidth, aHeight);
}

void
nsScrollbar::NativeResize(PRInt32 aX, PRInt32 aY,
                          PRInt32 aWidth, PRInt32 aHeight,
                          PRBool  aRepaint)
{
    LOG(("nsScrollbar::NativeResize [%p] %d %d %d %d\n", (void *)this,
         aX, aY, aWidth, aHeight));
    // clear our resize flag
    mNeedsResize = PR_FALSE;

    moz_container_move(MOZ_CONTAINER(gtk_widget_get_parent(mWidget)),
                       mWidget, aX, aY, aWidth, aHeight);
}

void
nsScrollbar::NativeShow (PRBool  aAction)
{
    LOG(("nsScrollbar::NativeShow [%p] %d\n", (void *)this, aAction));

    if (aAction) {
        // unset our flag now that our window has been shown
        mNeedsShow = PR_FALSE;
        gtk_widget_show(mWidget);
    }
    else {
        gtk_widget_hide(mWidget);
    }
}

void
nsScrollbar::OnValueChanged(void)
{
    LOG(("nsScrollbar::OnValueChanged [%p]\n", (void *)this));
    nsScrollbarEvent event;
    InitScrollbarEvent(event, NS_SCROLLBAR_POS);
    event.position = (PRUint32)mAdjustment->value;

    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsScrollbar::UpdateAdjustment(void)
{
    if (!mAdjustment)
        return;

    // Infinity in Mozilla is measured by setting the max range and
    // the thumb size to zero.  In the adjustment, this doesn't work.
    // Just set the upper bounds and the page size to one to get an
    // infinite scrollbar.
    if (mMaxRange == 0 && mThumbSize == 0) {
        mAdjustment->upper = 1;
        mAdjustment->page_size = 1;
    }
    else {
        mAdjustment->upper = mMaxRange;
        mAdjustment->page_size = mThumbSize;
    }

    LOG(("nsScrollbar::UpdateAdjustment [%p] upper: %d page_size %d\n",
         (void *)this, mAdjustment->upper, mAdjustment->page_size));

    gtk_adjustment_changed(mAdjustment);
}

/* static */
void
value_changed_cb (GtkAdjustment *adjustment, gpointer data)
{
    nsScrollbar *scrollbar = NS_STATIC_CAST(nsScrollbar *, data);

    scrollbar->OnValueChanged();
}

