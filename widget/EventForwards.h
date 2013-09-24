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

/**
 * All modifier keys should be defined here.  This is used for managing
 * modifier states for DOM Level 3 or later.
 */
enum Modifier
{
  MODIFIER_ALT        = 0x0001,
  MODIFIER_ALTGRAPH   = 0x0002,
  MODIFIER_CAPSLOCK   = 0x0004,
  MODIFIER_CONTROL    = 0x0008,
  MODIFIER_FN         = 0x0010,
  MODIFIER_META       = 0x0020,
  MODIFIER_NUMLOCK    = 0x0040,
  MODIFIER_SCROLLLOCK = 0x0080,
  MODIFIER_SHIFT      = 0x0100,
  MODIFIER_SYMBOLLOCK = 0x0200,
  MODIFIER_OS         = 0x0400
};

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
} // namespace mozilla

class nsEvent;
class nsGUIEvent;
class nsInputEvent;
class nsUIEvent;

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

#endif // mozilla_EventForwards_h__
