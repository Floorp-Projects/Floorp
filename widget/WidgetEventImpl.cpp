/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BasicEvents.h"
#include "mozilla/ContentEvents.h"
#include "mozilla/InternalMutationEvent.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/Preferences.h"
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
    case NS_DRAGDROP_ENTER:
    case NS_DRAGDROP_OVER:
    case NS_DRAGDROP_EXIT:
    case NS_DRAGDROP_DRAGDROP:
    case NS_DRAGDROP_GESTURE:
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

bool
WidgetEvent::HasKeyEventMessage() const
{
  switch (mMessage) {
    case eKeyDown:
    case eKeyPress:
    case eKeyUp:
    case eBeforeKeyDown:
    case eBeforeKeyUp:
    case eAfterKeyDown:
    case eAfterKeyUp:
      return true;
    default:
      return false;
  }
}

bool
WidgetEvent::HasIMEEventMessage() const
{
  switch (mMessage) {
    case NS_COMPOSITION_START:
    case NS_COMPOSITION_END:
    case NS_COMPOSITION_UPDATE:
    case NS_COMPOSITION_CHANGE:
    case NS_COMPOSITION_COMMIT_AS_IS:
    case NS_COMPOSITION_COMMIT:
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
    case ePointerEventClass:
      // We want synthesized mouse moves to cause mouseover and mouseout
      // DOM events (EventStateManager::PreHandleEvent), but not mousemove
      // DOM events.
      // Synthesized button up events also do not cause DOM events because they
      // do not have a reliable refPoint.
      return AsMouseEvent()->reason == WidgetMouseEvent::eReal;

    case eWheelEventClass: {
      // wheel event whose all delta values are zero by user pref applied, it
      // shouldn't cause a DOM event.
      const WidgetWheelEvent* wheelEvent = AsWheelEvent();
      return wheelEvent->deltaX != 0.0 || wheelEvent->deltaY != 0.0 ||
             wheelEvent->deltaZ != 0.0;
    }

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
 * mozilla::WidgetKeyboardEvent (TextEvents.h)
 ******************************************************************************/

#define NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName) MOZ_UTF16(aDOMKeyName),
const char16_t* WidgetKeyboardEvent::kKeyNames[] = {
#include "mozilla/KeyNameList.h"
};
#undef NS_DEFINE_KEYNAME

#define NS_DEFINE_PHYSICAL_KEY_CODE_NAME(aCPPName, aDOMCodeName) \
    MOZ_UTF16(aDOMCodeName),
const char16_t* WidgetKeyboardEvent::kCodeNames[] = {
#include "mozilla/PhysicalKeyCodeNameList.h"
};
#undef NS_DEFINE_PHYSICAL_KEY_CODE_NAME

WidgetKeyboardEvent::KeyNameIndexHashtable*
  WidgetKeyboardEvent::sKeyNameIndexHashtable = nullptr;
WidgetKeyboardEvent::CodeNameIndexHashtable*
  WidgetKeyboardEvent::sCodeNameIndexHashtable = nullptr;

bool
WidgetKeyboardEvent::ShouldCauseKeypressEvents() const
{
  // Currently, we don't dispatch keypress events of modifier keys.
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
      return false;
    default:
      return true;
  }
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
  static const char* kCommands[] = {
    "" // CommandDoNothing
#include "mozilla/CommandList.h"
  };
#undef NS_DEFINE_COMMAND

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
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_LEFT;
    case CODE_NAME_INDEX_AltRight:
    case CODE_NAME_INDEX_ControlRight:
    case CODE_NAME_INDEX_OSRight:
    case CODE_NAME_INDEX_ShiftRight:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_RIGHT;
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
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;
    default:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD;
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
    case KEY_NAME_INDEX_VolumeMute:
      return nsIDOMKeyEvent::DOM_VK_VOLUME_MUTE;
    case KEY_NAME_INDEX_VolumeDown:
      return nsIDOMKeyEvent::DOM_VK_VOLUME_DOWN;
    case KEY_NAME_INDEX_VolumeUp:
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
