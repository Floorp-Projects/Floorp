/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *  Zack Rusin <zack@kde.org>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Lars Knoll <knoll@kde.org>
 *   Zack Rusin <zack@kde.org>
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

#include "nsGUIEvent.h"
#include "nsQtEventDispatcher.h"
#include "nsIRenderingContext.h"
#include "nsIServiceManager.h"
#include "nsGfxCIID.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"

#include "mozqwidget.h"

#include <qapplication.h>
#include <qdesktopwidget.h>
#include <qwidget.h>
#include <qcursor.h>
#include <execinfo.h>
#include <stdlib.h>

#include <execinfo.h>
#include <stdlib.h>

static const int WHEEL_DELTA = 120;
static const int kWindowPositionSlop = 20;

struct nsKeyConverter
{
    int vkCode; // Platform independent key code
    int keysym; // Qt key code
};

static void backTrace()
{
    int levels = -1;
    QString s;
    void* trace[256];
    int n = backtrace(trace, 256);
    if (!n)
	return;
    char** strings = backtrace_symbols (trace, n);

    if ( levels != -1 )
        n = QMIN( n, levels );
    s = "[\n";

    for (int i = 0; i < n; ++i)
        s += QString::number(i) +
             QString::fromLatin1(": ") +
             QString::fromLatin1(strings[i]) + QString::fromLatin1("\n");
    s += "]\n";
    if (strings)
        free (strings);
    qDebug("stacktrace:\n%s", s.latin1());
}

static struct nsKeyConverter nsKeycodes[] =
{
//  { NS_VK_CANCEL,        Qt::Key_Cancel },
    { NS_VK_BACK,          Qt::Key_BackSpace },
    { NS_VK_TAB,           Qt::Key_Tab },
//  { NS_VK_CLEAR,         Qt::Key_Clear },
    { NS_VK_RETURN,        Qt::Key_Return },
    { NS_VK_RETURN,        Qt::Key_Enter },
    { NS_VK_SHIFT,         Qt::Key_Shift },
    { NS_VK_CONTROL,       Qt::Key_Control },
    { NS_VK_ALT,           Qt::Key_Alt },
    { NS_VK_PAUSE,         Qt::Key_Pause },
    { NS_VK_CAPS_LOCK,     Qt::Key_CapsLock },
    { NS_VK_ESCAPE,        Qt::Key_Escape },
    { NS_VK_SPACE,         Qt::Key_Space },
    { NS_VK_PAGE_UP,       Qt::Key_PageUp },
    { NS_VK_PAGE_DOWN,     Qt::Key_PageDown },
    { NS_VK_END,           Qt::Key_End },
    { NS_VK_HOME,          Qt::Key_Home },
    { NS_VK_LEFT,          Qt::Key_Left },
    { NS_VK_UP,            Qt::Key_Up },
    { NS_VK_RIGHT,         Qt::Key_Right },
    { NS_VK_DOWN,          Qt::Key_Down },
    { NS_VK_PRINTSCREEN,   Qt::Key_Print },
    { NS_VK_INSERT,        Qt::Key_Insert },
    { NS_VK_DELETE,        Qt::Key_Delete },

    { NS_VK_0,             Qt::Key_0 },
    { NS_VK_1,             Qt::Key_1 },
    { NS_VK_2,             Qt::Key_2 },
    { NS_VK_3,             Qt::Key_3 },
    { NS_VK_4,             Qt::Key_4 },
    { NS_VK_5,             Qt::Key_5 },
    { NS_VK_6,             Qt::Key_6 },
    { NS_VK_7,             Qt::Key_7 },
    { NS_VK_8,             Qt::Key_8 },
    { NS_VK_9,             Qt::Key_9 },

    { NS_VK_SEMICOLON,     Qt::Key_Semicolon },
    { NS_VK_EQUALS,        Qt::Key_Equal },

    { NS_VK_A,             Qt::Key_A },
    { NS_VK_B,             Qt::Key_B },
    { NS_VK_C,             Qt::Key_C },
    { NS_VK_D,             Qt::Key_D },
    { NS_VK_E,             Qt::Key_E },
    { NS_VK_F,             Qt::Key_F },
    { NS_VK_G,             Qt::Key_G },
    { NS_VK_H,             Qt::Key_H },
    { NS_VK_I,             Qt::Key_I },
    { NS_VK_J,             Qt::Key_J },
    { NS_VK_K,             Qt::Key_K },
    { NS_VK_L,             Qt::Key_L },
    { NS_VK_M,             Qt::Key_M },
    { NS_VK_N,             Qt::Key_N },
    { NS_VK_O,             Qt::Key_O },
    { NS_VK_P,             Qt::Key_P },
    { NS_VK_Q,             Qt::Key_Q },
    { NS_VK_R,             Qt::Key_R },
    { NS_VK_S,             Qt::Key_S },
    { NS_VK_T,             Qt::Key_T },
    { NS_VK_U,             Qt::Key_U },
    { NS_VK_V,             Qt::Key_V },
    { NS_VK_W,             Qt::Key_W },
    { NS_VK_X,             Qt::Key_X },
    { NS_VK_Y,             Qt::Key_Y },
    { NS_VK_Z,             Qt::Key_Z },

    { NS_VK_NUMPAD0,       Qt::Key_0 },
    { NS_VK_NUMPAD1,       Qt::Key_1 },
    { NS_VK_NUMPAD2,       Qt::Key_2 },
    { NS_VK_NUMPAD3,       Qt::Key_3 },
    { NS_VK_NUMPAD4,       Qt::Key_4 },
    { NS_VK_NUMPAD5,       Qt::Key_5 },
    { NS_VK_NUMPAD6,       Qt::Key_6 },
    { NS_VK_NUMPAD7,       Qt::Key_7 },
    { NS_VK_NUMPAD8,       Qt::Key_8 },
    { NS_VK_NUMPAD9,       Qt::Key_9 },
    { NS_VK_MULTIPLY,      Qt::Key_Asterisk },
    { NS_VK_ADD,           Qt::Key_Plus },
//  { NS_VK_SEPARATOR,     Qt::Key_Separator },
    { NS_VK_SUBTRACT,      Qt::Key_Minus },
    { NS_VK_DECIMAL,       Qt::Key_Period },
    { NS_VK_DIVIDE,        Qt::Key_Slash },
    { NS_VK_F1,            Qt::Key_F1 },
    { NS_VK_F2,            Qt::Key_F2 },
    { NS_VK_F3,            Qt::Key_F3 },
    { NS_VK_F4,            Qt::Key_F4 },
    { NS_VK_F5,            Qt::Key_F5 },
    { NS_VK_F6,            Qt::Key_F6 },
    { NS_VK_F7,            Qt::Key_F7 },
    { NS_VK_F8,            Qt::Key_F8 },
    { NS_VK_F9,            Qt::Key_F9 },
    { NS_VK_F10,           Qt::Key_F10 },
    { NS_VK_F11,           Qt::Key_F11 },
    { NS_VK_F12,           Qt::Key_F12 },
    { NS_VK_F13,           Qt::Key_F13 },
    { NS_VK_F14,           Qt::Key_F14 },
    { NS_VK_F15,           Qt::Key_F15 },
    { NS_VK_F16,           Qt::Key_F16 },
    { NS_VK_F17,           Qt::Key_F17 },
    { NS_VK_F18,           Qt::Key_F18 },
    { NS_VK_F19,           Qt::Key_F19 },
    { NS_VK_F20,           Qt::Key_F20 },
    { NS_VK_F21,           Qt::Key_F21 },
    { NS_VK_F22,           Qt::Key_F22 },
    { NS_VK_F23,           Qt::Key_F23 },
    { NS_VK_F24,           Qt::Key_F24 },

    { NS_VK_NUM_LOCK,      Qt::Key_NumLock },
    { NS_VK_SCROLL_LOCK,   Qt::Key_ScrollLock },

    { NS_VK_COMMA,         Qt::Key_Comma },
    { NS_VK_PERIOD,        Qt::Key_Period },
    { NS_VK_SLASH,         Qt::Key_Slash },
    { NS_VK_BACK_QUOTE,    Qt::Key_QuoteLeft },
    { NS_VK_OPEN_BRACKET,  Qt::Key_ParenLeft },
    { NS_VK_CLOSE_BRACKET, Qt::Key_ParenRight },
    { NS_VK_QUOTE,         Qt::Key_QuoteDbl },

    { NS_VK_META,          Qt::Key_Meta }
};

static PRInt32 NS_GetKey(PRInt32 aKey)
{
    PRInt32 length = sizeof(nsKeycodes) / sizeof(nsKeyConverter);

    for (PRInt32 i = 0; i < length; i++) {
        if (nsKeycodes[i].keysym == aKey) {
            return nsKeycodes[i].vkCode;
        }
    }
    return 0;
}

static PRBool
isContextMenuKey(const nsKeyEvent &aKeyEvent)
{
    return ((aKeyEvent.keyCode == NS_VK_F10 && aKeyEvent.isShift &&
             !aKeyEvent.isControl && !aKeyEvent.isMeta && !aKeyEvent.isAlt) ||
            (aKeyEvent.keyCode == NS_VK_CONTEXT_MENU && !aKeyEvent.isShift &&
             !aKeyEvent.isControl && !aKeyEvent.isMeta && !aKeyEvent.isAlt));
}

static void
keyEventToContextMenuEvent(const nsKeyEvent* aKeyEvent,
                           nsMouseEvent* aCMEvent)
{
    memcpy(aCMEvent, aKeyEvent, sizeof(nsInputEvent));
    aCMEvent->message = NS_CONTEXTMENU_KEY;
    aCMEvent->isShift = aCMEvent->isControl = PR_FALSE;
    aCMEvent->isAlt = aCMEvent->isMeta = PR_FALSE;
    aCMEvent->clickCount = 0;
    aCMEvent->acceptActivation = PR_FALSE;
}

nsCommonWidget::nsCommonWidget()
    : mContainer(0),
      mWidget(0),
      mListenForResizes(PR_FALSE),
      mNeedsResize(PR_FALSE),
      mNeedsShow(PR_FALSE),
      mIsShown(PR_FALSE)
{
}

NS_IMPL_ISUPPORTS_INHERITED0(nsCommonWidget,nsBaseWidget)
nsCommonWidget::~nsCommonWidget()
{
    delete mDispatcher;
    mDispatcher = 0;
}

void
nsCommonWidget::Initialize(QWidget *widget)
{
    Q_ASSERT(widget);

    mWidget = widget;
    mWidget->setMouseTracking(PR_TRUE);
    mWidget->setFocusPolicy(QWidget::WheelFocus);
}

NS_IMETHODIMP
nsCommonWidget::Show(PRBool aState)
{
    mIsShown = aState;

    // Ok, someone called show on a window that isn't sized to a sane
    // value.  Mark this window as needing to have Show() called on it
    // and return.
    if ((aState && !AreBoundsSane()) || !mWidget) {
        qDebug("XX Bounds are insane or window hasn't been created yet");
        mNeedsShow = PR_TRUE;
        return NS_OK;
    }

    // If someone is hiding this widget, clear any needing show flag.
    if (!aState)
        mNeedsShow = PR_FALSE;

    // If someone is showing this window and it needs a resize then
    // resize the widget.
    if (aState && mNeedsResize) {
        NativeResize(mBounds.width, mBounds.height,
                     PR_FALSE);
    }

    NativeShow(aState);

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::IsVisible(PRBool &visible)
{
    if (mWidget)
        visible = mWidget->isVisible();
    else
        visible = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY)
{
    if (mContainer) {
        PRInt32 screenWidth  = QApplication::desktop()->width();
        PRInt32 screenHeight = QApplication::desktop()->height();
        if (aAllowSlop) {
            if (*aX < (kWindowPositionSlop - mBounds.width))
                *aX = kWindowPositionSlop - mBounds.width;
            if (*aX > (screenWidth - kWindowPositionSlop))
                *aX = screenWidth - kWindowPositionSlop;
            if (*aY < (kWindowPositionSlop - mBounds.height))
                *aY = kWindowPositionSlop - mBounds.height;
            if (*aY > (screenHeight - kWindowPositionSlop))
                *aY = screenHeight - kWindowPositionSlop;
        } else {
            if (*aX < 0)
                *aX = 0;
            if (*aX > (screenWidth - mBounds.width))
                *aX = screenWidth - mBounds.width;
            if (*aY < 0)
                *aY = 0;
            if (*aY > (screenHeight - mBounds.height))
                *aY = screenHeight - mBounds.height;
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Move(PRInt32 x, PRInt32 y)
{
    if (x == mBounds.x && y == mBounds.y)
        return NS_OK;

    mBounds.x = x;
    mBounds.y = y;

    QPoint pos(x, y);
    if (mContainer) {
        if (mContainer->isPopup() && mContainer->parentWidget()) {
            pos = mContainer->parentWidget()->mapToGlobal(pos);
        }
        qDebug("pos is [%d,%d]", pos.x(), pos.y());
        mContainer->move(pos);
    }
    else if (mWidget)
        mWidget->move(pos);

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Resize(PRInt32 aWidth,
                       PRInt32 aHeight,
                       PRBool  aRepaint)
{
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    if (!mWidget || (mWidget->width() == aWidth &&
                     mWidget->height() == aHeight))
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
            if (mContainer || mNeedsShow)
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
    if (mContainer || mListenForResizes) {
        nsRect rect(mBounds.x, mBounds.y, aWidth, aHeight);
        nsEventStatus status;
        DispatchResizeEvent(rect, status);
    }

    return NS_OK;
}

/*
 * XXXX: This sucks because it basically hardcore duplicates the
 *       code from the above function.
 */
NS_IMETHODIMP
nsCommonWidget::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                       PRBool aRepaint)
{
    mBounds.x = aX;
    mBounds.y = aY;
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    if (!mWidget || (mWidget->x() == aX &&
                     mWidget->y() == aY &&
                     mWidget->height() == aHeight &&
                     mWidget->width() == aWidth))
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

    // synthesize a resize event if this isn't a toplevel
    if (mContainer || mListenForResizes) {
        nsRect rect(mBounds.x, mBounds.y, mBounds.width, mBounds.height);
        nsEventStatus status;
        DispatchResizeEvent(rect, status);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Enable(PRBool aState)
{
    if (mWidget)
        mWidget->setEnabled(aState);

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::IsEnabled(PRBool *aState)
{
    if (mWidget)
        *aState = mWidget->isEnabled();

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::SetFocus(PRBool aSet)
{
    if (mWidget) {
        if (aSet)
            mWidget->setFocus();
        else
            mWidget->clearFocus();
    }

    return NS_OK;
}

nsIFontMetrics*
nsCommonWidget::GetFont()
{
    qDebug("nsCommonWidget.cpp: GetFont not implemented");
    return nsnull;
}

NS_IMETHODIMP
nsCommonWidget::SetFont(const nsFont&)
{
    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Invalidate(PRBool aIsSynchronous)
{
    if (mContainer) {
        qDebug("invalidate 1");
        if (aIsSynchronous)
            mContainer->update();
        else
            mContainer->repaint();
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
    if (mContainer) {
        qDebug("invalidate 2");
        if (aIsSynchronous)
            mContainer->update(aRect.x, aRect.y, aRect.width, aRect.height);
        else
            mContainer->repaint(aRect.x, aRect.y, aRect.width, aRect.height);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Update()
{
    //qDebug("Update!!!!!!");
    if (mContainer) {
        mContainer->update();
    } else if (mWidget) {
        mWidget->update();
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::SetColorMap(nsColorMap*)
{
    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::Scroll(int aDx, int aDy, nsRect *aClipRect)
{
    if (mWidget)
        mWidget->scroll(aDx, aDy);

    // Update bounds on our child windows
    for (nsIWidget* kid = mFirstChild; kid; kid = kid->GetNextSibling()) {
        nsRect bounds;
        kid->GetBounds(bounds);
        bounds.x += aDx;
        bounds.y += aDy;
        NS_STATIC_CAST(nsBaseWidget*, kid)->SetBounds(bounds);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::ScrollWidgets(PRInt32 aDx,
                              PRInt32 aDy)
{
    if (mWidget)
        mWidget->scroll(aDx, aDy);

    return NS_OK;
}

void*
nsCommonWidget::GetNativeData(PRUint32 aDataType)
{
    switch(aDataType) {
    case NS_NATIVE_WINDOW:
        return mWidget;
        break;

    case NS_NATIVE_DISPLAY:
        if (mWidget)
            return mWidget->x11Display();
        break;

    case NS_NATIVE_WIDGET:
        return mWidget;
        break;

    case NS_NATIVE_PLUGIN_PORT:
        if (mWidget)
            return (void*)mWidget->winId();
        break;

    default:
        break;
    }

    return nsnull;
}

NS_IMETHODIMP
nsCommonWidget::SetTitle(const nsAString &str)
{
    nsAString::const_iterator it;
    QString qStr = QString::fromUcs2(str.BeginReading(it).get());

    if (mContainer)
        mContainer->setCaption(qStr);

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::SetMenuBar(nsIMenuBar*)
{
    qDebug("XXXXX SetMenuBar");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCommonWidget::ShowMenuBar(int)
{
    qDebug("XXXXX ShowMenuBar");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCommonWidget::WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect)
{
    aNewRect.width = aOldRect.width;
    aNewRect.height = aOldRect.height;

    if (mWidget) {
        PRInt32 x,y;

        QPoint offset(0,0);
        offset = mWidget->mapToGlobal(offset);
        x = offset.x();
        y = offset.y();

        aNewRect.x = aOldRect.x + x;
        aNewRect.y = aOldRect.y + y;
    }

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::ScreenToWidget(const nsRect &aOldRect, nsRect &aNewRect)
{
    if (mWidget) {
        PRInt32 X,Y;

        QPoint offset(0,0);
        offset = mWidget->mapFromGlobal(offset);
        X = offset.x();
        Y = offset.y();

        aNewRect.x = aOldRect.x + X;
        aNewRect.y = aOldRect.y + Y;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::BeginResizingChildren()
{
    qDebug("XXXXXX BeginResizingChildren");
    return  NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCommonWidget::EndResizingChildren()
{
    qDebug("XXXXXXX EndResizingChildren");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCommonWidget::GetPreferredSize(PRInt32 &aWidth, PRInt32 &aHeight)
{
    if (!mWidget)
        return NS_ERROR_FAILURE;

    QSize sh = mWidget->sizeHint();
    aWidth = QMAX(0, sh.width());
    aHeight = QMAX(0, sh.height());
    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::SetPreferredSize(int w, int h)
{
    qDebug("SetPreferredSize %d %d", w, h);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsCommonWidget::DispatchEvent(nsGUIEvent *aEvent, nsEventStatus &aStatus)
{
    aStatus = nsEventStatus_eIgnore;

    // hold a widget reference while we dispatch this event
    NS_ADDREF(aEvent->widget);

    if (mEventCallback)
        aStatus = (*mEventCallback)(aEvent);

    // dispatch to event listener if event was not consumed
    if ((aStatus != nsEventStatus_eIgnore) && mEventListener)
        aStatus = mEventListener->ProcessEvent(*aEvent);

    NS_IF_RELEASE(aEvent->widget);

    return NS_OK;
}

NS_IMETHODIMP
nsCommonWidget::CaptureRollupEvents(nsIRollupListener*, PRBool, PRBool)
{
    return NS_OK;
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

    DispatchEvent(&event, aStatus);
}

bool
nsCommonWidget::mousePressEvent(QMouseEvent *e)
{
    qDebug("mousePressEvent mWidget=%p", (void*)mWidget);
//     backTrace();
    PRUint32      eventType;

    switch (e->button()) {
    case Qt::MidButton:
        eventType = NS_MOUSE_MIDDLE_BUTTON_DOWN;
        break;
    case Qt::RightButton:
        eventType = NS_MOUSE_RIGHT_BUTTON_DOWN;
        break;
    default:
        eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
        break;
    }

    nsMouseEvent event(eventType, this);

    InitMouseEvent(&event, e, 1);

    nsEventStatus status;
    DispatchEvent(&event, status);

    // right menu click on linux should also pop up a context menu
    if (eventType == NS_MOUSE_RIGHT_BUTTON_DOWN) {
        nsMouseEvent contextMenuEvent(NS_CONTEXTMENU, this);
        InitMouseEvent(&contextMenuEvent, e, 1);
        DispatchEvent(&contextMenuEvent, status);
    }

    return ignoreEvent(status);
}

bool
nsCommonWidget::mouseReleaseEvent(QMouseEvent *e)
{
    qDebug("mouseReleaseEvent mWidget=%p", (void*)mWidget);
    PRUint32      eventType;

    switch (e->button()) {
    case Qt::MidButton:
        eventType = NS_MOUSE_MIDDLE_BUTTON_UP;
        break;
    case Qt::RightButton:
        eventType = NS_MOUSE_RIGHT_BUTTON_UP;
        break;
    default:
        eventType = NS_MOUSE_LEFT_BUTTON_UP;
        break;
    }

    nsMouseEvent event(eventType, this);

    InitMouseEvent(&event, e, 1);

    //not pressed
    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

bool
nsCommonWidget::mouseDoubleClickEvent(QMouseEvent *e)
{
    PRUint32      eventType;

    switch (e->button()) {
    case Qt::MidButton:
        eventType = NS_MOUSE_MIDDLE_BUTTON_DOWN;
        break;
    case Qt::RightButton:
        eventType = NS_MOUSE_RIGHT_BUTTON_DOWN;
        break;
    default:
        eventType = NS_MOUSE_LEFT_BUTTON_DOWN;
        break;
    }

    nsMouseEvent event(eventType, this);

    InitMouseEvent(&event, e, 2);
    //pressed
    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

bool
nsCommonWidget::mouseMoveEvent(QMouseEvent *e)
{
    nsMouseEvent event(NS_MOUSE_MOVE, this);

    InitMouseEvent(&event, e, 0);
    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

bool
nsCommonWidget::wheelEvent(QWheelEvent *e)
{
    nsMouseScrollEvent nsEvent(NS_MOUSE_SCROLL, this);

    InitMouseWheelEvent(&nsEvent, e);

    nsEventStatus status;
    DispatchEvent(&nsEvent, status);
    return ignoreEvent(status);
}

bool
nsCommonWidget::keyPressEvent(QKeyEvent *e)
{
    qDebug("keyPressEvent");

    nsEventStatus status;

    // If the key repeat flag isn't set then set it so we don't send
    // another key down event on the next key press -- DOM events are
    // key down, key press and key up.  X only has key press and key
    // release.  gtk2 already filters the extra key release events for
    // us.

    if (!e->isAutoRepeat()) {

        // send the key down event
        nsKeyEvent downEvent(NS_KEY_DOWN, this);
        InitKeyEvent(&downEvent, e);
        DispatchEvent(&downEvent, status);
    }

    nsKeyEvent event(NS_KEY_PRESS, this);
    InitKeyEvent(&event, e);
    event.charCode = (PRInt32)e->text()[0].unicode();

    // before we dispatch a key, check if it's the context menu key.
    // If so, send a context menu key event instead.
    if (isContextMenuKey(event)) {
        nsMouseEvent contextMenuEvent;
        keyEventToContextMenuEvent(&event, &contextMenuEvent);
        DispatchEvent(&contextMenuEvent, status);
    }
    else {
        // send the key press event
        DispatchEvent(&event, status);
    }

    return ignoreEvent(status);
}

bool
nsCommonWidget::keyReleaseEvent(QKeyEvent *e)
{
    nsKeyEvent event(NS_KEY_UP, this);

    InitKeyEvent(&event, e);

    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

bool
nsCommonWidget::focusInEvent(QFocusEvent *)
{
    //qDebug("focusInEvent mWidget=%p", (void*)mWidget);
    // dispatch a got focus event
    DispatchGotFocusEvent();

    // send the activate event if it wasn't already sent via any
    // SetFocus() calls that were the result of the GOTFOCUS event
    // above.
    if (mContainer)
        DispatchActivateEvent();
    return FALSE;
}

bool
nsCommonWidget::focusOutEvent(QFocusEvent *)
{
    qDebug("focusOutEvent mWidget=%p", (void*)mWidget);
    DispatchLostFocusEvent();
    return FALSE;
}

bool
nsCommonWidget::enterEvent(QEvent *)
{
    nsMouseEvent event(NS_MOUSE_ENTER, this);

    QPoint pt = QCursor::pos();

    event.point.x = nscoord(pt.x());
    event.point.y = nscoord(pt.y());

    nsEventStatus status;
    DispatchEvent(&event, status);
    return FALSE;
}

bool
nsCommonWidget::leaveEvent(QEvent *aEvent)
{
    nsMouseEvent event(NS_MOUSE_EXIT, this);

    QPoint pt = QCursor::pos();

    event.point.x = nscoord(pt.x());
    event.point.y = nscoord(pt.y());

    nsEventStatus status;
    DispatchEvent(&event, status);
    return FALSE;
}

bool
nsCommonWidget::paintEvent(QPaintEvent *e)
{
    qDebug("paintEvent: mWidget=%p x = %d, y = %d, width =  %d, height = %d", (void*)mWidget,
           e->rect().x(), e->rect().y(), e->rect().width(), e->rect().height());
//     qDebug("paintEvent: Widgetrect %d %d %d %d", mWidget->x(), mWidget->y(),
//            mWidget->width(), mWidget->height());

    QRect r = e->rect();
    if (!r.isValid())
        r = mWidget->rect();
    nsRect rect(r.x(), r.y(), r.width(), r.height());

    nsCOMPtr<nsIRenderingContext> rc = getter_AddRefs(GetRenderingContext());

    // Generate XPFE paint event
    nsPaintEvent event(NS_PAINT, this);
    event.point.x = 0;
    event.point.y = 0;
    event.rect = &rect;
    // XXX fix this!
    event.region = nsnull;
    // XXX fix this!
    event.renderingContext = rc;

    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

bool
nsCommonWidget::moveEvent(QMoveEvent *e)
{
    qDebug("moveEvent mWidget=%p %d %d", (void*)mWidget, e->pos().x(), e->pos().y());
    // can we shortcut?
    if (!mWidget || (mBounds.x == e->pos().x() &&
                     mBounds.y == e->pos().y()))
        return FALSE;

    // Toplevel windows need to have their bounds set so that we can
    // keep track of our location.  It's not often that the x,y is set
    // by the layout engine.  Width and height are set elsewhere.
    QPoint pos = e->pos();

    if (mWidget->isPopup()) {
        pos = mWidget->parentWidget()->mapToGlobal(pos);
        //     qDebug("global pos: %d/%d", pos.x(), pos.y());
    }
    if (mContainer) {
        // Need to translate this into the right coordinates
        nsRect oldrect, newrect;
        WidgetToScreen(oldrect, newrect);
        mBounds.x = newrect.x;
        mBounds.y = newrect.y;
    }

    nsGUIEvent event(NS_MOVE, this);
    event.point.x = pos.x();
    event.point.y = pos.y();

    // XXX mozilla will invalidate the entire window after this move
    // complete.  wtf?
    nsEventStatus status;
    DispatchEvent(&event, status);
    return ignoreEvent(status);
}

bool
nsCommonWidget::resizeEvent(QResizeEvent *e)
{
    nsRect rect;

    // Generate XPFE resize event
    GetBounds(rect);
    rect.width = e->size().width();
    rect.height = e->size().height();

    qDebug("resizeEvent: mWidget=%p, aWidth=%d, aHeight=%d, aX = %d, aY = %d", (void*)mWidget,
           rect.width, rect.height, rect.x, rect.y);

    nsEventStatus status;
    DispatchResizeEvent(rect, status);
    return ignoreEvent(status);
}

bool
nsCommonWidget::closeEvent(QCloseEvent *)
{
    nsGUIEvent event(NS_XUL_CLOSE, this);

    event.point.x = 0;
    event.point.y = 0;

    nsEventStatus status;
    DispatchEvent(&event, status);

    return ignoreEvent(status);
}

bool
nsCommonWidget::contextMenuEvent(QContextMenuEvent *)
{
    //qDebug("context menu");
    return false;
}

bool
nsCommonWidget::imStartEvent(QIMEvent *)
{
    qDebug("imStartEvent");
    return false;
}

bool
nsCommonWidget::imComposeEvent(QIMEvent *)
{
    qDebug("imComposeEvent");
    return false;
}

bool
nsCommonWidget::imEndEvent(QIMEvent * )
{
    qDebug("imComposeEvent");
    return false;
}

bool
nsCommonWidget::dragEnterEvent(QDragEnterEvent *)
{
    qDebug("dragEnterEvent");
    return false;
}

bool
nsCommonWidget::dragMoveEvent(QDragMoveEvent *)
{
    qDebug("dragMoveEvent");
    return false;
}

bool
nsCommonWidget::dragLeaveEvent(QDragLeaveEvent *)
{
    qDebug("dragLeaveEvent");
    return false;
}

bool
nsCommonWidget::dropEvent(QDropEvent *)
{
    qDebug("dropEvent");
    return false;
}

bool
nsCommonWidget::showEvent(QShowEvent *)
{
    qDebug("showEvent mWidget=%p", (void*)mWidget);

    QRect r = mWidget->rect();
    nsRect rect(r.x(), r.y(), r.width(), r.height());

    nsCOMPtr<nsIRenderingContext> rc = getter_AddRefs(GetRenderingContext());
       // Generate XPFE paint event
    nsPaintEvent event(NS_PAINT, this);
    event.point.x = 0;
    event.point.y = 0;
    event.rect = &rect;
    // XXX fix this!
    event.region = nsnull;
    // XXX fix this!
    event.renderingContext = rc;

    nsEventStatus status;
    DispatchEvent(&event, status);

    return false;
}

bool
nsCommonWidget::hideEvent(QHideEvent *)
{
    qDebug("hideEvent mWidget=%p", (void*)mWidget);
    return false;
}

void
nsCommonWidget::InitKeyEvent(nsKeyEvent *nsEvent, QKeyEvent *qEvent)
{
    nsEvent->isShift   = qEvent->state() & Qt::ShiftButton;
    nsEvent->isControl = qEvent->state() & Qt::ControlButton;
    nsEvent->isAlt     = qEvent->state() & Qt::AltButton;
    nsEvent->isMeta    = qEvent->state() & Qt::MetaButton;
    nsEvent->time      = 0;

    if (qEvent->text().length() && qEvent->text()[0].isPrint()) {
        nsEvent->charCode = (PRInt32)qEvent->text()[0].unicode();
    }
    else {
        nsEvent->charCode = 0;
    }

    if (nsEvent->charCode) {
        nsEvent->keyCode = 0;
    }
    else {
        nsEvent->keyCode = NS_GetKey(qEvent->key());
    }
}

void
nsCommonWidget::InitMouseEvent(nsMouseEvent *nsEvent, QMouseEvent *qEvent, int aClickCount)
{
    nsEvent->point.x = nscoord(qEvent->x());
    nsEvent->point.y = nscoord(qEvent->y());

    nsEvent->isShift         = qEvent->state() & Qt::ShiftButton;
    nsEvent->isControl       = qEvent->state() & Qt::ControlButton;
    nsEvent->isAlt           = qEvent->state() & Qt::AltButton;
    nsEvent->isMeta          = qEvent->state() & Qt::MetaButton;
    nsEvent->clickCount      = aClickCount;
}

void
nsCommonWidget::InitMouseWheelEvent(nsMouseScrollEvent *aEvent,
                                    QWheelEvent *qEvent)
{
    switch (qEvent->orientation()) {
    case Qt::Vertical:
        aEvent->scrollFlags = nsMouseScrollEvent::kIsVertical;
        break;
    case Qt::Horizontal:
        aEvent->scrollFlags = nsMouseScrollEvent::kIsHorizontal;
        break;
    default:
        Q_ASSERT(0);
        break;
    }
    aEvent->delta = (int)((qEvent->delta() / WHEEL_DELTA) * -3);

    aEvent->point.x = nscoord(qEvent->x());
    aEvent->point.y = nscoord(qEvent->y());

    aEvent->isShift         = qEvent->state() & Qt::ShiftButton;
    aEvent->isControl       = qEvent->state() & Qt::ControlButton;
    aEvent->isAlt           = qEvent->state() & Qt::AltButton;
    aEvent->isMeta          = qEvent->state() & Qt::MetaButton;
    aEvent->time            = 0;
}

void
nsCommonWidget::CommonCreate(nsIWidget *aParent, PRBool aListenForResizes)
{
    mParent = aParent;
    mListenForResizes = aListenForResizes;
}

PRBool
nsCommonWidget::AreBoundsSane() const
{
    if (mBounds.width > 0 && mBounds.height > 0)
        return PR_TRUE;

    return PR_FALSE;
}

NS_IMETHODIMP
nsCommonWidget::Create(nsIWidget *aParent, const nsRect &aRect, EVENT_CALLBACK aHandleEventFunction,
                       nsIDeviceContext *aContext, nsIAppShell *aAppShell, nsIToolkit *aToolkit,
                       nsWidgetInitData *aInitData)
{
    return NativeCreate(aParent, nsnull, aRect, aHandleEventFunction, aContext, aAppShell,
                        aToolkit, aInitData);
}

NS_IMETHODIMP
nsCommonWidget::Create(nsNativeWidget aParent, const nsRect &aRect, EVENT_CALLBACK aHandleEventFunction,
                       nsIDeviceContext *aContext, nsIAppShell *aAppShell, nsIToolkit *aToolkit,
                       nsWidgetInitData *aInitData)
{
    return NativeCreate(nsnull, (QWidget*)aParent, aRect, aHandleEventFunction, aContext, aAppShell,
                        aToolkit, aInitData);
}

nsresult
nsCommonWidget::NativeCreate(nsIWidget        *aParent,
                             QWidget          *aNativeParent,
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
                             aInitData->mWindowType == eWindowType_toplevel ||
                             aInitData->mWindowType == eWindowType_invisible) ?
                            nsnull : aParent;

    // initialize all the common bits of this class
    BaseCreate(baseParent, aRect, aHandleEventFunction, aContext,
               aAppShell, aToolkit, aInitData);

    // Do we need to listen for resizes?
    PRBool listenForResizes = PR_FALSE;;
    if (aNativeParent || (aInitData && aInitData->mListenForResizes))
        listenForResizes = PR_TRUE;

    // and do our common creation
    CommonCreate(aParent, listenForResizes);

    // save our bounds
    mBounds = aRect;

    QWidget *parent = 0;
    if (aParent != nsnull)
        parent = (QWidget*)aParent->GetNativeData(NS_NATIVE_WIDGET);
    else
        parent = aNativeParent;

    mWidget = createQWidget(parent, aInitData);

    Initialize(mWidget);

    Resize(mBounds.width, mBounds.height, PR_FALSE);

    return NS_OK;
}

nsCursor nsCommonWidget::GetCursor()
{
    return mCursor;
}

NS_METHOD nsCommonWidget::SetCursor(nsCursor aCursor)
{
    mCursor = aCursor;
    Qt::CursorShape cursor = Qt::ArrowCursor;
    switch(mCursor) {
    case eCursor_standard:
	cursor = Qt::ArrowCursor;
        break;
    case eCursor_wait:
        cursor = Qt::WaitCursor;
        break;
    case eCursor_select:
        cursor = Qt::IbeamCursor;
        break;
    case eCursor_hyperlink:
        cursor = Qt::PointingHandCursor;
        break;
    case eCursor_ew_resize:
        cursor = Qt::SplitHCursor;
        break;
    case eCursor_ns_resize:
        cursor = Qt::SplitVCursor;
        break;
    case eCursor_nw_resize:
    case eCursor_se_resize:
	cursor = Qt::SizeBDiagCursor;
        break;
    case eCursor_ne_resize:
    case eCursor_sw_resize:
	cursor = Qt::SizeFDiagCursor;
        break;
    case eCursor_crosshair:
    case eCursor_move:
	cursor = Qt::SizeAllCursor;
        break;
    case eCursor_help:
	cursor = Qt::WhatsThisCursor;
        break;
    case eCursor_copy:
    case eCursor_alias:
        break;
    case eCursor_context_menu:
    case eCursor_cell:
    case eCursor_grab:
    case eCursor_grabbing:
    case eCursor_spinning:
    case eCursor_zoom_in:
    case eCursor_zoom_out:

    default:
        break;
    }
    mWidget->setCursor(cursor);
    return NS_OK;
}

bool nsCommonWidget::ignoreEvent(nsEventStatus aStatus) const
{
      switch(aStatus) {
    case nsEventStatus_eIgnore:
        return(PR_FALSE);

    case nsEventStatus_eConsumeNoDefault:
        return(PR_TRUE);

    case nsEventStatus_eConsumeDoDefault:
        return(PR_FALSE);

    default:
        NS_ASSERTION(0,"Illegal nsEventStatus enumeration value");
        break;
    }
    return(PR_FALSE);
}

NS_METHOD nsCommonWidget::SetModal(PRBool aModal)
{
    qDebug("------------> SetModal mWidget=%p",(void*) mWidget);

    MozQWidget *mozWidget = dynamic_cast<MozQWidget*>(mWidget);
    if (mozWidget)
        mozWidget->setModal(aModal);

    return NS_OK;
}

NS_IMETHODIMP nsCommonWidget::GetScreenBounds(nsRect &aRect)
{
	nsRect origin(0,0,mBounds.width,mBounds.height);
	WidgetToScreen(origin, aRect);
	return NS_OK;
}

void
nsCommonWidget::NativeShow(PRBool aState)
{
    mNeedsShow = PR_FALSE;

    if (!mWidget) {
         //XXX: apperently can be null during the printing, check whether
         //     that's true
         qDebug("nsCommon::Show : widget empty");
         return;
    }

    mWidget->setShown(aState);
}

void
nsCommonWidget::NativeResize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
    mNeedsResize = PR_FALSE;

    mWidget->resize( aWidth, aHeight);

    if (aRepaint) {
        if (mWidget->isVisible())
            mWidget->repaint(false);
    }
}

void
nsCommonWidget::NativeResize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight,
                             PRBool aRepaint)
{
    mNeedsResize = PR_FALSE;

    mWidget->setGeometry( aX, aY, aWidth, aHeight);

    if (aRepaint) {
        if (mWidget->isVisible())
            mWidget->repaint(false);
    }
}
