/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventForwards_h__
#define mozilla_EventForwards_h__

#include <stdint.h>

/**
 * XXX Following enums should be in BasicEvents.h.  However, currently, it's
 *     impossible to use foward delearation for enum.
 */

/**
 * Return status for event processors.
 */
enum nsEventStatus
{
  // The event is ignored, do default processing
  nsEventStatus_eIgnore,
  // The event is consumed, don't do default processing
  nsEventStatus_eConsumeNoDefault,
  // The event is consumed, but do default processing
  nsEventStatus_eConsumeDoDefault
};

namespace mozilla {

typedef uint16_t Modifiers;

#define NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName) \
  KEY_NAME_INDEX_##aCPPName,

enum KeyNameIndex
{
#include "nsDOMKeyNameList.h"
  // There shouldn't be "," at the end of enum definition, this dummy item
  // avoids bustage on some platforms.
  NUMBER_OF_KEY_NAME_INDEX
};

#undef NS_DEFINE_KEYNAME

} // namespace mozilla

/**
 * All header files should include this header instead of *Events.h.
 */

// BasicEvents.h
namespace mozilla {
struct EventFlags;

class WidgetEvent;
class WidgetGUIEvent;
class WidgetInputEvent;
class InternalUIEvent;
} // namespace mozilla

// TextEvents.h
struct nsAlternativeCharCode;
struct nsTextRangeStyle;
struct nsTextRange;

class nsKeyEvent;
class nsTextEvent;
class nsCompositionEvent;
class nsQueryContentEvent;
class nsSelectionEvent;

// MouseEvents.h
class nsMouseEvent_base;
class nsMouseEvent;
class nsDragEvent;
class nsMouseScrollEvent;

namespace mozilla {
class WheelEvent;
} // namespace mozilla

// TouchEvents.h
class nsGestureNotifyEvent;
class nsTouchEvent;
class nsSimpleGestureEvent;

// ContentEvents.h
class nsScriptErrorEvent;
class nsScrollPortEvent;
class nsScrollAreaEvent;
class nsFormEvent;
class nsClipboardEvent;
class nsFocusEvent;
class nsTransitionEvent;
class nsAnimationEvent;

// MiscEvents.h
class nsCommandEvent;
class nsContentCommandEvent;
class nsPluginEvent;

// content/events/public/nsMutationEvent.h
class nsMutationEvent;

// TODO: Remove following typedefs
typedef mozilla::WidgetEvent      nsEvent;
typedef mozilla::WidgetGUIEvent   nsGUIEvent;
typedef mozilla::WidgetInputEvent nsInputEvent;
typedef mozilla::InternalUIEvent  nsUIEvent;

#endif // mozilla_EventForwards_h__
