/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasicEvents.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/EventStateManager.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/Maybe.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPrefs_mousewheel.h"
#include "mozilla/StaticPrefs_ui.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/WritingModes.h"
#include "mozilla/dom/KeyboardEventBinding.h"
#include "nsCommandParams.h"
#include "nsContentUtils.h"
#include "nsIContent.h"
#include "nsIDragSession.h"
#include "nsPrintfCString.h"

#if defined(XP_WIN)
#  include "npapi.h"
#  include "WinUtils.h"
#endif  // #if defined (XP_WIN)

#if defined(MOZ_WIDGET_GTK) || defined(XP_MACOSX)
#  include "NativeKeyBindings.h"
#endif  // #if defined(MOZ_WIDGET_GTK) || defined(XP_MACOSX)

namespace mozilla {

/******************************************************************************
 * Global helper methods
 ******************************************************************************/

const char* ToChar(EventMessage aEventMessage) {
  switch (aEventMessage) {
#define NS_EVENT_MESSAGE(aMessage) \
  case aMessage:                   \
    return #aMessage;

#include "mozilla/EventMessageList.h"

#undef NS_EVENT_MESSAGE
    default:
      return "illegal event message";
  }
}

const char* ToChar(EventClassID aEventClassID) {
  switch (aEventClassID) {
#define NS_ROOT_EVENT_CLASS(aPrefix, aName) \
  case eBasic##aName##Class:                \
    return "eBasic" #aName "Class";

#define NS_EVENT_CLASS(aPrefix, aName) \
  case e##aName##Class:                \
    return "e" #aName "Class";

#include "mozilla/EventClassList.h"

#undef NS_EVENT_CLASS
#undef NS_ROOT_EVENT_CLASS
    default:
      return "illegal event class ID";
  }
}

const nsCString ToString(KeyNameIndex aKeyNameIndex) {
  if (aKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
    return "USE_STRING"_ns;
  }
  nsAutoString keyName;
  WidgetKeyboardEvent::GetDOMKeyName(aKeyNameIndex, keyName);
  return NS_ConvertUTF16toUTF8(keyName);
}

const nsCString ToString(CodeNameIndex aCodeNameIndex) {
  if (aCodeNameIndex == CODE_NAME_INDEX_USE_STRING) {
    return "USE_STRING"_ns;
  }
  nsAutoString codeName;
  WidgetKeyboardEvent::GetDOMCodeName(aCodeNameIndex, codeName);
  return NS_ConvertUTF16toUTF8(codeName);
}

const char* ToChar(Command aCommand) {
  if (aCommand == Command::DoNothing) {
    return "CommandDoNothing";
  }

  switch (aCommand) {
#define NS_DEFINE_COMMAND(aName, aCommandStr) \
  case Command::aName:                        \
    return "Command::" #aName;
#define NS_DEFINE_COMMAND_WITH_PARAM(aName, aCommandStr, aParam) \
  case Command::aName:                                           \
    return "Command::" #aName;
#define NS_DEFINE_COMMAND_NO_EXEC_COMMAND(aName) \
  case Command::aName:                           \
    return "Command::" #aName;

#include "mozilla/CommandList.h"

#undef NS_DEFINE_COMMAND
#undef NS_DEFINE_COMMAND_WITH_PARAM
#undef NS_DEFINE_COMMAND_NO_EXEC_COMMAND

    default:
      return "illegal command value";
  }
}

const nsCString GetDOMKeyCodeName(uint32_t aKeyCode) {
  switch (aKeyCode) {
#define NS_DISALLOW_SAME_KEYCODE
#define NS_DEFINE_VK(aDOMKeyName, aDOMKeyCode) \
  case aDOMKeyCode:                            \
    return nsLiteralCString(#aDOMKeyName);

#include "mozilla/VirtualKeyCodeList.h"

#undef NS_DEFINE_VK
#undef NS_DISALLOW_SAME_KEYCODE

    default:
      return nsPrintfCString("Invalid DOM keyCode (0x%08X)", aKeyCode);
  }
}

bool IsValidRawTextRangeValue(RawTextRangeType aRawTextRangeType) {
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

RawTextRangeType ToRawTextRangeType(TextRangeType aTextRangeType) {
  return static_cast<RawTextRangeType>(aTextRangeType);
}

TextRangeType ToTextRangeType(RawTextRangeType aRawTextRangeType) {
  MOZ_ASSERT(IsValidRawTextRangeValue(aRawTextRangeType));
  return static_cast<TextRangeType>(aRawTextRangeType);
}

const char* ToChar(TextRangeType aTextRangeType) {
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

SelectionType ToSelectionType(TextRangeType aTextRangeType) {
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
 * non class method implementation
 ******************************************************************************/

static nsTHashMap<nsDepCharHashKey, Command>* sCommandHashtable = nullptr;

Command GetInternalCommand(const char* aCommandName,
                           const nsCommandParams* aCommandParams) {
  if (!aCommandName) {
    return Command::DoNothing;
  }

  // Special cases for "cmd_align".  It's mapped to multiple internal commands
  // with additional param.  Therefore, we cannot handle it with the hashtable.
  if (!strcmp(aCommandName, "cmd_align")) {
    if (!aCommandParams) {
      // Note that if this is called by EditorCommand::IsCommandEnabled(),
      // it cannot set aCommandParams.  So, don't warn in this case even though
      // this is illegal case for DoCommandParams().
      return Command::FormatJustify;
    }
    nsAutoCString cValue;
    nsresult rv = aCommandParams->GetCString("state_attribute", cValue);
    if (NS_FAILED(rv)) {
      nsString value;  // Avoid copying the string buffer with using nsString.
      rv = aCommandParams->GetString("state_attribute", value);
      if (NS_FAILED(rv)) {
        return Command::FormatJustifyNone;
      }
      CopyUTF16toUTF8(value, cValue);
    }
    if (cValue.LowerCaseEqualsASCII("left")) {
      return Command::FormatJustifyLeft;
    }
    if (cValue.LowerCaseEqualsASCII("right")) {
      return Command::FormatJustifyRight;
    }
    if (cValue.LowerCaseEqualsASCII("center")) {
      return Command::FormatJustifyCenter;
    }
    if (cValue.LowerCaseEqualsASCII("justify")) {
      return Command::FormatJustifyFull;
    }
    if (cValue.IsEmpty()) {
      return Command::FormatJustifyNone;
    }
    return Command::DoNothing;
  }

  if (!sCommandHashtable) {
    sCommandHashtable = new nsTHashMap<nsDepCharHashKey, Command>();
#define NS_DEFINE_COMMAND(aName, aCommandStr) \
  sCommandHashtable->InsertOrUpdate(#aCommandStr, Command::aName);

#define NS_DEFINE_COMMAND_WITH_PARAM(aName, aCommandStr, aParam)

#define NS_DEFINE_COMMAND_NO_EXEC_COMMAND(aName)

#include "mozilla/CommandList.h"

#undef NS_DEFINE_COMMAND
#undef NS_DEFINE_COMMAND_WITH_PARAM
#undef NS_DEFINE_COMMAND_NO_EXEC_COMMAND
  }
  Command command = Command::DoNothing;
  if (!sCommandHashtable->Get(aCommandName, &command)) {
    return Command::DoNothing;
  }
  return command;
}

/******************************************************************************
 * As*Event() implementation
 ******************************************************************************/

#define NS_ROOT_EVENT_CLASS(aPrefix, aName)
#define NS_EVENT_CLASS(aPrefix, aName)                         \
  aPrefix##aName* WidgetEvent::As##aName() { return nullptr; } \
                                                               \
  const aPrefix##aName* WidgetEvent::As##aName() const {       \
    return const_cast<WidgetEvent*>(this)->As##aName();        \
  }

#include "mozilla/EventClassList.h"

#undef NS_EVENT_CLASS
#undef NS_ROOT_EVENT_CLASS

/******************************************************************************
 * mozilla::WidgetEvent
 *
 * Event struct type checking methods.
 ******************************************************************************/

bool WidgetEvent::IsQueryContentEvent() const {
  return mClass == eQueryContentEventClass;
}

bool WidgetEvent::IsSelectionEvent() const {
  return mClass == eSelectionEventClass;
}

bool WidgetEvent::IsContentCommandEvent() const {
  return mClass == eContentCommandEventClass;
}

/******************************************************************************
 * mozilla::WidgetEvent
 *
 * Event message checking methods.
 ******************************************************************************/

bool WidgetEvent::HasMouseEventMessage() const {
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

bool WidgetEvent::HasDragEventMessage() const {
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
bool WidgetEvent::IsKeyEventMessage(EventMessage aMessage) {
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

bool WidgetEvent::HasIMEEventMessage() const {
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

bool WidgetEvent::HasPluginActivationEventMessage() const {
  return mMessage == ePluginActivate || mMessage == ePluginFocus;
}

/******************************************************************************
 * mozilla::WidgetEvent
 *
 * Specific event checking methods.
 ******************************************************************************/

bool WidgetEvent::CanBeSentToRemoteProcess() const {
  // If this event is explicitly marked as shouldn't be sent to remote process,
  // just return false.
  if (IsCrossProcessForwardingStopped()) {
    return false;
  }

  if (mClass == eKeyboardEventClass || mClass == eWheelEventClass) {
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

bool WidgetEvent::WillBeSentToRemoteProcess() const {
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
  return EventStateManager::IsTopLevelRemoteTarget(originalTarget);
}

bool WidgetEvent::IsIMERelatedEvent() const {
  return HasIMEEventMessage() || IsQueryContentEvent() || IsSelectionEvent();
}

bool WidgetEvent::IsUsingCoordinates() const {
  const WidgetMouseEvent* mouseEvent = AsMouseEvent();
  if (mouseEvent) {
    return !mouseEvent->IsContextMenuKeyEvent();
  }
  return !HasKeyEventMessage() && !IsIMERelatedEvent() &&
         !HasPluginActivationEventMessage() && !IsContentCommandEvent();
}

bool WidgetEvent::IsTargetedAtFocusedWindow() const {
  const WidgetMouseEvent* mouseEvent = AsMouseEvent();
  if (mouseEvent) {
    return mouseEvent->IsContextMenuKeyEvent();
  }
  return HasKeyEventMessage() || IsIMERelatedEvent() || IsContentCommandEvent();
}

bool WidgetEvent::IsTargetedAtFocusedContent() const {
  const WidgetMouseEvent* mouseEvent = AsMouseEvent();
  if (mouseEvent) {
    return mouseEvent->IsContextMenuKeyEvent();
  }
  return HasKeyEventMessage() || IsIMERelatedEvent();
}

bool WidgetEvent::IsAllowedToDispatchDOMEvent() const {
  switch (mClass) {
    case eMouseEventClass:
      if (mMessage == eMouseTouchDrag) {
        return false;
      }
      [[fallthrough]];
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

bool WidgetEvent::IsAllowedToDispatchInSystemGroup() const {
  // We don't expect to implement default behaviors with pointer events because
  // if we do, prevent default on mouse events can't prevent default behaviors
  // anymore.
  return mClass != ePointerEventClass;
}

bool WidgetEvent::IsBlockedForFingerprintingResistance() const {
  if (!nsContentUtils::ShouldResistFingerprinting()) {
    return false;
  }

  switch (mClass) {
    case eKeyboardEventClass: {
      const WidgetKeyboardEvent* keyboardEvent = AsKeyboardEvent();

      return (keyboardEvent->mKeyNameIndex == KEY_NAME_INDEX_Alt ||
              keyboardEvent->mKeyNameIndex == KEY_NAME_INDEX_Shift ||
              keyboardEvent->mKeyNameIndex == KEY_NAME_INDEX_Control ||
              keyboardEvent->mKeyNameIndex == KEY_NAME_INDEX_AltGraph);
    }
    case ePointerEventClass: {
      const WidgetPointerEvent* pointerEvent = AsPointerEvent();

      // We suppress the pointer events if it is not primary for fingerprinting
      // resistance. It is because of that we want to spoof any pointer event
      // into a mouse pointer event and the mouse pointer event only has
      // isPrimary as true.
      return !pointerEvent->mIsPrimary;
    }
    default:
      return false;
  }
}

/******************************************************************************
 * mozilla::WidgetEvent
 *
 * Misc methods.
 ******************************************************************************/

static dom::EventTarget* GetTargetForDOMEvent(dom::EventTarget* aTarget) {
  return aTarget ? aTarget->GetTargetForDOMEvent() : nullptr;
}

dom::EventTarget* WidgetEvent::GetDOMEventTarget() const {
  return GetTargetForDOMEvent(mTarget);
}

dom::EventTarget* WidgetEvent::GetCurrentDOMEventTarget() const {
  return GetTargetForDOMEvent(mCurrentTarget);
}

dom::EventTarget* WidgetEvent::GetOriginalDOMEventTarget() const {
  if (mOriginalTarget) {
    return GetTargetForDOMEvent(mOriginalTarget);
  }
  return GetDOMEventTarget();
}

void WidgetEvent::PreventDefault(bool aCalledByDefaultHandler,
                                 nsIPrincipal* aPrincipal) {
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

bool WidgetEvent::IsUserAction() const {
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
Modifier WidgetInputEvent::GetModifier(const nsAString& aDOMKeyName) {
  if (aDOMKeyName.EqualsLiteral("Accel")) {
    return AccelModifier();
  }
  KeyNameIndex keyNameIndex = WidgetKeyboardEvent::GetKeyNameIndex(aDOMKeyName);
  return WidgetKeyboardEvent::GetModifierForKeyName(keyNameIndex);
}

/* static */
Modifier WidgetInputEvent::AccelModifier() {
  static Modifier sAccelModifier = MODIFIER_NONE;
  if (sAccelModifier == MODIFIER_NONE) {
    switch (Preferences::GetInt("ui.key.accelKey", 0)) {
      case dom::KeyboardEvent_Binding::DOM_VK_META:
        sAccelModifier = MODIFIER_META;
        break;
      case dom::KeyboardEvent_Binding::DOM_VK_WIN:
        sAccelModifier = MODIFIER_OS;
        break;
      case dom::KeyboardEvent_Binding::DOM_VK_ALT:
        sAccelModifier = MODIFIER_ALT;
        break;
      case dom::KeyboardEvent_Binding::DOM_VK_CONTROL:
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
 * mozilla::WidgetMouseEvent (MouseEvents.h)
 ******************************************************************************/

/* static */
bool WidgetMouseEvent::IsMiddleClickPasteEnabled() {
  return Preferences::GetBool("middlemouse.paste", false);
}

/******************************************************************************
 * mozilla::WidgetDragEvent (MouseEvents.h)
 ******************************************************************************/

void WidgetDragEvent::InitDropEffectForTests() {
  MOZ_ASSERT(mFlags.mIsSynthesizedForTests);

  nsCOMPtr<nsIDragSession> session = nsContentUtils::GetDragSession();
  if (NS_WARN_IF(!session)) {
    return;
  }

  uint32_t effectAllowed = session->GetEffectAllowedForTests();
  uint32_t desiredDropEffect = nsIDragService::DRAGDROP_ACTION_NONE;
#ifdef XP_MACOSX
  if (IsAlt()) {
    desiredDropEffect = IsMeta() ? nsIDragService::DRAGDROP_ACTION_LINK
                                 : nsIDragService::DRAGDROP_ACTION_COPY;
  }
#else
  // On Linux, we know user's intention from API, but we should use
  // same modifiers as Windows for tests because GNOME on Ubuntu use
  // them and that makes each test simpler.
  if (IsControl()) {
    desiredDropEffect = IsShift() ? nsIDragService::DRAGDROP_ACTION_LINK
                                  : nsIDragService::DRAGDROP_ACTION_COPY;
  } else if (IsShift()) {
    desiredDropEffect = nsIDragService::DRAGDROP_ACTION_MOVE;
  }
#endif  // #ifdef XP_MACOSX #else
  // First, use modifier state for preferring action which is explicitly
  // specified by the synthesizer.
  if (!(desiredDropEffect &= effectAllowed)) {
    // Otherwise, use an action which is allowed at starting the session.
    desiredDropEffect = effectAllowed;
  }
  if (desiredDropEffect & nsIDragService::DRAGDROP_ACTION_MOVE) {
    session->SetDragAction(nsIDragService::DRAGDROP_ACTION_MOVE);
  } else if (desiredDropEffect & nsIDragService::DRAGDROP_ACTION_COPY) {
    session->SetDragAction(nsIDragService::DRAGDROP_ACTION_COPY);
  } else if (desiredDropEffect & nsIDragService::DRAGDROP_ACTION_LINK) {
    session->SetDragAction(nsIDragService::DRAGDROP_ACTION_LINK);
  } else {
    session->SetDragAction(nsIDragService::DRAGDROP_ACTION_NONE);
  }
}

/******************************************************************************
 * mozilla::WidgetWheelEvent (MouseEvents.h)
 ******************************************************************************/

/* static */
double WidgetWheelEvent::ComputeOverriddenDelta(double aDelta,
                                                bool aIsForVertical) {
  if (!StaticPrefs::
          mousewheel_system_scroll_override_on_root_content_enabled()) {
    return aDelta;
  }
  int32_t intFactor =
      aIsForVertical
          ? StaticPrefs::
                mousewheel_system_scroll_override_on_root_content_vertical_factor()
          : StaticPrefs::
                mousewheel_system_scroll_override_on_root_content_horizontal_factor();
  // Making the scroll speed slower doesn't make sense. So, ignore odd factor
  // which is less than 1.0.
  if (intFactor <= 100) {
    return aDelta;
  }
  double factor = static_cast<double>(intFactor) / 100;
  return aDelta * factor;
}

double WidgetWheelEvent::OverriddenDeltaX() const {
  if (!mAllowToOverrideSystemScrollSpeed) {
    return mDeltaX;
  }
  return ComputeOverriddenDelta(mDeltaX, false);
}

double WidgetWheelEvent::OverriddenDeltaY() const {
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

void WidgetKeyboardEvent::InitAllEditCommands(
    const Maybe<WritingMode>& aWritingMode) {
  // If this event is synthesized for tests, we don't need to retrieve the
  // command via the main process.  So, we don't need widget and can trust
  // the event.
  if (!mFlags.mIsSynthesizedForTests) {
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

    MOZ_ASSERT(
        XRE_IsParentProcess(),
        "It's too expensive to retrieve all edit commands from remote process");
    MOZ_ASSERT(!AreAllEditCommandsInitialized(),
               "Shouldn't be called two or more times");
  }

  DebugOnly<bool> okIgnored = InitEditCommandsFor(
      nsIWidget::NativeKeyBindingsForSingleLineEditor, aWritingMode);
  NS_WARNING_ASSERTION(
      okIgnored,
      "InitEditCommandsFor(nsIWidget::NativeKeyBindingsForSingleLineEditor) "
      "failed, but ignored");
  okIgnored = InitEditCommandsFor(
      nsIWidget::NativeKeyBindingsForMultiLineEditor, aWritingMode);
  NS_WARNING_ASSERTION(
      okIgnored,
      "InitEditCommandsFor(nsIWidget::NativeKeyBindingsForMultiLineEditor) "
      "failed, but ignored");
  okIgnored = InitEditCommandsFor(nsIWidget::NativeKeyBindingsForRichTextEditor,
                                  aWritingMode);
  NS_WARNING_ASSERTION(
      okIgnored,
      "InitEditCommandsFor(nsIWidget::NativeKeyBindingsForRichTextEditor) "
      "failed, but ignored");
}

bool WidgetKeyboardEvent::InitEditCommandsFor(
    nsIWidget::NativeKeyBindingsType aType,
    const Maybe<WritingMode>& aWritingMode) {
  bool& initialized = IsEditCommandsInitializedRef(aType);
  if (initialized) {
    return true;
  }
  nsTArray<CommandInt>& commands = EditCommandsRef(aType);

  // If this event is synthesized for tests, we shouldn't access customized
  // shortcut settings of the environment.  Therefore, we don't need to check
  // whether `widget` is set or not.  And we can treat synthesized events are
  // always trusted.
  if (mFlags.mIsSynthesizedForTests) {
    MOZ_DIAGNOSTIC_ASSERT(IsTrusted());
#if defined(MOZ_WIDGET_GTK) || defined(XP_MACOSX)
    // TODO: We should implement `NativeKeyBindings` for Windows and Android
    //       too in bug 1301497 for getting rid of the #if.
    widget::NativeKeyBindings::GetEditCommandsForTests(aType, *this,
                                                       aWritingMode, commands);
#endif
    initialized = true;
    return true;
  }

  if (NS_WARN_IF(!mWidget) || NS_WARN_IF(!IsTrusted())) {
    return false;
  }
  // `nsIWidget::GetEditCommands()` will retrieve `WritingMode` at selection
  // again, but it should be almost zero-cost since `TextEventDispatcher`
  // caches the value.
  nsCOMPtr<nsIWidget> widget = mWidget;
  initialized = widget->GetEditCommands(aType, *this, commands);
  return initialized;
}

bool WidgetKeyboardEvent::ExecuteEditCommands(
    nsIWidget::NativeKeyBindingsType aType, DoCommandCallback aCallback,
    void* aCallbackData) {
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

  if (!IsEditCommandsInitializedRef(aType)) {
    Maybe<WritingMode> writingMode;
    if (RefPtr<widget::TextEventDispatcher> textEventDispatcher =
            mWidget->GetTextEventDispatcher()) {
      writingMode = textEventDispatcher->MaybeWritingModeAtSelection();
    }
    if (NS_WARN_IF(!InitEditCommandsFor(aType, writingMode))) {
      return false;
    }
  }

  const nsTArray<CommandInt>& commands = EditCommandsRef(aType);
  if (commands.IsEmpty()) {
    return false;
  }

  for (CommandInt command : commands) {
    aCallback(static_cast<Command>(command), aCallbackData);
  }
  return true;
}

bool WidgetKeyboardEvent::ShouldCauseKeypressEvents() const {
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

static bool HasASCIIDigit(const ShortcutKeyCandidateArray& aCandidates) {
  for (uint32_t i = 0; i < aCandidates.Length(); ++i) {
    uint32_t ch = aCandidates[i].mCharCode;
    if (ch >= '0' && ch <= '9') return true;
  }
  return false;
}

static bool CharsCaseInsensitiveEqual(uint32_t aChar1, uint32_t aChar2) {
  return aChar1 == aChar2 || (IS_IN_BMP(aChar1) && IS_IN_BMP(aChar2) &&
                              ToLowerCase(static_cast<char16_t>(aChar1)) ==
                                  ToLowerCase(static_cast<char16_t>(aChar2)));
}

static bool IsCaseChangeableChar(uint32_t aChar) {
  return IS_IN_BMP(aChar) && ToLowerCase(static_cast<char16_t>(aChar)) !=
                                 ToUpperCase(static_cast<char16_t>(aChar));
}

void WidgetKeyboardEvent::GetShortcutKeyCandidates(
    ShortcutKeyCandidateArray& aCandidates) const {
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

void WidgetKeyboardEvent::GetAccessKeyCandidates(
    nsTArray<uint32_t>& aCandidates) const {
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
    uint32_t ch[2] = {mAlternativeCharCodes[i].mUnshiftedCharCode,
                      mAlternativeCharCodes[i].mShiftedCharCode};
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
#define NS_MODIFIER_SHIFT 1
#define NS_MODIFIER_CONTROL 2
#define NS_MODIFIER_ALT 4
#define NS_MODIFIER_META 8
#define NS_MODIFIER_OS 16

static Modifiers PrefFlagsToModifiers(int32_t aPrefFlags) {
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

bool WidgetKeyboardEvent::ModifiersMatchWithAccessKey(
    AccessKeyType aType) const {
  if (!ModifiersForAccessKeyMatching()) {
    return false;
  }
  return ModifiersForAccessKeyMatching() == AccessKeyModifiers(aType);
}

Modifiers WidgetKeyboardEvent::ModifiersForAccessKeyMatching() const {
  static const Modifiers kModifierMask = MODIFIER_SHIFT | MODIFIER_CONTROL |
                                         MODIFIER_ALT | MODIFIER_META |
                                         MODIFIER_OS;
  return mModifiers & kModifierMask;
}

/* static */
Modifiers WidgetKeyboardEvent::AccessKeyModifiers(AccessKeyType aType) {
  switch (StaticPrefs::ui_key_generalAccessKey()) {
    case -1:
      break;  // use the individual prefs
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
      return PrefFlagsToModifiers(StaticPrefs::ui_key_chromeAccess());
    case AccessKeyType::eContent:
      return PrefFlagsToModifiers(StaticPrefs::ui_key_contentAccess());
    default:
      return MODIFIER_NONE;
  }
}

/* static */
void WidgetKeyboardEvent::Shutdown() {
  delete sKeyNameIndexHashtable;
  sKeyNameIndexHashtable = nullptr;
  delete sCodeNameIndexHashtable;
  sCodeNameIndexHashtable = nullptr;
  // Although sCommandHashtable is not a member of WidgetKeyboardEvent, but
  // let's delete it here since we need to do it at same time.
  delete sCommandHashtable;
  sCommandHashtable = nullptr;
}

/* static */
void WidgetKeyboardEvent::GetDOMKeyName(KeyNameIndex aKeyNameIndex,
                                        nsAString& aKeyName) {
  if (aKeyNameIndex >= KEY_NAME_INDEX_USE_STRING) {
    aKeyName.Truncate();
    return;
  }

  MOZ_RELEASE_ASSERT(
      static_cast<size_t>(aKeyNameIndex) < ArrayLength(kKeyNames),
      "Illegal key enumeration value");
  aKeyName = kKeyNames[aKeyNameIndex];
}

/* static */
void WidgetKeyboardEvent::GetDOMCodeName(CodeNameIndex aCodeNameIndex,
                                         nsAString& aCodeName) {
  if (aCodeNameIndex >= CODE_NAME_INDEX_USE_STRING) {
    aCodeName.Truncate();
    return;
  }

  MOZ_RELEASE_ASSERT(
      static_cast<size_t>(aCodeNameIndex) < ArrayLength(kCodeNames),
      "Illegal physical code enumeration value");
  aCodeName = kCodeNames[aCodeNameIndex];
}

/* static */
KeyNameIndex WidgetKeyboardEvent::GetKeyNameIndex(const nsAString& aKeyValue) {
  if (!sKeyNameIndexHashtable) {
    sKeyNameIndexHashtable = new KeyNameIndexHashtable(ArrayLength(kKeyNames));
    for (size_t i = 0; i < ArrayLength(kKeyNames); i++) {
      sKeyNameIndexHashtable->InsertOrUpdate(nsDependentString(kKeyNames[i]),
                                             static_cast<KeyNameIndex>(i));
    }
  }
  return sKeyNameIndexHashtable->MaybeGet(aKeyValue).valueOr(
      KEY_NAME_INDEX_USE_STRING);
}

/* static */
CodeNameIndex WidgetKeyboardEvent::GetCodeNameIndex(
    const nsAString& aCodeValue) {
  if (!sCodeNameIndexHashtable) {
    sCodeNameIndexHashtable =
        new CodeNameIndexHashtable(ArrayLength(kCodeNames));
    for (size_t i = 0; i < ArrayLength(kCodeNames); i++) {
      sCodeNameIndexHashtable->InsertOrUpdate(nsDependentString(kCodeNames[i]),
                                              static_cast<CodeNameIndex>(i));
    }
  }
  return sCodeNameIndexHashtable->MaybeGet(aCodeValue)
      .valueOr(CODE_NAME_INDEX_USE_STRING);
}

/* static */
uint32_t WidgetKeyboardEvent::GetFallbackKeyCodeOfPunctuationKey(
    CodeNameIndex aCodeNameIndex) {
  switch (aCodeNameIndex) {
    case CODE_NAME_INDEX_Semicolon:  // VK_OEM_1 on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_SEMICOLON;
    case CODE_NAME_INDEX_Equal:  // VK_OEM_PLUS on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_EQUALS;
    case CODE_NAME_INDEX_Comma:  // VK_OEM_COMMA on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_COMMA;
    case CODE_NAME_INDEX_Minus:  // VK_OEM_MINUS on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_HYPHEN_MINUS;
    case CODE_NAME_INDEX_Period:  // VK_OEM_PERIOD on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_PERIOD;
    case CODE_NAME_INDEX_Slash:  // VK_OEM_2 on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_SLASH;
    case CODE_NAME_INDEX_Backquote:  // VK_OEM_3 on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_BACK_QUOTE;
    case CODE_NAME_INDEX_BracketLeft:  // VK_OEM_4 on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_OPEN_BRACKET;
    case CODE_NAME_INDEX_Backslash:  // VK_OEM_5 on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_BACK_SLASH;
    case CODE_NAME_INDEX_BracketRight:  // VK_OEM_6 on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_CLOSE_BRACKET;
    case CODE_NAME_INDEX_Quote:  // VK_OEM_7 on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_QUOTE;
    case CODE_NAME_INDEX_IntlBackslash:  // VK_OEM_5 on Windows (ABNT, etc)
    case CODE_NAME_INDEX_IntlYen:        // VK_OEM_5 on Windows (JIS)
    case CODE_NAME_INDEX_IntlRo:         // VK_OEM_102 on Windows
      return dom::KeyboardEvent_Binding::DOM_VK_BACK_SLASH;
    default:
      return 0;
  }
}

/* static */ const char* WidgetKeyboardEvent::GetCommandStr(Command aCommand) {
#define NS_DEFINE_COMMAND(aName, aCommandStr) , #aCommandStr
#define NS_DEFINE_COMMAND_WITH_PARAM(aName, aCommandStr, aParam) , #aCommandStr
#define NS_DEFINE_COMMAND_NO_EXEC_COMMAND(aName) , ""
  static const char* const kCommands[] = {
      ""  // DoNothing
#include "mozilla/CommandList.h"
  };
#undef NS_DEFINE_COMMAND
#undef NS_DEFINE_COMMAND_WITH_PARAM
#undef NS_DEFINE_COMMAND_NO_EXEC_COMMAND

  MOZ_RELEASE_ASSERT(static_cast<size_t>(aCommand) < ArrayLength(kCommands),
                     "Illegal command enumeration value");
  return kCommands[static_cast<CommandInt>(aCommand)];
}

/* static */
uint32_t WidgetKeyboardEvent::ComputeLocationFromCodeValue(
    CodeNameIndex aCodeNameIndex) {
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

/* static */
uint32_t WidgetKeyboardEvent::ComputeKeyCodeFromKeyNameIndex(
    KeyNameIndex aKeyNameIndex) {
  switch (aKeyNameIndex) {
    case KEY_NAME_INDEX_Cancel:
      return dom::KeyboardEvent_Binding::DOM_VK_CANCEL;
    case KEY_NAME_INDEX_Help:
      return dom::KeyboardEvent_Binding::DOM_VK_HELP;
    case KEY_NAME_INDEX_Backspace:
      return dom::KeyboardEvent_Binding::DOM_VK_BACK_SPACE;
    case KEY_NAME_INDEX_Tab:
      return dom::KeyboardEvent_Binding::DOM_VK_TAB;
    case KEY_NAME_INDEX_Clear:
      return dom::KeyboardEvent_Binding::DOM_VK_CLEAR;
    case KEY_NAME_INDEX_Enter:
      return dom::KeyboardEvent_Binding::DOM_VK_RETURN;
    case KEY_NAME_INDEX_Shift:
      return dom::KeyboardEvent_Binding::DOM_VK_SHIFT;
    case KEY_NAME_INDEX_Control:
      return dom::KeyboardEvent_Binding::DOM_VK_CONTROL;
    case KEY_NAME_INDEX_Alt:
      return dom::KeyboardEvent_Binding::DOM_VK_ALT;
    case KEY_NAME_INDEX_Pause:
      return dom::KeyboardEvent_Binding::DOM_VK_PAUSE;
    case KEY_NAME_INDEX_CapsLock:
      return dom::KeyboardEvent_Binding::DOM_VK_CAPS_LOCK;
    case KEY_NAME_INDEX_Hiragana:
    case KEY_NAME_INDEX_Katakana:
    case KEY_NAME_INDEX_HiraganaKatakana:
    case KEY_NAME_INDEX_KanaMode:
      return dom::KeyboardEvent_Binding::DOM_VK_KANA;
    case KEY_NAME_INDEX_HangulMode:
      return dom::KeyboardEvent_Binding::DOM_VK_HANGUL;
    case KEY_NAME_INDEX_Eisu:
      return dom::KeyboardEvent_Binding::DOM_VK_EISU;
    case KEY_NAME_INDEX_JunjaMode:
      return dom::KeyboardEvent_Binding::DOM_VK_JUNJA;
    case KEY_NAME_INDEX_FinalMode:
      return dom::KeyboardEvent_Binding::DOM_VK_FINAL;
    case KEY_NAME_INDEX_HanjaMode:
      return dom::KeyboardEvent_Binding::DOM_VK_HANJA;
    case KEY_NAME_INDEX_KanjiMode:
      return dom::KeyboardEvent_Binding::DOM_VK_KANJI;
    case KEY_NAME_INDEX_Escape:
      return dom::KeyboardEvent_Binding::DOM_VK_ESCAPE;
    case KEY_NAME_INDEX_Convert:
      return dom::KeyboardEvent_Binding::DOM_VK_CONVERT;
    case KEY_NAME_INDEX_NonConvert:
      return dom::KeyboardEvent_Binding::DOM_VK_NONCONVERT;
    case KEY_NAME_INDEX_Accept:
      return dom::KeyboardEvent_Binding::DOM_VK_ACCEPT;
    case KEY_NAME_INDEX_ModeChange:
      return dom::KeyboardEvent_Binding::DOM_VK_MODECHANGE;
    case KEY_NAME_INDEX_PageUp:
      return dom::KeyboardEvent_Binding::DOM_VK_PAGE_UP;
    case KEY_NAME_INDEX_PageDown:
      return dom::KeyboardEvent_Binding::DOM_VK_PAGE_DOWN;
    case KEY_NAME_INDEX_End:
      return dom::KeyboardEvent_Binding::DOM_VK_END;
    case KEY_NAME_INDEX_Home:
      return dom::KeyboardEvent_Binding::DOM_VK_HOME;
    case KEY_NAME_INDEX_ArrowLeft:
      return dom::KeyboardEvent_Binding::DOM_VK_LEFT;
    case KEY_NAME_INDEX_ArrowUp:
      return dom::KeyboardEvent_Binding::DOM_VK_UP;
    case KEY_NAME_INDEX_ArrowRight:
      return dom::KeyboardEvent_Binding::DOM_VK_RIGHT;
    case KEY_NAME_INDEX_ArrowDown:
      return dom::KeyboardEvent_Binding::DOM_VK_DOWN;
    case KEY_NAME_INDEX_Select:
      return dom::KeyboardEvent_Binding::DOM_VK_SELECT;
    case KEY_NAME_INDEX_Print:
      return dom::KeyboardEvent_Binding::DOM_VK_PRINT;
    case KEY_NAME_INDEX_Execute:
      return dom::KeyboardEvent_Binding::DOM_VK_EXECUTE;
    case KEY_NAME_INDEX_PrintScreen:
      return dom::KeyboardEvent_Binding::DOM_VK_PRINTSCREEN;
    case KEY_NAME_INDEX_Insert:
      return dom::KeyboardEvent_Binding::DOM_VK_INSERT;
    case KEY_NAME_INDEX_Delete:
      return dom::KeyboardEvent_Binding::DOM_VK_DELETE;
    case KEY_NAME_INDEX_OS:
      // case KEY_NAME_INDEX_Super:
      // case KEY_NAME_INDEX_Hyper:
      return dom::KeyboardEvent_Binding::DOM_VK_WIN;
    case KEY_NAME_INDEX_ContextMenu:
      return dom::KeyboardEvent_Binding::DOM_VK_CONTEXT_MENU;
    case KEY_NAME_INDEX_Standby:
      return dom::KeyboardEvent_Binding::DOM_VK_SLEEP;
    case KEY_NAME_INDEX_F1:
      return dom::KeyboardEvent_Binding::DOM_VK_F1;
    case KEY_NAME_INDEX_F2:
      return dom::KeyboardEvent_Binding::DOM_VK_F2;
    case KEY_NAME_INDEX_F3:
      return dom::KeyboardEvent_Binding::DOM_VK_F3;
    case KEY_NAME_INDEX_F4:
      return dom::KeyboardEvent_Binding::DOM_VK_F4;
    case KEY_NAME_INDEX_F5:
      return dom::KeyboardEvent_Binding::DOM_VK_F5;
    case KEY_NAME_INDEX_F6:
      return dom::KeyboardEvent_Binding::DOM_VK_F6;
    case KEY_NAME_INDEX_F7:
      return dom::KeyboardEvent_Binding::DOM_VK_F7;
    case KEY_NAME_INDEX_F8:
      return dom::KeyboardEvent_Binding::DOM_VK_F8;
    case KEY_NAME_INDEX_F9:
      return dom::KeyboardEvent_Binding::DOM_VK_F9;
    case KEY_NAME_INDEX_F10:
      return dom::KeyboardEvent_Binding::DOM_VK_F10;
    case KEY_NAME_INDEX_F11:
      return dom::KeyboardEvent_Binding::DOM_VK_F11;
    case KEY_NAME_INDEX_F12:
      return dom::KeyboardEvent_Binding::DOM_VK_F12;
    case KEY_NAME_INDEX_F13:
      return dom::KeyboardEvent_Binding::DOM_VK_F13;
    case KEY_NAME_INDEX_F14:
      return dom::KeyboardEvent_Binding::DOM_VK_F14;
    case KEY_NAME_INDEX_F15:
      return dom::KeyboardEvent_Binding::DOM_VK_F15;
    case KEY_NAME_INDEX_F16:
      return dom::KeyboardEvent_Binding::DOM_VK_F16;
    case KEY_NAME_INDEX_F17:
      return dom::KeyboardEvent_Binding::DOM_VK_F17;
    case KEY_NAME_INDEX_F18:
      return dom::KeyboardEvent_Binding::DOM_VK_F18;
    case KEY_NAME_INDEX_F19:
      return dom::KeyboardEvent_Binding::DOM_VK_F19;
    case KEY_NAME_INDEX_F20:
      return dom::KeyboardEvent_Binding::DOM_VK_F20;
    case KEY_NAME_INDEX_F21:
      return dom::KeyboardEvent_Binding::DOM_VK_F21;
    case KEY_NAME_INDEX_F22:
      return dom::KeyboardEvent_Binding::DOM_VK_F22;
    case KEY_NAME_INDEX_F23:
      return dom::KeyboardEvent_Binding::DOM_VK_F23;
    case KEY_NAME_INDEX_F24:
      return dom::KeyboardEvent_Binding::DOM_VK_F24;
    case KEY_NAME_INDEX_NumLock:
      return dom::KeyboardEvent_Binding::DOM_VK_NUM_LOCK;
    case KEY_NAME_INDEX_ScrollLock:
      return dom::KeyboardEvent_Binding::DOM_VK_SCROLL_LOCK;
    case KEY_NAME_INDEX_AudioVolumeMute:
      return dom::KeyboardEvent_Binding::DOM_VK_VOLUME_MUTE;
    case KEY_NAME_INDEX_AudioVolumeDown:
      return dom::KeyboardEvent_Binding::DOM_VK_VOLUME_DOWN;
    case KEY_NAME_INDEX_AudioVolumeUp:
      return dom::KeyboardEvent_Binding::DOM_VK_VOLUME_UP;
    case KEY_NAME_INDEX_Meta:
      return dom::KeyboardEvent_Binding::DOM_VK_META;
    case KEY_NAME_INDEX_AltGraph:
      return dom::KeyboardEvent_Binding::DOM_VK_ALTGR;
    case KEY_NAME_INDEX_Process:
      return dom::KeyboardEvent_Binding::DOM_VK_PROCESSKEY;
    case KEY_NAME_INDEX_Attn:
      return dom::KeyboardEvent_Binding::DOM_VK_ATTN;
    case KEY_NAME_INDEX_CrSel:
      return dom::KeyboardEvent_Binding::DOM_VK_CRSEL;
    case KEY_NAME_INDEX_ExSel:
      return dom::KeyboardEvent_Binding::DOM_VK_EXSEL;
    case KEY_NAME_INDEX_EraseEof:
      return dom::KeyboardEvent_Binding::DOM_VK_EREOF;
    case KEY_NAME_INDEX_Play:
      return dom::KeyboardEvent_Binding::DOM_VK_PLAY;
    case KEY_NAME_INDEX_ZoomToggle:
    case KEY_NAME_INDEX_ZoomIn:
    case KEY_NAME_INDEX_ZoomOut:
      return dom::KeyboardEvent_Binding::DOM_VK_ZOOM;
    default:
      return 0;
  }
}

/* static */
CodeNameIndex WidgetKeyboardEvent::ComputeCodeNameIndexFromKeyNameIndex(
    KeyNameIndex aKeyNameIndex, const Maybe<uint32_t>& aLocation) {
  if (aLocation.isSome() &&
      aLocation.value() ==
          dom::KeyboardEvent_Binding::DOM_KEY_LOCATION_NUMPAD) {
    // On macOS, NumLock is not supported.  Therefore, this handles
    // control key values except "Enter" only on non-macOS platforms.
    switch (aKeyNameIndex) {
#ifndef XP_MACOSX
      case KEY_NAME_INDEX_Insert:
        return CODE_NAME_INDEX_Numpad0;
      case KEY_NAME_INDEX_End:
        return CODE_NAME_INDEX_Numpad1;
      case KEY_NAME_INDEX_ArrowDown:
        return CODE_NAME_INDEX_Numpad2;
      case KEY_NAME_INDEX_PageDown:
        return CODE_NAME_INDEX_Numpad3;
      case KEY_NAME_INDEX_ArrowLeft:
        return CODE_NAME_INDEX_Numpad4;
      case KEY_NAME_INDEX_Clear:
        // FYI: "Clear" on macOS should be DOM_KEY_LOCATION_STANDARD.
        return CODE_NAME_INDEX_Numpad5;
      case KEY_NAME_INDEX_ArrowRight:
        return CODE_NAME_INDEX_Numpad6;
      case KEY_NAME_INDEX_Home:
        return CODE_NAME_INDEX_Numpad7;
      case KEY_NAME_INDEX_ArrowUp:
        return CODE_NAME_INDEX_Numpad8;
      case KEY_NAME_INDEX_PageUp:
        return CODE_NAME_INDEX_Numpad9;
      case KEY_NAME_INDEX_Delete:
        return CODE_NAME_INDEX_NumpadDecimal;
#endif  // #ifndef XP_MACOSX
      case KEY_NAME_INDEX_Enter:
        return CODE_NAME_INDEX_NumpadEnter;
      default:
        return CODE_NAME_INDEX_UNKNOWN;
    }
  }

  if (WidgetKeyboardEvent::IsLeftOrRightModiferKeyNameIndex(aKeyNameIndex)) {
    if (aLocation.isSome() &&
        (aLocation.value() !=
             dom::KeyboardEvent_Binding::DOM_KEY_LOCATION_LEFT &&
         aLocation.value() !=
             dom::KeyboardEvent_Binding::DOM_KEY_LOCATION_RIGHT)) {
      return CODE_NAME_INDEX_UNKNOWN;
    }
    bool isRight =
        aLocation.isSome() &&
        aLocation.value() == dom::KeyboardEvent_Binding::DOM_KEY_LOCATION_RIGHT;
    switch (aKeyNameIndex) {
      case KEY_NAME_INDEX_Alt:
        return isRight ? CODE_NAME_INDEX_AltRight : CODE_NAME_INDEX_AltLeft;
      case KEY_NAME_INDEX_Control:
        return isRight ? CODE_NAME_INDEX_ControlRight
                       : CODE_NAME_INDEX_ControlLeft;
      case KEY_NAME_INDEX_Shift:
        return isRight ? CODE_NAME_INDEX_ShiftRight : CODE_NAME_INDEX_ShiftLeft;
#if defined(XP_WIN)
      case KEY_NAME_INDEX_Meta:
        return CODE_NAME_INDEX_UNKNOWN;
      case KEY_NAME_INDEX_OS:  // win key.
        return isRight ? CODE_NAME_INDEX_OSRight : CODE_NAME_INDEX_OSLeft;
#elif defined(XP_MACOSX) || defined(ANDROID)
      case KEY_NAME_INDEX_Meta:  // command key.
        return isRight ? CODE_NAME_INDEX_OSRight : CODE_NAME_INDEX_OSLeft;
      case KEY_NAME_INDEX_OS:
        return CODE_NAME_INDEX_UNKNOWN;
#else
      case KEY_NAME_INDEX_Meta:  // Alt + Shift.
        return isRight ? CODE_NAME_INDEX_AltRight : CODE_NAME_INDEX_AltLeft;
      case KEY_NAME_INDEX_OS:  // Super/Hyper key.
        return isRight ? CODE_NAME_INDEX_OSRight : CODE_NAME_INDEX_OSLeft;
#endif
      default:
        return CODE_NAME_INDEX_UNKNOWN;
    }
  }

  if (aLocation.isSome() &&
      aLocation.value() !=
          dom::KeyboardEvent_Binding::DOM_KEY_LOCATION_STANDARD) {
    return CODE_NAME_INDEX_UNKNOWN;
  }

  switch (aKeyNameIndex) {
    // Standard section:
    case KEY_NAME_INDEX_Escape:
      return CODE_NAME_INDEX_Escape;
    case KEY_NAME_INDEX_Tab:
      return CODE_NAME_INDEX_Tab;
    case KEY_NAME_INDEX_CapsLock:
      return CODE_NAME_INDEX_CapsLock;
    case KEY_NAME_INDEX_ContextMenu:
      return CODE_NAME_INDEX_ContextMenu;
    case KEY_NAME_INDEX_Backspace:
      return CODE_NAME_INDEX_Backspace;
    case KEY_NAME_INDEX_Enter:
      return CODE_NAME_INDEX_Enter;
#ifdef XP_MACOSX
    // Although, macOS does not fire native key event of "Fn" key, we support
    // Fn key event if it's sent by other apps directly.
    case KEY_NAME_INDEX_Fn:
      return CODE_NAME_INDEX_Fn;
#endif  // #ifdef

    // Arrow Pad section:
    case KEY_NAME_INDEX_ArrowLeft:
      return CODE_NAME_INDEX_ArrowLeft;
    case KEY_NAME_INDEX_ArrowUp:
      return CODE_NAME_INDEX_ArrowUp;
    case KEY_NAME_INDEX_ArrowDown:
      return CODE_NAME_INDEX_ArrowDown;
    case KEY_NAME_INDEX_ArrowRight:
      return CODE_NAME_INDEX_ArrowRight;

      // Control Pad section:
#ifndef XP_MACOSX
    case KEY_NAME_INDEX_Insert:
      return CODE_NAME_INDEX_Insert;
#else
    case KEY_NAME_INDEX_Help:
      return CODE_NAME_INDEX_Help;
#endif  // #ifndef XP_MACOSX #else
    case KEY_NAME_INDEX_Delete:
      return CODE_NAME_INDEX_Delete;
    case KEY_NAME_INDEX_Home:
      return CODE_NAME_INDEX_Home;
    case KEY_NAME_INDEX_End:
      return CODE_NAME_INDEX_End;
    case KEY_NAME_INDEX_PageUp:
      return CODE_NAME_INDEX_PageUp;
    case KEY_NAME_INDEX_PageDown:
      return CODE_NAME_INDEX_PageDown;

    // Function keys:
    case KEY_NAME_INDEX_F1:
      return CODE_NAME_INDEX_F1;
    case KEY_NAME_INDEX_F2:
      return CODE_NAME_INDEX_F2;
    case KEY_NAME_INDEX_F3:
      return CODE_NAME_INDEX_F3;
    case KEY_NAME_INDEX_F4:
      return CODE_NAME_INDEX_F4;
    case KEY_NAME_INDEX_F5:
      return CODE_NAME_INDEX_F5;
    case KEY_NAME_INDEX_F6:
      return CODE_NAME_INDEX_F6;
    case KEY_NAME_INDEX_F7:
      return CODE_NAME_INDEX_F7;
    case KEY_NAME_INDEX_F8:
      return CODE_NAME_INDEX_F8;
    case KEY_NAME_INDEX_F9:
      return CODE_NAME_INDEX_F9;
    case KEY_NAME_INDEX_F10:
      return CODE_NAME_INDEX_F10;
    case KEY_NAME_INDEX_F11:
      return CODE_NAME_INDEX_F11;
    case KEY_NAME_INDEX_F12:
      return CODE_NAME_INDEX_F12;
    case KEY_NAME_INDEX_F13:
      return CODE_NAME_INDEX_F13;
    case KEY_NAME_INDEX_F14:
      return CODE_NAME_INDEX_F14;
    case KEY_NAME_INDEX_F15:
      return CODE_NAME_INDEX_F15;
    case KEY_NAME_INDEX_F16:
      return CODE_NAME_INDEX_F16;
    case KEY_NAME_INDEX_F17:
      return CODE_NAME_INDEX_F17;
    case KEY_NAME_INDEX_F18:
      return CODE_NAME_INDEX_F18;
    case KEY_NAME_INDEX_F19:
      return CODE_NAME_INDEX_F19;
    case KEY_NAME_INDEX_F20:
      return CODE_NAME_INDEX_F20;
#ifndef XP_MACOSX
    case KEY_NAME_INDEX_F21:
      return CODE_NAME_INDEX_F21;
    case KEY_NAME_INDEX_F22:
      return CODE_NAME_INDEX_F22;
    case KEY_NAME_INDEX_F23:
      return CODE_NAME_INDEX_F23;
    case KEY_NAME_INDEX_F24:
      return CODE_NAME_INDEX_F24;
    case KEY_NAME_INDEX_Pause:
      return CODE_NAME_INDEX_Pause;
    case KEY_NAME_INDEX_PrintScreen:
      return CODE_NAME_INDEX_PrintScreen;
    case KEY_NAME_INDEX_ScrollLock:
      return CODE_NAME_INDEX_ScrollLock;
#endif  // #ifndef XP_MACOSX

      // NumLock key:
#ifndef XP_MACOSX
    case KEY_NAME_INDEX_NumLock:
      return CODE_NAME_INDEX_NumLock;
#else
    case KEY_NAME_INDEX_Clear:
      return CODE_NAME_INDEX_NumLock;
#endif  // #ifndef XP_MACOSX #else

    // Media keys:
    case KEY_NAME_INDEX_AudioVolumeDown:
      return CODE_NAME_INDEX_VolumeDown;
    case KEY_NAME_INDEX_AudioVolumeMute:
      return CODE_NAME_INDEX_VolumeMute;
    case KEY_NAME_INDEX_AudioVolumeUp:
      return CODE_NAME_INDEX_VolumeUp;
#ifndef XP_MACOSX
    case KEY_NAME_INDEX_BrowserBack:
      return CODE_NAME_INDEX_BrowserBack;
    case KEY_NAME_INDEX_BrowserFavorites:
      return CODE_NAME_INDEX_BrowserFavorites;
    case KEY_NAME_INDEX_BrowserForward:
      return CODE_NAME_INDEX_BrowserForward;
    case KEY_NAME_INDEX_BrowserRefresh:
      return CODE_NAME_INDEX_BrowserRefresh;
    case KEY_NAME_INDEX_BrowserSearch:
      return CODE_NAME_INDEX_BrowserSearch;
    case KEY_NAME_INDEX_BrowserStop:
      return CODE_NAME_INDEX_BrowserStop;
    case KEY_NAME_INDEX_MediaPlayPause:
      return CODE_NAME_INDEX_MediaPlayPause;
    case KEY_NAME_INDEX_MediaStop:
      return CODE_NAME_INDEX_MediaStop;
    case KEY_NAME_INDEX_MediaTrackNext:
      return CODE_NAME_INDEX_MediaTrackNext;
    case KEY_NAME_INDEX_MediaTrackPrevious:
      return CODE_NAME_INDEX_MediaTrackPrevious;
    case KEY_NAME_INDEX_LaunchApplication1:
      return CODE_NAME_INDEX_LaunchApp1;
#endif  // #ifndef XP_MACOSX

      // Only Windows and GTK supports the following multimedia keys.
#if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)
    case KEY_NAME_INDEX_BrowserHome:
      return CODE_NAME_INDEX_BrowserHome;
    case KEY_NAME_INDEX_LaunchApplication2:
      return CODE_NAME_INDEX_LaunchApp2;
#endif  // #if defined(XP_WIN) || defined(MOZ_WIDGET_GTK)

      // Only GTK and Android supports the following multimedia keys.
#if defined(MOZ_WIDGET_GTK) || defined(ANDROID)
    case KEY_NAME_INDEX_Eject:
      return CODE_NAME_INDEX_Eject;
    case KEY_NAME_INDEX_WakeUp:
      return CODE_NAME_INDEX_WakeUp;
#endif  // #if defined(MOZ_WIDGET_GTK) || defined(ANDROID)

      // Only Windows does not support Help key (and macOS handled above).
#if !defined(XP_WIN) && !defined(XP_MACOSX)
    case KEY_NAME_INDEX_Help:
      return CODE_NAME_INDEX_Help;
#endif  // #if !defined(XP_WIN) && !defined(XP_MACOSX)

      // IME specific keys:
#ifdef XP_WIN
    case KEY_NAME_INDEX_Convert:
      return CODE_NAME_INDEX_Convert;
    case KEY_NAME_INDEX_NonConvert:
      return CODE_NAME_INDEX_NonConvert;
    case KEY_NAME_INDEX_Alphanumeric:
      return CODE_NAME_INDEX_CapsLock;
    case KEY_NAME_INDEX_KanaMode:
    case KEY_NAME_INDEX_Romaji:
    case KEY_NAME_INDEX_Katakana:
    case KEY_NAME_INDEX_Hiragana:
      return CODE_NAME_INDEX_KanaMode;
    case KEY_NAME_INDEX_Hankaku:
    case KEY_NAME_INDEX_Zenkaku:
    case KEY_NAME_INDEX_KanjiMode:
      return CODE_NAME_INDEX_Backquote;
    case KEY_NAME_INDEX_HanjaMode:
      return CODE_NAME_INDEX_Lang2;
    case KEY_NAME_INDEX_HangulMode:
      return CODE_NAME_INDEX_Lang1;
#endif  // #ifdef XP_WIN

#ifdef MOZ_WIDGET_GTK
    case KEY_NAME_INDEX_Convert:
      return CODE_NAME_INDEX_Convert;
    case KEY_NAME_INDEX_NonConvert:
      return CODE_NAME_INDEX_NonConvert;
    case KEY_NAME_INDEX_Alphanumeric:
      return CODE_NAME_INDEX_CapsLock;
    case KEY_NAME_INDEX_HiraganaKatakana:
      return CODE_NAME_INDEX_KanaMode;
    case KEY_NAME_INDEX_ZenkakuHankaku:
      return CODE_NAME_INDEX_Backquote;
#endif  // #ifdef MOZ_WIDGET_GTK

#ifdef ANDROID
    case KEY_NAME_INDEX_Convert:
      return CODE_NAME_INDEX_Convert;
    case KEY_NAME_INDEX_NonConvert:
      return CODE_NAME_INDEX_NonConvert;
    case KEY_NAME_INDEX_HiraganaKatakana:
      return CODE_NAME_INDEX_KanaMode;
    case KEY_NAME_INDEX_ZenkakuHankaku:
      return CODE_NAME_INDEX_Backquote;
    case KEY_NAME_INDEX_Eisu:
      return CODE_NAME_INDEX_Lang2;
    case KEY_NAME_INDEX_KanjiMode:
      return CODE_NAME_INDEX_Lang1;
#endif  // #ifdef ANDROID

#ifdef XP_MACOSX
    case KEY_NAME_INDEX_Eisu:
      return CODE_NAME_INDEX_Lang2;
    case KEY_NAME_INDEX_KanjiMode:
      return CODE_NAME_INDEX_Lang1;
#endif  // #ifdef XP_MACOSX

    default:
      return CODE_NAME_INDEX_UNKNOWN;
  }
}

/* static */
Modifier WidgetKeyboardEvent::GetModifierForKeyName(
    KeyNameIndex aKeyNameIndex) {
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

/* static */
bool WidgetKeyboardEvent::IsLockableModifier(KeyNameIndex aKeyNameIndex) {
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

/******************************************************************************
 * mozilla::InternalEditorInputEvent (TextEvents.h)
 ******************************************************************************/

#define NS_DEFINE_INPUTTYPE(aCPPName, aDOMName) (u"" aDOMName),
const char16_t* const InternalEditorInputEvent::kInputTypeNames[] = {
#include "mozilla/InputTypeList.h"
};
#undef NS_DEFINE_INPUTTYPE

InternalEditorInputEvent::InputTypeHashtable*
    InternalEditorInputEvent::sInputTypeHashtable = nullptr;

/* static */
void InternalEditorInputEvent::Shutdown() {
  delete sInputTypeHashtable;
  sInputTypeHashtable = nullptr;
}

/* static */
void InternalEditorInputEvent::GetDOMInputTypeName(EditorInputType aInputType,
                                                   nsAString& aInputTypeName) {
  if (static_cast<size_t>(aInputType) >=
      static_cast<size_t>(EditorInputType::eUnknown)) {
    aInputTypeName.Truncate();
    return;
  }

  MOZ_RELEASE_ASSERT(
      static_cast<size_t>(aInputType) < ArrayLength(kInputTypeNames),
      "Illegal input type enumeration value");
  aInputTypeName.Assign(kInputTypeNames[static_cast<size_t>(aInputType)]);
}

/* static */
EditorInputType InternalEditorInputEvent::GetEditorInputType(
    const nsAString& aInputType) {
  if (aInputType.IsEmpty()) {
    return EditorInputType::eUnknown;
  }

  if (!sInputTypeHashtable) {
    sInputTypeHashtable = new InputTypeHashtable(ArrayLength(kInputTypeNames));
    for (size_t i = 0; i < ArrayLength(kInputTypeNames); i++) {
      sInputTypeHashtable->InsertOrUpdate(nsDependentString(kInputTypeNames[i]),
                                          static_cast<EditorInputType>(i));
    }
  }
  return sInputTypeHashtable->MaybeGet(aInputType)
      .valueOr(EditorInputType::eUnknown);
}

}  // namespace mozilla
