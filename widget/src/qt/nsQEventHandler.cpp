/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */



#include "nsWidget.h"
#include "nsWindow.h"
#include "nsTextWidget.h"
#include "nsCheckButton.h"
#include "nsRadioButton.h"
#include "nsScrollbar.h"
#include "nsFileWidget.h"
#include "nsGUIEvent.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "stdio.h"
#include "nsQEventHandler.h"

//JCG #define DBG 1
#ifdef DBG
PRUint32                    gQEventHandlerCount = 0;
#endif

static NS_DEFINE_IID(kCWindow,        NS_WINDOW_CID);
static NS_DEFINE_IID(kCChild,         NS_CHILD_CID);

struct EventInfo 
{
    nsWidget *widget;  // the widget
    nsRect   *rect;    // the rect
};

struct nsKeyConverter 
{
    int vkCode; // Platform independent key code
    int keysym; // QT key code
};

struct nsKeyConverter nsKeycodes[] = 
{
//    { NS_VK_CANCEL,        Qt::Key_Cancel },
    { NS_VK_BACK,          Qt::Key_BackSpace },
    { NS_VK_TAB,           Qt::Key_Tab },
//    { NS_VK_CLEAR,         Qt::Key_Clear },
    { NS_VK_RETURN,        Qt::Key_Return },
    { NS_VK_RETURN,        Qt::Key_Enter },
    { NS_VK_SHIFT,         Qt::Key_Shift },
    { NS_VK_CONTROL,       Qt::Key_Control },
    { NS_VK_ALT,           Qt::Key_Alt },
    { NS_VK_ALT,           Qt::Key_Meta },
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
//    { NS_VK_SEPARATOR,     Qt::Key_Separator },
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
    { NS_VK_QUOTE,         Qt::Key_QuoteDbl }
};

PRLogModuleInfo * QtEventsLM   = PR_NewLogModule("QtEvents");

/**
 * Constructor for QT event handler.
 *
 */
nsQEventHandler::nsQEventHandler(nsWidget *aWidget) : QObject()
{
    PR_LOG(QtEventsLM, PR_LOG_DEBUG, ("nsQEventHandler::nsQEventHandler()\n"));
    mEnabled = true;
    mDestroyed = false;
    mWidget = aWidget;
#ifdef DBG
  gQEventHandlerCount++;
  printf("DBG: nsQEventHandler CTOR. Count: %d\n",gQEventHandlerCount);
#endif
}

nsQEventHandler::~nsQEventHandler()
{
    PR_LOG(QtEventsLM, PR_LOG_DEBUG, ("nsQEventHandler::~nsQEventHandler()\n"));
    mWidget = nsnull;
#ifdef DBG
  gQEventHandlerCount--;
  printf("DBG: nsQEventHandler DTOR. Count: %d\n",gQEventHandlerCount);
#endif
}

void nsQEventHandler::Enable(bool aEnable)
{
  mEnabled = aEnable;
}

void nsQEventHandler::Destroy(void)
{
  mDestroyed = true;
  mWidget = nsnull;
}

bool nsQEventHandler::eventFilter(QObject * object, QEvent * event)
{
    bool handled = false;

    if (mDestroyed)
      return true;

    switch (event->type()) {
      case QEvent::MouseButtonPress:
        if (mEnabled)
          handled = MouseButtonEvent((QMouseEvent *) event, mWidget, true, 1);
        else
          handled = true;
        break;

      case QEvent::MouseButtonRelease:
        if (mEnabled)
          handled = MouseButtonEvent((QMouseEvent *) event, mWidget, false, 1);
        else
          handled = true;
        break;

      case QEvent::MouseButtonDblClick:
        if (mEnabled)
          handled = MouseButtonEvent((QMouseEvent *) event, mWidget, true, 2);
        else
          handled = true;
        break;

      case QEvent::MouseMove:
        if (mEnabled)
          handled = MouseMovedEvent((QMouseEvent *) event, mWidget);
        else
          handled = true;
        break;

      case QEvent::KeyPress:
        if (mEnabled)
          handled = KeyPressEvent((QKeyEvent *) event, mWidget);
        else
          handled = true;
        break;
        
      case QEvent::KeyRelease:
        if (mEnabled)
          handled = KeyReleaseEvent((QKeyEvent *) event, mWidget);
        else
          handled = true;
        break;

      case QEvent::Enter:
        if (mEnabled)
          handled = MouseEnterEvent(event, mWidget);
        else
          handled = true;
        break;

      case QEvent::Leave:
        if (mEnabled)
          handled = MouseExitEvent(event, mWidget);
        else
          handled = true;
        break;

      case QEvent::Close:
        if (!mWidget->IsPopup()) {
          handled = DestroyEvent((QCloseEvent *) event, mWidget);
        }
        break;

      case QEvent::Destroy:
        handled = DestroyEvent((QCloseEvent *) event, mWidget);
        break;

      case QEvent::Resize:
        handled = ResizeEvent((QResizeEvent *) event, mWidget);
        break;

      case QEvent::Move:
        handled = MoveEvent((QMoveEvent *) event, mWidget);
        break;

      case QEvent::Paint:
        handled = PaintEvent((QPaintEvent *) event, mWidget);
        break;

      case QEvent::FocusIn:
        if (mEnabled)
          handled = FocusInEvent((QFocusEvent *) event, mWidget);
        else
          handled = true;
        break;

      case QEvent::FocusOut:
        if (mEnabled)
          handled = FocusOutEvent((QFocusEvent *) event, mWidget);
        else
          handled = true;
        break;

      default:
        break;
    }
    return handled;
}


bool nsQEventHandler::MouseButtonEvent(QMouseEvent * event, 
                                       nsWidget    * widget,
                                       bool          buttonDown,
                                       int           clickCnt)
{
    if (event && widget) {
        nsMouseEvent nsEvent;

        PR_LOG(QtEventsLM, 
               PR_LOG_DEBUG, 
               ("nsQEventHandler::MouseButtonEvent for %s(%p) at (%d,%y)\n", 
                widget->GetName(), widget, event->x(), event->y()));

        switch (event->button()) {
          case LeftButton:
            nsEvent.message = buttonDown ? NS_MOUSE_LEFT_BUTTON_DOWN : 
                NS_MOUSE_LEFT_BUTTON_UP;
            break;

          case RightButton:
            nsEvent.message = buttonDown ? NS_MOUSE_RIGHT_BUTTON_DOWN :
                NS_MOUSE_RIGHT_BUTTON_UP;
            break;

          case MidButton:
            nsEvent.message = buttonDown ? NS_MOUSE_MIDDLE_BUTTON_DOWN :
                NS_MOUSE_MIDDLE_BUTTON_UP;
            break;

          default:
            // This shouldn't happen!
            NS_ASSERTION(0, "Bad MousePressedEvent!");
            nsEvent.message = NS_MOUSE_MOVE;
            break;
        }

        nsEvent.point.x         = event->x();
        nsEvent.point.y         = event->y();
        nsEvent.widget          = widget;
        nsEvent.nativeMsg	= (void*)event;
        nsEvent.eventStructType = NS_MOUSE_EVENT;
        nsEvent.clickCount      = clickCnt;
        nsEvent.isShift         = (event->state() & ShiftButton) ? PR_TRUE : PR_FALSE;
        nsEvent.isControl       = (event->state() & ControlButton) ? PR_TRUE : PR_FALSE;
        nsEvent.isAlt           = (event->state() & AltButton) ? PR_TRUE : PR_FALSE;
        nsEvent.isMeta		= PR_FALSE;
        nsEvent.time       = PR_IntervalNow();
        widget->AddRef();
        widget->DispatchMouseEvent(nsEvent);
        widget->Release();
    }
    /* Below are a series of slimey hacks to do the right thing with    */
    /* Mouse Events - Qt gets a go at all events for native widgets     */
    /* (i.e. NOT nsWidget or nsWindow) and at Button Press Events for   */
    /* nsWidget & nsWindow, unless either the EventHandler is destroyed */
    /* (i.e. "widget" is NULL) or the widget is deleted (i.e. IsInDTOR  */
    /* returns TRUE).  The last item is particularly slimey...          */

    if (widget && !widget->IsInDTOR()) {
      if (widget->GetName() == QWidget::tr("nsWindow")
          || widget->GetName() == QWidget::tr("nsWidget")) {
        if (buttonDown) {
          return false;
        }
        else {
          return true;
        }
      }
      else {
          return false;
      }
    }
    else
      return true;
}

bool nsQEventHandler::MouseMovedEvent(QMouseEvent * event, nsWidget * widget)
{
    if (event && widget) {
      // Generate XPFE mouse moved event
      nsMouseEvent nsEvent;

      PR_LOG(QtEventsLM, 
             PR_LOG_DEBUG, 
             ("nsQEventHandler::MouseMovedEvent for %s(%p) at (%d,%d)\n", 
              widget->GetName(), widget, event->x(), event->y()));

      nsEvent.point.x         = event->x();
      nsEvent.point.y         = event->y();
      nsEvent.message         = NS_MOUSE_MOVE;
      nsEvent.widget          = widget;
      nsEvent.nativeMsg	    = (void*)event;
      nsEvent.eventStructType = NS_MOUSE_EVENT;
      nsEvent.time       = PR_IntervalNow();
      widget->AddRef();
      widget->DispatchMouseEvent(nsEvent);
      widget->Release();
    }
    /* Below are a series of slimey hacks to do the right thing with    */
    /* Mouse Move Events - Qt only gets a go at events for non-deleted  */
    /* native widgets (i.e. NOT nsWidget or nsWindow AND IsInDTOR       */
    /* returns FALSE)  Using IsInDTOR like this is particularly slimey! */

    if (widget && !widget->IsInDTOR()) {
      if (widget->GetName() == QWidget::tr("nsWindow")
          || widget->GetName() == QWidget::tr("nsWidget")) {
        return true;
      }
      else
        return false;
    }
    else
      return true;
}

bool nsQEventHandler::MouseEnterEvent(QEvent * event, nsWidget * widget)
{
    if (event && widget) {
        nsMouseEvent nsEvent;

        PR_LOG(QtEventsLM, 
               PR_LOG_DEBUG, 
               ("nsQEventHandler::MouseEnterEvent for %s\n", 
                widget->GetName()));

        nsEvent.message         = NS_MOUSE_ENTER;
        nsEvent.widget          = widget;
        nsEvent.nativeMsg	= (void*)event;
        nsEvent.eventStructType = NS_MOUSE_EVENT;
        nsEvent.time       = PR_IntervalNow();
        widget->AddRef();
        widget->DispatchMouseEvent(nsEvent);
        widget->Release();
    }
    return true;
}

bool nsQEventHandler::MouseExitEvent(QEvent * event, nsWidget * widget)
{
    if (event && widget) {
        nsMouseEvent nsEvent;

        PR_LOG(QtEventsLM, 
               PR_LOG_DEBUG, 
               ("nsQEventHandler::MouseExitEvent for %s\n", 
                widget->GetName()));

        nsEvent.message         = NS_MOUSE_EXIT;
        nsEvent.widget          = widget;
        nsEvent.nativeMsg	= (void*)event;
        nsEvent.eventStructType = NS_MOUSE_EVENT;
        nsEvent.time       = PR_IntervalNow();
        widget->AddRef();
        widget->DispatchMouseEvent(nsEvent);
        widget->Release();
    }
    return true;
}

bool nsQEventHandler::DestroyEvent(QCloseEvent * event, nsWidget * widget)
{
    if (event && widget) {
      // Generate XPFE destroy event

      PR_LOG(QtEventsLM, 
             PR_LOG_DEBUG, 
             ("nsQEventHandler::DestroyEvent for %s\n", 
              widget->GetName()));

      ((nsWindow *)widget)->SetIsDestroying(PR_TRUE);
      widget->Destroy();
    }
    return true;
}

bool nsQEventHandler::ResizeEvent(QResizeEvent * event, nsWidget * widget)
{
    if (event && widget) {
      // Generate XPFE resize event
      nsSizeEvent nsEvent;

      PR_LOG(QtEventsLM, 
             PR_LOG_DEBUG, 
             ("nsQEventHandler::ResizeEvent for %s(%p)\n", 
              widget->GetName(),
              widget));

      nsEvent.message = NS_SIZE;
      nsEvent.widget  = widget;
      nsEvent.eventStructType = NS_SIZE_EVENT;

      PR_LOG(QtEventsLM, 
             PR_LOG_DEBUG, 
             ("nsQEventHandler::ResizeEvent: old size:%dx%d, new size:%dx%d\n",
             event->oldSize().width(),
             event->oldSize().height(),
             event->size().width(),
             event->size().height()));

      nsRect * rect   = new nsRect();
      widget->GetBounds(*rect);
      rect->width     = event->size().width();
      rect->height    = event->size().height();

      nsEvent.point.x    = 0;
      nsEvent.point.y    = 0;
      nsEvent.windowSize = rect;
      nsEvent.mWinHeight = event->size().height();
      nsEvent.mWinWidth  = event->size().width();
      nsEvent.time       = PR_IntervalNow();

      widget->OnResize(*rect);
      delete rect;
    }
    return false;
}

bool nsQEventHandler::MoveEvent(QMoveEvent * event, nsWidget * widget)
{
    if (event && widget) {
        PR_LOG(QtEventsLM, 
               PR_LOG_DEBUG, 
               ("nsQEventHandler::MoveEvent for %s\n", 
                widget->GetName()));

        // Generate XPFE move event
        widget->OnMove(event->pos().x(),event->pos().y());
    }
    return true;
}

bool nsQEventHandler::PaintEvent(QPaintEvent * event, nsWidget * widget)
{
    if (event && widget) {
        // Generate XPFE paint event
        nsPaintEvent nsEvent;

        PR_LOG(QtEventsLM, 
               PR_LOG_DEBUG, 
               ("nsQEventHandler::PaintEvent for %s(%p)\n", 
                widget->GetName(),
                widget));

        nsEvent.message = NS_PAINT;
        nsEvent.widget  = widget;
        nsEvent.eventStructType = NS_PAINT_EVENT;
        nsEvent.time       = PR_IntervalNow();
        
        QRect qrect = event->rect();

        PR_LOG(QtEventsLM, 
               PR_LOG_DEBUG, 
               ("nsQEventHandler::PaintEvent: need to paint:x=%d,y=%d,w=%d,h=%d\n",
                qrect.x(), qrect.y(), qrect.width(), qrect.height()));

        nsRect * rect = new nsRect(qrect.x(), qrect.y(), qrect.width(), qrect.height());
        nsEvent.rect = rect;

        widget->AddRef();
        widget->OnPaint(nsEvent);
        widget->Release();

        delete rect;
    }
    if (widget && widget->GetName() == QWidget::tr("nsWindow")
        || widget->GetName() == QWidget::tr("nsWidget")) {
        return true;
    }
    else {
        return false;
    }
}

bool nsQEventHandler::KeyPressEvent(QKeyEvent * event, nsWidget * widget)
{
    if (event && widget) {
        PR_LOG(QtEventsLM, 
               PR_LOG_DEBUG, 
               ("nsQEventHandler::KeyPressEvent for %s\n", 
                widget->GetName()));

        if (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control 
            || event->key() == Qt::Key_Alt)
           return false;

        nsKeyEvent nsEvent;

        nsEvent.message         = NS_KEY_DOWN;
        nsEvent.eventStructType = NS_KEY_EVENT;
        nsEvent.widget          = widget;
        nsEvent.keyCode         = GetNSKey(event->key(), event->state());
        nsEvent.isShift         = (event->state() & ShiftButton) ? PR_TRUE : PR_FALSE;
        nsEvent.isControl       = (event->state() & ControlButton) ? PR_TRUE : PR_FALSE;
        nsEvent.isAlt           = (event->state() & AltButton) ? PR_TRUE : PR_FALSE;
        nsEvent.isMeta		= PR_FALSE;
        nsEvent.point.x         = 0;
        nsEvent.point.y         = 0;
        nsEvent.time       = PR_IntervalNow();

        nsEvent.charCode = 0;
        widget->AddRef();
        ((nsWindow *)widget)->OnKey(nsEvent);

        nsEvent.message         = NS_KEY_PRESS;
        nsEvent.eventStructType = NS_KEY_EVENT;
        nsEvent.widget          = widget;
        nsEvent.isShift         = (event->state() & ShiftButton) ? PR_TRUE : PR_FALSE;
        nsEvent.isControl       = (event->state() & ControlButton) ? PR_TRUE : PR_FALSE;
        nsEvent.isAlt           = (event->state() & AltButton) ? PR_TRUE : PR_FALSE;
        nsEvent.isMeta		= PR_FALSE;
        nsEvent.point.x         = 0;
        nsEvent.point.y         = 0;
        nsEvent.time       = PR_IntervalNow();
        if (event->text().length() && event->text()[0].isPrint()) {
            nsEvent.charCode = (PRInt32)event->text()[0].unicode();
        }
        else {
            nsEvent.charCode = 0;
        }
        if (nsEvent.charCode) {
           nsEvent.keyCode = 0;
           nsEvent.isShift = PR_FALSE;
        }
        else
          nsEvent.keyCode = GetNSKey(event->key(), event->state());

        ((nsWindow *)widget)->OnKey(nsEvent);
        widget->Release();
    }
    return true;
}

bool nsQEventHandler::KeyReleaseEvent(QKeyEvent * event, nsWidget * widget)
{
    if (event && widget) {

        PR_LOG(QtEventsLM, 
               PR_LOG_DEBUG, 
               ("nsQEventHandler::KeyReleaseEvent for %s\n", 
                widget->GetName()));

        if (event->key() == Qt::Key_Shift || event->key() == Qt::Key_Control 
            || event->key() == Qt::Key_Alt)
           return false;

        nsKeyEvent nsEvent;

        nsEvent.message = NS_KEY_UP;
        nsEvent.eventStructType = NS_KEY_EVENT;
        nsEvent.widget = widget;
        nsEvent.charCode = 0;
        nsEvent.keyCode = GetNSKey(event->key(), event->state());
        nsEvent.isShift = event->state() & ShiftButton;
        nsEvent.isControl = event->state() & ControlButton;
        nsEvent.isAlt = event->state() & AltButton;
        nsEvent.isMeta = PR_FALSE;
        nsEvent.time       = PR_IntervalNow();
        widget->AddRef();
        ((nsWindow *)widget)->OnKey(nsEvent);
        widget->Release();
    }
    return true;
}

PRInt32 nsQEventHandler::GetNSKey(PRInt32 key, PRInt32 state)
{
    PRInt32 nsKey = 0;
    PRInt32 length = sizeof(nsKeycodes) / sizeof(nsKeyConverter);

    for (PRInt32 i = 0; i < length; i++) {
        if (nsKeycodes[i].keysym == key) {
            return nsKeycodes[i].vkCode;
        }
    }
    return nsKey;
}

bool nsQEventHandler::FocusInEvent(QFocusEvent * event, nsWidget * widget)
{
    if (event && widget) {
        nsGUIEvent aEvent;

      PR_LOG(QtEventsLM, 
             PR_LOG_DEBUG, 
             ("nsQEventHandler::FocusInEvent for %s\n", 
              widget->GetName()));

        aEvent.message         = NS_GOTFOCUS;
        aEvent.eventStructType = NS_GUI_EVENT;
        aEvent.widget          = widget;
        aEvent.time       = PR_IntervalNow();
        aEvent.point.x = 0;
        aEvent.point.y = 0;
        widget->AddRef();
        ((nsWindow *)widget)->DispatchFocus(aEvent);
        widget->Release();
    }    
    return true;
}

bool nsQEventHandler::FocusOutEvent(QFocusEvent * event, nsWidget * widget)
{
    if (event && widget) {
        nsGUIEvent aEvent;

      PR_LOG(QtEventsLM, 
             PR_LOG_DEBUG, 
             ("nsQEventHandler::FocusOutEvent for %s\n", 
              widget->GetName()));

        aEvent.message         = NS_LOSTFOCUS;
        aEvent.eventStructType = NS_GUI_EVENT;
        aEvent.widget          = widget;
        aEvent.time       = PR_IntervalNow();
        aEvent.point.x = 0;
        aEvent.point.y = 0;

        widget->AddRef();
        ((nsWindow *)widget)->DispatchFocus(aEvent);
        widget->Release();
    }
    return true;
}
