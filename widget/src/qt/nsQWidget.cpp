/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *		John C. Griggs <jcgriggs@sympatico.ca>
 *      	Denis Issoupov <denis@macadamian.com> 
 *      	Wes Morgan <wmorga13@calvin.edu> 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIServiceManager.h"
#include "nsGUIEvent.h"
#include "nsQWidget.h"
#include "nsWidget.h"
#include "nsClipboard.h"
#include "nsDragService.h"
#include "nsFontMetricsQT.h"
#include "nsQApplication.h"

#include <qobjectlist.h>

//JCG #define DBG_JCG 1
//JCG #define DBG_JCG_EVENT 1

#ifdef DBG_JCG
PRUint32 gQBaseWidgetCount = 0;
PRUint32 gQBaseWidgetID = 0;

PRUint32 gQWidgetCount = 0;
PRUint32 gQWidgetID = 0;
#endif

static NS_DEFINE_CID(kCDragServiceCID, NS_DRAGSERVICE_CID);

struct nsKeyConverter
{
  int vkCode; // Platform independent key code
  int keysym; // QT key code
};

struct nsKeyConverter nsKeycodes[] =
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

PRBool NS_IsMouseInWindow(void *aWin,PRInt32 aMouseX,PRInt32 aMouseY)
{
  QWidget *qWin = (QWidget*)aWin;
  QPoint origin = qWin->mapToGlobal(QPoint(0,0));

  if (aMouseX > origin.x() && aMouseX < (origin.x() + qWin->width())
      && aMouseY > origin.y() && aMouseY < (origin.y() + qWin->height())) {
    return PR_TRUE;
  }
  return PR_FALSE;
}

unsigned int NS_GetQWFlags(nsBorderStyle aBorder,nsWindowType aType)
{
  QWidget::WFlags w = 0;

  if (aBorder != eBorderStyle_default) {
    if (aBorder & eBorderStyle_border) {
      w |= Qt::WStyle_DialogBorder;
    }
    if (aBorder & eBorderStyle_resizeh) {
      w |= Qt::WStyle_NormalBorder;
    }
    if (aBorder & eBorderStyle_title) {
      w |= Qt::WStyle_Title;
    }
    if (aBorder & eBorderStyle_menu) {
      w |= Qt::WStyle_SysMenu | Qt::WStyle_Title;
    }
    if (aBorder & eBorderStyle_minimize) {
      w |= Qt::WStyle_Minimize;
    }
    if (aBorder & eBorderStyle_maximize) {
      w |= Qt::WStyle_Maximize;
    }
    if (aBorder & eBorderStyle_close) {
#ifdef DEBUG
      printf("JCG: eBorderStyle_close isn't handled yet... please fix me\n");
#endif
    }
  }
  w |= (w) ? Qt::WStyle_Customize : 0;

  switch (aType) {
    case eWindowType_toplevel:
      w |= Qt::WType_TopLevel | Qt::WDestructiveClose | Qt::WGroupLeader;
      break;

    case eWindowType_dialog:
      w |= Qt::WType_TopLevel | Qt::WStyle_Dialog;
      break;

    case eWindowType_popup:
      w |= Qt::WType_Popup;
      break;

    case eWindowType_child:
      break;
  }
  w |= Qt::WRepaintNoErase | Qt::WResizeNoErase;

  return((unsigned int)w);
}

//=============================================================================
//
// nsQBaseWidget class
//
//=============================================================================
nsQBaseWidget::nsQBaseWidget(nsWidget* aWidget)
{
  mWidget = aWidget;
  mEnabled = PR_TRUE;
  mDestroyed = PR_FALSE;

#ifdef DBG_JCG
  gQBaseWidgetCount++;
  mQBaseID = gQBaseWidgetID++;
  printf("JCG: nsQBaseWidget CTOR (%p), ID: %d, Count: %d, nsWidget: %p\n",
         this,mQBaseID,gQBaseWidgetCount,mWidget);
#endif
}

nsQBaseWidget::~nsQBaseWidget()
{
#ifdef DBG_JCG
  gQBaseWidgetCount--;
  printf("JCG: nsQBaseWidget DTOR (%p), ID: %d, Count: %d, nsWidget: %p\n",
         this,mQBaseID,gQBaseWidgetCount,mWidget);
#endif
  Destroy();
  if (mQWidget) {
    delete mQWidget;
    mQWidget = nsnull;
  }
}

PRBool nsQBaseWidget::CreateNative(QWidget *aParent,const char* aName,
                                   unsigned int aFlags)
{
  if (!(mQWidget = new nsQWidget(aParent,aName,(QWidget::WFlags)aFlags))) {
    return PR_FALSE;
  }
  mQWidget->installEventFilter(this);
  return PR_TRUE;
}

void nsQBaseWidget::Destroy()
{
#ifdef DBG_JCG
  printf("JCG: nsQBaseWidget Destroy (%p), ID: %d, Count: %d\n",
         this,mQBaseID,gQBaseWidgetCount);
#endif
  mDestroyed = PR_TRUE;

  if (mWidget) {
    mWidget = nsnull;
  }
  if (mQWidget) {
    const QObjectList *kids;
    QObject *kid = nsnull;

    kids = mQWidget->children();
    if (kids) {
      QObjectListIt it(*kids);
      QPoint pt(0,0);

      while ((kid = it.current())) {
#ifdef DBG_JCG
        printf("JCG: (%p) Reparent: %p\n",this,kid);
#endif
        ((QWidget*)kid)->reparent(nsQApplication::GetMasterWidget(),pt);
      }
    }
  }
}

void nsQBaseWidget::Enable(PRBool aState)
{
  mEnabled = aState;
}

PRBool nsQBaseWidget::HandlePopup(void *aEvent)
{
  QMouseEvent *qEvent = (QMouseEvent*)aEvent;

  if (mWidget) {
    return(mWidget->HandlePopup(qEvent->globalX(),qEvent->globalY()));
  }
  return(PR_FALSE); 
}

void nsQBaseWidget::SetCursor(nsCursor aCursor)
{
  PRBool  cursorSet = PR_FALSE;
  QCursor newCursor(ArrowCursor);

  switch (aCursor) {
    case eCursor_select:
      newCursor.setShape(IbeamCursor);
      cursorSet = PR_TRUE;
      break;

    case eCursor_wait:
      newCursor.setShape(WaitCursor);
      cursorSet = PR_TRUE;
      break;

    case eCursor_hyperlink:
      newCursor.setShape(PointingHandCursor);
      cursorSet = PR_TRUE;
      break;

    case eCursor_standard:
      cursorSet = PR_TRUE;
      break;

    case eCursor_sizeNS:
    case eCursor_arrow_south:
    case eCursor_arrow_south_plus:
    case eCursor_arrow_north:
    case eCursor_arrow_north_plus:
      newCursor.setShape(SizeVerCursor);
      cursorSet = PR_TRUE;
      break;

    case eCursor_sizeWE:
    case eCursor_arrow_east:
    case eCursor_arrow_east_plus:
    case eCursor_arrow_west:
    case eCursor_arrow_west_plus:
      newCursor.setShape(SizeHorCursor);
      cursorSet = PR_TRUE;
      break;

    case eCursor_sizeNW:
    case eCursor_sizeSE:
      newCursor.setShape(SizeFDiagCursor);
      cursorSet = PR_TRUE;
      break;

    case eCursor_sizeNE:
    case eCursor_sizeSW:
      newCursor.setShape(SizeBDiagCursor);
      cursorSet = PR_TRUE;
      break;

    case eCursor_crosshair:
      newCursor.setShape(CrossCursor);
      cursorSet = PR_TRUE;
      break;

    case eCursor_move:
      newCursor.setShape(SizeAllCursor);
      cursorSet = PR_TRUE;
      break;

    case eCursor_help:
/* Question + Arrow */
    case eCursor_cell:
/* Plus */
    case eCursor_grab:
    case eCursor_grabbing:
/* Hand1 */
    case eCursor_spinning:
/* Exchange */

    case eCursor_copy: // CSS3
    case eCursor_alias:
    case eCursor_context_menu:

    // XXX: these CSS3 cursors need to be implemented
    // For CSS3 Cursor Definitions, See:
    // www.w3.org/TR/css3-userint
    case eCursor_count_up:
    case eCursor_count_down:
    case eCursor_count_up_down:
      break;
    // XXX: these CSS3 cursors need to be implemented

    default:
      NS_ASSERTION(PR_FALSE, "Invalid cursor type");
      break;
  }
  if (cursorSet) {
    // Since nsEventStateManager::UpdateCursor() doesn't use the same
    // nsWidget* that is given in DispatchEvent().
    qApp->setOverrideCursor(newCursor,PR_TRUE);
  }
}

void nsQBaseWidget::SetFont(nsFontQT *aFont)
{
  QFont *qtFont = aFont->GetQFont();
  if (qtFont)
    mQWidget->setFont(*qtFont);
}

bool nsQBaseWidget::eventFilter(QObject *aObj,QEvent *aEvent)
{
  bool handled = false;

  if (mDestroyed) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Destroyed\n");
#endif
    return true;
  }
  switch (aEvent->type()) {
    case QEvent::MouseButtonPress:
#ifdef DBG_JCG_EVENT
        printf("JCG: Mouse Button Pushed. Widget: %p\n",mWidget);
#endif
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Handled\n");
#endif
        handled = MouseButtonEvent((QMouseEvent*)aEvent,PR_TRUE,1);
      }
      else
        handled = true;
      break;

    case QEvent::MouseButtonRelease:
#ifdef DBG_JCG_EVENT
        printf("JCG: Mouse Button Released widget: %p\n",mWidget);
#endif
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Handled\n");
#endif
        handled = MouseButtonEvent((QMouseEvent*)aEvent,PR_FALSE,1);
      }
      else
        handled = true;
      break;

    case QEvent::MouseButtonDblClick:
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Mouse Button Double-clicked widget: %p\n",mWidget);
#endif
        handled = MouseButtonEvent((QMouseEvent*)aEvent,PR_TRUE,2);
      }
      else
        handled = true;
      break;

    case QEvent::MouseMove:
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Mouse Moved widget: %p\n",mWidget);
#endif
        handled = MouseMovedEvent((QMouseEvent*)aEvent);
      }
      else
        handled = true;
      break;

    case QEvent::Wheel:
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Mouse Wheel widget: %p\n",mWidget);
#endif
        handled = MouseWheelEvent((QWheelEvent*)aEvent);
      }
      else
        handled = true;
      break;

    case QEvent::KeyPress:
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Key Pressed widget: %p\n",mWidget);
#endif
        handled = KeyPressEvent((QKeyEvent*)aEvent);
      }
      else
        handled = true;
      break;

    case QEvent::KeyRelease:
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Key Released widget: %p\n",mWidget);
#endif
        handled = KeyReleaseEvent((QKeyEvent*)aEvent);
      }
      else
        handled = true;
      break;

    case QEvent::Enter:
#ifdef DBG_JCG_EVENT
        printf("JCG: Mouse Enter widget: %p\n",mWidget);
#endif
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Handled\n");
#endif
        handled = MouseEnterEvent(aEvent);
      }
      else
        handled = true;
      break;

    case QEvent::Leave:
#ifdef DBG_JCG_EVENT
        printf("JCG: Mouse Exit widget: %p\n",mWidget);
#endif
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Handled\n");
#endif
        handled = MouseExitEvent(aEvent);
      }
      else
        handled = true;
      break;

    case QEvent::Close:
      if (!mWidget->IsPopup()) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Close widget: %p\n",mWidget);
#endif
        handled = DestroyEvent();
      }
      break;

    case QEvent::Destroy:
#ifdef DBG_JCG_EVENT
      printf("JCG: Destroy widget: %p\n",mWidget);
#endif
      handled = DestroyEvent();
      break;

    case QEvent::Resize:
#ifdef DBG_JCG_EVENT
      printf("JCG: Resize widget: %p\n",mWidget);
#endif
      handled = ResizeEvent((QResizeEvent*)aEvent);
      break;

    case QEvent::Move:
#ifdef DBG_JCG_EVENT
      printf("JCG: Move widget: %p\n",mWidget);
#endif
      handled = MoveEvent((QMoveEvent*)aEvent);
      break;

    case QEvent::Paint:
#ifdef DBG_JCG_EVENT
      printf("JCG: Paint widget: %p\n",mWidget);
#endif
      handled = PaintEvent((QPaintEvent*)aEvent);
      break;

    case QEvent::FocusIn:
#ifdef DBG_JCG_EVENT
        printf("JCG: Focus In widget: %p\n",mWidget);
#endif
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Handled\n");
#endif
        handled = FocusInEvent();
      }
      else
        handled = true;
      break;

    case QEvent::FocusOut:
#ifdef DBG_JCG_EVENT
        printf("JCG: Focus Out widget: %p\n",mWidget);
#endif
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Handled\n");
#endif
        handled = FocusOutEvent();
      }
      else
        handled = true;
      break;

    case QEvent::DragEnter:
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Drag Enter widget: %p\n",mWidget);
#endif
        handled = DragEnterEvent((QDragEnterEvent*)aEvent);
      }
      else
        handled = true;
      break;

    case QEvent::DragLeave:
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Drag Leave widget: %p\n",mWidget);
#endif
        handled = DragLeaveEvent((QDragLeaveEvent*)aEvent);
      }
      else
        handled = true;
      break;

    case QEvent::DragMove:
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Drag Move widget: %p\n",mWidget);
#endif
        handled = DragMoveEvent((QDragMoveEvent*)aEvent);
      }
      else
        handled = true;
      break;

    case QEvent::Drop:
      if (mEnabled) {
#ifdef DBG_JCG_EVENT
        printf("JCG: Drop widget: %p\n",mWidget);
#endif
        handled =  DropEvent((QDropEvent*)aEvent);
      }
      else
        handled = true;
      break;

    default:
#ifdef DBG_JCG_EVENT
      printf("JCG: widget: %p, Other: %d\n",mWidget,aEvent->type());
#endif
      break;
  }
  return handled;
}

PRBool nsQBaseWidget::MouseButtonEvent(QMouseEvent *aEvent,PRBool aButtonDown,
                                       int aClickCount)
{
  if (aEvent && mWidget) {
    nsMouseEvent nsEvent;

    switch (aEvent->button()) {
      case LeftButton:
        nsEvent.message = aButtonDown ? NS_MOUSE_LEFT_BUTTON_DOWN
                                      : NS_MOUSE_LEFT_BUTTON_UP;
        break;

      case RightButton:
        nsEvent.message = aButtonDown ? NS_MOUSE_RIGHT_BUTTON_DOWN
                                      : NS_MOUSE_RIGHT_BUTTON_UP;
        break;

      case MidButton:
        nsEvent.message = aButtonDown ? NS_MOUSE_MIDDLE_BUTTON_DOWN
                                      : NS_MOUSE_MIDDLE_BUTTON_UP;
        break;

      default:
        // This shouldn't happen!
        NS_ASSERTION(0, "Bad MousePressedEvent!");
        nsEvent.message = NS_MOUSE_MOVE;
        break;
    }
    nsEvent.point.x         = nscoord(aEvent->x());
    nsEvent.point.y         = nscoord(aEvent->y());
    nsEvent.widget          = mWidget;
    nsEvent.nativeMsg       = (void*)aEvent;
    nsEvent.eventStructType = NS_MOUSE_EVENT;
    nsEvent.clickCount      = aClickCount;
    nsEvent.isShift         = aEvent->state() & ShiftButton;
    nsEvent.isControl       = aEvent->state() & ControlButton;
    nsEvent.isAlt           = aEvent->state() & AltButton;
    nsEvent.isMeta          = PR_FALSE;
    nsEvent.time            = 0;

    mWidget->DispatchMouseEvent(nsEvent);
  }
  if (aButtonDown)
    return PR_FALSE;
  else
    return PR_TRUE;
}

PRBool nsQBaseWidget::MouseMovedEvent(QMouseEvent *aEvent)
{
  if (aEvent && mWidget) {
    // Generate XPFE mouse moved event
    nsMouseEvent nsEvent;

    nsEvent.point.x         = nscoord(aEvent->x());
    nsEvent.point.y         = nscoord(aEvent->y());
    nsEvent.message         = NS_MOUSE_MOVE;
    nsEvent.widget          = mWidget;
    nsEvent.nativeMsg       = (void*)aEvent;
    nsEvent.eventStructType = NS_MOUSE_EVENT;
    nsEvent.time            = 0;
    nsEvent.isShift         = aEvent->state() & ShiftButton;
    nsEvent.isControl       = aEvent->state() & ControlButton;
    nsEvent.isAlt           = aEvent->state() & AltButton;
    nsEvent.isMeta          = PR_FALSE;

    mWidget->DispatchMouseEvent(nsEvent);
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::MouseEnterEvent(QEvent *aEvent)
{
  if (aEvent && mWidget) {
    nsMouseEvent nsEvent;

    nsEvent.message         = NS_MOUSE_ENTER;
    nsEvent.widget          = mWidget;
    nsEvent.nativeMsg       = (void*)aEvent;
    nsEvent.eventStructType = NS_MOUSE_EVENT;
    nsEvent.time            = 0;

    mWidget->DispatchMouseEvent(nsEvent);
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::MouseExitEvent(QEvent *aEvent)
{
  if (aEvent && mWidget) {
    nsMouseEvent nsEvent;

    nsEvent.message         = NS_MOUSE_EXIT;
    nsEvent.widget          = mWidget;
    nsEvent.nativeMsg       = (void*)aEvent;
    nsEvent.eventStructType = NS_MOUSE_EVENT;
    nsEvent.time            = 0;

    mWidget->DispatchMouseEvent(nsEvent);
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::MouseWheelEvent(QWheelEvent *aEvent)
{
  if (aEvent && mWidget) {
    nsMouseScrollEvent nsEvent;

    nsEvent.scrollFlags     = nsMouseScrollEvent::kIsVertical;
    nsEvent.delta           = (int)((aEvent->delta()/120) * -3);
    nsEvent.message         = NS_MOUSE_SCROLL;
    nsEvent.widget          = mWidget;
    nsEvent.nativeMsg       = (void*)aEvent;
    nsEvent.eventStructType = NS_MOUSE_SCROLL_EVENT;
    nsEvent.time            = 0;
    nsEvent.point.x         = nscoord(aEvent->x());
    nsEvent.point.y         = nscoord(aEvent->y());
    nsEvent.isShift         = aEvent->state() & ShiftButton;
    nsEvent.isControl       = aEvent->state() & ControlButton;
    nsEvent.isAlt           = aEvent->state() & AltButton;
    nsEvent.isMeta          = PR_FALSE;

    mWidget->DispatchMouseScrollEvent(nsEvent);
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::DestroyEvent()
{
  if (mWidget) {
    // Generate XPFE destroy event
    mWidget->Destroy();
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::ResizeEvent(QResizeEvent *aEvent)
{
  if (aEvent && mWidget) {
    nsRect rect;

    // Generate XPFE resize event
    mWidget->GetBounds(rect);
    rect.width = aEvent->size().width();
    rect.height = aEvent->size().height();

    mWidget->OnResize(rect);
  }
  return PR_FALSE;
}

PRBool nsQBaseWidget::MoveEvent(QMoveEvent *aEvent)
{
  if (aEvent && mWidget) {
    // Generate XPFE move event
    mWidget->OnMove(aEvent->pos().x(),aEvent->pos().y());
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::PaintEvent(QPaintEvent *aEvent)
{
  if (aEvent && mWidget) {
    nsPaintEvent nsEvent;
    nsRect rect(aEvent->rect().x(),aEvent->rect().y(),
                aEvent->rect().width(),aEvent->rect().height());

    // Generate XPFE paint event
    nsEvent.message         = NS_PAINT;
    nsEvent.widget          = mWidget;
    nsEvent.eventStructType = NS_PAINT_EVENT;
    nsEvent.time            = 0;
    nsEvent.rect            = &rect;

    mWidget->OnPaint(nsEvent);
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::KeyPressEvent(QKeyEvent *aEvent)
{
  if (aEvent && mWidget) {
    if (aEvent->key() == Qt::Key_Shift || aEvent->key() == Qt::Key_Control
        || aEvent->key() == Qt::Key_Alt) {
      return PR_FALSE;
    }
    nsKeyEvent nsEvent;

    nsEvent.message         = NS_KEY_DOWN;
    nsEvent.eventStructType = NS_KEY_EVENT;
    nsEvent.widget          = mWidget;
    nsEvent.keyCode         = NS_GetKey(aEvent->key());
    nsEvent.isShift         = aEvent->state() & ShiftButton;
    nsEvent.isControl       = aEvent->state() & ControlButton;
    nsEvent.isAlt           = aEvent->state() & AltButton;
    nsEvent.isMeta          = PR_FALSE;
    nsEvent.point.x         = 0;
    nsEvent.point.y         = 0;
    nsEvent.time            = 0;
    nsEvent.charCode        = 0;

    mWidget->OnKey(nsEvent);

    nsEvent.message         = NS_KEY_PRESS;
    nsEvent.eventStructType = NS_KEY_EVENT;
    nsEvent.widget          = mWidget;
    nsEvent.isShift         = aEvent->state() & ShiftButton;
    nsEvent.isControl       = aEvent->state() & ControlButton;
    nsEvent.isAlt           = aEvent->state() & AltButton;
    nsEvent.isMeta          = PR_FALSE;
    nsEvent.point.x         = 0;
    nsEvent.point.y         = 0;
    nsEvent.time            = 0;
    if (aEvent->text().length() && aEvent->text()[0].isPrint()) {
      nsEvent.charCode = (PRInt32)aEvent->text()[0].unicode();
    }
    else {
      nsEvent.charCode = 0;
    }
    if (nsEvent.charCode) {
       nsEvent.keyCode = 0;
       nsEvent.isShift = PR_FALSE;
    }
    else
      nsEvent.keyCode = NS_GetKey(aEvent->key());

    mWidget->OnKey(nsEvent);
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::KeyReleaseEvent(QKeyEvent *aEvent)
{
  if (aEvent && mWidget) {
    if (aEvent->key() == Qt::Key_Shift || aEvent->key() == Qt::Key_Control
        || aEvent->key() == Qt::Key_Alt) {
      return PR_FALSE;
    }
    nsKeyEvent nsEvent;

    nsEvent.message         = NS_KEY_UP;
    nsEvent.eventStructType = NS_KEY_EVENT;
    nsEvent.widget          = mWidget;
    nsEvent.charCode        = 0;
    nsEvent.keyCode         = NS_GetKey(aEvent->key());
    nsEvent.isShift         = aEvent->state() & ShiftButton;
    nsEvent.isControl       = aEvent->state() & ControlButton;
    nsEvent.isAlt           = aEvent->state() & AltButton;
    nsEvent.isMeta          = PR_FALSE;
    nsEvent.time            = 0;

    mWidget->OnKey(nsEvent);
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::FocusInEvent()
{
  if (mWidget) {
    mWidget->DispatchFocusInEvent();
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::FocusOutEvent()
{
  if (mWidget) {
    mWidget->DispatchFocusOutEvent();
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::DragEnterEvent(QDragEnterEvent *aEvent)
{
  if (mWidget && aEvent) {
    int res;
    nsMouseEvent nsEvent;

    nsEvent.message = NS_DRAGDROP_ENTER;
    nsEvent.eventStructType = NS_DRAGDROP_EVENT;
    nsEvent.widget = mWidget;
    nsEvent.point.x = aEvent->pos().x();
    nsEvent.point.y = aEvent->pos().y();

    nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
    if (dragService != 0) {
      dragService->StartDragSession();

      nsCOMPtr<nsIDragSessionQt> qtSession(do_QueryInterface(dragService));
      if (qtSession) {
        qtSession->SetDragReference(aEvent);
        aEvent->accept(0);
      }
    }
    res = mWidget->DispatchMouseEvent(nsEvent);
#ifdef NS_DEBUG
    printf("NS_DRAGDROP_ENTER %i\n",res);
#endif
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::DragMoveEvent(QDragMoveEvent *aEvent)
{
  if (mWidget && aEvent) {
    nsMouseEvent nsEvent;

    nsEvent.message = NS_DRAGDROP_OVER;
    nsEvent.eventStructType = NS_DRAGDROP_EVENT;
    nsEvent.widget = mWidget;
    nsEvent.point.x = aEvent->pos().x();
    nsEvent.point.y = aEvent->pos().y();

    mWidget->DispatchMouseEvent(nsEvent);

    nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
    if (dragService != 0) {
      nsCOMPtr<nsIDragSession> dragSession = do_QueryInterface(dragService);
      if (dragSession != 0) {
        PRBool aCanDrop;

        dragSession->GetCanDrop(&aCanDrop);
        aEvent->accept(aCanDrop);
        dragSession->SetCanDrop(0);
      }
    }
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::DragLeaveEvent(QDragLeaveEvent *aEvent)
{
  if (mWidget) {
    nsMouseEvent nsEvent;

    nsEvent.message = NS_DRAGDROP_EXIT;
    nsEvent.eventStructType = NS_DRAGDROP_EVENT;
    nsEvent.widget = mWidget;

    mWidget->DispatchMouseEvent(nsEvent);
#ifdef NS_DEBUG
    printf("NS_DRAGDROP_EXIT\n");
#endif

    nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
    if (dragService != 0) {
      nsCOMPtr<nsIDragSession> dragSession = do_QueryInterface(dragService);
      if (dragSession != 0)
        dragSession->SetCanDrop(0);
      dragService->EndDragSession();
    }
  }
  return PR_TRUE;
}

PRBool nsQBaseWidget::DropEvent(QDropEvent *aEvent)
{
  if (mWidget && aEvent) {
    nsMouseEvent nsEvent;
    int res;

    nsEvent.message = NS_DRAGDROP_DROP;
    nsEvent.eventStructType = NS_DRAGDROP_EVENT;
    nsEvent.widget = mWidget;
    nsEvent.point.x = aEvent->pos().x();
    nsEvent.point.y = aEvent->pos().y();

    res = mWidget->DispatchMouseEvent(nsEvent);
#ifdef NS_DEBUG
    printf("NS_DRAGDROP_DROP %i\n",res);
#endif
  }
  return PR_TRUE;
}

//=============================================================================
//
// nsQWidget class
//
//=============================================================================
nsQWidget::nsQWidget(QWidget *aParent,const char *aName,unsigned int aFlags)
             : QWidget(aParent,aName,(WFlags)aFlags)
{
  setAcceptDrops(PR_TRUE);
  setBackgroundMode(QWidget::NoBackground);
  setMouseTracking(PR_TRUE);
  setFocusPolicy(QWidget::WheelFocus);

#ifdef DBG_JCG
  gQWidgetCount++;
  mQWidgetID = gQWidgetID++;
  printf("JCG: nsQWidget CTOR (%p), ID: %d, Count: %d, Parent: %p\n",
         this,mQWidgetID,gQWidgetCount,aParent);
#endif
}

nsQWidget::~nsQWidget()
{
#ifdef DBG_JCG
  gQWidgetCount--;
  printf("JCG: nsQWidget DTOR (%p), ID: %d, Count: %d\n",
         this,mQWidgetID,gQWidgetCount);
#endif
}

void qt_enter_modal(QWidget*);          // defined in qapplication_x11.cpp
void qt_leave_modal(QWidget*);          // --- "" ---

void nsQWidget::SetModal(PRBool aState)
{
  if (aState) {
    setWFlags(Qt::WType_Modal);
    setWState(Qt::WState_Modal);
    qt_enter_modal(this);
  }
  else {
    clearWFlags(Qt::WType_Modal);
    clearWState(Qt::WState_Modal);
    qt_leave_modal(this);
  }
}
