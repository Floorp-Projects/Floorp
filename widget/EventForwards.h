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
  nsEventStatus_eConsumeDoDefault,
  // Value is not for use, only for serialization
  nsEventStatus_eSentinel
};

namespace mozilla {

/**
 * Event messages
 */

typedef uint16_t EventMessageType;

enum EventMessage : EventMessageType
{

#define NS_EVENT_MESSAGE(aMessage, aValue) aMessage = aValue,

#include "mozilla/EventMessageList.h"

#undef NS_EVENT_MESSAGE

  // For preventing bustage due to "," after the last item.
  eEventMessage_MaxValue
};

/**
 * Event class IDs
 */

typedef uint8_t EventClassIDType;

enum EventClassID : EventClassIDType
{
  // The event class name will be:
  //   eBasicEventClass for WidgetEvent
  //   eFooEventClass for WidgetFooEvent or InternalFooEvent
#define NS_ROOT_EVENT_CLASS(aPrefix, aName)   eBasic##aName##Class
#define NS_EVENT_CLASS(aPrefix, aName)      , e##aName##Class

#include "mozilla/EventClassList.h"

#undef NS_EVENT_CLASS
#undef NS_ROOT_EVENT_CLASS
};

typedef uint16_t Modifiers;

#define NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName) \
  KEY_NAME_INDEX_##aCPPName,

enum KeyNameIndex
{
#include "mozilla/KeyNameList.h"
  // If a DOM keyboard event is synthesized by script, this is used.  Then,
  // specified key name should be stored and use it as .key value.
  KEY_NAME_INDEX_USE_STRING
};

#undef NS_DEFINE_KEYNAME

#define NS_DEFINE_PHYSICAL_KEY_CODE_NAME(aCPPName, aDOMCodeName) \
  CODE_NAME_INDEX_##aCPPName,

enum CodeNameIndex
{
#include "mozilla/PhysicalKeyCodeNameList.h"
  // If a DOM keyboard event is synthesized by script, this is used.  Then,
  // specified code name should be stored and use it as .code value.
  CODE_NAME_INDEX_USE_STRING
};

#undef NS_DEFINE_PHYSICAL_KEY_CODE_NAME

#define NS_DEFINE_COMMAND(aName, aCommandStr) , Command##aName

typedef int8_t CommandInt;
enum Command : CommandInt
{
  CommandDoNothing

#include "mozilla/CommandList.h"
};
#undef NS_DEFINE_COMMAND

} // namespace mozilla

/**
 * All header files should include this header instead of *Events.h.
 */

namespace mozilla {

#define NS_EVENT_CLASS(aPrefix, aName) class aPrefix##aName;
#define NS_ROOT_EVENT_CLASS(aPrefix, aName) NS_EVENT_CLASS(aPrefix, aName)

#include "mozilla/EventClassList.h"

#undef NS_EVENT_CLASS
#undef NS_ROOT_EVENT_CLASS

// BasicEvents.h
struct BaseEventFlags;
struct EventFlags;

// TextEvents.h
struct AlternativeCharCode;

// TextRange.h
struct TextRangeStyle;
struct TextRange;

class TextRangeArray;

// FontRange.h
struct FontRange;

} // namespace mozilla

#endif // mozilla_EventForwards_h__
