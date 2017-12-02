/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxPrefs.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIDOMEventTarget.h"
#include "nsPrintfCString.h"

namespace mozilla {

/******************************************************************************
 * Global helper methods
 ******************************************************************************/

const char*
ToChar(EventMessage aEventMessage)
{
  switch (aEventMessage) {

#define NS_EVENT_MESSAGE(aMessage) \
    case aMessage: \
      return #aMessage;

#include "mozilla/EventMessageList.h"

#undef NS_EVENT_MESSAGE
    default:
      return "illegal event message";
  }
}

const char*
ToChar(EventClassID aEventClassID)
{
  switch (aEventClassID) {

#define NS_ROOT_EVENT_CLASS(aPrefix, aName) \
    case eBasic##aName##Class: \
      return "eBasic" #aName "Class";

#define NS_EVENT_CLASS(aPrefix, aName) \
    case e##aName##Class: \
      return "e" #aName "Class";

#include "mozilla/EventClassList.h"

#undef NS_EVENT_CLASS
#undef NS_ROOT_EVENT_CLASS
    default:
      return "illegal event class ID";
  }
}

const nsCString
ToString(KeyNameIndex aKeyNameIndex)
{
  if (aKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
    return NS_LITERAL_CSTRING("USE_STRING");
  }
  nsAutoString keyName;
  WidgetKeyboardEvent::GetDOMKeyName(aKeyNameIndex, keyName);
  return NS_ConvertUTF16toUTF8(keyName);
}

const nsCString
ToString(CodeNameIndex aCodeNameIndex)
{
  if (aCodeNameIndex == CODE_NAME_INDEX_USE_STRING) {
    return NS_LITERAL_CSTRING("USE_STRING");
  }
  nsAutoString codeName;
  WidgetKeyboardEvent::GetDOMCodeName(aCodeNameIndex, codeName);
  return NS_ConvertUTF16toUTF8(codeName);
}

const char*
ToChar(Command aCommand)
{
  if (aCommand == CommandDoNothing) {
    return "CommandDoNothing";
  }

  switch (aCommand) {

#define NS_DEFINE_COMMAND(aName, aCommandStr) \
    case Command##aName: \
      return "Command" #aName;
#define NS_DEFINE_COMMAND_NO_EXEC_COMMAND(aName) \
    case Command##aName: \
      return "Command" #aName;

#include "mozilla/CommandList.h"

#undef NS_DEFINE_COMMAND
#undef NS_DEFINE_COMMAND_NO_EXEC_COMMAND

    default:
      return "illegal command value";
  }
}

const nsCString
GetDOMKeyCodeName(uint32_t aKeyCode)
{
  switch (aKeyCode) {
#define NS_DISALLOW_SAME_KEYCODE
#define NS_DEFINE_VK(aDOMKeyName, aDOMKeyCode) \
    case aDOMKeyCode: \
      return NS_LITERAL_CSTRING(#aDOMKeyName);

#include "mozilla/VirtualKeyCodeList.h"

#undef NS_DEFINE_VK
#undef NS_DISALLOW_SAME_KEYCODE

    default:
      return nsPrintfCString("Invalid DOM keyCode (0x%08X)", aKeyCode);
  }
}

bool
IsValidRawTextRangeValue(RawTextRangeType aRawTextRangeType)
{
  switch (static_cast<TextRangeType>(aRawTextRangeType)) {
    case TextRangeType::eUninitialized:
    case TextRangeType::eCaret:
    case TextRangeType::eRawClause:
    case TextRangeType::eSelectedRawClause:
    case TextRangeType::eConvertedClause:
    case TextRangeType::eSelectedClause:
      return true;
    default:
      return false;
  }
}

RawTextRangeType
ToRawTextRangeType(TextRangeType aTextRangeType)
{
  return static_cast<RawTextRangeType>(aTextRangeType);
}

TextRangeType
ToTextRangeType(RawTextRangeType aRawTextRangeType)
{
  MOZ_ASSERT(IsValidRawTextRangeValue(aRawTextRangeType));
  return static_cast<TextRangeType>(aRawTextRangeType);
}

const char*
ToChar(TextRangeType aTextRangeType)
{
  switch (aTextRangeType) {
    case TextRangeType::eUninitialized:
      return "TextRangeType::eUninitialized";
    case TextRangeType::eCaret:
      return "TextRangeType::eCaret";
    case TextRangeType::eRawClause:
      return "TextRangeType::eRawClause";
    case TextRangeType::eSelectedRawClause:
      return "TextRangeType::eSelectedRawClause";
    case TextRangeType::eConvertedClause:
      return "TextRangeType::eConvertedClause";
    case TextRangeType::eSelectedClause:
      return "TextRangeType::eSelectedClause";
    default:
      return "Invalid TextRangeType";
  }
}

SelectionType
ToSelectionType(TextRangeType aTextRangeType)
{
  switch (aTextRangeType) {
    case TextRangeType::eRawClause:
      return SelectionType::eIMERawClause;
    case TextRangeType::eSelectedRawClause:
      return SelectionType::eIMESelectedRawClause;
    case TextRangeType::eConvertedClause:
      return SelectionType::eIMEConvertedClause;
    case TextRangeType::eSelectedClause:
      return SelectionType::eIMESelectedClause;
    default:
      MOZ_CRASH("TextRangeType is invalid");
      return SelectionType::eNormal;
  }
}

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
  return mClass == eQueryContentEventClass;
}

bool
WidgetEvent::IsSelectionEvent() const
{
  return mClass == eSelectionEventClass;
}

bool
WidgetEvent::IsContentCommandEvent() const
{
  return mClass == eContentCommandEventClass;
}

bool
WidgetEvent::IsNativeEventDelivererForPlugin() const
{
  return mClass == ePluginEventClass;
}


/******************************************************************************
 * mozilla::WidgetEvent
 *
 * Event message checking methods.
 ******************************************************************************/

bool
WidgetEvent::HasMouseEventMessage() const
{
  switch (mMessage) {
    case eMouseDown:
    case eMouseUp:
    case eMouseClick:
    case eMouseDoubleClick:
    case eMouseAuxClick:
    case eMouseEnterIntoWidget:
    case eMouseExitFromWidget:
    case eMouseActivate:
    case eMouseOver:
    case eMouseOut:
    case eMouseHitTest:
    case eMouseMove:
      return true;
    default:
      return false;
  }
}

bool
WidgetEvent::HasDragEventMessage() const
{
  switch (mMessage) {
    case eDragEnter:
    case eDragOver:
    case eDragExit:
    case eDrag:
    case eDragEnd:
    case eDragStart:
    case eDrop:
    case eDragLeave:
      return true;
    default:
      return false;
  }
}

/* static */
bool
WidgetEvent::IsKeyEventMessage(EventMessage aMessage)
{
  switch (aMessage) {
    case eKeyDown:
    case eKeyPress:
    case eKeyUp:
    case eKeyDownOnPlugin:
    case eKeyUpOnPlugin:
    case eAccessKeyNotFound:
      return true;
    default:
      return false;
  }
}

bool
WidgetEvent::HasIMEEventMessage() const
{
  switch (mMessage) {
    case eCompositionStart:
    case eCompositionEnd:
    case eCompositionUpdate:
    case eCompositionChange:
    case eCompositionCommitAsIs:
    case eCompositionCommit:
      return true;
    default:
      return false;
  }
}

bool
WidgetEvent::HasPluginActivationEventMessage() const
{
  return mMessage == ePluginActivate ||
         mMessage == ePluginFocus;
}

/******************************************************************************
 * mozilla::WidgetEvent
 *
 * Specific event checking methods.
 ******************************************************************************/

bool
WidgetEvent::CanBeSentToRemoteProcess() const
{
  // If this event is explicitly marked as shouldn't be sent to remote process,
  // just return false.
  if (IsCrossProcessForwardingStopped()) {
    return false;
  }

  if (mClass == eKeyboardEventClass ||
      mClass == eWheelEventClass) {
    return true;
  }

  switch (mMessage) {
    case eMouseDown:
    case eMouseUp:
    case eMouseMove:
    case eContextMenu:
    case eMouseEnterIntoWidget:
    case eMouseExitFromWidget:
    case eMouseTouchDrag:
    case eTouchStart:
    case eTouchMove:
    case eTouchEnd:
    case eTouchCancel:
    case eDragOver:
    case eDragExit:
    case eDrop:
      return true;
    default:
      return false;
  }
}

bool
WidgetEvent::WillBeSentToRemoteProcess() const
{
  // This event won't be posted to remote process if it's already explicitly
  // stopped.
  if (IsCrossProcessForwardingStopped()) {
    return false;
  }

  // When mOriginalTarget is nullptr, this method shouldn't be used.
  if (NS_WARN_IF(!mOriginalTarget)) {
    return false;
  }

  nsCOMPtr<nsIContent> originalTarget = do_QueryInterface(mOriginalTarget);
  return EventStateManager::IsRemoteTarget(originalTarget);
}

bool
WidgetEvent::IsRetargetedNativeEventDelivererForPlugin() const
{
  const WidgetPluginEvent* pluginEvent = AsPluginEvent();
  return pluginEvent && pluginEvent->mRetargetToFocusedDocument;
}

bool
WidgetEvent::IsNonRetargetedNativeEventDelivererForPlugin() const
{
  const WidgetPluginEvent* pluginEvent = AsPluginEvent();
  return pluginEvent && !pluginEvent->mRetargetToFocusedDocument;
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
         !IsContentCommandEvent();
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
  switch (mClass) {
    case eMouseEventClass:
      // When content PreventDefault on ePointerDown, we will stop dispatching
      // the subsequent mouse events (eMouseDown, eMouseUp, eMouseMove). But we
      // still need the mouse events to be handled in EventStateManager to
      // generate other events (e.g. eMouseClick). So we only stop dispatching
      // them to DOM.
      if (DefaultPreventedByContent() &&
          (mMessage == eMouseMove || mMessage == eMouseDown ||
           mMessage == eMouseUp)) {
        return false;
      }
      if (mMessage == eMouseTouchDrag) {
        return false;
      }
      MOZ_FALLTHROUGH;
    case ePointerEventClass:
      // We want synthesized mouse moves to cause mouseover and mouseout
      // DOM events (EventStateManager::PreHandleEvent), but not mousemove
      // DOM events.
      // Synthesized button up events also do not cause DOM events because they
      // do not have a reliable mRefPoint.
      return AsMouseEvent()->mReason == WidgetMouseEvent::eReal;

    case eWheelEventClass: {
      // wheel event whose all delta values are zero by user pref applied, it
      // shouldn't cause a DOM event.
      const WidgetWheelEvent* wheelEvent = AsWheelEvent();
      return wheelEvent->mDeltaX != 0.0 || wheelEvent->mDeltaY != 0.0 ||
             wheelEvent->mDeltaZ != 0.0;
    }
    case eTouchEventClass:
      return mMessage != eTouchPointerCancel;
    // Following events are handled in EventStateManager, so, we don't need to
    // dispatch DOM event for them into the DOM tree.
    case eQueryContentEventClass:
    case eSelectionEventClass:
    case eContentCommandEventClass:
      return false;

    default:
      return true;
  }
}

bool
WidgetEvent::IsAllowedToDispatchInSystemGroup() const
{
  // We don't expect to implement default behaviors with pointer events because
  // if we do, prevent default on mouse events can't prevent default behaviors
  // anymore.
  return mClass != ePointerEventClass;
}

/******************************************************************************
 * mozilla::WidgetEvent
 *
 * Misc methods.
 ******************************************************************************/

static dom::EventTarget*
GetTargetForDOMEvent(nsIDOMEventTarget* aTarget)
{
  return aTarget ? aTarget->GetTargetForDOMEvent() : nullptr;
}

dom::EventTarget*
WidgetEvent::GetDOMEventTarget() const
{
  return GetTargetForDOMEvent(mTarget);
}

dom::EventTarget*
WidgetEvent::GetCurrentDOMEventTarget() const
{
  return GetTargetForDOMEvent(mCurrentTarget);
}

dom::EventTarget*
WidgetEvent::GetOriginalDOMEventTarget() const
{
  if (mOriginalTarget) {
    return GetTargetForDOMEvent(mOriginalTarget);
  }
  return GetDOMEventTarget();
}

void
WidgetEvent::PreventDefault(bool aCalledByDefaultHandler,
                            nsIPrincipal* aPrincipal)
{
  if (mMessage == ePointerDown) {
    if (aCalledByDefaultHandler) {
      // Shouldn't prevent default on pointerdown by default handlers to stop
      // firing legacy mouse events. Use MOZ_ASSERT to catch incorrect usages
      // in debug builds.
      MOZ_ASSERT(false);
      return;
    }
    if (aPrincipal) {
      nsAutoString addonId;
      Unused << NS_WARN_IF(NS_FAILED(aPrincipal->GetAddonId(addonId)));
      if (!addonId.IsEmpty()) {
        // Ignore the case that it's called by a web extension.
        return;
      }
    }
  }
  mFlags.PreventDefault(aCalledByDefaultHandler);
}

bool
WidgetEvent::IsUserAction() const
{
  if (!IsTrusted()) {
    return false;
  }
  // FYI: eMouseScrollEventClass and ePointerEventClass represent
  //      user action but they are synthesized events.
  switch (mClass) {
    case eKeyboardEventClass:
    case eCompositionEventClass:
    case eMouseScrollEventClass:
    case eWheelEventClass:
    case eGestureNotifyEventClass:
    case eSimpleGestureEventClass:
    case eTouchEventClass:
    case eCommandEventClass:
    case eContentCommandEventClass:
    case ePluginEventClass:
      return true;
    case eMouseEventClass:
    case eDragEventClass:
    case ePointerEventClass:
      return AsMouseEvent()->IsReal();
    default:
      return false;
  }
}

/******************************************************************************
 * mozilla::WidgetInputEvent
 ******************************************************************************/

/* static */
Modifier
WidgetInputEvent::GetModifier(const nsAString& aDOMKeyName)
{
  if (aDOMKeyName.EqualsLiteral("Accel")) {
    return AccelModifier();
  }
  KeyNameIndex keyNameIndex = WidgetKeyboardEvent::GetKeyNameIndex(aDOMKeyName);
  return WidgetKeyboardEvent::GetModifierForKeyName(keyNameIndex);
}

/* static */
Modifier
WidgetInputEvent::AccelModifier()
{
  static Modifier sAccelModifier = MODIFIER_NONE;
  if (sAccelModifier == MODIFIER_NONE) {
    switch (Preferences::GetInt("ui.key.accelKey", 0)) {
      case nsIDOMKeyEvent::DOM_VK_META:
        sAccelModifier = MODIFIER_META;
        break;
      case nsIDOMKeyEvent::DOM_VK_WIN:
        sAccelModifier = MODIFIER_OS;
        break;
      case nsIDOMKeyEvent::DOM_VK_ALT:
        sAccelModifier = MODIFIER_ALT;
        break;
      case nsIDOMKeyEvent::DOM_VK_CONTROL:
        sAccelModifier = MODIFIER_CONTROL;
        break;
      default:
#ifdef XP_MACOSX
        sAccelModifier = MODIFIER_META;
#else
        sAccelModifier = MODIFIER_CONTROL;
#endif
        break;
    }
  }
  return sAccelModifier;
}

/******************************************************************************
 * mozilla::WidgetWheelEvent (MouseEvents.h)
 ******************************************************************************/

/* static */ double
WidgetWheelEvent::ComputeOverriddenDelta(double aDelta, bool aIsForVertical)
{
  if (!gfxPrefs::MouseWheelHasRootScrollDeltaOverride()) {
    return aDelta;
  }
  int32_t intFactor = aIsForVertical
                      ? gfxPrefs::MouseWheelRootScrollVerticalFactor()
                      : gfxPrefs::MouseWheelRootScrollHorizontalFactor();
  // Making the scroll speed slower doesn't make sense. So, ignore odd factor
  // which is less than 1.0.
  if (intFactor <= 100) {
    return aDelta;
  }
  double factor = static_cast<double>(intFactor) / 100;
  return aDelta * factor;
}

double
WidgetWheelEvent::OverriddenDeltaX() const
{
  if (!mAllowToOverrideSystemScrollSpeed) {
    return mDeltaX;
  }
  return ComputeOverriddenDelta(mDeltaX, false);
}

double
WidgetWheelEvent::OverriddenDeltaY() const
{
  if (!mAllowToOverrideSystemScrollSpeed) {
    return mDeltaY;
  }
  return ComputeOverriddenDelta(mDeltaY, true);
}

/******************************************************************************
 * mozilla::WidgetKeyboardEvent (TextEvents.h)
 ******************************************************************************/

#define NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName) (u"" aDOMKeyName),
const char16_t* const WidgetKeyboardEvent::kKeyNames[] = {
#include "mozilla/KeyNameList.h"
};
#undef NS_DEFINE_KEYNAME

#define NS_DEFINE_PHYSICAL_KEY_CODE_NAME(aCPPName, aDOMCodeName) \
    (u"" aDOMCodeName),
const char16_t* const WidgetKeyboardEvent::kCodeNames[] = {
#include "mozilla/PhysicalKeyCodeNameList.h"
};
#undef NS_DEFINE_PHYSICAL_KEY_CODE_NAME

WidgetKeyboardEvent::KeyNameIndexHashtable*
  WidgetKeyboardEvent::sKeyNameIndexHashtable = nullptr;
WidgetKeyboardEvent::CodeNameIndexHashtable*
  WidgetKeyboardEvent::sCodeNameIndexHashtable = nullptr;

void
WidgetKeyboardEvent::InitAllEditCommands()
{
  // If the event was created without widget, e.g., created event in chrome
  // script, this shouldn't execute native key bindings.
  if (NS_WARN_IF(!mWidget)) {
    return;
  }

  // This event should be trusted event here and we shouldn't expose native
  // key binding information to web contents with untrusted events.
  if (NS_WARN_IF(!IsTrusted())) {
    return;
  }

  MOZ_ASSERT(XRE_IsParentProcess(),
    "It's too expensive to retrieve all edit commands from remote process");
  MOZ_ASSERT(!AreAllEditCommandsInitialized(),
    "Shouldn't be called two or more times");

  InitEditCommandsFor(nsIWidget::NativeKeyBindingsForSingleLineEditor);
  InitEditCommandsFor(nsIWidget::NativeKeyBindingsForMultiLineEditor);
  InitEditCommandsFor(nsIWidget::NativeKeyBindingsForRichTextEditor);
}

void
WidgetKeyboardEvent::InitEditCommandsFor(nsIWidget::NativeKeyBindingsType aType)
{
  if (NS_WARN_IF(!mWidget) || NS_WARN_IF(!IsTrusted())) {
    return;
  }

  bool& initialized = IsEditCommandsInitializedRef(aType);
  if (initialized) {
    return;
  }
  nsTArray<CommandInt>& commands = EditCommandsRef(aType);
  mWidget->GetEditCommands(aType, *this, commands);
  initialized = true;
}

bool
WidgetKeyboardEvent::ExecuteEditCommands(nsIWidget::NativeKeyBindingsType aType,
                                         DoCommandCallback aCallback,
                                         void* aCallbackData)
{
  // If the event was created without widget, e.g., created event in chrome
  // script, this shouldn't execute native key bindings.
  if (NS_WARN_IF(!mWidget)) {
    return false;
  }

  // This event should be trusted event here and we shouldn't expose native
  // key binding information to web contents with untrusted events.
  if (NS_WARN_IF(!IsTrusted())) {
    return false;
  }

  InitEditCommandsFor(aType);

  const nsTArray<CommandInt>& commands = EditCommandsRef(aType);
  if (commands.IsEmpty()) {
    return false;
  }

  for (CommandInt command : commands) {
    aCallback(static_cast<Command>(command), aCallbackData);
  }
  return true;
}

bool
WidgetKeyboardEvent::ShouldCauseKeypressEvents() const
{
  // Currently, we don't dispatch keypress events of modifier keys and
  // dead keys.
  switch (mKeyNameIndex) {
    case KEY_NAME_INDEX_Alt:
    case KEY_NAME_INDEX_AltGraph:
    case KEY_NAME_INDEX_CapsLock:
    case KEY_NAME_INDEX_Control:
    case KEY_NAME_INDEX_Fn:
    case KEY_NAME_INDEX_FnLock:
    // case KEY_NAME_INDEX_Hyper:
    case KEY_NAME_INDEX_Meta:
    case KEY_NAME_INDEX_NumLock:
    case KEY_NAME_INDEX_OS:
    case KEY_NAME_INDEX_ScrollLock:
    case KEY_NAME_INDEX_Shift:
    // case KEY_NAME_INDEX_Super:
    case KEY_NAME_INDEX_Symbol:
    case KEY_NAME_INDEX_SymbolLock:
    case KEY_NAME_INDEX_Dead:
      return false;
    default:
      return true;
  }
}

static bool
HasASCIIDigit(const ShortcutKeyCandidateArray& aCandidates)
{
  for (uint32_t i = 0; i < aCandidates.Length(); ++i) {
    uint32_t ch = aCandidates[i].mCharCode;
    if (ch >= '0' && ch <= '9')
      return true;
  }
  return false;
}

static bool
CharsCaseInsensitiveEqual(uint32_t aChar1, uint32_t aChar2)
{
  return aChar1 == aChar2 ||
         (IS_IN_BMP(aChar1) && IS_IN_BMP(aChar2) &&
          ToLowerCase(static_cast<char16_t>(aChar1)) ==
            ToLowerCase(static_cast<char16_t>(aChar2)));
}

static bool
IsCaseChangeableChar(uint32_t aChar)
{
  return IS_IN_BMP(aChar) &&
         ToLowerCase(static_cast<char16_t>(aChar)) !=
           ToUpperCase(static_cast<char16_t>(aChar));
}

void
WidgetKeyboardEvent::GetShortcutKeyCandidates(
                       ShortcutKeyCandidateArray& aCandidates) const
{
  MOZ_ASSERT(aCandidates.IsEmpty(), "aCandidates must be empty");

  // ShortcutKeyCandidate::mCharCode is a candidate charCode.
  // ShortcutKeyCandidate::mIgnoreShift means the mCharCode should be tried to
  // execute a command with/without shift key state. If this is TRUE, the
  // shifted key state should be ignored. Otherwise, don't ignore the state.
  // the priority of the charCodes are (shift key is not pressed):
  //   0: PseudoCharCode()/false,
  //   1: unshiftedCharCodes[0]/false, 2: unshiftedCharCodes[1]/false...
  // the priority of the charCodes are (shift key is pressed):
  //   0: PseudoCharCode()/false,
  //   1: shiftedCharCodes[0]/false, 2: shiftedCharCodes[0]/true,
  //   3: shiftedCharCodes[1]/false, 4: shiftedCharCodes[1]/true...
  uint32_t pseudoCharCode = PseudoCharCode();
  if (pseudoCharCode) {
    ShortcutKeyCandidate key(pseudoCharCode, false);
    aCandidates.AppendElement(key);
  }

  uint32_t len = mAlternativeCharCodes.Length();
  if (!IsShift()) {
    for (uint32_t i = 0; i < len; ++i) {
      uint32_t ch = mAlternativeCharCodes[i].mUnshiftedCharCode;
      if (!ch || ch == pseudoCharCode) {
        continue;
      }
      ShortcutKeyCandidate key(ch, false);
      aCandidates.AppendElement(key);
    }
    // If unshiftedCharCodes doesn't have numeric but shiftedCharCode has it,
    // this keyboard layout is AZERTY or similar layout, probably.
    // In this case, Accel+[0-9] should be accessible without shift key.
    // However, the priority should be lowest.
    if (!HasASCIIDigit(aCandidates)) {
      for (uint32_t i = 0; i < len; ++i) {
        uint32_t ch = mAlternativeCharCodes[i].mShiftedCharCode;
        if (ch >= '0' && ch <= '9') {
          ShortcutKeyCandidate key(ch, false);
          aCandidates.AppendElement(key);
          break;
        }
      }
    }
  } else {
    for (uint32_t i = 0; i < len; ++i) {
      uint32_t ch = mAlternativeCharCodes[i].mShiftedCharCode;
      if (!ch) {
        continue;
      }

      if (ch != pseudoCharCode) {
        ShortcutKeyCandidate key(ch, false);
        aCandidates.AppendElement(key);
      }

      // If the char is an alphabet, the shift key state should not be
      // ignored. E.g., Ctrl+Shift+C should not execute Ctrl+C.

      // And checking the charCode is same as unshiftedCharCode too.
      // E.g., for Ctrl+Shift+(Plus of Numpad) should not run Ctrl+Plus.
      uint32_t unshiftCh = mAlternativeCharCodes[i].mUnshiftedCharCode;
      if (CharsCaseInsensitiveEqual(ch, unshiftCh)) {
        continue;
      }

      // On the Hebrew keyboard layout on Windows, the unshifted char is a
      // localized character but the shifted char is a Latin alphabet,
      // then, we should not execute without the shift state. See bug 433192.
      if (IsCaseChangeableChar(ch)) {
        continue;
      }

      // Setting the alternative charCode candidates for retry without shift
      // key state only when the shift key is pressed.
      ShortcutKeyCandidate key(ch, true);
      aCandidates.AppendElement(key);
    }
  }

  // Special case for "Space" key.  With some keyboard layouts, "Space" with
  // or without Shift key causes non-ASCII space.  For such keyboard layouts,
  // we should guarantee that the key press works as an ASCII white space key
  // press.  However, if the space key is assigned to a function key, it
  // shouldn't work as a space key.
  if (mKeyNameIndex == KEY_NAME_INDEX_USE_STRING &&
      mCodeNameIndex == CODE_NAME_INDEX_Space && pseudoCharCode != ' ') {
    ShortcutKeyCandidate spaceKey(' ', false);
    aCandidates.AppendElement(spaceKey);
  }
}

void
WidgetKeyboardEvent::GetAccessKeyCandidates(nsTArray<uint32_t>& aCandidates) const
{
  MOZ_ASSERT(aCandidates.IsEmpty(), "aCandidates must be empty");

  // return the lower cased charCode candidates for access keys.
  // the priority of the charCodes are:
  //   0: charCode, 1: unshiftedCharCodes[0], 2: shiftedCharCodes[0]
  //   3: unshiftedCharCodes[1], 4: shiftedCharCodes[1],...
  uint32_t pseudoCharCode = PseudoCharCode();
  if (pseudoCharCode) {
    uint32_t ch = pseudoCharCode;
    if (IS_IN_BMP(ch)) {
      ch = ToLowerCase(static_cast<char16_t>(ch));
    }
    aCandidates.AppendElement(ch);
  }
  for (uint32_t i = 0; i < mAlternativeCharCodes.Length(); ++i) {
    uint32_t ch[2] =
      { mAlternativeCharCodes[i].mUnshiftedCharCode,
        mAlternativeCharCodes[i].mShiftedCharCode };
    for (uint32_t j = 0; j < 2; ++j) {
      if (!ch[j]) {
        continue;
      }
      if (IS_IN_BMP(ch[j])) {
        ch[j] = ToLowerCase(static_cast<char16_t>(ch[j]));
      }
      // Don't append the charcode that was already appended.
      if (aCandidates.IndexOf(ch[j]) == aCandidates.NoIndex) {
        aCandidates.AppendElement(ch[j]);
      }
    }
  }
  // Special case for "Space" key.  With some keyboard layouts, "Space" with
  // or without Shift key causes non-ASCII space.  For such keyboard layouts,
  // we should guarantee that the key press works as an ASCII white space key
  // press.  However, if the space key is assigned to a function key, it
  // shouldn't work as a space key.
  if (mKeyNameIndex == KEY_NAME_INDEX_USE_STRING &&
      mCodeNameIndex == CODE_NAME_INDEX_Space && pseudoCharCode != ' ') {
    aCandidates.AppendElement(' ');
  }
}

// mask values for ui.key.chromeAccess and ui.key.contentAccess
#define NS_MODIFIER_SHIFT    1
#define NS_MODIFIER_CONTROL  2
#define NS_MODIFIER_ALT      4
#define NS_MODIFIER_META     8
#define NS_MODIFIER_OS       16

static Modifiers PrefFlagsToModifiers(int32_t aPrefFlags)
{
  Modifiers result = 0;
  if (aPrefFlags & NS_MODIFIER_SHIFT) {
    result |= MODIFIER_SHIFT;
  }
  if (aPrefFlags & NS_MODIFIER_CONTROL) {
    result |= MODIFIER_CONTROL;
  }
  if (aPrefFlags & NS_MODIFIER_ALT) {
    result |= MODIFIER_ALT;
  }
  if (aPrefFlags & NS_MODIFIER_META) {
    result |= MODIFIER_META;
  }
  if (aPrefFlags & NS_MODIFIER_OS) {
    result |= MODIFIER_OS;
  }
  return result;
}

bool
WidgetKeyboardEvent::ModifiersMatchWithAccessKey(AccessKeyType aType) const
{
  if (!ModifiersForAccessKeyMatching()) {
    return false;
  }
  return ModifiersForAccessKeyMatching() == AccessKeyModifiers(aType);
}

Modifiers
WidgetKeyboardEvent::ModifiersForAccessKeyMatching() const
{
  static const Modifiers kModifierMask =
    MODIFIER_SHIFT | MODIFIER_CONTROL |
    MODIFIER_ALT | MODIFIER_META | MODIFIER_OS;
  return mModifiers & kModifierMask;
}

/* static */
Modifiers
WidgetKeyboardEvent::AccessKeyModifiers(AccessKeyType aType)
{
  switch (GenericAccessModifierKeyPref()) {
    case -1:
      break; // use the individual prefs
    case NS_VK_SHIFT:
      return MODIFIER_SHIFT;
    case NS_VK_CONTROL:
      return MODIFIER_CONTROL;
    case NS_VK_ALT:
      return MODIFIER_ALT;
    case NS_VK_META:
      return MODIFIER_META;
    case NS_VK_WIN:
      return MODIFIER_OS;
    default:
      return MODIFIER_NONE;
  }

  switch (aType) {
    case AccessKeyType::eChrome:
      return PrefFlagsToModifiers(ChromeAccessModifierMaskPref());
    case AccessKeyType::eContent:
      return PrefFlagsToModifiers(ContentAccessModifierMaskPref());
    default:
      return MODIFIER_NONE;
  }
}

/* static */
int32_t
WidgetKeyboardEvent::GenericAccessModifierKeyPref()
{
  static bool sInitialized = false;
  static int32_t sValue = -1;
  if (!sInitialized) {
    nsresult rv =
      Preferences::AddIntVarCache(&sValue, "ui.key.generalAccessKey", sValue);
    sInitialized = NS_SUCCEEDED(rv);
    MOZ_ASSERT(sInitialized);
  }
  return sValue;
}

/* static */
int32_t
WidgetKeyboardEvent::ChromeAccessModifierMaskPref()
{
  static bool sInitialized = false;
  static int32_t sValue = 0;
  if (!sInitialized) {
    nsresult rv =
      Preferences::AddIntVarCache(&sValue, "ui.key.chromeAccess", sValue);
    sInitialized = NS_SUCCEEDED(rv);
    MOZ_ASSERT(sInitialized);
  }
  return sValue;
}

/* static */
int32_t
WidgetKeyboardEvent::ContentAccessModifierMaskPref()
{
  static bool sInitialized = false;
  static int32_t sValue = 0;
  if (!sInitialized) {
    nsresult rv =
      Preferences::AddIntVarCache(&sValue, "ui.key.contentAccess", sValue);
    sInitialized = NS_SUCCEEDED(rv);
    MOZ_ASSERT(sInitialized);
  }
  return sValue;
}

/* static */ void
WidgetKeyboardEvent::Shutdown()
{
  delete sKeyNameIndexHashtable;
  sKeyNameIndexHashtable = nullptr;
  delete sCodeNameIndexHashtable;
  sCodeNameIndexHashtable = nullptr;
}

/* static */ void
WidgetKeyboardEvent::GetDOMKeyName(KeyNameIndex aKeyNameIndex,
                                   nsAString& aKeyName)
{
  if (aKeyNameIndex >= KEY_NAME_INDEX_USE_STRING) {
    aKeyName.Truncate();
    return;
  }

  MOZ_RELEASE_ASSERT(static_cast<size_t>(aKeyNameIndex) <
                       ArrayLength(kKeyNames),
                     "Illegal key enumeration value");
  aKeyName = kKeyNames[aKeyNameIndex];
}

/* static */ void
WidgetKeyboardEvent::GetDOMCodeName(CodeNameIndex aCodeNameIndex,
                                    nsAString& aCodeName)
{
  if (aCodeNameIndex >= CODE_NAME_INDEX_USE_STRING) {
    aCodeName.Truncate();
    return;
  }

  MOZ_RELEASE_ASSERT(static_cast<size_t>(aCodeNameIndex) <
                       ArrayLength(kCodeNames),
                     "Illegal physical code enumeration value");
  aCodeName = kCodeNames[aCodeNameIndex];
}

/* static */ KeyNameIndex
WidgetKeyboardEvent::GetKeyNameIndex(const nsAString& aKeyValue)
{
  if (!sKeyNameIndexHashtable) {
    sKeyNameIndexHashtable =
      new KeyNameIndexHashtable(ArrayLength(kKeyNames));
    for (size_t i = 0; i < ArrayLength(kKeyNames); i++) {
      sKeyNameIndexHashtable->Put(nsDependentString(kKeyNames[i]),
                                  static_cast<KeyNameIndex>(i));
    }
  }
  KeyNameIndex result = KEY_NAME_INDEX_USE_STRING;
  sKeyNameIndexHashtable->Get(aKeyValue, &result);
  return result;
}

/* static */ CodeNameIndex
WidgetKeyboardEvent::GetCodeNameIndex(const nsAString& aCodeValue)
{
  if (!sCodeNameIndexHashtable) {
    sCodeNameIndexHashtable =
      new CodeNameIndexHashtable(ArrayLength(kCodeNames));
    for (size_t i = 0; i < ArrayLength(kCodeNames); i++) {
      sCodeNameIndexHashtable->Put(nsDependentString(kCodeNames[i]),
                                   static_cast<CodeNameIndex>(i));
    }
  }
  CodeNameIndex result = CODE_NAME_INDEX_USE_STRING;
  sCodeNameIndexHashtable->Get(aCodeValue, &result);
  return result;
}

/* static */ const char*
WidgetKeyboardEvent::GetCommandStr(Command aCommand)
{
#define NS_DEFINE_COMMAND(aName, aCommandStr) , #aCommandStr
#define NS_DEFINE_COMMAND_NO_EXEC_COMMAND(aName)
  static const char* const kCommands[] = {
    "" // CommandDoNothing
#include "mozilla/CommandList.h"
  };
#undef NS_DEFINE_COMMAND
#undef NS_DEFINE_COMMAND_NO_EXEC_COMMAND

  MOZ_RELEASE_ASSERT(static_cast<size_t>(aCommand) < ArrayLength(kCommands),
                     "Illegal command enumeration value");
  return kCommands[aCommand];
}

/* static */ uint32_t
WidgetKeyboardEvent::ComputeLocationFromCodeValue(CodeNameIndex aCodeNameIndex)
{
  // Following commented out cases are not defined in PhysicalKeyCodeNameList.h
  // but are defined by D3E spec.  So, they should be uncommented when the
  // code values are defined in the header.
  switch (aCodeNameIndex) {
    case CODE_NAME_INDEX_AltLeft:
    case CODE_NAME_INDEX_ControlLeft:
    case CODE_NAME_INDEX_OSLeft:
    case CODE_NAME_INDEX_ShiftLeft:
      return eKeyLocationLeft;
    case CODE_NAME_INDEX_AltRight:
    case CODE_NAME_INDEX_ControlRight:
    case CODE_NAME_INDEX_OSRight:
    case CODE_NAME_INDEX_ShiftRight:
      return eKeyLocationRight;
    case CODE_NAME_INDEX_Numpad0:
    case CODE_NAME_INDEX_Numpad1:
    case CODE_NAME_INDEX_Numpad2:
    case CODE_NAME_INDEX_Numpad3:
    case CODE_NAME_INDEX_Numpad4:
    case CODE_NAME_INDEX_Numpad5:
    case CODE_NAME_INDEX_Numpad6:
    case CODE_NAME_INDEX_Numpad7:
    case CODE_NAME_INDEX_Numpad8:
    case CODE_NAME_INDEX_Numpad9:
    case CODE_NAME_INDEX_NumpadAdd:
    case CODE_NAME_INDEX_NumpadBackspace:
    case CODE_NAME_INDEX_NumpadClear:
    case CODE_NAME_INDEX_NumpadClearEntry:
    case CODE_NAME_INDEX_NumpadComma:
    case CODE_NAME_INDEX_NumpadDecimal:
    case CODE_NAME_INDEX_NumpadDivide:
    case CODE_NAME_INDEX_NumpadEnter:
    case CODE_NAME_INDEX_NumpadEqual:
    case CODE_NAME_INDEX_NumpadMemoryAdd:
    case CODE_NAME_INDEX_NumpadMemoryClear:
    case CODE_NAME_INDEX_NumpadMemoryRecall:
    case CODE_NAME_INDEX_NumpadMemoryStore:
    case CODE_NAME_INDEX_NumpadMemorySubtract:
    case CODE_NAME_INDEX_NumpadMultiply:
    case CODE_NAME_INDEX_NumpadParenLeft:
    case CODE_NAME_INDEX_NumpadParenRight:
    case CODE_NAME_INDEX_NumpadSubtract:
      return eKeyLocationNumpad;
    default:
      return eKeyLocationStandard;
  }
}

/* static */ uint32_t
WidgetKeyboardEvent::ComputeKeyCodeFromKeyNameIndex(KeyNameIndex aKeyNameIndex)
{
  switch (aKeyNameIndex) {
    case KEY_NAME_INDEX_Cancel:
      return nsIDOMKeyEvent::DOM_VK_CANCEL;
    case KEY_NAME_INDEX_Help:
      return nsIDOMKeyEvent::DOM_VK_HELP;
    case KEY_NAME_INDEX_Backspace:
      return nsIDOMKeyEvent::DOM_VK_BACK_SPACE;
    case KEY_NAME_INDEX_Tab:
      return nsIDOMKeyEvent::DOM_VK_TAB;
    case KEY_NAME_INDEX_Clear:
      return nsIDOMKeyEvent::DOM_VK_CLEAR;
    case KEY_NAME_INDEX_Enter:
      return nsIDOMKeyEvent::DOM_VK_RETURN;
    case KEY_NAME_INDEX_Shift:
      return nsIDOMKeyEvent::DOM_VK_SHIFT;
    case KEY_NAME_INDEX_Control:
      return nsIDOMKeyEvent::DOM_VK_CONTROL;
    case KEY_NAME_INDEX_Alt:
      return nsIDOMKeyEvent::DOM_VK_ALT;
    case KEY_NAME_INDEX_Pause:
      return nsIDOMKeyEvent::DOM_VK_PAUSE;
    case KEY_NAME_INDEX_CapsLock:
      return nsIDOMKeyEvent::DOM_VK_CAPS_LOCK;
    case KEY_NAME_INDEX_Hiragana:
    case KEY_NAME_INDEX_Katakana:
    case KEY_NAME_INDEX_HiraganaKatakana:
    case KEY_NAME_INDEX_KanaMode:
      return nsIDOMKeyEvent::DOM_VK_KANA;
    case KEY_NAME_INDEX_HangulMode:
      return nsIDOMKeyEvent::DOM_VK_HANGUL;
    case KEY_NAME_INDEX_Eisu:
      return nsIDOMKeyEvent::DOM_VK_EISU;
    case KEY_NAME_INDEX_JunjaMode:
      return nsIDOMKeyEvent::DOM_VK_JUNJA;
    case KEY_NAME_INDEX_FinalMode:
      return nsIDOMKeyEvent::DOM_VK_FINAL;
    case KEY_NAME_INDEX_HanjaMode:
      return nsIDOMKeyEvent::DOM_VK_HANJA;
    case KEY_NAME_INDEX_KanjiMode:
      return nsIDOMKeyEvent::DOM_VK_KANJI;
    case KEY_NAME_INDEX_Escape:
      return nsIDOMKeyEvent::DOM_VK_ESCAPE;
    case KEY_NAME_INDEX_Convert:
      return nsIDOMKeyEvent::DOM_VK_CONVERT;
    case KEY_NAME_INDEX_NonConvert:
      return nsIDOMKeyEvent::DOM_VK_NONCONVERT;
    case KEY_NAME_INDEX_Accept:
      return nsIDOMKeyEvent::DOM_VK_ACCEPT;
    case KEY_NAME_INDEX_ModeChange:
      return nsIDOMKeyEvent::DOM_VK_MODECHANGE;
    case KEY_NAME_INDEX_PageUp:
      return nsIDOMKeyEvent::DOM_VK_PAGE_UP;
    case KEY_NAME_INDEX_PageDown:
      return nsIDOMKeyEvent::DOM_VK_PAGE_DOWN;
    case KEY_NAME_INDEX_End:
      return nsIDOMKeyEvent::DOM_VK_END;
    case KEY_NAME_INDEX_Home:
      return nsIDOMKeyEvent::DOM_VK_HOME;
    case KEY_NAME_INDEX_ArrowLeft:
      return nsIDOMKeyEvent::DOM_VK_LEFT;
    case KEY_NAME_INDEX_ArrowUp:
      return nsIDOMKeyEvent::DOM_VK_UP;
    case KEY_NAME_INDEX_ArrowRight:
      return nsIDOMKeyEvent::DOM_VK_RIGHT;
    case KEY_NAME_INDEX_ArrowDown:
      return nsIDOMKeyEvent::DOM_VK_DOWN;
    case KEY_NAME_INDEX_Select:
      return nsIDOMKeyEvent::DOM_VK_SELECT;
    case KEY_NAME_INDEX_Print:
      return nsIDOMKeyEvent::DOM_VK_PRINT;
    case KEY_NAME_INDEX_Execute:
      return nsIDOMKeyEvent::DOM_VK_EXECUTE;
    case KEY_NAME_INDEX_PrintScreen:
      return nsIDOMKeyEvent::DOM_VK_PRINTSCREEN;
    case KEY_NAME_INDEX_Insert:
      return nsIDOMKeyEvent::DOM_VK_INSERT;
    case KEY_NAME_INDEX_Delete:
      return nsIDOMKeyEvent::DOM_VK_DELETE;
    case KEY_NAME_INDEX_OS:
    // case KEY_NAME_INDEX_Super:
    // case KEY_NAME_INDEX_Hyper:
      return nsIDOMKeyEvent::DOM_VK_WIN;
    case KEY_NAME_INDEX_ContextMenu:
      return nsIDOMKeyEvent::DOM_VK_CONTEXT_MENU;
    case KEY_NAME_INDEX_Standby:
      return nsIDOMKeyEvent::DOM_VK_SLEEP;
    case KEY_NAME_INDEX_F1:
      return nsIDOMKeyEvent::DOM_VK_F1;
    case KEY_NAME_INDEX_F2:
      return nsIDOMKeyEvent::DOM_VK_F2;
    case KEY_NAME_INDEX_F3:
      return nsIDOMKeyEvent::DOM_VK_F3;
    case KEY_NAME_INDEX_F4:
      return nsIDOMKeyEvent::DOM_VK_F4;
    case KEY_NAME_INDEX_F5:
      return nsIDOMKeyEvent::DOM_VK_F5;
    case KEY_NAME_INDEX_F6:
      return nsIDOMKeyEvent::DOM_VK_F6;
    case KEY_NAME_INDEX_F7:
      return nsIDOMKeyEvent::DOM_VK_F7;
    case KEY_NAME_INDEX_F8:
      return nsIDOMKeyEvent::DOM_VK_F8;
    case KEY_NAME_INDEX_F9:
      return nsIDOMKeyEvent::DOM_VK_F9;
    case KEY_NAME_INDEX_F10:
      return nsIDOMKeyEvent::DOM_VK_F10;
    case KEY_NAME_INDEX_F11:
      return nsIDOMKeyEvent::DOM_VK_F11;
    case KEY_NAME_INDEX_F12:
      return nsIDOMKeyEvent::DOM_VK_F12;
    case KEY_NAME_INDEX_F13:
      return nsIDOMKeyEvent::DOM_VK_F13;
    case KEY_NAME_INDEX_F14:
      return nsIDOMKeyEvent::DOM_VK_F14;
    case KEY_NAME_INDEX_F15:
      return nsIDOMKeyEvent::DOM_VK_F15;
    case KEY_NAME_INDEX_F16:
      return nsIDOMKeyEvent::DOM_VK_F16;
    case KEY_NAME_INDEX_F17:
      return nsIDOMKeyEvent::DOM_VK_F17;
    case KEY_NAME_INDEX_F18:
      return nsIDOMKeyEvent::DOM_VK_F18;
    case KEY_NAME_INDEX_F19:
      return nsIDOMKeyEvent::DOM_VK_F19;
    case KEY_NAME_INDEX_F20:
      return nsIDOMKeyEvent::DOM_VK_F20;
    case KEY_NAME_INDEX_F21:
      return nsIDOMKeyEvent::DOM_VK_F21;
    case KEY_NAME_INDEX_F22:
      return nsIDOMKeyEvent::DOM_VK_F22;
    case KEY_NAME_INDEX_F23:
      return nsIDOMKeyEvent::DOM_VK_F23;
    case KEY_NAME_INDEX_F24:
      return nsIDOMKeyEvent::DOM_VK_F24;
    case KEY_NAME_INDEX_NumLock:
      return nsIDOMKeyEvent::DOM_VK_NUM_LOCK;
    case KEY_NAME_INDEX_ScrollLock:
      return nsIDOMKeyEvent::DOM_VK_SCROLL_LOCK;
    case KEY_NAME_INDEX_AudioVolumeMute:
      return nsIDOMKeyEvent::DOM_VK_VOLUME_MUTE;
    case KEY_NAME_INDEX_AudioVolumeDown:
      return nsIDOMKeyEvent::DOM_VK_VOLUME_DOWN;
    case KEY_NAME_INDEX_AudioVolumeUp:
      return nsIDOMKeyEvent::DOM_VK_VOLUME_UP;
    case KEY_NAME_INDEX_Meta:
      return nsIDOMKeyEvent::DOM_VK_META;
    case KEY_NAME_INDEX_AltGraph:
      return nsIDOMKeyEvent::DOM_VK_ALTGR;
    case KEY_NAME_INDEX_Attn:
      return nsIDOMKeyEvent::DOM_VK_ATTN;
    case KEY_NAME_INDEX_CrSel:
      return nsIDOMKeyEvent::DOM_VK_CRSEL;
    case KEY_NAME_INDEX_ExSel:
      return nsIDOMKeyEvent::DOM_VK_EXSEL;
    case KEY_NAME_INDEX_EraseEof:
      return nsIDOMKeyEvent::DOM_VK_EREOF;
    case KEY_NAME_INDEX_Play:
      return nsIDOMKeyEvent::DOM_VK_PLAY;
    case KEY_NAME_INDEX_ZoomToggle:
    case KEY_NAME_INDEX_ZoomIn:
    case KEY_NAME_INDEX_ZoomOut:
      return nsIDOMKeyEvent::DOM_VK_ZOOM;
    default:
      return 0;
  }
}

/* static */ Modifier
WidgetKeyboardEvent::GetModifierForKeyName(KeyNameIndex aKeyNameIndex)
{
  switch (aKeyNameIndex) {
    case KEY_NAME_INDEX_Alt:
      return MODIFIER_ALT;
    case KEY_NAME_INDEX_AltGraph:
      return MODIFIER_ALTGRAPH;
    case KEY_NAME_INDEX_CapsLock:
      return MODIFIER_CAPSLOCK;
    case KEY_NAME_INDEX_Control:
      return MODIFIER_CONTROL;
    case KEY_NAME_INDEX_Fn:
      return MODIFIER_FN;
    case KEY_NAME_INDEX_FnLock:
      return MODIFIER_FNLOCK;
    // case KEY_NAME_INDEX_Hyper:
    case KEY_NAME_INDEX_Meta:
      return MODIFIER_META;
    case KEY_NAME_INDEX_NumLock:
      return MODIFIER_NUMLOCK;
    case KEY_NAME_INDEX_OS:
      return MODIFIER_OS;
    case KEY_NAME_INDEX_ScrollLock:
      return MODIFIER_SCROLLLOCK;
    case KEY_NAME_INDEX_Shift:
      return MODIFIER_SHIFT;
    // case KEY_NAME_INDEX_Super:
    case KEY_NAME_INDEX_Symbol:
      return MODIFIER_SYMBOL;
    case KEY_NAME_INDEX_SymbolLock:
      return MODIFIER_SYMBOLLOCK;
    default:
      return MODIFIER_NONE;
  }
}

/* static */ bool
WidgetKeyboardEvent::IsLockableModifier(KeyNameIndex aKeyNameIndex)
{
  switch (aKeyNameIndex) {
    case KEY_NAME_INDEX_CapsLock:
    case KEY_NAME_INDEX_FnLock:
    case KEY_NAME_INDEX_NumLock:
    case KEY_NAME_INDEX_ScrollLock:
    case KEY_NAME_INDEX_SymbolLock:
      return true;
    default:
      return false;
  }
}

} // namespace mozilla
