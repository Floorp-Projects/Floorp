/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "KeyboardLayout.h"
#include "nsIMM32Handler.h"

#include "nsMemory.h"
#include "nsToolkit.h"
#include "nsQuickSort.h"
#include "nsAlgorithm.h"
#include "nsGUIEvent.h"
#include "nsUnicharUtils.h"
#include "WidgetUtils.h"
#include "WinUtils.h"
#include "nsWindowDbg.h"
#include "nsServiceManagerUtils.h"
#include "nsPrintfCString.h"

#include "nsIDOMKeyEvent.h"
#include "nsIIdleServiceInternal.h"

#include "npapi.h"

#include <windows.h>
#include <winuser.h>
#include <algorithm>

#ifndef WINABLEAPI
#include <winable.h>
#endif

namespace mozilla {
namespace widget {

struct DeadKeyEntry
{
  PRUnichar BaseChar;
  PRUnichar CompositeChar;
};


class DeadKeyTable
{
  friend class KeyboardLayout;

  uint16_t mEntries;
  // KeyboardLayout::AddDeadKeyTable() will allocate as many entries as
  // required.  It is the only way to create new DeadKeyTable instances.
  DeadKeyEntry mTable[1];

  void Init(const DeadKeyEntry* aDeadKeyArray, uint32_t aEntries)
  {
    mEntries = aEntries;
    memcpy(mTable, aDeadKeyArray, aEntries * sizeof(DeadKeyEntry));
  }

  static uint32_t SizeInBytes(uint32_t aEntries)
  {
    return offsetof(DeadKeyTable, mTable) + aEntries * sizeof(DeadKeyEntry);
  }

public:
  uint32_t Entries() const
  {
    return mEntries;
  }

  bool IsEqual(const DeadKeyEntry* aDeadKeyArray, uint32_t aEntries) const
  {
    return (mEntries == aEntries &&
            !memcmp(mTable, aDeadKeyArray,
                    aEntries * sizeof(DeadKeyEntry)));
  }

  PRUnichar GetCompositeChar(PRUnichar aBaseChar) const;
};


/*****************************************************************************
 * mozilla::widget::ModifierKeyState
 *****************************************************************************/

void
ModifierKeyState::Update()
{
  mModifiers = 0;
  if (IS_VK_DOWN(VK_SHIFT)) {
    mModifiers |= MODIFIER_SHIFT;
  }
  if (IS_VK_DOWN(VK_CONTROL)) {
    mModifiers |= MODIFIER_CONTROL;
  }
  if (IS_VK_DOWN(VK_MENU)) {
    mModifiers |= MODIFIER_ALT;
  }
  if (IS_VK_DOWN(VK_LWIN) || IS_VK_DOWN(VK_RWIN)) {
    mModifiers |= MODIFIER_OS;
  }
  if (::GetKeyState(VK_CAPITAL) & 1) {
    mModifiers |= MODIFIER_CAPSLOCK;
  }
  if (::GetKeyState(VK_NUMLOCK) & 1) {
    mModifiers |= MODIFIER_NUMLOCK;
  }
  if (::GetKeyState(VK_SCROLL) & 1) {
    mModifiers |= MODIFIER_SCROLLLOCK;
  }

  EnsureAltGr();
}

void
ModifierKeyState::InitInputEvent(nsInputEvent& aInputEvent) const
{
  aInputEvent.modifiers = mModifiers;

  switch(aInputEvent.eventStructType) {
    case NS_MOUSE_EVENT:
    case NS_MOUSE_SCROLL_EVENT:
    case NS_WHEEL_EVENT:
    case NS_DRAG_EVENT:
    case NS_SIMPLE_GESTURE_EVENT:
      InitMouseEvent(aInputEvent);
      break;
    default:
      break;
  }
}

void
ModifierKeyState::InitMouseEvent(nsInputEvent& aMouseEvent) const
{
  NS_ASSERTION(aMouseEvent.eventStructType == NS_MOUSE_EVENT ||
               aMouseEvent.eventStructType == NS_WHEEL_EVENT ||
               aMouseEvent.eventStructType == NS_DRAG_EVENT ||
               aMouseEvent.eventStructType == NS_SIMPLE_GESTURE_EVENT,
               "called with non-mouse event");

  nsMouseEvent_base& mouseEvent = static_cast<nsMouseEvent_base&>(aMouseEvent);
  mouseEvent.buttons = 0;
  if (::GetKeyState(VK_LBUTTON) < 0) {
    mouseEvent.buttons |= nsMouseEvent::eLeftButtonFlag;
  }
  if (::GetKeyState(VK_RBUTTON) < 0) {
    mouseEvent.buttons |= nsMouseEvent::eRightButtonFlag;
  }
  if (::GetKeyState(VK_MBUTTON) < 0) {
    mouseEvent.buttons |= nsMouseEvent::eMiddleButtonFlag;
  }
  if (::GetKeyState(VK_XBUTTON1) < 0) {
    mouseEvent.buttons |= nsMouseEvent::e4thButtonFlag;
  }
  if (::GetKeyState(VK_XBUTTON2) < 0) {
    mouseEvent.buttons |= nsMouseEvent::e5thButtonFlag;
  }
}

/*****************************************************************************
 * mozilla::widget::UniCharsAndModifiers
 *****************************************************************************/

void
UniCharsAndModifiers::Append(PRUnichar aUniChar, Modifiers aModifiers)
{
  MOZ_ASSERT(mLength < 5);
  mChars[mLength] = aUniChar;
  mModifiers[mLength] = aModifiers;
  mLength++;
}

void
UniCharsAndModifiers::FillModifiers(Modifiers aModifiers)
{
  for (uint32_t i = 0; i < mLength; i++) {
    mModifiers[i] = aModifiers;
  }
}

bool
UniCharsAndModifiers::UniCharsEqual(const UniCharsAndModifiers& aOther) const
{
  if (mLength != aOther.mLength) {
    return false;
  }
  return !memcmp(mChars, aOther.mChars, mLength * sizeof(PRUnichar));
}

bool
UniCharsAndModifiers::UniCharsCaseInsensitiveEqual(
                        const UniCharsAndModifiers& aOther) const
{
  if (mLength != aOther.mLength) {
    return false;
  }

  nsCaseInsensitiveStringComparator comp;
  return !comp(mChars, aOther.mChars, mLength, aOther.mLength);
}

UniCharsAndModifiers&
UniCharsAndModifiers::operator+=(const UniCharsAndModifiers& aOther)
{
  uint32_t copyCount = std::min(aOther.mLength, 5 - mLength);
  NS_ENSURE_TRUE(copyCount > 0, *this);
  memcpy(&mChars[mLength], aOther.mChars, copyCount * sizeof(PRUnichar));
  memcpy(&mModifiers[mLength], aOther.mModifiers,
         copyCount * sizeof(Modifiers));
  mLength += copyCount;
  return *this;
}

UniCharsAndModifiers
UniCharsAndModifiers::operator+(const UniCharsAndModifiers& aOther) const
{
  UniCharsAndModifiers result(*this);
  result += aOther;
  return result;
}

/*****************************************************************************
 * mozilla::widget::VirtualKey
 *****************************************************************************/

inline PRUnichar
VirtualKey::GetCompositeChar(ShiftState aShiftState, PRUnichar aBaseChar) const
{
  return mShiftStates[aShiftState].DeadKey.Table->GetCompositeChar(aBaseChar);
}

const DeadKeyTable*
VirtualKey::MatchingDeadKeyTable(const DeadKeyEntry* aDeadKeyArray,
                                 uint32_t aEntries) const
{
  if (!mIsDeadKey) {
    return nullptr;
  }

  for (ShiftState shiftState = 0; shiftState < 16; shiftState++) {
    if (!IsDeadKey(shiftState)) {
      continue;
    }
    const DeadKeyTable* dkt = mShiftStates[shiftState].DeadKey.Table;
    if (dkt && dkt->IsEqual(aDeadKeyArray, aEntries)) {
      return dkt;
    }
  }

  return nullptr;
}

void
VirtualKey::SetNormalChars(ShiftState aShiftState,
                           const PRUnichar* aChars,
                           uint32_t aNumOfChars)
{
  NS_ASSERTION(aShiftState < ArrayLength(mShiftStates), "invalid index");

  SetDeadKey(aShiftState, false);

  for (uint32_t index = 0; index < aNumOfChars; index++) {
    // Ignore legacy non-printable control characters
    mShiftStates[aShiftState].Normal.Chars[index] =
      (aChars[index] >= 0x20) ? aChars[index] : 0;
  }

  uint32_t len = ArrayLength(mShiftStates[aShiftState].Normal.Chars);
  for (uint32_t index = aNumOfChars; index < len; index++) {
    mShiftStates[aShiftState].Normal.Chars[index] = 0;
  }
}

void
VirtualKey::SetDeadChar(ShiftState aShiftState, PRUnichar aDeadChar)
{
  NS_ASSERTION(aShiftState < ArrayLength(mShiftStates), "invalid index");

  SetDeadKey(aShiftState, true);

  mShiftStates[aShiftState].DeadKey.DeadChar = aDeadChar;
  mShiftStates[aShiftState].DeadKey.Table = nullptr;
}

UniCharsAndModifiers
VirtualKey::GetUniChars(ShiftState aShiftState) const
{
  UniCharsAndModifiers result = GetNativeUniChars(aShiftState);

  const ShiftState STATE_ALT_CONTROL = (STATE_ALT | STATE_CONTROL);
  if (!(aShiftState & STATE_ALT_CONTROL)) {
    return result;
  }

  if (!result.mLength) {
    result = GetNativeUniChars(aShiftState & ~STATE_ALT_CONTROL);
    result.FillModifiers(ShiftStateToModifiers(aShiftState));
    return result;
  }

  if ((aShiftState & STATE_ALT_CONTROL) == STATE_ALT_CONTROL) {
    // Even if the shifted chars and the unshifted chars are same, we
    // should consume the Alt key state and the Ctrl key state when
    // AltGr key is pressed. Because if we don't consume them, the input
    // events are ignored on nsEditor. (I.e., Users cannot input the
    // characters with this key combination.)
    Modifiers finalModifiers = ShiftStateToModifiers(aShiftState);
    finalModifiers &= ~(MODIFIER_ALT | MODIFIER_CONTROL);
    result.FillModifiers(finalModifiers);
    return result;
  }

  UniCharsAndModifiers unmodifiedReslt =
    GetNativeUniChars(aShiftState & ~STATE_ALT_CONTROL);
  if (!result.UniCharsEqual(unmodifiedReslt)) {
    // Otherwise, we should consume the Alt key state and the Ctrl key state
    // only when the shifted chars and unshifted chars are different.
    Modifiers finalModifiers = ShiftStateToModifiers(aShiftState);
    finalModifiers &= ~(MODIFIER_ALT | MODIFIER_CONTROL);
    result.FillModifiers(finalModifiers);
  }
  return result;
}


UniCharsAndModifiers
VirtualKey::GetNativeUniChars(ShiftState aShiftState) const
{
  UniCharsAndModifiers result;
  Modifiers modifiers = ShiftStateToModifiers(aShiftState);
  if (IsDeadKey(aShiftState)) {
    result.Append(mShiftStates[aShiftState].DeadKey.DeadChar, modifiers);
    return result;
  }

  uint32_t index;
  uint32_t len = ArrayLength(mShiftStates[aShiftState].Normal.Chars);
  for (index = 0;
       index < len && mShiftStates[aShiftState].Normal.Chars[index]; index++) {
    result.Append(mShiftStates[aShiftState].Normal.Chars[index], modifiers);
  }
  return result;
}

// static
void
VirtualKey::FillKbdState(PBYTE aKbdState,
                         const ShiftState aShiftState)
{
  NS_ASSERTION(aShiftState < 16, "aShiftState out of range");

  if (aShiftState & STATE_SHIFT) {
    aKbdState[VK_SHIFT] |= 0x80;
  } else {
    aKbdState[VK_SHIFT]  &= ~0x80;
    aKbdState[VK_LSHIFT] &= ~0x80;
    aKbdState[VK_RSHIFT] &= ~0x80;
  }

  if (aShiftState & STATE_CONTROL) {
    aKbdState[VK_CONTROL] |= 0x80;
  } else {
    aKbdState[VK_CONTROL]  &= ~0x80;
    aKbdState[VK_LCONTROL] &= ~0x80;
    aKbdState[VK_RCONTROL] &= ~0x80;
  }

  if (aShiftState & STATE_ALT) {
    aKbdState[VK_MENU] |= 0x80;
  } else {
    aKbdState[VK_MENU]  &= ~0x80;
    aKbdState[VK_LMENU] &= ~0x80;
    aKbdState[VK_RMENU] &= ~0x80;
  }

  if (aShiftState & STATE_CAPSLOCK) {
    aKbdState[VK_CAPITAL] |= 0x01;
  } else {
    aKbdState[VK_CAPITAL] &= ~0x01;
  }
}

/*****************************************************************************
 * mozilla::widget::NativeKey
 *****************************************************************************/

NativeKey::NativeKey(nsWindowBase* aWidget,
                     const MSG& aKeyOrCharMessage,
                     const ModifierKeyState& aModKeyState,
                     const nsFakeCharMessage* aFakeCharMessage) :
  mWidget(aWidget), mMsg(aKeyOrCharMessage), mDOMKeyCode(0),
  mModKeyState(aModKeyState), mVirtualKeyCode(0), mOriginalVirtualKeyCode(0),
  mIsFakeCharMsg(false)
{
  MOZ_ASSERT(aWidget);
  KeyboardLayout* keyboardLayout = KeyboardLayout::GetInstance();
  mKeyboardLayout = keyboardLayout->GetLayout();
  mScanCode = WinUtils::GetScanCode(mMsg.lParam);
  mIsExtended = WinUtils::IsExtendedScanCode(mMsg.lParam);
  memset(&mCharMsg, 0, sizeof(MSG));
  // On WinXP and WinServer2003, we cannot compute the virtual keycode for
  // extended keys due to the API limitation.
  bool canComputeVirtualKeyCodeFromScanCode =
    (!mIsExtended || WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION);
  switch (mMsg.message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
      // Store following WM_*CHAR message into mCharMsg.
      if (aFakeCharMessage) {
        mCharMsg = aFakeCharMessage->GetCharMessage(mMsg.hwnd);
        mIsFakeCharMsg = true;
      } else {
        MSG msg;
        if (WinUtils::PeekMessage(&msg, mMsg.hwnd, WM_KEYFIRST, WM_KEYLAST,
                                  PM_NOREMOVE | PM_NOYIELD) &&
            (msg.message == WM_CHAR || msg.message == WM_SYSCHAR ||
             msg.message == WM_DEADCHAR)) {
          mCharMsg = msg;
        }
      }
    case WM_KEYUP:
    case WM_SYSKEYUP: {
      // First, resolve the IME converted virtual keycode to its original
      // keycode.
      if (mMsg.wParam == VK_PROCESSKEY) {
        mOriginalVirtualKeyCode =
          static_cast<uint8_t>(::ImmGetVirtualKey(mMsg.hwnd));
      } else {
        mOriginalVirtualKeyCode = static_cast<uint8_t>(mMsg.wParam);
      }

      // Most keys are not distinguished as left or right keys.
      bool isLeftRightDistinguishedKey = false;

      // mOriginalVirtualKeyCode must not distinguish left or right of
      // Shift, Control or Alt.
      switch (mOriginalVirtualKeyCode) {
        case VK_SHIFT:
        case VK_CONTROL:
        case VK_MENU:
          isLeftRightDistinguishedKey = true;
          break;
        case VK_LSHIFT:
        case VK_RSHIFT:
          mVirtualKeyCode = mOriginalVirtualKeyCode;
          mOriginalVirtualKeyCode = VK_SHIFT;
          isLeftRightDistinguishedKey = true;
          break;
        case VK_LCONTROL:
        case VK_RCONTROL:
          mVirtualKeyCode = mOriginalVirtualKeyCode;
          mOriginalVirtualKeyCode = VK_CONTROL;
          isLeftRightDistinguishedKey = true;
          break;
        case VK_LMENU:
        case VK_RMENU:
          mVirtualKeyCode = mOriginalVirtualKeyCode;
          mOriginalVirtualKeyCode = VK_MENU;
          isLeftRightDistinguishedKey = true;
          break;
      }

      // If virtual keycode (left-right distinguished keycode) is already
      // computed, we don't need to do anymore.
      if (mVirtualKeyCode) {
        break;
      }

      // If the keycode doesn't have LR distinguished keycode, we just set
      // mOriginalVirtualKeyCode to mVirtualKeyCode.  Note that don't compute
      // it from MapVirtualKeyEx() because the scan code might be wrong if
      // the message is sent/posted by other application.  Then, we will compute
      // unexpected keycode from the scan code.
      if (!isLeftRightDistinguishedKey) {
        break;
      }

      if (!canComputeVirtualKeyCodeFromScanCode) {
        // The right control key and the right alt key are extended keys.
        // Therefore, we never get VK_RCONTRL and VK_RMENU for the result of
        // MapVirtualKeyEx() on WinXP or WinServer2003.
        //
        // If VK_CONTROL or VK_MENU key message is caused by an extended key,
        // we should assume that the right key of them is pressed.
        switch (mOriginalVirtualKeyCode) {
          case VK_CONTROL:
            mVirtualKeyCode = VK_RCONTROL;
            break;
          case VK_MENU:
            mVirtualKeyCode = VK_RMENU;
            break;
          case VK_SHIFT:
            // Neither left shift nor right shift is not an extended key,
            // let's use VK_LSHIFT for invalid scan code.
            mVirtualKeyCode = VK_LSHIFT;
            break;
          default:
            MOZ_NOT_REACHED("Unsupported mOriginalVirtualKeyCode");
            break;
        }
        break;
      }

      NS_ASSERTION(!mVirtualKeyCode,
                   "mVirtualKeyCode has been computed already");

      // Otherwise, compute the virtual keycode with MapVirtualKeyEx().
      mVirtualKeyCode = ComputeVirtualKeyCodeFromScanCodeEx();

      // The result might be unexpected value due to the scan code is
      // wrong.  For example, any key messages can be generated by
      // SendMessage() or PostMessage() from applications.  So, it's possible
      // failure.  Then, let's respect the extended flag even if it might be
      // set intentionally.
      switch (mOriginalVirtualKeyCode) {
        case VK_CONTROL:
          if (mVirtualKeyCode != VK_LCONTROL &&
              mVirtualKeyCode != VK_RCONTROL) {
            mVirtualKeyCode = mIsExtended ? VK_RCONTROL : VK_LCONTROL;
          }
          break;
        case VK_MENU:
          if (mVirtualKeyCode != VK_LMENU && mVirtualKeyCode != VK_RMENU) {
            mVirtualKeyCode = mIsExtended ? VK_RMENU : VK_LMENU;
          }
          break;
        case VK_SHIFT:
          if (mVirtualKeyCode != VK_LSHIFT && mVirtualKeyCode != VK_RSHIFT) {
            // Neither left shift nor right shift is not an extended key,
            // let's use VK_LSHIFT for invalid scan code.
            mVirtualKeyCode = VK_LSHIFT;
          }
          break;
        default:
          MOZ_NOT_REACHED("Unsupported mOriginalVirtualKeyCode");
          break;
      }
      break;
    }
    case WM_CHAR:
    case WM_UNICHAR:
    case WM_SYSCHAR:
      // We cannot compute the virtual key code from WM_CHAR message on WinXP
      // if it's caused by an extended key.
      if (!canComputeVirtualKeyCodeFromScanCode) {
        break;
      }
      mVirtualKeyCode = mOriginalVirtualKeyCode =
        ComputeVirtualKeyCodeFromScanCodeEx();
      break;
    default:
      MOZ_NOT_REACHED("Unsupported message");
      break;
  }

  if (!mVirtualKeyCode) {
    mVirtualKeyCode = mOriginalVirtualKeyCode;
  }

  mDOMKeyCode =
    keyboardLayout->ConvertNativeKeyCodeToDOMKeyCode(mOriginalVirtualKeyCode);
  mKeyNameIndex =
    keyboardLayout->ConvertNativeKeyCodeToKeyNameIndex(mOriginalVirtualKeyCode);

  keyboardLayout->InitNativeKey(*this, mModKeyState);

  mIsDeadKey =
    (mCharMsg.message == WM_DEADCHAR ||
     keyboardLayout->IsDeadKey(mOriginalVirtualKeyCode, mModKeyState));
  mIsPrintableKey = KeyboardLayout::IsPrintableCharKey(mOriginalVirtualKeyCode);
}

bool
NativeKey::IsIMEDoingKakuteiUndo() const
{
  // Following message pattern is caused by "Kakutei-Undo" of ATOK or WXG:
  // ---------------------------------------------------------------------------
  // WM_KEYDOWN              * n (wParam = VK_BACK, lParam = 0x1)
  // WM_KEYUP                * 1 (wParam = VK_BACK, lParam = 0xC0000001) # ATOK
  // WM_IME_STARTCOMPOSITION * 1 (wParam = 0x0, lParam = 0x0)
  // WM_IME_COMPOSITION      * 1 (wParam = 0x0, lParam = 0x1BF)
  // WM_CHAR                 * n (wParam = VK_BACK, lParam = 0x1)
  // WM_KEYUP                * 1 (wParam = VK_BACK, lParam = 0xC00E0001)
  // ---------------------------------------------------------------------------
  // This doesn't match usual key message pattern such as:
  //   WM_KEYDOWN -> WM_CHAR -> WM_KEYDOWN -> WM_CHAR -> ... -> WM_KEYUP
  // See following bugs for the detail.
  // https://bugzilla.mozilla.gr.jp/show_bug.cgi?id=2885 (written in Japanese)
  // https://bugzilla.mozilla.org/show_bug.cgi?id=194559 (written in English)
  MSG startCompositionMsg, compositionMsg, charMsg;
  return WinUtils::PeekMessage(&startCompositionMsg, mMsg.hwnd,
                               WM_IME_STARTCOMPOSITION, WM_IME_STARTCOMPOSITION,
                               PM_NOREMOVE | PM_NOYIELD) &&
         WinUtils::PeekMessage(&compositionMsg, mMsg.hwnd, WM_IME_COMPOSITION,
                               WM_IME_COMPOSITION, PM_NOREMOVE | PM_NOYIELD) &&
         WinUtils::PeekMessage(&charMsg, mMsg.hwnd, WM_CHAR, WM_CHAR,
                               PM_NOREMOVE | PM_NOYIELD) &&
         startCompositionMsg.wParam == 0x0 &&
         startCompositionMsg.lParam == 0x0 &&
         compositionMsg.wParam == 0x0 &&
         compositionMsg.lParam == 0x1BF &&
         charMsg.wParam == VK_BACK && charMsg.lParam == 0x1 &&
         startCompositionMsg.time <= compositionMsg.time &&
         compositionMsg.time <= charMsg.time;
}

UINT
NativeKey::GetScanCodeWithExtendedFlag() const
{
  // MapVirtualKeyEx() has been improved for supporting extended keys since
  // Vista.  When we call it for mapping a scancode of an extended key and
  // a virtual keycode, we need to add 0xE000 to the scancode.
  // On Win XP and Win Server 2003, this doesn't support. On them, we have
  // no way to get virtual keycodes from scancode of extended keys.
  if (!mIsExtended ||
      WinUtils::GetWindowsVersion() < WinUtils::VISTA_VERSION) {
    return mScanCode;
  }
  return (0xE000 | mScanCode);
}

uint32_t
NativeKey::GetKeyLocation() const
{
  switch (mVirtualKeyCode) {
    case VK_LSHIFT:
    case VK_LCONTROL:
    case VK_LMENU:
    case VK_LWIN:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_LEFT;

    case VK_RSHIFT:
    case VK_RCONTROL:
    case VK_RMENU:
    case VK_RWIN:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_RIGHT;

    case VK_RETURN:
      // XXX This code assumes that all keyboard drivers use same mapping.
      return !mIsExtended ? nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD :
                            nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;

    case VK_INSERT:
    case VK_DELETE:
    case VK_END:
    case VK_DOWN:
    case VK_NEXT:
    case VK_LEFT:
    case VK_CLEAR:
    case VK_RIGHT:
    case VK_HOME:
    case VK_UP:
    case VK_PRIOR:
      // XXX This code assumes that all keyboard drivers use same mapping.
      return mIsExtended ? nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD :
                           nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;

    // NumLock key isn't included due to IE9's behavior.
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
    case VK_DECIMAL:
    case VK_DIVIDE:
    case VK_MULTIPLY:
    case VK_SUBTRACT:
    case VK_ADD:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_NUMPAD;

    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
      NS_WARNING("Failed to decide the key location?");

    default:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD;
  }
}

uint8_t
NativeKey::ComputeVirtualKeyCodeFromScanCode() const
{
  return static_cast<uint8_t>(
           ::MapVirtualKeyEx(mScanCode, MAPVK_VSC_TO_VK, mKeyboardLayout));
}

uint8_t
NativeKey::ComputeVirtualKeyCodeFromScanCodeEx() const
{
  bool VistaOrLater =
    (WinUtils::GetWindowsVersion() >= WinUtils::VISTA_VERSION);
  // NOTE: WinXP doesn't support mapping scan code to virtual keycode of
  //       extended keys.
  NS_ENSURE_TRUE(!mIsExtended || VistaOrLater, 0);
  return static_cast<uint8_t>(
           ::MapVirtualKeyEx(GetScanCodeWithExtendedFlag(), MAPVK_VSC_TO_VK_EX,
                             mKeyboardLayout));
}

PRUnichar
NativeKey::ComputeUnicharFromScanCode() const
{
  return static_cast<PRUnichar>(
           ::MapVirtualKeyEx(ComputeVirtualKeyCodeFromScanCode(),
                             MAPVK_VK_TO_CHAR, mKeyboardLayout));
}

void
NativeKey::InitKeyEvent(nsKeyEvent& aKeyEvent,
                        const ModifierKeyState& aModKeyState) const
{
  nsIntPoint point(0, 0);
  mWidget->InitEvent(aKeyEvent, &point);

  switch (aKeyEvent.message) {
    case NS_KEY_DOWN:
      aKeyEvent.keyCode = mDOMKeyCode;
      break;
    case NS_KEY_UP:
      aKeyEvent.keyCode = mDOMKeyCode;
      // Set defaultPrevented of the key event if the VK_MENU is not a system
      // key release, so that the menu bar does not trigger.  This helps avoid
      // triggering the menu bar for ALT key accelerators used in assistive
      // technologies such as Window-Eyes and ZoomText or for switching open
      // state of IME.
      aKeyEvent.mFlags.mDefaultPrevented =
        (mOriginalVirtualKeyCode == VK_MENU && mMsg.message != WM_SYSKEYUP);
      break;
    case NS_KEY_PRESS:
      break;
    default:
      MOZ_NOT_REACHED("Invalid event message");
      break;
  }

  aKeyEvent.mKeyNameIndex = mKeyNameIndex;
  aKeyEvent.location = GetKeyLocation();
  aModKeyState.InitInputEvent(aKeyEvent);
}

bool
NativeKey::DispatchKeyEvent(nsKeyEvent& aKeyEvent,
                            const MSG* aMsgSentToPlugin) const
{
  KeyboardLayout::NotifyIdleServiceOfUserActivity();

  NPEvent pluginEvent;
  if (aMsgSentToPlugin &&
      mWidget->GetInputContext().mIMEState.mEnabled == IMEState::PLUGIN) {
    pluginEvent.event = aMsgSentToPlugin->message;
    pluginEvent.wParam = aMsgSentToPlugin->wParam;
    pluginEvent.lParam = aMsgSentToPlugin->lParam;
    aKeyEvent.pluginEvent = static_cast<void*>(&pluginEvent);
  }

  return mWidget->DispatchWindowEvent(&aKeyEvent);
}

bool
NativeKey::DispatchKeyDownEvent(bool* aEventDispatched) const
{
  MOZ_ASSERT(mMsg.message == WM_KEYDOWN || mMsg.message == WM_SYSKEYDOWN);

  if (aEventDispatched) {
    *aEventDispatched = false;
  }

  // Ignore [shift+]alt+space so the OS can handle it.
  if (mModKeyState.IsAlt() && !mModKeyState.IsControl() &&
      mVirtualKeyCode == VK_SPACE) {
    return false;
  }

  nsKeyEvent keydownEvent(true, NS_KEY_DOWN, mWidget);
  InitKeyEvent(keydownEvent, mModKeyState);
  if (aEventDispatched) {
    *aEventDispatched = true;
  }
  return DispatchKeyEvent(keydownEvent, &mMsg);
}

bool
NativeKey::HandleKeyDownMessage(bool* aEventDispatched) const
{
  MOZ_ASSERT(mMsg.message == WM_KEYDOWN || mMsg.message == WM_SYSKEYDOWN);

  if (aEventDispatched) {
    *aEventDispatched = false;
  }

  bool defaultPrevented = false;
  if (mIsFakeCharMsg ||
      !RedirectedKeyDownMessageManager::IsRedirectedMessage(mMsg)) {
    bool isIMEEnabled = WinUtils::IsIMEEnabled(mWidget->GetInputContext());
    bool eventDispatched;
    defaultPrevented = DispatchKeyDownEvent(&eventDispatched);
    if (aEventDispatched) {
      *aEventDispatched = eventDispatched;
    }
    if (!eventDispatched) {
      // If keydown event was not dispatched, keypress event shouldn't be
      // caused with the message.
      RedirectedKeyDownMessageManager::Forget();
      return false;
    }

    // If IMC wasn't associated to the window but is associated it now (i.e.,
    // focus is moved from a non-editable editor to an editor by keydown
    // event handler), WM_CHAR and WM_SYSCHAR shouldn't cause first character
    // inputting if IME is opened.  But then, we should redirect the native
    // keydown message to IME.
    // However, note that if focus has been already moved to another
    // application, we shouldn't redirect the message to it because the keydown
    // message is processed by us, so, nobody shouldn't process it.
    HWND focusedWnd = ::GetFocus();
    if (!defaultPrevented && !mIsFakeCharMsg && focusedWnd &&
        !mWidget->PluginHasFocus() && !isIMEEnabled &&
        WinUtils::IsIMEEnabled(mWidget->GetInputContext())) {
      RedirectedKeyDownMessageManager::RemoveNextCharMessage(focusedWnd);

      INPUT keyinput;
      keyinput.type = INPUT_KEYBOARD;
      keyinput.ki.wVk = mOriginalVirtualKeyCode;
      keyinput.ki.wScan = mScanCode;
      keyinput.ki.dwFlags = KEYEVENTF_SCANCODE;
      if (mIsExtended) {
        keyinput.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
      }
      keyinput.ki.time = 0;
      keyinput.ki.dwExtraInfo = 0;

      RedirectedKeyDownMessageManager::WillRedirect(mMsg, defaultPrevented);

      ::SendInput(1, &keyinput, sizeof(keyinput));

      // Return here.  We shouldn't dispatch keypress event for this WM_KEYDOWN.
      // If it's needed, it will be dispatched after next (redirected)
      // WM_KEYDOWN.
      return true;
    }

    if (mWidget->Destroyed()) {
      // If this was destroyed by the keydown event handler, we shouldn't
      // dispatch keypress event on this window.
      return true;
    }
  } else {
    defaultPrevented = RedirectedKeyDownMessageManager::DefaultPrevented();
    // If this is redirected keydown message, we have dispatched the keydown
    // event already.
    if (aEventDispatched) {
      *aEventDispatched = true;
    }
  }

  RedirectedKeyDownMessageManager::Forget();

  // If the key was processed by IME, we shouldn't dispatch keypress event.
  if (mOriginalVirtualKeyCode == VK_PROCESSKEY) {
    return defaultPrevented;
  }

  // If we won't be getting a WM_CHAR, WM_SYSCHAR or WM_DEADCHAR, synthesize a keypress
  // for almost all keys
  switch (mDOMKeyCode) {
    case NS_VK_SHIFT:
    case NS_VK_CONTROL:
    case NS_VK_ALT:
    case NS_VK_CAPS_LOCK:
    case NS_VK_NUM_LOCK:
    case NS_VK_SCROLL_LOCK:
    case NS_VK_WIN:
      return defaultPrevented;
  }

  EventFlags extraFlags;
  extraFlags.mDefaultPrevented = defaultPrevented;

  if (NeedsToHandleWithoutFollowingCharMessages()) {
    return DispatchKeyPressEventsAndDiscardsCharMessages(extraFlags);
  }

  if (IsFollowedByCharMessage()) {
    return DispatchKeyPressEventForFollowingCharMessage(extraFlags);
  }

  if (!mModKeyState.IsControl() && !mModKeyState.IsAlt() &&
      !mModKeyState.IsWin() && IsPrintableKey()) {
    // If this is simple KeyDown event but next message is not WM_CHAR,
    // this event may not input text, so we should ignore this event.
    // See bug 314130.
    return mWidget->PluginHasFocus() && defaultPrevented;
  }

  if (IsDeadKey()) {
    return mWidget->PluginHasFocus() && defaultPrevented;
  }

  return DispatchKeyPressEventsWithKeyboardLayout(extraFlags);
}

bool
NativeKey::HandleCharMessage(const MSG& aCharMsg,
                             bool* aEventDispatched,
                             const EventFlags* aExtraFlags) const
{
  MOZ_ASSERT(mMsg.message == WM_KEYDOWN || mMsg.message == WM_SYSKEYDOWN ||
             mMsg.message == WM_CHAR || mMsg.message == WM_SYSCHAR);
  MOZ_ASSERT(aCharMsg.message == WM_CHAR || aCharMsg.message == WM_SYSCHAR);

  if (aEventDispatched) {
    *aEventDispatched = false;
  }

  // Alt+Space key is handled by OS, we shouldn't touch it.
  if (mModKeyState.IsAlt() && !mModKeyState.IsControl() &&
      mVirtualKeyCode == VK_SPACE) {
    return false;
  }

  // Bug 818235: Ignore Ctrl+Enter.
  if (!mModKeyState.IsAlt() && mModKeyState.IsControl() &&
      mVirtualKeyCode == VK_RETURN) {
    return false;
  }

  // XXXmnakao I think that if aNativeKeyDown is null, such lonely WM_CHAR
  //           should cause composition events because they are not caused
  //           by actual keyboard operation.

  static const PRUnichar U_SPACE = 0x20;
  static const PRUnichar U_EQUAL = 0x3D;

  // First, handle normal text input or non-printable key case here.
  if ((!mModKeyState.IsAlt() && !mModKeyState.IsControl()) ||
      mModKeyState.IsAltGr() ||
      (mOriginalVirtualKeyCode &&
       !KeyboardLayout::IsPrintableCharKey(mOriginalVirtualKeyCode))) {
    nsKeyEvent keypressEvent(true, NS_KEY_PRESS, mWidget);
    if (aExtraFlags) {
      keypressEvent.mFlags.Union(*aExtraFlags);
    }
    if (aCharMsg.wParam >= U_SPACE) {
      keypressEvent.charCode = static_cast<uint32_t>(aCharMsg.wParam);
    } else {
      keypressEvent.keyCode = mDOMKeyCode;
    }
    // When AltGr (Alt+Ctrl) is pressed, that causes normal text input.
    // At this time, if either alt or ctrl flag is set, nsEditor ignores the
    // keypress event.  For avoiding this issue, we should remove ctrl and alt
    // flags.
    ModifierKeyState modKeyState(mModKeyState);
    modKeyState.Unset(MODIFIER_ALT | MODIFIER_CONTROL);
    InitKeyEvent(keypressEvent, modKeyState);
    if (aEventDispatched) {
      *aEventDispatched = true;
    }
    return DispatchKeyEvent(keypressEvent, &aCharMsg);
  }

  // XXX It seems that following code was implemented for shortcut key
  //     handling.  However, it's now handled in WM_KEYDOWN message handler.
  //     So, this actually runs only when WM_CHAR is sent/posted without
  //     WM_KEYDOWN.  I think that we don't need to keypress event in such
  //     case especially for shortcut keys.

  PRUnichar uniChar;
  // Ctrl+A Ctrl+Z, see Programming Windows 3.1 page 110 for details
  if (mModKeyState.IsControl() && aCharMsg.wParam <= 0x1A) {
    // Bug 16486: Need to account for shift here.
    uniChar = aCharMsg.wParam - 1 + (mModKeyState.IsShift() ? 'A' : 'a');
  } else if (mModKeyState.IsControl() && aCharMsg.wParam <= 0x1F) {
    // Bug 50255: <ctrl><[> and <ctrl><]> are not being processed.
    // also fixes ctrl+\ (x1c), ctrl+^ (x1e) and ctrl+_ (x1f)
    // for some reason the keypress handler need to have the uniChar code set
    // with the addition of a upper case A not the lower case.
    uniChar = aCharMsg.wParam - 1 + 'A';
  } else if (aCharMsg.wParam < U_SPACE ||
             (aCharMsg.wParam == U_EQUAL && mModKeyState.IsControl())) {
    uniChar = 0;
  } else {
    uniChar = aCharMsg.wParam;
  }

  // Bug 50255 and Bug 351310: Keep the characters unshifted for shortcuts and
  // accesskeys and make sure that numbers are always passed as such.
  if (uniChar && (mModKeyState.IsControl() || mModKeyState.IsAlt())) {
    KeyboardLayout* keyboardLayout = KeyboardLayout::GetInstance();
    PRUnichar unshiftedCharCode =
      (mVirtualKeyCode >= '0' && mVirtualKeyCode <= '9') ?
        mVirtualKeyCode :  mModKeyState.IsShift() ?
                             ComputeUnicharFromScanCode() : 0;
    // Ignore diacritics (top bit set) and key mapping errors (char code 0)
    if (static_cast<int32_t>(unshiftedCharCode) > 0) {
      uniChar = unshiftedCharCode;
    }
  }

  // Bug 285161 and Bug 295095: They were caused by the initial fix for
  // bug 178110.  When pressing (alt|ctrl)+char, the char must be lowercase
  // unless shift is pressed too.
  if (!mModKeyState.IsShift() &&
      (mModKeyState.IsAlt() || mModKeyState.IsControl())) {
    uniChar = towlower(uniChar);
  }

  nsKeyEvent keypressEvent(true, NS_KEY_PRESS, mWidget);
  if (aExtraFlags) {
    keypressEvent.mFlags.Union(*aExtraFlags);
  }
  keypressEvent.charCode = uniChar;
  if (!keypressEvent.charCode) {
    keypressEvent.keyCode = mDOMKeyCode;
  }
  InitKeyEvent(keypressEvent, mModKeyState);
  if (aEventDispatched) {
    *aEventDispatched = true;
  }
  return DispatchKeyEvent(keypressEvent, &aCharMsg);
}

bool
NativeKey::HandleKeyUpMessage(bool* aEventDispatched) const
{
  MOZ_ASSERT(mMsg.message == WM_KEYUP || mMsg.message == WM_SYSKEYUP);

  if (aEventDispatched) {
    *aEventDispatched = false;
  }

  // Ignore [shift+]alt+space so the OS can handle it.
  if (mModKeyState.IsAlt() && !mModKeyState.IsControl() &&
      mVirtualKeyCode == VK_SPACE) {
    return false;
  }

  nsKeyEvent keyupEvent(true, NS_KEY_UP, mWidget);
  InitKeyEvent(keyupEvent, mModKeyState);
  if (aEventDispatched) {
    *aEventDispatched = true;
  }
  return DispatchKeyEvent(keyupEvent, &mMsg);
}

bool
NativeKey::NeedsToHandleWithoutFollowingCharMessages() const
{
  MOZ_ASSERT(mMsg.message == WM_KEYDOWN || mMsg.message == WM_SYSKEYDOWN);

  // Enter and backspace are always handled here to avoid for example the
  // confusion between ctrl-enter and ctrl-J.
  if (mDOMKeyCode == NS_VK_RETURN || mDOMKeyCode == NS_VK_BACK) {
    return true;
  }

  // If any modifier keys which may cause printable keys becoming non-printable
  // are not pressed, we don't need special handling for the key.
  if (!mModKeyState.IsControl() && !mModKeyState.IsAlt() &&
      !mModKeyState.IsWin()) {
    return false;
  }

  // If the key event causes dead key event, we don't need to dispatch keypress
  // event.
  if (mIsDeadKey) {
    return false;
  }

  // Even if the key is a printable key, it might cause non-printable character
  // input with modifier key(s).
  return IsPrintableKey();
}

const MSG&
NativeKey::RemoveFollowingCharMessage() const
{
  MOZ_ASSERT(IsFollowedByCharMessage());

  if (mIsFakeCharMsg) {
    return mCharMsg;
  }

  MSG msg;
  if (!WinUtils::GetMessage(&msg, mMsg.hwnd,
                            mCharMsg.message, mCharMsg.message)) {
    MOZ_NOT_REACHED("We lost the following char message");
    return mCharMsg;
  }

  MOZ_ASSERT(mCharMsg.message == msg.message &&
             mCharMsg.wParam == msg.wParam &&
             mCharMsg.lParam == msg.lParam);

  return mCharMsg;
}

void
NativeKey::RemoveMessageAndDispatchPluginEvent(UINT aFirstMsg,
                                               UINT aLastMsg) const
{
  MSG msg;
  if (mIsFakeCharMsg) {
    if (aFirstMsg > WM_CHAR || aLastMsg < WM_CHAR) {
      return;
    }
    msg = mCharMsg;
  } else {
    WinUtils::GetMessage(&msg, mMsg.hwnd, aFirstMsg, aLastMsg);
  }
  mWidget->DispatchPluginEvent(msg);
}

bool
NativeKey::DispatchKeyPressEventsAndDiscardsCharMessages(
                        const EventFlags& aExtraFlags) const
{
  MOZ_ASSERT(mMsg.message == WM_KEYDOWN || mMsg.message == WM_SYSKEYDOWN);

  // Remove a possible WM_CHAR or WM_SYSCHAR messages from the message queue.
  // They can be more than one because of:
  //  * Dead-keys not pairing with base character
  //  * Some keyboard layouts may map up to 4 characters to the single key
  bool anyCharMessagesRemoved = false;

  if (mIsFakeCharMsg) {
    RemoveMessageAndDispatchPluginEvent(WM_KEYFIRST, WM_KEYLAST);
    anyCharMessagesRemoved = true;
  } else {
    MSG msg;
    bool gotMsg =
      WinUtils::PeekMessage(&msg, mMsg.hwnd, WM_KEYFIRST, WM_KEYLAST,
                            PM_NOREMOVE | PM_NOYIELD);
    while (gotMsg && (msg.message == WM_CHAR || msg.message == WM_SYSCHAR)) {
      RemoveMessageAndDispatchPluginEvent(WM_KEYFIRST, WM_KEYLAST);
      anyCharMessagesRemoved = true;
      gotMsg = WinUtils::PeekMessage(&msg, mMsg.hwnd, WM_KEYFIRST, WM_KEYLAST,
                                     PM_NOREMOVE | PM_NOYIELD);
    }
  }

  if (!anyCharMessagesRemoved &&
      mDOMKeyCode == NS_VK_BACK && IsIMEDoingKakuteiUndo()) {
    MOZ_ASSERT(!mIsFakeCharMsg);
    RemoveMessageAndDispatchPluginEvent(WM_CHAR, WM_CHAR);
  }

  return DispatchKeyPressEventsWithKeyboardLayout(aExtraFlags);
}

bool
NativeKey::DispatchKeyPressEventsWithKeyboardLayout(
                        const EventFlags& aExtraFlags) const
{
  MOZ_ASSERT(mMsg.message == WM_KEYDOWN || mMsg.message == WM_SYSKEYDOWN);
  MOZ_ASSERT(!mIsDeadKey);

  KeyboardLayout* keyboardLayout = KeyboardLayout::GetInstance();

  UniCharsAndModifiers inputtingChars(mCommittedCharsAndModifiers);
  UniCharsAndModifiers shiftedChars;
  UniCharsAndModifiers unshiftedChars;
  uint32_t shiftedLatinChar = 0;
  uint32_t unshiftedLatinChar = 0;

  if (!KeyboardLayout::IsPrintableCharKey(mVirtualKeyCode)) {
    inputtingChars.Clear();
  }

  if (mModKeyState.IsControl() ^ mModKeyState.IsAlt()) {
    ModifierKeyState capsLockState(
                       mModKeyState.GetModifiers() & MODIFIER_CAPSLOCK);

    unshiftedChars =
      keyboardLayout->GetUniCharsAndModifiers(mVirtualKeyCode, capsLockState);
    capsLockState.Set(MODIFIER_SHIFT);
    shiftedChars =
      keyboardLayout->GetUniCharsAndModifiers(mVirtualKeyCode, capsLockState);

    // The current keyboard cannot input alphabets or numerics,
    // we should append them for Shortcut/Access keys.
    // E.g., for Cyrillic keyboard layout.
    capsLockState.Unset(MODIFIER_SHIFT);
    WidgetUtils::GetLatinCharCodeForKeyCode(mDOMKeyCode,
                                            capsLockState.GetModifiers(),
                                            &unshiftedLatinChar,
                                            &shiftedLatinChar);

    // If the shiftedLatinChar isn't 0, the key code is NS_VK_[A-Z].
    if (shiftedLatinChar) {
      // If the produced characters of the key on current keyboard layout
      // are same as computed Latin characters, we shouldn't append the
      // Latin characters to alternativeCharCode.
      if (unshiftedLatinChar == unshiftedChars.mChars[0] &&
          shiftedLatinChar == shiftedChars.mChars[0]) {
        shiftedLatinChar = unshiftedLatinChar = 0;
      }
    } else if (unshiftedLatinChar) {
      // If the shiftedLatinChar is 0, the keyCode doesn't produce
      // alphabet character.  At that time, the character may be produced
      // with Shift key.  E.g., on French keyboard layout, NS_VK_PERCENT
      // key produces LATIN SMALL LETTER U WITH GRAVE (U+00F9) without
      // Shift key but with Shift key, it produces '%'.
      // If the unshiftedLatinChar is produced by the key on current
      // keyboard layout, we shouldn't append it to alternativeCharCode.
      if (unshiftedLatinChar == unshiftedChars.mChars[0] ||
          unshiftedLatinChar == shiftedChars.mChars[0]) {
        unshiftedLatinChar = 0;
      }
    }

    // If the charCode is not ASCII character, we should replace the
    // charCode with ASCII character only when Ctrl is pressed.
    // But don't replace the charCode when the charCode is not same as
    // unmodified characters. In such case, Ctrl is sometimes used for a
    // part of character inputting key combination like Shift.
    if (mModKeyState.IsControl()) {
      uint32_t ch =
        mModKeyState.IsShift() ? shiftedLatinChar : unshiftedLatinChar;
      if (ch &&
          (!inputtingChars.mLength ||
           inputtingChars.UniCharsCaseInsensitiveEqual(
             mModKeyState.IsShift() ? shiftedChars : unshiftedChars))) {
        inputtingChars.Clear();
        inputtingChars.Append(ch, mModKeyState.GetModifiers());
      }
    }
  }

  if (inputtingChars.IsEmpty() &&
      shiftedChars.IsEmpty() && unshiftedChars.IsEmpty()) {
    nsKeyEvent keypressEvent(true, NS_KEY_PRESS, mWidget);
    keypressEvent.mFlags.Union(aExtraFlags);
    keypressEvent.keyCode = mDOMKeyCode;
    InitKeyEvent(keypressEvent, mModKeyState);
    return (DispatchKeyEvent(keypressEvent) || aExtraFlags.mDefaultPrevented);
  }

  uint32_t longestLength =
    std::max(inputtingChars.mLength,
             std::max(shiftedChars.mLength, unshiftedChars.mLength));
  uint32_t skipUniChars = longestLength - inputtingChars.mLength;
  uint32_t skipShiftedChars = longestLength - shiftedChars.mLength;
  uint32_t skipUnshiftedChars = longestLength - unshiftedChars.mLength;
  UINT keyCode = !inputtingChars.mLength ? mDOMKeyCode : 0;
  bool defaultPrevented = aExtraFlags.mDefaultPrevented;
  for (uint32_t cnt = 0; cnt < longestLength; cnt++) {
    uint16_t uniChar, shiftedChar, unshiftedChar;
    uniChar = shiftedChar = unshiftedChar = 0;
    ModifierKeyState modKeyState(mModKeyState);
    if (skipUniChars <= cnt) {
      if (cnt - skipUniChars  < inputtingChars.mLength) {
        // If key in combination with Alt and/or Ctrl produces a different
        // character than without them then do not report these flags
        // because it is separate keyboard layout shift state. If dead-key
        // and base character does not produce a valid composite character
        // then both produced dead-key character and following base
        // character may have different modifier flags, too.
        modKeyState.Unset(MODIFIER_SHIFT | MODIFIER_CONTROL | MODIFIER_ALT |
                          MODIFIER_ALTGRAPH | MODIFIER_CAPSLOCK);
        modKeyState.Set(inputtingChars.mModifiers[cnt - skipUniChars]);
      }
      uniChar = inputtingChars.mChars[cnt - skipUniChars];
    }
    if (skipShiftedChars <= cnt)
      shiftedChar = shiftedChars.mChars[cnt - skipShiftedChars];
    if (skipUnshiftedChars <= cnt)
      unshiftedChar = unshiftedChars.mChars[cnt - skipUnshiftedChars];
    nsAutoTArray<nsAlternativeCharCode, 5> altArray;

    if (shiftedChar || unshiftedChar) {
      nsAlternativeCharCode chars(unshiftedChar, shiftedChar);
      altArray.AppendElement(chars);
    }
    if (cnt == longestLength - 1) {
      if (unshiftedLatinChar || shiftedLatinChar) {
        nsAlternativeCharCode chars(unshiftedLatinChar, shiftedLatinChar);
        altArray.AppendElement(chars);
      }

      // Typically, following virtual keycodes are used for a key which can
      // input the character.  However, these keycodes are also used for
      // other keys on some keyboard layout.  E.g., in spite of Shift+'1'
      // inputs '+' on Thai keyboard layout, a key which is at '=/+'
      // key on ANSI keyboard layout is VK_OEM_PLUS.  Native applications
      // handle it as '+' key if Ctrl key is pressed.
      PRUnichar charForOEMKeyCode = 0;
      switch (mVirtualKeyCode) {
        case VK_OEM_PLUS:   charForOEMKeyCode = '+'; break;
        case VK_OEM_COMMA:  charForOEMKeyCode = ','; break;
        case VK_OEM_MINUS:  charForOEMKeyCode = '-'; break;
        case VK_OEM_PERIOD: charForOEMKeyCode = '.'; break;
      }
      if (charForOEMKeyCode &&
          charForOEMKeyCode != unshiftedChars.mChars[0] &&
          charForOEMKeyCode != shiftedChars.mChars[0] &&
          charForOEMKeyCode != unshiftedLatinChar &&
          charForOEMKeyCode != shiftedLatinChar) {
        nsAlternativeCharCode OEMChars(charForOEMKeyCode, charForOEMKeyCode);
        altArray.AppendElement(OEMChars);
      }
    }

    nsKeyEvent keypressEvent(true, NS_KEY_PRESS, mWidget);
    keypressEvent.mFlags.Union(aExtraFlags);
    keypressEvent.charCode = uniChar;
    keypressEvent.alternativeCharCodes.AppendElements(altArray);
    InitKeyEvent(keypressEvent, modKeyState);
    defaultPrevented = (DispatchKeyEvent(keypressEvent) || defaultPrevented);
  }

  return defaultPrevented;
}

bool
NativeKey::DispatchKeyPressEventForFollowingCharMessage(
                        const EventFlags& aExtraFlags) const
{
  MOZ_ASSERT(mMsg.message == WM_KEYDOWN || mMsg.message == WM_SYSKEYDOWN);

  const MSG& msg = RemoveFollowingCharMessage();
  if (mIsFakeCharMsg) {
    if (msg.message == WM_DEADCHAR) {
      return aExtraFlags.mDefaultPrevented;
    }
#ifdef DEBUG
    if (IsPrintableKey()) {
      nsPrintfCString log(
        "mOriginalVirtualKeyCode=0x%02X, mCommittedCharsAndModifiers={ "
        "mChars=[ 0x%04X, 0x%04X, 0x%04X, 0x%04X, 0x%04X ], mLength=%d }, "
        "wParam=0x%04X",
        mOriginalVirtualKeyCode, mCommittedCharsAndModifiers.mChars[0],
        mCommittedCharsAndModifiers.mChars[1],
        mCommittedCharsAndModifiers.mChars[2],
        mCommittedCharsAndModifiers.mChars[3],
        mCommittedCharsAndModifiers.mChars[4],
        mCommittedCharsAndModifiers.mLength, msg.wParam);
      if (mCommittedCharsAndModifiers.IsEmpty()) {
        log.Insert("length is zero: ", 0);
        NS_ERROR(log.get());
        NS_ABORT();
      } else if (mCommittedCharsAndModifiers.mChars[0] != msg.wParam) {
        log.Insert("character mismatch: ", 0);
        NS_ERROR(log.get());
        NS_ABORT();
      }
    }
#endif // #ifdef DEBUG
    return HandleCharMessage(msg, nullptr, &aExtraFlags);
  }

  // If prevent default set for keydown, do same for keypress
  if (msg.message == WM_DEADCHAR) {
    bool defaultPrevented = aExtraFlags.mDefaultPrevented;
    if (mWidget->PluginHasFocus()) {
      // We need to send the removed message to focused plug-in.
      defaultPrevented = mWidget->DispatchPluginEvent(msg) || defaultPrevented;
    }
    return defaultPrevented;
  }

  bool defaultPrevented = (HandleCharMessage(msg, nullptr, &aExtraFlags) ||
                           aExtraFlags.mDefaultPrevented);
  // If a syschar keypress wasn't processed, Windows may want to
  // handle it to activate a native menu.
  if (!defaultPrevented && msg.message == WM_SYSCHAR) {
    ::DefWindowProcW(msg.hwnd, msg.message, msg.wParam, msg.lParam);
  }
  return defaultPrevented;
}

/*****************************************************************************
 * mozilla::widget::KeyboardLayout
 *****************************************************************************/

KeyboardLayout* KeyboardLayout::sInstance = nullptr;
nsIIdleServiceInternal* KeyboardLayout::sIdleService = nullptr;

// static
KeyboardLayout*
KeyboardLayout::GetInstance()
{
  if (!sInstance) {
    sInstance = new KeyboardLayout();
    nsCOMPtr<nsIIdleServiceInternal> idleService =
      do_GetService("@mozilla.org/widget/idleservice;1");
    // The refcount will be decreased at shutting down.
    sIdleService = idleService.forget().get();
  }
  return sInstance;
}

// static
void
KeyboardLayout::Shutdown()
{
  delete sInstance;
  sInstance = nullptr;
  NS_IF_RELEASE(sIdleService);
}

// static
void
KeyboardLayout::NotifyIdleServiceOfUserActivity()
{
  sIdleService->ResetIdleTimeOut(0);
}

KeyboardLayout::KeyboardLayout() :
  mKeyboardLayout(0), mIsOverridden(false),
  mIsPendingToRestoreKeyboardLayout(false)
{
  mDeadKeyTableListHead = nullptr;

  // NOTE: LoadLayout() should be called via OnLayoutChange().
}

KeyboardLayout::~KeyboardLayout()
{
  ReleaseDeadKeyTables();
}

bool
KeyboardLayout::IsPrintableCharKey(uint8_t aVirtualKey)
{
  return GetKeyIndex(aVirtualKey) >= 0;
}

WORD
KeyboardLayout::ComputeScanCodeForVirtualKeyCode(uint8_t aVirtualKeyCode) const
{
  return static_cast<WORD>(
           ::MapVirtualKeyEx(aVirtualKeyCode, MAPVK_VK_TO_VSC, GetLayout()));
}

bool
KeyboardLayout::IsDeadKey(uint8_t aVirtualKey,
                          const ModifierKeyState& aModKeyState) const
{
  int32_t virtualKeyIndex = GetKeyIndex(aVirtualKey);
  if (virtualKeyIndex < 0) {
    return false;
  }

  return mVirtualKeys[virtualKeyIndex].IsDeadKey(
           VirtualKey::ModifiersToShiftState(aModKeyState.GetModifiers()));
}

void
KeyboardLayout::InitNativeKey(NativeKey& aNativeKey,
                              const ModifierKeyState& aModKeyState)
{
  if (mIsPendingToRestoreKeyboardLayout) {
    LoadLayout(::GetKeyboardLayout(0));
  }

  uint8_t virtualKey = aNativeKey.GetOriginalVirtualKeyCode();
  int32_t virtualKeyIndex = GetKeyIndex(virtualKey);

  if (virtualKeyIndex < 0) {
    // Does not produce any printable characters, but still preserves the
    // dead-key state.
    return;
  }

  bool isKeyDown = aNativeKey.IsKeyDownMessage();
  uint8_t shiftState =
    VirtualKey::ModifiersToShiftState(aModKeyState.GetModifiers());

  if (mVirtualKeys[virtualKeyIndex].IsDeadKey(shiftState)) {
    if ((isKeyDown && mActiveDeadKey < 0) ||
        (!isKeyDown && mActiveDeadKey == virtualKey)) {
      //  First dead key event doesn't generate characters.
      if (isKeyDown) {
        // Dead-key state activated at keydown.
        mActiveDeadKey = virtualKey;
        mDeadKeyShiftState = shiftState;
      }
      UniCharsAndModifiers deadChars =
        mVirtualKeys[virtualKeyIndex].GetNativeUniChars(shiftState);
      NS_ASSERTION(deadChars.mLength == 1,
                   "dead key must generate only one character");
      aNativeKey.mKeyNameIndex =
        WidgetUtils::GetDeadKeyNameIndex(deadChars.mChars[0]);
      return;
    }

    // Dead-key followed by another dead-key. Reset dead-key state and
    // return both dead-key characters.
    int32_t activeDeadKeyIndex = GetKeyIndex(mActiveDeadKey);
    UniCharsAndModifiers prevDeadChars =
      mVirtualKeys[activeDeadKeyIndex].GetUniChars(mDeadKeyShiftState);
    UniCharsAndModifiers newChars =
      mVirtualKeys[virtualKeyIndex].GetUniChars(shiftState);
    // But keypress events should be fired for each committed character.
    aNativeKey.mCommittedCharsAndModifiers = prevDeadChars + newChars;
    if (isKeyDown) {
      DeactivateDeadKeyState();
    }
    return;
  }

  UniCharsAndModifiers baseChars =
    mVirtualKeys[virtualKeyIndex].GetUniChars(shiftState);
  if (mActiveDeadKey < 0) {
    // No dead-keys are active. Just return the produced characters.
    aNativeKey.mCommittedCharsAndModifiers = baseChars;
    return;
  }

  // Dead-key was active. See if pressed base character does produce
  // valid composite character.
  int32_t activeDeadKeyIndex = GetKeyIndex(mActiveDeadKey);
  PRUnichar compositeChar = (baseChars.mLength == 1 && baseChars.mChars[0]) ?
    mVirtualKeys[activeDeadKeyIndex].GetCompositeChar(mDeadKeyShiftState,
                                                      baseChars.mChars[0]) : 0;
  if (compositeChar) {
    // Active dead-key and base character does produce exactly one
    // composite character.
    aNativeKey.mCommittedCharsAndModifiers.Append(compositeChar,
                                                  baseChars.mModifiers[0]);
    if (isKeyDown) {
      DeactivateDeadKeyState();
    }
    return;
  }

  // There is no valid dead-key and base character combination.
  // Return dead-key character followed by base character.
  UniCharsAndModifiers deadChars =
    mVirtualKeys[activeDeadKeyIndex].GetUniChars(mDeadKeyShiftState);
  // But keypress events should be fired for each committed character.
  aNativeKey.mCommittedCharsAndModifiers = deadChars + baseChars;
  if (isKeyDown) {
    DeactivateDeadKeyState();
  }

  return;
}

UniCharsAndModifiers
KeyboardLayout::GetUniCharsAndModifiers(
                  uint8_t aVirtualKey,
                  const ModifierKeyState& aModKeyState) const
{
  UniCharsAndModifiers result;
  int32_t key = GetKeyIndex(aVirtualKey);
  if (key < 0) {
    return result;
  }
  return mVirtualKeys[key].
    GetUniChars(VirtualKey::ModifiersToShiftState(aModKeyState.GetModifiers()));
}

void
KeyboardLayout::LoadLayout(HKL aLayout)
{
  mIsPendingToRestoreKeyboardLayout = false;

  if (mKeyboardLayout == aLayout) {
    return;
  }

  mKeyboardLayout = aLayout;

  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));

  BYTE originalKbdState[256];
  // Bitfield with all shift states that have at least one dead-key.
  uint16_t shiftStatesWithDeadKeys = 0;
  // Bitfield with all shift states that produce any possible dead-key base
  // characters.
  uint16_t shiftStatesWithBaseChars = 0;

  mActiveDeadKey = -1;

  ReleaseDeadKeyTables();

  ::GetKeyboardState(originalKbdState);

  // For each shift state gather all printable characters that are produced
  // for normal case when no any dead-key is active.

  for (VirtualKey::ShiftState shiftState = 0; shiftState < 16; shiftState++) {
    VirtualKey::FillKbdState(kbdState, shiftState);
    for (uint32_t virtualKey = 0; virtualKey < 256; virtualKey++) {
      int32_t vki = GetKeyIndex(virtualKey);
      if (vki < 0) {
        continue;
      }
      NS_ASSERTION(uint32_t(vki) < ArrayLength(mVirtualKeys), "invalid index");
      PRUnichar uniChars[5];
      int32_t ret =
        ::ToUnicodeEx(virtualKey, 0, kbdState, (LPWSTR)uniChars,
                      ArrayLength(uniChars), 0, mKeyboardLayout);
      // dead-key
      if (ret < 0) {
        shiftStatesWithDeadKeys |= (1 << shiftState);
        // Repeat dead-key to deactivate it and get its character
        // representation.
        PRUnichar deadChar[2];
        ret = ::ToUnicodeEx(virtualKey, 0, kbdState, (LPWSTR)deadChar,
                            ArrayLength(deadChar), 0, mKeyboardLayout);
        NS_ASSERTION(ret == 2, "Expecting twice repeated dead-key character");
        mVirtualKeys[vki].SetDeadChar(shiftState, deadChar[0]);
      } else {
        if (ret == 1) {
          // dead-key can pair only with exactly one base character.
          shiftStatesWithBaseChars |= (1 << shiftState);
        }
        mVirtualKeys[vki].SetNormalChars(shiftState, uniChars, ret);
      }
    }
  }

  // Now process each dead-key to find all its base characters and resulting
  // composite characters.
  for (VirtualKey::ShiftState shiftState = 0; shiftState < 16; shiftState++) {
    if (!(shiftStatesWithDeadKeys & (1 << shiftState))) {
      continue;
    }

    VirtualKey::FillKbdState(kbdState, shiftState);

    for (uint32_t virtualKey = 0; virtualKey < 256; virtualKey++) {
      int32_t vki = GetKeyIndex(virtualKey);
      if (vki >= 0 && mVirtualKeys[vki].IsDeadKey(shiftState)) {
        DeadKeyEntry deadKeyArray[256];
        int32_t n = GetDeadKeyCombinations(virtualKey, kbdState,
                                           shiftStatesWithBaseChars,
                                           deadKeyArray,
                                           ArrayLength(deadKeyArray));
        const DeadKeyTable* dkt =
          mVirtualKeys[vki].MatchingDeadKeyTable(deadKeyArray, n);
        if (!dkt) {
          dkt = AddDeadKeyTable(deadKeyArray, n);
        }
        mVirtualKeys[vki].AttachDeadKeyTable(shiftState, dkt);
      }
    }
  }

  ::SetKeyboardState(originalKbdState);
}

inline int32_t
KeyboardLayout::GetKeyIndex(uint8_t aVirtualKey)
{
// Currently these 68 (NS_NUM_OF_KEYS) virtual keys are assumed
// to produce visible representation:
// 0x20 - VK_SPACE          ' '
// 0x30..0x39               '0'..'9'
// 0x41..0x5A               'A'..'Z'
// 0x60..0x69               '0'..'9' on numpad
// 0x6A - VK_MULTIPLY       '*' on numpad
// 0x6B - VK_ADD            '+' on numpad
// 0x6D - VK_SUBTRACT       '-' on numpad
// 0x6E - VK_DECIMAL        '.' on numpad
// 0x6F - VK_DIVIDE         '/' on numpad
// 0x6E - VK_DECIMAL        '.'
// 0xBA - VK_OEM_1          ';:' for US
// 0xBB - VK_OEM_PLUS       '+' any country
// 0xBC - VK_OEM_COMMA      ',' any country
// 0xBD - VK_OEM_MINUS      '-' any country
// 0xBE - VK_OEM_PERIOD     '.' any country
// 0xBF - VK_OEM_2          '/?' for US
// 0xC0 - VK_OEM_3          '`~' for US
// 0xDB - VK_OEM_4          '[{' for US
// 0xDC - VK_OEM_5          '\|' for US
// 0xDD - VK_OEM_6          ']}' for US
// 0xDE - VK_OEM_7          ''"' for US
// 0xDF - VK_OEM_8
// 0xE1 - no name
// 0xE2 - VK_OEM_102        '\_' for JIS
// 0xE3 - no name
// 0xE4 - no name

  static const int8_t xlat[256] =
  {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
  //-----------------------------------------------------------------------
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 00
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 10
     0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 20
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, -1, -1, -1, -1, -1, -1,   // 30
    -1, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,   // 40
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, -1, -1, -1, -1, -1,   // 50
    37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, -1, 49, 50, 51,   // 60
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 70
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 80
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 90
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // A0
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 52, 53, 54, 55, 56, 57,   // B0
    58, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // C0
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 59, 60, 61, 62, 63,   // D0
    -1, 64, 65, 66, 67, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // E0
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1    // F0
  };

  return xlat[aVirtualKey];
}

int
KeyboardLayout::CompareDeadKeyEntries(const void* aArg1,
                                      const void* aArg2,
                                      void*)
{
  const DeadKeyEntry* arg1 = static_cast<const DeadKeyEntry*>(aArg1);
  const DeadKeyEntry* arg2 = static_cast<const DeadKeyEntry*>(aArg2);

  return arg1->BaseChar - arg2->BaseChar;
}

const DeadKeyTable*
KeyboardLayout::AddDeadKeyTable(const DeadKeyEntry* aDeadKeyArray,
                                uint32_t aEntries)
{
  DeadKeyTableListEntry* next = mDeadKeyTableListHead;

  const size_t bytes = offsetof(DeadKeyTableListEntry, data) +
    DeadKeyTable::SizeInBytes(aEntries);
  uint8_t* p = new uint8_t[bytes];

  mDeadKeyTableListHead = reinterpret_cast<DeadKeyTableListEntry*>(p);
  mDeadKeyTableListHead->next = next;

  DeadKeyTable* dkt =
    reinterpret_cast<DeadKeyTable*>(mDeadKeyTableListHead->data);

  dkt->Init(aDeadKeyArray, aEntries);

  return dkt;
}

void
KeyboardLayout::ReleaseDeadKeyTables()
{
  while (mDeadKeyTableListHead) {
    uint8_t* p = reinterpret_cast<uint8_t*>(mDeadKeyTableListHead);
    mDeadKeyTableListHead = mDeadKeyTableListHead->next;

    delete [] p;
  }
}

bool
KeyboardLayout::EnsureDeadKeyActive(bool aIsActive,
                                    uint8_t aDeadKey,
                                    const PBYTE aDeadKeyKbdState)
{
  int32_t ret;
  do {
    PRUnichar dummyChars[5];
    ret = ::ToUnicodeEx(aDeadKey, 0, (PBYTE)aDeadKeyKbdState,
                        (LPWSTR)dummyChars, ArrayLength(dummyChars), 0,
                        mKeyboardLayout);
    // returned values:
    // <0 - Dead key state is active. The keyboard driver will wait for next
    //      character.
    //  1 - Previous pressed key was a valid base character that produced
    //      exactly one composite character.
    // >1 - Previous pressed key does not produce any composite characters.
    //      Return dead-key character followed by base character(s).
  } while ((ret < 0) != aIsActive);

  return (ret < 0);
}

void
KeyboardLayout::DeactivateDeadKeyState()
{
  if (mActiveDeadKey < 0) {
    return;
  }

  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));

  VirtualKey::FillKbdState(kbdState, mDeadKeyShiftState);

  EnsureDeadKeyActive(false, mActiveDeadKey, kbdState);
  mActiveDeadKey = -1;
}

bool
KeyboardLayout::AddDeadKeyEntry(PRUnichar aBaseChar,
                                PRUnichar aCompositeChar,
                                DeadKeyEntry* aDeadKeyArray,
                                uint32_t aEntries)
{
  for (uint32_t index = 0; index < aEntries; index++) {
    if (aDeadKeyArray[index].BaseChar == aBaseChar) {
      return false;
    }
  }

  aDeadKeyArray[aEntries].BaseChar = aBaseChar;
  aDeadKeyArray[aEntries].CompositeChar = aCompositeChar;

  return true;
}

uint32_t
KeyboardLayout::GetDeadKeyCombinations(uint8_t aDeadKey,
                                       const PBYTE aDeadKeyKbdState,
                                       uint16_t aShiftStatesWithBaseChars,
                                       DeadKeyEntry* aDeadKeyArray,
                                       uint32_t aMaxEntries)
{
  bool deadKeyActive = false;
  uint32_t entries = 0;
  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));

  for (uint32_t shiftState = 0; shiftState < 16; shiftState++) {
    if (!(aShiftStatesWithBaseChars & (1 << shiftState))) {
      continue;
    }

    VirtualKey::FillKbdState(kbdState, shiftState);

    for (uint32_t virtualKey = 0; virtualKey < 256; virtualKey++) {
      int32_t vki = GetKeyIndex(virtualKey);
      // Dead-key can pair only with such key that produces exactly one base
      // character.
      if (vki >= 0 &&
          mVirtualKeys[vki].GetNativeUniChars(shiftState).mLength == 1) {
        // Ensure dead-key is in active state, when it swallows entered
        // character and waits for the next pressed key.
        if (!deadKeyActive) {
          deadKeyActive = EnsureDeadKeyActive(true, aDeadKey,
                                              aDeadKeyKbdState);
        }

        // Depending on the character the followed the dead-key, the keyboard
        // driver can produce one composite character, or a dead-key character
        // followed by a second character.
        PRUnichar compositeChars[5];
        int32_t ret =
          ::ToUnicodeEx(virtualKey, 0, kbdState, (LPWSTR)compositeChars,
                        ArrayLength(compositeChars), 0, mKeyboardLayout);
        switch (ret) {
          case 0:
            // This key combination does not produce any characters. The
            // dead-key is still in active state.
            break;
          case 1: {
            // Exactly one composite character produced. Now, when dead-key
            // is not active, repeat the last character one more time to
            // determine the base character.
            PRUnichar baseChars[5];
            ret = ::ToUnicodeEx(virtualKey, 0, kbdState, (LPWSTR)baseChars,
                                ArrayLength(baseChars), 0, mKeyboardLayout);
            NS_ASSERTION(ret == 1, "One base character expected");
            if (ret == 1 && entries < aMaxEntries &&
                AddDeadKeyEntry(baseChars[0], compositeChars[0],
                                aDeadKeyArray, entries)) {
              entries++;
            }
            deadKeyActive = false;
            break;
          }
          default:
            // 1. Unexpected dead-key. Dead-key chaining is not supported.
            // 2. More than one character generated. This is not a valid
            //    dead-key and base character combination.
            deadKeyActive = false;
            break;
        }
      }
    }
  }

  if (deadKeyActive) {
    deadKeyActive = EnsureDeadKeyActive(false, aDeadKey, aDeadKeyKbdState);
  }

  NS_QuickSort(aDeadKeyArray, entries, sizeof(DeadKeyEntry),
               CompareDeadKeyEntries, nullptr);
  return entries;
}

uint32_t
KeyboardLayout::ConvertNativeKeyCodeToDOMKeyCode(UINT aNativeKeyCode) const
{
  // Alphabet or Numeric or Numpad or Function keys
  if ((aNativeKeyCode >= 0x30 && aNativeKeyCode <= 0x39) ||
      (aNativeKeyCode >= 0x41 && aNativeKeyCode <= 0x5A) ||
      (aNativeKeyCode >= 0x60 && aNativeKeyCode <= 0x87)) {
    return static_cast<uint32_t>(aNativeKeyCode);
  }
  switch (aNativeKeyCode) {
    // Following keycodes are same as our DOM keycodes
    case VK_CANCEL:
    case VK_BACK:
    case VK_TAB:
    case VK_CLEAR:
    case VK_RETURN:
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU: // Alt
    case VK_PAUSE:
    case VK_CAPITAL: // CAPS LOCK
    case VK_KANA: // same as VK_HANGUL
    case VK_JUNJA:
    case VK_FINAL:
    case VK_HANJA: // same as VK_KANJI
    case VK_ESCAPE:
    case VK_CONVERT:
    case VK_NONCONVERT:
    case VK_ACCEPT:
    case VK_MODECHANGE:
    case VK_SPACE:
    case VK_PRIOR: // PAGE UP
    case VK_NEXT: // PAGE DOWN
    case VK_END:
    case VK_HOME:
    case VK_LEFT:
    case VK_UP:
    case VK_RIGHT:
    case VK_DOWN:
    case VK_SELECT:
    case VK_PRINT:
    case VK_EXECUTE:
    case VK_SNAPSHOT:
    case VK_INSERT:
    case VK_DELETE:
    case VK_APPS: // Context Menu
    case VK_SLEEP:
    case VK_NUMLOCK:
    case VK_SCROLL: // SCROLL LOCK
    case VK_ATTN: // Attension key of IBM midrange computers, e.g., AS/400
    case VK_CRSEL: // Cursor Selection
    case VK_EXSEL: // Extend Selection
    case VK_EREOF: // Erase EOF key of IBM 3270 keyboard layout
    case VK_PLAY:
    case VK_ZOOM:
    case VK_PA1: // PA1 key of IBM 3270 keyboard layout
      return uint32_t(aNativeKeyCode);

    case VK_HELP:
      return NS_VK_HELP;

    // Windows key should be mapped to a Win keycode
    // They should be able to be distinguished by DOM3 KeyboardEvent.location
    case VK_LWIN:
    case VK_RWIN:
      return NS_VK_WIN;

    case VK_VOLUME_MUTE:
      return NS_VK_VOLUME_MUTE;
    case VK_VOLUME_DOWN:
      return NS_VK_VOLUME_DOWN;
    case VK_VOLUME_UP:
      return NS_VK_VOLUME_UP;

    // Following keycodes are not defined in our DOM keycodes.
    case VK_BROWSER_BACK:
    case VK_BROWSER_FORWARD:
    case VK_BROWSER_REFRESH:
    case VK_BROWSER_STOP:
    case VK_BROWSER_SEARCH:
    case VK_BROWSER_FAVORITES:
    case VK_BROWSER_HOME:
    case VK_MEDIA_NEXT_TRACK:
    case VK_MEDIA_STOP:
    case VK_MEDIA_PLAY_PAUSE:
    case VK_LAUNCH_MAIL:
    case VK_LAUNCH_MEDIA_SELECT:
    case VK_LAUNCH_APP1:
    case VK_LAUNCH_APP2:
      return 0;

    // Following OEM specific virtual keycodes should pass through DOM keyCode
    // for compatibility with the other browsers on Windows.

    // Following OEM specific virtual keycodes are defined for Fujitsu/OASYS.
    case VK_OEM_FJ_JISHO:
    case VK_OEM_FJ_MASSHOU:
    case VK_OEM_FJ_TOUROKU:
    case VK_OEM_FJ_LOYA:
    case VK_OEM_FJ_ROYA:
    // Not sure what means "ICO".
    case VK_ICO_HELP:
    case VK_ICO_00:
    case VK_ICO_CLEAR:
    // Following OEM specific virtual keycodes are defined for Nokia/Ericsson.
    case VK_OEM_RESET:
    case VK_OEM_JUMP:
    case VK_OEM_PA1:
    case VK_OEM_PA2:
    case VK_OEM_PA3:
    case VK_OEM_WSCTRL:
    case VK_OEM_CUSEL:
    case VK_OEM_ATTN:
    case VK_OEM_FINISH:
    case VK_OEM_COPY:
    case VK_OEM_AUTO:
    case VK_OEM_ENLW:
    case VK_OEM_BACKTAB:
    // VK_OEM_CLEAR is defined as not OEM specific, but let's pass though
    // DOM keyCode like other OEM specific virtual keycodes.
    case VK_OEM_CLEAR:
      return uint32_t(aNativeKeyCode);

    // 0xE1 is an OEM specific virtual keycode. However, the value is already
    // used in our DOM keyCode for AltGr on Linux. So, this virtual keycode
    // cannot pass through DOM keyCode.
    case 0xE1:
      return 0;

    // Following keycodes are OEM keys which are keycodes for non-alphabet and
    // non-numeric keys, we should compute each keycode of them from unshifted
    // character which is inputted by each key.  But if the unshifted character
    // is not an ASCII character but shifted character is an ASCII character,
    // we should refer it.
    case VK_OEM_1:
    case VK_OEM_PLUS:
    case VK_OEM_COMMA:
    case VK_OEM_MINUS:
    case VK_OEM_PERIOD:
    case VK_OEM_2:
    case VK_OEM_3:
    case VK_OEM_4:
    case VK_OEM_5:
    case VK_OEM_6:
    case VK_OEM_7:
    case VK_OEM_8:
    case VK_OEM_102:
    {
      NS_ASSERTION(IsPrintableCharKey(aNativeKeyCode),
                   "The key must be printable");
      ModifierKeyState modKeyState(0);
      UniCharsAndModifiers uniChars =
        GetUniCharsAndModifiers(aNativeKeyCode, modKeyState);
      if (uniChars.mLength != 1 ||
          uniChars.mChars[0] < ' ' || uniChars.mChars[0] > 0x7F) {
        modKeyState.Set(MODIFIER_SHIFT);
        uniChars = GetUniCharsAndModifiers(aNativeKeyCode, modKeyState);
        if (uniChars.mLength != 1 ||
            uniChars.mChars[0] < ' ' || uniChars.mChars[0] > 0x7F) {
          return 0;
        }
      }
      return WidgetUtils::ComputeKeyCodeFromChar(uniChars.mChars[0]);
    }

    // VK_PROCESSKEY means IME already consumed the key event.
    case VK_PROCESSKEY:
      return 0;
    // VK_PACKET is generated by SendInput() API, we don't need to
    // care this message as key event.
    case VK_PACKET:
      return 0;
  }
  NS_WARNING("Unknown key code comes, please check latest MSDN document,"
             " there may be some new keycodes we have not known.");
  return 0;
}

KeyNameIndex
KeyboardLayout::ConvertNativeKeyCodeToKeyNameIndex(uint8_t aVirtualKey) const
{
  switch (aVirtualKey) {

#define NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex) \
    case aNativeKey: return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

    default:
      return IsPrintableCharKey(aVirtualKey) ? KEY_NAME_INDEX_PrintableKey :
                                               KEY_NAME_INDEX_Unidentified;
  }
}

/*****************************************************************************
 * mozilla::widget::DeadKeyTable
 *****************************************************************************/

PRUnichar
DeadKeyTable::GetCompositeChar(PRUnichar aBaseChar) const
{
  // Dead-key table is sorted by BaseChar in ascending order.
  // Usually they are too small to use binary search.

  for (uint32_t index = 0; index < mEntries; index++) {
    if (mTable[index].BaseChar == aBaseChar) {
      return mTable[index].CompositeChar;
    }
    if (mTable[index].BaseChar > aBaseChar) {
      break;
    }
  }

  return 0;
}

/*****************************************************************************
 * mozilla::widget::RedirectedKeyDownMessage
 *****************************************************************************/

MSG RedirectedKeyDownMessageManager::sRedirectedKeyDownMsg;
bool RedirectedKeyDownMessageManager::sDefaultPreventedOfRedirectedMsg = false;

// static
bool
RedirectedKeyDownMessageManager::IsRedirectedMessage(const MSG& aMsg)
{
  return (aMsg.message == WM_KEYDOWN || aMsg.message == WM_SYSKEYDOWN) &&
         (sRedirectedKeyDownMsg.message == aMsg.message &&
          WinUtils::GetScanCode(sRedirectedKeyDownMsg.lParam) ==
            WinUtils::GetScanCode(aMsg.lParam));
}

// static
void
RedirectedKeyDownMessageManager::RemoveNextCharMessage(HWND aWnd)
{
  MSG msg;
  if (WinUtils::PeekMessage(&msg, aWnd, WM_KEYFIRST, WM_KEYLAST,
                            PM_NOREMOVE | PM_NOYIELD) &&
      (msg.message == WM_CHAR || msg.message == WM_SYSCHAR)) {
    WinUtils::GetMessage(&msg, aWnd, msg.message, msg.message);
  }
}

} // namespace widget
} // namespace mozilla

