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
  mOnDestroyCalled = PR_FALSE;
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
nsCommonWidget::CommonCreate(nsIWidget *aParent)
{
  mParent = aParent;
}

void
nsCommonWidget::InitPaintEvent(nsPaintEvent &aEvent)
{
  memset(&aEvent, 0, sizeof(nsPaintEvent));
  aEvent.eventStructType = NS_PAINT_EVENT;
  aEvent.message = NS_PAINT;
  aEvent.widget = NS_STATIC_CAST(nsIWidget *, this);
}

void
nsCommonWidget::InitSizeEvent(nsSizeEvent &aEvent)
{
  memset(&aEvent, 0, sizeof(nsSizeEvent));
  aEvent.eventStructType = NS_SIZE_EVENT;
  aEvent.message = NS_SIZE;
  aEvent.widget = NS_STATIC_CAST(nsIWidget *, this);
}

void
nsCommonWidget::InitGUIEvent(nsGUIEvent &aEvent, PRUint32 aMsg)
{
  memset(&aEvent, 0, sizeof(nsGUIEvent));
  aEvent.eventStructType = NS_GUI_EVENT;
  aEvent.message = aMsg;
  aEvent.widget = NS_STATIC_CAST(nsIWidget *, this);
}

void
nsCommonWidget::InitMouseEvent(nsMouseEvent &aEvent, PRUint32 aMsg)
{
  memset(&aEvent, 0, sizeof(nsMouseEvent));
  aEvent.eventStructType = NS_MOUSE_EVENT;
  aEvent.message = aMsg;
  aEvent.widget = NS_STATIC_CAST(nsIWidget *, this);
}

void
nsCommonWidget::InitButtonEvent(nsMouseEvent &aEvent, PRUint32 aMsg,
				GdkEventButton *aGdkEvent)
{
  memset(&aEvent, 0, sizeof(nsMouseEvent));
  aEvent.eventStructType = NS_MOUSE_EVENT;
  aEvent.message = aMsg;
  aEvent.widget = NS_STATIC_CAST(nsIWidget *, this);

  aEvent.point.x = nscoord(aGdkEvent->x);
  aEvent.point.y = nscoord(aGdkEvent->y);

  aEvent.isShift   = (aGdkEvent->state & GDK_SHIFT_MASK)
    ? PR_TRUE : PR_FALSE;
  aEvent.isControl = (aGdkEvent->state & GDK_CONTROL_MASK)
    ? PR_TRUE : PR_FALSE;
  aEvent.isAlt     = (aGdkEvent->state & GDK_MOD1_MASK)
    ? PR_TRUE : PR_FALSE;
  aEvent.isMeta    = PR_FALSE; // Gtk+ doesn't have meta

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
				     GdkEventScroll *aGdkEvent, PRUint32 aMsg)
{
  memset(&aEvent, 0, sizeof(nsMouseScrollEvent));
  aEvent.eventStructType = NS_MOUSE_SCROLL_EVENT;
  aEvent.message = aMsg;
  aEvent.widget = NS_STATIC_CAST(nsIWidget *, this);

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

  aEvent.isShift   = (aGdkEvent->state & GDK_SHIFT_MASK)
    ? PR_TRUE : PR_FALSE;
  aEvent.isControl = (aGdkEvent->state & GDK_CONTROL_MASK)
    ? PR_TRUE : PR_FALSE;
  aEvent.isAlt     = (aGdkEvent->state & GDK_MOD1_MASK)
    ? PR_TRUE : PR_FALSE;
  aEvent.isMeta    = PR_FALSE; // Gtk+ doesn't have meta

}

void
nsCommonWidget::InitKeyEvent(nsKeyEvent &aEvent, GdkEventKey *aGdkEvent,
			     PRUint32 aMsg)
{
  memset(&aEvent, 0, sizeof(nsKeyEvent));
  aEvent.eventStructType = NS_KEY_EVENT;
  aEvent.message   = aMsg;
  aEvent.widget    = NS_STATIC_CAST(nsIWidget *, this);
  aEvent.keyCode   = GdkKeyCodeToDOMKeyCode(aGdkEvent->keyval);
  aEvent.charCode  = 0;
  aEvent.isShift   = (aGdkEvent->state & GDK_SHIFT_MASK)
    ? PR_TRUE : PR_FALSE;
  aEvent.isControl = (aGdkEvent->state & GDK_CONTROL_MASK)
    ? PR_TRUE : PR_FALSE;
  aEvent.isAlt     = (aGdkEvent->state & GDK_MOD1_MASK)
    ? PR_TRUE : PR_FALSE;
  aEvent.isMeta    = PR_FALSE; // Gtk+ doesn't have meta
}

void
nsCommonWidget::DispatchGotFocusEvent(void)
{
  nsGUIEvent event;
  InitGUIEvent(event, NS_GOTFOCUS);
  nsEventStatus status;
  DispatchEvent(&event, status);
}

void
nsCommonWidget::DispatchLostFocusEvent(void)
{
  nsGUIEvent event;
  InitGUIEvent(event, NS_LOSTFOCUS);
  nsEventStatus status;
  DispatchEvent(&event, status);
}

void
nsCommonWidget::DispatchActivateEvent(void)
{
  nsGUIEvent event;
  InitGUIEvent(event, NS_ACTIVATE);
  nsEventStatus status;
  DispatchEvent(&event, status);
}

void
nsCommonWidget::DispatchDeactivateEvent(void)
{
  nsGUIEvent event;
  InitGUIEvent(event, NS_DEACTIVATE);
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

  NS_RELEASE(aEvent->widget);

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

  nsCOMPtr<nsIWidget> kungFuDeathGrip = this;

  nsGUIEvent event;
  InitGUIEvent(event, NS_DESTROY);
  
  nsEventStatus status;
  DispatchEvent(&event, status);
}
