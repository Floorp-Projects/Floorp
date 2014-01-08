/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasicEvents.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/MutationEvent.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"

namespace mozilla {

/******************************************************************************
 * As*Event() implementation
 ******************************************************************************/

#define NS_ROOT_EVENT_CLASS(aPrefix, aName)
#define NS_EVENT_CLASS(aPrefix, aName) \
aPrefix##aName* \
WidgetEvent::As##aName() \
{ \
  return nullptr; \
} \
\
const aPrefix##aName* \
WidgetEvent::As##aName() const \
{ \
  return const_cast<WidgetEvent*>(this)->As##aName(); \
}

#include "mozilla/EventClassList.h"

#undef NS_EVENT_CLASS
#undef NS_ROOT_EVENT_CLASS

/******************************************************************************
 * mozilla::WidgetEvent
 *
 * Event struct type checking methods.
 ******************************************************************************/

bool
WidgetEvent::IsQueryContentEvent() const
{
  return eventStructType == NS_QUERY_CONTENT_EVENT;
}

bool
WidgetEvent::IsSelectionEvent() const
{
  return eventStructType == NS_SELECTION_EVENT;
}

bool
WidgetEvent::IsContentCommandEvent() const
{
  return eventStructType == NS_CONTENT_COMMAND_EVENT;
}

bool
WidgetEvent::IsNativeEventDelivererForPlugin() const
{
  return eventStructType == NS_PLUGIN_EVENT;
}


/******************************************************************************
 * mozilla::WidgetEvent
 *
 * Event message checking methods.
 ******************************************************************************/

bool
WidgetEvent::HasMouseEventMessage() const
{
  switch (message) {
    case NS_MOUSE_BUTTON_DOWN:
    case NS_MOUSE_BUTTON_UP:
    case NS_MOUSE_CLICK:
    case NS_MOUSE_DOUBLECLICK:
    case NS_MOUSE_ENTER:
    case NS_MOUSE_EXIT:
    case NS_MOUSE_ACTIVATE:
    case NS_MOUSE_ENTER_SYNTH:
    case NS_MOUSE_EXIT_SYNTH:
    case NS_MOUSE_MOZHITTEST:
    case NS_MOUSE_MOVE:
      return true;
    default:
      return false;
  }
}

bool
WidgetEvent::HasDragEventMessage() const
{
  switch (message) {
    case NS_DRAGDROP_ENTER:
    case NS_DRAGDROP_OVER:
    case NS_DRAGDROP_EXIT:
    case NS_DRAGDROP_DRAGDROP:
    case NS_DRAGDROP_GESTURE:
    case NS_DRAGDROP_DRAG:
    case NS_DRAGDROP_END:
    case NS_DRAGDROP_START:
    case NS_DRAGDROP_DROP:
    case NS_DRAGDROP_LEAVE_SYNTH:
      return true;
    default:
      return false;
  }
}

bool
WidgetEvent::HasKeyEventMessage() const
{
  switch (message) {
    case NS_KEY_DOWN:
    case NS_KEY_PRESS:
    case NS_KEY_UP:
      return true;
    default:
      return false;
  }
}

bool
WidgetEvent::HasIMEEventMessage() const
{
  switch (message) {
    case NS_TEXT_TEXT:
    case NS_COMPOSITION_START:
    case NS_COMPOSITION_END:
    case NS_COMPOSITION_UPDATE:
      return true;
    default:
      return false;
  }
}

bool
WidgetEvent::HasPluginActivationEventMessage() const
{
  return message == NS_PLUGIN_ACTIVATE ||
         message == NS_PLUGIN_FOCUS;
}

/******************************************************************************
 * mozilla::WidgetEvent
 *
 * Specific event checking methods.
 ******************************************************************************/

bool
WidgetEvent::IsRetargetedNativeEventDelivererForPlugin() const
{
  const WidgetPluginEvent* pluginEvent = AsPluginEvent();
  return pluginEvent && pluginEvent->retargetToFocusedDocument;
}

bool
WidgetEvent::IsNonRetargetedNativeEventDelivererForPlugin() const
{
  const WidgetPluginEvent* pluginEvent = AsPluginEvent();
  return pluginEvent && !pluginEvent->retargetToFocusedDocument;
}

bool
WidgetEvent::IsIMERelatedEvent() const
{
  return HasIMEEventMessage() || IsQueryContentEvent() || IsSelectionEvent();
}

bool
WidgetEvent::IsUsingCoordinates() const
{
  const WidgetMouseEvent* mouseEvent = AsMouseEvent();
  if (mouseEvent) {
    return !mouseEvent->IsContextMenuKeyEvent();
  }
  return !HasKeyEventMessage() && !IsIMERelatedEvent() &&
         !HasPluginActivationEventMessage() &&
         !IsNativeEventDelivererForPlugin() &&
         !IsContentCommandEvent() &&
         message != NS_PLUGIN_RESOLUTION_CHANGED;
}

bool
WidgetEvent::IsTargetedAtFocusedWindow() const
{
  const WidgetMouseEvent* mouseEvent = AsMouseEvent();
  if (mouseEvent) {
    return mouseEvent->IsContextMenuKeyEvent();
  }
  return HasKeyEventMessage() || IsIMERelatedEvent() ||
         IsContentCommandEvent() ||
         IsRetargetedNativeEventDelivererForPlugin();
}

bool
WidgetEvent::IsTargetedAtFocusedContent() const
{
  const WidgetMouseEvent* mouseEvent = AsMouseEvent();
  if (mouseEvent) {
    return mouseEvent->IsContextMenuKeyEvent();
  }
  return HasKeyEventMessage() || IsIMERelatedEvent() ||
         IsRetargetedNativeEventDelivererForPlugin();
}

bool
WidgetEvent::IsAllowedToDispatchDOMEvent() const
{
  switch (eventStructType) {
    case NS_MOUSE_EVENT:
    case NS_POINTER_EVENT:
      // We want synthesized mouse moves to cause mouseover and mouseout
      // DOM events (nsEventStateManager::PreHandleEvent), but not mousemove
      // DOM events.
      // Synthesized button up events also do not cause DOM events because they
      // do not have a reliable refPoint.
      return AsMouseEvent()->reason == WidgetMouseEvent::eReal;

    case NS_WHEEL_EVENT: {
      // wheel event whose all delta values are zero by user pref applied, it
      // shouldn't cause a DOM event.
      const WidgetWheelEvent* wheelEvent = AsWheelEvent();
      return wheelEvent->deltaX != 0.0 || wheelEvent->deltaY != 0.0 ||
             wheelEvent->deltaZ != 0.0;
    }

    default:
      return true;
  }
}

/******************************************************************************
 * mozilla::WidgetKeyboardEvent (TextEvents.h)
 ******************************************************************************/

/*static*/ void
WidgetKeyboardEvent::GetDOMKeyName(KeyNameIndex aKeyNameIndex,
                                   nsAString& aKeyName)
{
  // The expected way to implement this function would be to use a
  // switch statement.  By using a table-based implementation, below, we
  // ensure that this function executes in constant time in cases where
  // compilers wouldn't be able to convert the switch statement to a
  // jump table.  This table-based implementation also minimizes the
  // space required by the code and data.
#define KEY_STR_NUM_INTERNAL(line) key##line
#define KEY_STR_NUM(line) KEY_STR_NUM_INTERNAL(line)

  // Catch non-ASCII DOM key names in our key name list.
#define NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName)                      \
  static_assert(sizeof(aDOMKeyName) == MOZ_ARRAY_LENGTH(aDOMKeyName), \
                "Invalid DOM key name");
#include "nsDOMKeyNameList.h"
#undef NS_DEFINE_KEYNAME

  struct KeyNameTable
  {
#define NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName)          \
    char16_t KEY_STR_NUM(__LINE__)[sizeof(aDOMKeyName)];
#include "nsDOMKeyNameList.h"
#undef NS_DEFINE_KEYNAME
  };

  static const KeyNameTable kKeyNameTable = {
#define NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName) MOZ_UTF16(aDOMKeyName),
#include "nsDOMKeyNameList.h"
#undef NS_DEFINE_KEYNAME
  };

  static const uint16_t kKeyNameOffsets[] = {
#define NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName)          \
    offsetof(struct KeyNameTable, KEY_STR_NUM(__LINE__)),
#include "nsDOMKeyNameList.h"
#undef NS_DEFINE_KEYNAME
    // Include this entry so we can compute lengths easily.
    sizeof(kKeyNameTable)
  };

  // Use the sizeof trick rather than MOZ_ARRAY_LENGTH to avoid problems
  // with constexpr functions called inside static_assert with some
  // compilers.
  static_assert(KEY_NAME_INDEX_USE_STRING ==
                (sizeof(kKeyNameOffsets)/sizeof(kKeyNameOffsets[0])) - 1,
                "Invalid enumeration values!");

  if (aKeyNameIndex >= KEY_NAME_INDEX_USE_STRING) {
    aKeyName.Truncate();
    return;
  }

  uint16_t offset = kKeyNameOffsets[aKeyNameIndex];
  uint16_t nextOffset = kKeyNameOffsets[aKeyNameIndex + 1];
  const char16_t* table = reinterpret_cast<const char16_t*>(&kKeyNameTable);

  // Subtract off 1 for the null terminator.
  aKeyName.Assign(table + offset, nextOffset - offset - 1);

#undef KEY_STR_NUM
#undef KEY_STR_NUM_INTERNAL
}

} // namespace mozilla
