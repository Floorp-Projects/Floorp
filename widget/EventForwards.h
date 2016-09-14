/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventForwards_h__
#define mozilla_EventForwards_h__

#include <stdint.h>

#include "nsTArray.h"

class nsCString;

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

#define NS_EVENT_MESSAGE(aMessage) aMessage,
#define NS_EVENT_MESSAGE_FIRST_LAST(aMessage, aFirst, aLast) \
  aMessage##First = aFirst, aMessage##Last = aLast,

#include "mozilla/EventMessageList.h"

#undef NS_EVENT_MESSAGE
#undef NS_EVENT_MESSAGE_FIRST_LAST

  // For preventing bustage due to "," after the last item.
  eEventMessage_MaxValue
};

const char* ToChar(EventMessage aEventMessage);

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

const char* ToChar(EventClassID aEventClassID);

typedef uint16_t Modifiers;

#define NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName) \
  KEY_NAME_INDEX_##aCPPName,

typedef uint16_t KeyNameIndexType;
enum KeyNameIndex : KeyNameIndexType
{
#include "mozilla/KeyNameList.h"
  // If a DOM keyboard event is synthesized by script, this is used.  Then,
  // specified key name should be stored and use it as .key value.
  KEY_NAME_INDEX_USE_STRING
};

#undef NS_DEFINE_KEYNAME

const nsCString ToString(KeyNameIndex aKeyNameIndex);

#define NS_DEFINE_PHYSICAL_KEY_CODE_NAME(aCPPName, aDOMCodeName) \
  CODE_NAME_INDEX_##aCPPName,

typedef uint8_t CodeNameIndexType;
enum CodeNameIndex : CodeNameIndexType
{
#include "mozilla/PhysicalKeyCodeNameList.h"
  // If a DOM keyboard event is synthesized by script, this is used.  Then,
  // specified code name should be stored and use it as .code value.
  CODE_NAME_INDEX_USE_STRING
};

#undef NS_DEFINE_PHYSICAL_KEY_CODE_NAME

const nsCString ToString(CodeNameIndex aCodeNameIndex);

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

class WidgetEventTime;

class NativeEventData;

// TextEvents.h
struct AlternativeCharCode;
struct ShortcutKeyCandidate;

typedef nsTArray<ShortcutKeyCandidate> ShortcutKeyCandidateArray;
typedef AutoTArray<ShortcutKeyCandidate, 10> AutoShortcutKeyCandidateArray;

// TextRange.h
typedef uint8_t RawTextRangeType;
enum class TextRangeType : RawTextRangeType;

struct TextRangeStyle;
struct TextRange;

class TextRangeArray;

// FontRange.h
struct FontRange;

} // namespace mozilla

#endif // mozilla_EventForwards_h__
