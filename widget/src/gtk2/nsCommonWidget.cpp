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

#include "nsCommonWidget.h"
#include "nsGtkKeyUtils.h"

nsCommonWidget::nsCommonWidget()
{
    mIsTopLevel       = PR_FALSE;
    mIsDestroyed      = PR_FALSE;
    mNeedsResize      = PR_FALSE;
    mListenForResizes = PR_FALSE;
    mIsShown          = PR_FALSE;
    mNeedsShow        = PR_FALSE;
    mEnabled          = PR_TRUE;
    mCreated          = PR_FALSE;
    mPlaced           = PR_FALSE;

    mPreferredWidth   = 0;
    mPreferredHeight  = 0;
}

nsCommonWidget::~nsCommonWidget()
{
}

nsIWidget *
nsCommonWidget::GetParent(void)
{
    nsIWidget *retval;
    retval = mParent;
    NS_IF_ADDREF(retval);
    return retval;
}

void
nsCommonWidget::CommonCreate(nsIWidget *aParent, PRBool aListenForResizes)
{
    mParent = aParent;
    mListenForResizes = aListenForResizes;
    mCreated = PR_TRUE;
}

void
nsCommonWidget::InitButtonEvent(nsMouseEvent &aEvent,
                                GdkEventButton *aGdkEvent)
{
    aEvent.point.x = nscoord(aGdkEvent->x);
    aEvent.point.y = nscoord(aGdkEvent->y);

    aEvent.isShift   = (aGdkEvent->state & GDK_SHIFT_MASK)
        ? PR_TRUE : PR_FALSE;
    aEvent.isControl = (aGdkEvent->state & GDK_CONTROL_MASK)
        ? PR_TRUE : PR_FALSE;
    aEvent.isAlt     = (aGdkEvent->state & GDK_MOD1_MASK)
        ? PR_TRUE : PR_FALSE;
    aEvent.isMeta    = (aGdkEvent->state & GDK_MOD4_MASK)
        ? PR_TRUE : PR_FALSE;

    switch (aGdkEvent->type) {
    case GDK_2BUTTON_PRESS:
        aEvent.clickCount = 2;
        break;
    case GDK_3BUTTON_PRESS:
        aEvent.clickCount = 3;
        break;
        // default is one click
    default:
        aEvent.clickCount = 1;
    }
}

void
nsCommonWidget::InitMouseScrollEvent(nsMouseScrollEvent &aEvent,
                                     GdkEventScroll *aGdkEvent)
{
    switch (aGdkEvent->direction) {
    case GDK_SCROLL_UP:
        aEvent.scrollFlags = nsMouseScrollEvent::kIsVertical;
        aEvent.delta = -3;
        break;
    case GDK_SCROLL_DOWN:
        aEvent.scrollFlags = nsMouseScrollEvent::kIsVertical;
        aEvent.delta = 3;
        break;
    case GDK_SCROLL_LEFT:
        aEvent.scrollFlags = nsMouseScrollEvent::kIsHorizontal;
        aEvent.delta = -3;
        break;
    case GDK_SCROLL_RIGHT:
        aEvent.scrollFlags = nsMouseScrollEvent::kIsHorizontal;
        aEvent.delta = 3;
        break;
    }

    aEvent.point.x = nscoord(aGdkEvent->x);
    aEvent.point.y = nscoord(aGdkEvent->y);

    aEvent.isShift   = (aGdkEvent->state & GDK_SHIFT_MASK)
        ? PR_TRUE : PR_FALSE;
    aEvent.isControl = (aGdkEvent->state & GDK_CONTROL_MASK)
        ? PR_TRUE : PR_FALSE;
    aEvent.isAlt     = (aGdkEvent->state & GDK_MOD1_MASK)
        ? PR_TRUE : PR_FALSE;
    aEvent.isMeta    = (aGdkEvent->state & GDK_MOD4_MASK)
        ? PR_TRUE : PR_FALSE;
    
}

void
nsCommonWidget::InitKeyEvent(nsKeyEvent &aEvent, GdkEventKey *aGdkEvent)
{
    aEvent.keyCode   = GdkKeyCodeToDOMKeyCode(aGdkEvent->keyval);
    aEvent.isShift   = (aGdkEvent->state & GDK_SHIFT_MASK)
        ? PR_TRUE : PR_FALSE;
    aEvent.isControl = (aGdkEvent->state & GDK_CONTROL_MASK)
        ? PR_TRUE : PR_FALSE;
    aEvent.isAlt     = (aGdkEvent->state & GDK_MOD1_MASK)
        ? PR_TRUE : PR_FALSE;
    aEvent.isMeta    = (aGdkEvent->state & GDK_MOD4_MASK)
        ? PR_TRUE : PR_FALSE;
    aEvent.time      = aGdkEvent->time;
}

void
nsCommonWidget::DispatchGotFocusEvent(void)
{
    nsGUIEvent event(NS_GOTFOCUS, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsCommonWidget::DispatchLostFocusEvent(void)
{
    nsGUIEvent event(NS_LOSTFOCUS, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsCommonWidget::DispatchActivateEvent(void)
{
    nsGUIEvent event(NS_ACTIVATE, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsCommonWidget::DispatchDeactivateEvent(void)
{
    nsGUIEvent event(NS_DEACTIVATE, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

void
nsCommonWidget::DispatchResizeEvent(nsRect &aRect, nsEventStatus &aStatus)
{
    nsSizeEvent event(NS_SIZE, this);

    event.windowSize = &aRect;
    event.point.x = aRect.x;
    event.point.y = aRect.y;
    event.mWinWidth = aRect.width;
    event.mWinHeight = aRect.height;

    nsEventStatus status;
    DispatchEvent(&event, status); 
}

NS_IMETHODIMP
nsCommonWidget::DispatchEvent(nsGUIEvent *aEvent,
                              nsEventStatus &aStatus)
{
    aStatus = nsEventStatus_eIgnore;

    // hold a widget reference while we dispatch this event
    NS_ADDREF(aEvent->widget);

    // send it to the standard callback
    if (mEventCallback)
        aStatus = (* mEventCallback)(aEvent);

    // dispatch to event listener if event was not consumed
    if ((aStatus != nsEventStatus_eIgnore) && mEventListener)
        aStatus = mEventListener->ProcessEvent(*aEvent);

    NS_IF_RELEASE(aEvent->widget);

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Show(PRBool aState)
{
    mIsShown = aState;

    LOG(("nsCommonWidget::Show [%p] state %d\n", (void *)this, aState));

    // Ok, someone called show on a window that isn't sized to a sane
    // value.  Mark this window as needing to have Show() called on it
    // and return.
    if ((aState && !AreBoundsSane()) || !mCreated) {
        LOG(("\tbounds are insane or window hasn't been created yet\n"));
        mNeedsShow = PR_TRUE;
        return NS_OK;
    }

    // If someone is hiding this widget, clear any needing show flag.
    if (!aState)
        mNeedsShow = PR_FALSE;

    // If someone is showing this window and it needs a resize then
    // resize the widget.
    if (aState && mNeedsResize) {
        LOG(("\tresizing\n"));
        NativeResize(mBounds.x, mBounds.y, mBounds.width, mBounds.height,
                     PR_FALSE);
    }

    NativeShow(aState);

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    if (!mCreated)
        return NS_OK;

    // There are several cases here that we need to handle, based on a
    // matrix of the visibility of the widget, the sanity of this resize
    // and whether or not the widget was previously sane.

    // Has this widget been set to visible?
    if (mIsShown) {
        // Are the bounds sane?
        if (AreBoundsSane()) {
            // Yep?  Resize the window
            //Maybe, the toplevel has moved
            if (mIsTopLevel)
                NativeResize(mBounds.x, mBounds.y,
                             mBounds.width, mBounds.height, aRepaint);
            else
                NativeResize(mBounds.width, mBounds.height, aRepaint);

            // Does it need to be shown because it was previously insane?
            if (mNeedsShow)
                NativeShow(PR_TRUE);
        }
        else {
            // If someone has set this so that the needs show flag is false
            // and it needs to be hidden, update the flag and hide the
            // window.  This flag will be cleared the next time someone
            // hides the window or shows it.  It also prevents us from
            // calling NativeShow(PR_FALSE) excessively on the window which
            // causes unneeded X traffic.
            if (!mNeedsShow) {
                mNeedsShow = PR_TRUE;
                NativeShow(PR_FALSE);
            }
        }
    }
    // If the widget hasn't been shown, mark the widget as needing to be
    // resized before it is shown.
    else {
        if (AreBoundsSane() && mListenForResizes) {
            // For widgets that we listen for resizes for (widgets created
            // with native parents) we apparently _always_ have to resize.  I
            // dunno why, but apparently we're lame like that.
            NativeResize(aWidth, aHeight, aRepaint);
        }
        else {
            mNeedsResize = PR_TRUE;
        }
    }

    // synthesize a resize event if this isn't a toplevel
    if (mIsTopLevel || mListenForResizes) {
        nsRect rect(mBounds.x, mBounds.y, aWidth, aHeight);
        nsEventStatus status;
        DispatchResizeEvent(rect, status);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                       PRBool aRepaint)
{
    mBounds.x = aX;
    mBounds.y = aY;
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    mPlaced = PR_TRUE;

    if (!mCreated)
        return NS_OK;

    // There are several cases here that we need to handle, based on a
    // matrix of the visibility of the widget, the sanity of this resize
    // and whether or not the widget was previously sane.

    // Has this widget been set to visible?
    if (mIsShown) {
        // Are the bounds sane?
        if (AreBoundsSane()) {
            // Yep?  Resize the window
            NativeResize(aX, aY, aWidth, aHeight, aRepaint);
            // Does it need to be shown because it was previously insane?
            if (mNeedsShow)
                NativeShow(PR_TRUE);
        }
        else {
            // If someone has set this so that the needs show flag is false
            // and it needs to be hidden, update the flag and hide the
            // window.  This flag will be cleared the next time someone
            // hides the window or shows it.  It also prevents us from
            // calling NativeShow(PR_FALSE) excessively on the window which
            // causes unneeded X traffic.
            if (!mNeedsShow) {
                mNeedsShow = PR_TRUE;
                NativeShow(PR_FALSE);
            }
        }
    }
    // If the widget hasn't been shown, mark the widget as needing to be
    // resized before it is shown
    else {
        if (AreBoundsSane() && mListenForResizes){
            // For widgets that we listen for resizes for (widgets created
            // with native parents) we apparently _always_ have to resize.  I
            // dunno why, but apparently we're lame like that.
            NativeResize(aX, aY, aWidth, aHeight, aRepaint);
        }
        else {
            mNeedsResize = PR_TRUE;
        }
    }

    if (mIsTopLevel || mListenForResizes) {
        // synthesize a resize event
        nsRect rect(aX, aY, aWidth, aHeight);
        nsEventStatus status;
        DispatchResizeEvent(rect, status);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::GetPreferredSize(PRInt32 &aWidth,
                                 PRInt32 &aHeight)
{
    aWidth  = mPreferredWidth;
    aHeight = mPreferredHeight;
    return (mPreferredWidth != 0 && mPreferredHeight != 0) ? 
        NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsCommonWidget::SetPreferredSize(PRInt32 aWidth,
                                 PRInt32 aHeight)
{
    mPreferredWidth  = aWidth;
    mPreferredHeight = aHeight;
    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Enable(PRBool aState)
{
    mEnabled = aState;

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::IsEnabled(PRBool *aState)
{
    *aState = mEnabled;

    return NS_OK;
}

void
nsCommonWidget::OnDestroy(void)
{
    if (mOnDestroyCalled)
        return;

    mOnDestroyCalled = PR_TRUE;

    // release references to children, device context, toolkit + app shell
    nsBaseWidget::OnDestroy();

    // let go of our parent
    mParent = nsnull;

    nsCOMPtr<nsIWidget> kungFuDeathGrip = this;

    nsGUIEvent event(NS_DESTROY, this);
    nsEventStatus status;
    DispatchEvent(&event, status);
}

PRBool
nsCommonWidget::AreBoundsSane(void)
{
    if (mBounds.width > 0 && mBounds.height > 0)
        return PR_TRUE;

    return PR_FALSE;
}
