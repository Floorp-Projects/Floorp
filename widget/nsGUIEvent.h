/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGUIEvent_h__
#define nsGUIEvent_h__

#include "mozilla/BasicEvents.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"

#define NS_EVENT_TYPE_NULL                   0
#define NS_EVENT_TYPE_ALL                  1 // Not a real event type

#define NS_IS_INPUT_EVENT(evnt) \
       (((evnt)->eventStructType == NS_INPUT_EVENT) || \
        ((evnt)->eventStructType == NS_MOUSE_EVENT) || \
        ((evnt)->eventStructType == NS_KEY_EVENT) || \
        ((evnt)->eventStructType == NS_TOUCH_EVENT) || \
        ((evnt)->eventStructType == NS_DRAG_EVENT) || \
        ((evnt)->eventStructType == NS_MOUSE_SCROLL_EVENT) || \
        ((evnt)->eventStructType == NS_WHEEL_EVENT) || \
        ((evnt)->eventStructType == NS_SIMPLE_GESTURE_EVENT))

#define NS_IS_MOUSE_EVENT(evnt) \
       (((evnt)->message == NS_MOUSE_BUTTON_DOWN) || \
        ((evnt)->message == NS_MOUSE_BUTTON_UP) || \
        ((evnt)->message == NS_MOUSE_CLICK) || \
        ((evnt)->message == NS_MOUSE_DOUBLECLICK) || \
        ((evnt)->message == NS_MOUSE_ENTER) || \
        ((evnt)->message == NS_MOUSE_EXIT) || \
        ((evnt)->message == NS_MOUSE_ACTIVATE) || \
        ((evnt)->message == NS_MOUSE_ENTER_SYNTH) || \
        ((evnt)->message == NS_MOUSE_EXIT_SYNTH) || \
        ((evnt)->message == NS_MOUSE_MOZHITTEST) || \
        ((evnt)->message == NS_MOUSE_MOVE))

#define NS_IS_MOUSE_EVENT_STRUCT(evnt) \
       ((evnt)->eventStructType == NS_MOUSE_EVENT || \
        (evnt)->eventStructType == NS_DRAG_EVENT)

#define NS_IS_MOUSE_LEFT_CLICK(evnt) \
       ((evnt)->eventStructType == NS_MOUSE_EVENT && \
        (evnt)->message == NS_MOUSE_CLICK && \
        static_cast<nsMouseEvent*>((evnt))->button == nsMouseEvent::eLeftButton)

#define NS_IS_CONTEXT_MENU_KEY(evnt) \
       ((evnt)->eventStructType == NS_MOUSE_EVENT && \
        (evnt)->message == NS_CONTEXTMENU && \
        static_cast<nsMouseEvent*>((evnt))->context == nsMouseEvent::eContextMenuKey)

#define NS_IS_DRAG_EVENT(evnt) \
       (((evnt)->message == NS_DRAGDROP_ENTER) || \
        ((evnt)->message == NS_DRAGDROP_OVER) || \
        ((evnt)->message == NS_DRAGDROP_EXIT) || \
        ((evnt)->message == NS_DRAGDROP_DRAGDROP) || \
        ((evnt)->message == NS_DRAGDROP_GESTURE) || \
        ((evnt)->message == NS_DRAGDROP_DRAG) || \
        ((evnt)->message == NS_DRAGDROP_END) || \
        ((evnt)->message == NS_DRAGDROP_START) || \
        ((evnt)->message == NS_DRAGDROP_DROP) || \
        ((evnt)->message == NS_DRAGDROP_LEAVE_SYNTH))

#define NS_IS_KEY_EVENT(evnt) \
       (((evnt)->message == NS_KEY_DOWN) ||  \
        ((evnt)->message == NS_KEY_PRESS) || \
        ((evnt)->message == NS_KEY_UP))

#define NS_IS_IME_EVENT(evnt) \
       (((evnt)->message == NS_TEXT_TEXT) ||  \
        ((evnt)->message == NS_COMPOSITION_START) ||  \
        ((evnt)->message == NS_COMPOSITION_END) || \
        ((evnt)->message == NS_COMPOSITION_UPDATE))

#define NS_IS_ACTIVATION_EVENT(evnt) \
        (((evnt)->message == NS_PLUGIN_ACTIVATE) || \
        ((evnt)->message == NS_PLUGIN_FOCUS))

#define NS_IS_QUERY_CONTENT_EVENT(evnt) \
       ((evnt)->eventStructType == NS_QUERY_CONTENT_EVENT)

#define NS_IS_SELECTION_EVENT(evnt) \
       (((evnt)->message == NS_SELECTION_SET))

#define NS_IS_CONTENT_COMMAND_EVENT(evnt) \
       ((evnt)->eventStructType == NS_CONTENT_COMMAND_EVENT)

#define NS_IS_PLUGIN_EVENT(evnt) \
       (((evnt)->message == NS_PLUGIN_INPUT_EVENT) || \
        ((evnt)->message == NS_PLUGIN_FOCUS_EVENT))

#define NS_IS_RETARGETED_PLUGIN_EVENT(evnt) \
       (NS_IS_PLUGIN_EVENT(evnt) && \
        (static_cast<nsPluginEvent*>(evnt)->retargetToFocusedDocument))

#define NS_IS_NON_RETARGETED_PLUGIN_EVENT(evnt) \
       (NS_IS_PLUGIN_EVENT(evnt) && \
        !(static_cast<nsPluginEvent*>(evnt)->retargetToFocusedDocument))

// Be aware the query content events and the selection events are a part of IME
// processing.  So, you shouldn't use NS_IS_IME_EVENT macro directly in most
// cases, you should use NS_IS_IME_RELATED_EVENT instead.
#define NS_IS_IME_RELATED_EVENT(evnt) \
  (NS_IS_IME_EVENT(evnt) || \
   NS_IS_QUERY_CONTENT_EVENT(evnt) || \
   NS_IS_SELECTION_EVENT(evnt))

/**
 * Whether the event should be handled by the frame of the mouse cursor
 * position or not.  When it should be handled there (e.g., the mouse events),
 * this returns TRUE.
 */
inline bool NS_IsEventUsingCoordinates(nsEvent* aEvent)
{
  return !NS_IS_KEY_EVENT(aEvent) && !NS_IS_IME_RELATED_EVENT(aEvent) &&
         !NS_IS_CONTEXT_MENU_KEY(aEvent) && !NS_IS_ACTIVATION_EVENT(aEvent) &&
         !NS_IS_PLUGIN_EVENT(aEvent) &&
         !NS_IS_CONTENT_COMMAND_EVENT(aEvent) &&
         aEvent->message != NS_PLUGIN_RESOLUTION_CHANGED;
}

/**
 * Whether the event should be handled by the focused DOM window in the
 * same top level window's or not.  E.g., key events, IME related events
 * (including the query content events, they are used in IME transaction)
 * should be handled by the (last) focused window rather than the dispatched
 * window.
 *
 * NOTE: Even if this returns TRUE, the event isn't going to be handled by the
 * application level active DOM window which is on another top level window.
 * So, when the event is fired on a deactive window, the event is going to be
 * handled by the last focused DOM window in the last focused window.
 */
inline bool NS_IsEventTargetedAtFocusedWindow(nsEvent* aEvent)
{
  return NS_IS_KEY_EVENT(aEvent) || NS_IS_IME_RELATED_EVENT(aEvent) ||
         NS_IS_CONTEXT_MENU_KEY(aEvent) ||
         NS_IS_CONTENT_COMMAND_EVENT(aEvent) ||
         NS_IS_RETARGETED_PLUGIN_EVENT(aEvent);
}

/**
 * Whether the event should be handled by the focused content or not.  E.g.,
 * key events, IME related events and other input events which are not handled
 * by the frame of the mouse cursor position.
 *
 * NOTE: Even if this returns TRUE, the event isn't going to be handled by the
 * application level active DOM window which is on another top level window.
 * So, when the event is fired on a deactive window, the event is going to be
 * handled by the last focused DOM element of the last focused DOM window in
 * the last focused window.
 */
inline bool NS_IsEventTargetedAtFocusedContent(nsEvent* aEvent)
{
  return NS_IS_KEY_EVENT(aEvent) || NS_IS_IME_RELATED_EVENT(aEvent) ||
         NS_IS_CONTEXT_MENU_KEY(aEvent) ||
         NS_IS_RETARGETED_PLUGIN_EVENT(aEvent);
}

/**
 * Whether the event should cause a DOM event.
 */
inline bool NS_IsAllowedToDispatchDOMEvent(nsEvent* aEvent)
{
  switch (aEvent->eventStructType) {
    case NS_MOUSE_EVENT:
      // We want synthesized mouse moves to cause mouseover and mouseout
      // DOM events (nsEventStateManager::PreHandleEvent), but not mousemove
      // DOM events.
      // Synthesized button up events also do not cause DOM events because they
      // do not have a reliable refPoint.
      return static_cast<nsMouseEvent*>(aEvent)->reason == nsMouseEvent::eReal;

    case NS_WHEEL_EVENT: {
      // wheel event whose all delta values are zero by user pref applied, it
      // shouldn't cause a DOM event.
      mozilla::WheelEvent* wheelEvent =
        static_cast<mozilla::WheelEvent*>(aEvent);
      return wheelEvent->deltaX != 0.0 || wheelEvent->deltaY != 0.0 ||
             wheelEvent->deltaZ != 0.0;
    }

    default:
      return true;
  }
}

#endif // nsGUIEvent_h__
