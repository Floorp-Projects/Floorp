/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_EventForwards_h__
#define mozilla_EventForwards_h__

#include <stdint.h>

#include "nsStringFwd.h"
#include "nsTArray.h"

/**
 * XXX Following enums should be in BasicEvents.h.  However, currently, it's
 *     impossible to use foward delearation for enum.
 */

/**
 * Return status for event processors.
 */
enum nsEventStatus {
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

enum class CanBubble { eYes, eNo };

enum class Cancelable { eYes, eNo };

enum class ChromeOnlyDispatch { eYes, eNo };

enum class Trusted { eYes, eNo };

enum class Composed { eYes, eNo, eDefault };

/**
 * Event messages
 */

typedef uint16_t EventMessageType;

enum EventMessage : EventMessageType {

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

enum EventClassID : EventClassIDType {
// The event class name will be:
//   eBasicEventClass for WidgetEvent
//   eFooEventClass for WidgetFooEvent or InternalFooEvent
#define NS_ROOT_EVENT_CLASS(aPrefix, aName) eBasic##aName##Class
#define NS_EVENT_CLASS(aPrefix, aName) , e##aName##Class

#include "mozilla/EventClassList.h"

#undef NS_EVENT_CLASS
#undef NS_ROOT_EVENT_CLASS
};

const char* ToChar(EventClassID aEventClassID);

typedef uint16_t Modifiers;

#define NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName) KEY_NAME_INDEX_##aCPPName,

typedef uint16_t KeyNameIndexType;
enum KeyNameIndex : KeyNameIndexType {
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
enum CodeNameIndex : CodeNameIndexType {
#include "mozilla/PhysicalKeyCodeNameList.h"
  // If a DOM keyboard event is synthesized by script, this is used.  Then,
  // specified code name should be stored and use it as .code value.
  CODE_NAME_INDEX_USE_STRING
};

#undef NS_DEFINE_PHYSICAL_KEY_CODE_NAME

const nsCString ToString(CodeNameIndex aCodeNameIndex);

#define NS_DEFINE_INPUTTYPE(aCPPName, aDOMName) e##aCPPName,

typedef uint8_t EditorInputTypeType;
enum class EditorInputType : EditorInputTypeType {
#include "mozilla/InputTypeList.h"
  // If a DOM input event is synthesized by script, this is used.  Then,
  // specified input type should be stored as string and use it as .inputType
  // value.
  eUnknown,
};

#undef NS_DEFINE_INPUTTYPE

inline bool ExposesClipboardDataOrDataTransfer(EditorInputType aInputType) {
  switch (aInputType) {
    case EditorInputType::eInsertFromPaste:
    case EditorInputType::eInsertFromPasteAsQuotation:
      return true;
    default:
      return false;
  }
}

/**
 * IsDataAvailableOnTextEditor() returns true if aInputType on TextEditor
 * should have non-null InputEvent.data value.
 */
inline bool IsDataAvailableOnTextEditor(EditorInputType aInputType) {
  switch (aInputType) {
    case EditorInputType::eInsertText:
    case EditorInputType::eInsertCompositionText:
    case EditorInputType::eInsertFromComposition:  // Only level 2
    case EditorInputType::eInsertFromPaste:
    case EditorInputType::eInsertFromPasteAsQuotation:
    case EditorInputType::eInsertTranspose:
    case EditorInputType::eInsertFromDrop:
    case EditorInputType::eInsertReplacementText:
    case EditorInputType::eInsertFromYank:
    case EditorInputType::eFormatSetBlockTextDirection:
    case EditorInputType::eFormatSetInlineTextDirection:
      return true;
    default:
      return false;
  }
}

/**
 * IsDataAvailableOnHTMLEditor() returns true if aInputType on HTMLEditor
 * should have non-null InputEvent.data value.
 */
inline bool IsDataAvailableOnHTMLEditor(EditorInputType aInputType) {
  switch (aInputType) {
    case EditorInputType::eInsertText:
    case EditorInputType::eInsertCompositionText:
    case EditorInputType::eInsertFromComposition:  // Only level 2
    case EditorInputType::eFormatSetBlockTextDirection:
    case EditorInputType::eFormatSetInlineTextDirection:
    case EditorInputType::eInsertLink:
    case EditorInputType::eFormatBackColor:
    case EditorInputType::eFormatFontColor:
    case EditorInputType::eFormatFontName:
      return true;
    default:
      return false;
  }
}

/**
 * IsDataTransferAvailableOnHTMLEditor() returns true if aInputType on
 * HTMLEditor should have non-null InputEvent.dataTransfer value.
 */
inline bool IsDataTransferAvailableOnHTMLEditor(EditorInputType aInputType) {
  switch (aInputType) {
    case EditorInputType::eInsertFromPaste:
    case EditorInputType::eInsertFromPasteAsQuotation:
    case EditorInputType::eInsertFromDrop:
    case EditorInputType::eInsertTranspose:
    case EditorInputType::eInsertReplacementText:
    case EditorInputType::eInsertFromYank:
      return true;
    default:
      return false;
  }
}

#define NS_DEFINE_COMMAND(aName, aCommandStr) , aName
#define NS_DEFINE_COMMAND_WITH_PARAM(aName, aCommandStr, aParam) , aName
#define NS_DEFINE_COMMAND_NO_EXEC_COMMAND(aName) , aName

typedef int8_t CommandInt;
enum class Command : CommandInt {
  DoNothing

#include "mozilla/CommandList.h"
};
#undef NS_DEFINE_COMMAND
#undef NS_DEFINE_COMMAND_WITH_PARAM
#undef NS_DEFINE_COMMAND_NO_EXEC_COMMAND

const char* ToChar(Command aCommand);

/**
 * Return a command value for aCommandName.
 * NOTE: We use overloads instead of default constructor with EmptyString()
 *       because using it here requires to include another header file but
 *       this header file shouldn't include other header files as far as
 *       possible to avoid making rebuild time of them longer.
 * XXX: Is there a better place to put `Command` related methods instead of
 *      global scope in `mozilla` namespace?
 *
 * @param aCommandName  Should be a XUL command name like "cmd_bold" (case
 *                      sensitive).
 * @param aValue        Additional parameter value of aCommandName.
 *                      It depends on the command whethere it's case sensitive
 *                      or not.
 */
Command GetInternalCommand(const char* aCommandName);
Command GetInternalCommand(const char* aCommandName, const nsAString& aValue);

}  // namespace mozilla

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
enum class AccessKeyType;

struct AlternativeCharCode;
struct ShortcutKeyCandidate;

typedef nsTArray<ShortcutKeyCandidate> ShortcutKeyCandidateArray;
typedef AutoTArray<ShortcutKeyCandidate, 10> AutoShortcutKeyCandidateArray;

// TextRange.h
typedef uint8_t RawTextRangeType;
enum class TextRangeType : RawTextRangeType;

struct TextRangeStyle;
struct TextRange;

class EditCommands;
class TextRangeArray;

// FontRange.h
struct FontRange;

enum MouseButton { eNotPressed = -1, eLeft = 0, eMiddle = 1, eRight = 2 };

enum MouseButtonsFlag {
  eNoButtons = 0x00,
  eLeftFlag = 0x01,
  eRightFlag = 0x02,
  eMiddleFlag = 0x04,
  // typicall, "back" button being left side of 5-button
  // mice, see "buttons" attribute document of DOM3 Events.
  e4thFlag = 0x08,
  // typicall, "forward" button being right side of 5-button
  // mice, see "buttons" attribute document of DOM3 Events.
  e5thFlag = 0x10
};

}  // namespace mozilla

#endif  // mozilla_EventForwards_h__
