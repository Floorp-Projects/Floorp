/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "KeyboardLayout.h"
#include "nsWindow.h"
#include "nsIMM32Handler.h"

#include "nsMemory.h"
#include "nsToolkit.h"
#include "nsQuickSort.h"
#include "nsAlgorithm.h"
#include "nsGUIEvent.h"
#include "WidgetUtils.h"
#include "WinUtils.h"

#include "nsIDOMKeyEvent.h"

#include <windows.h>
#include <winuser.h>

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

  PRUint16 mEntries;
  // KeyboardLayout::AddDeadKeyTable() will allocate as many entries as
  // required.  It is the only way to create new DeadKeyTable instances.
  DeadKeyEntry mTable[1];

  void Init(const DeadKeyEntry* aDeadKeyArray, PRUint32 aEntries)
  {
    mEntries = aEntries;
    memcpy(mTable, aDeadKeyArray, aEntries * sizeof(DeadKeyEntry));
  }

  static PRUint32 SizeInBytes(PRUint32 aEntries)
  {
    return offsetof(DeadKeyTable, mTable) + aEntries * sizeof(DeadKeyEntry);
  }

public:
  PRUint32 Entries() const
  {
    return mEntries;
  }

  bool IsEqual(const DeadKeyEntry* aDeadKeyArray, PRUint32 aEntries) const
  {
    return (mEntries == aEntries &&
            !memcmp(mTable, aDeadKeyArray,
                    aEntries * sizeof(DeadKeyEntry)));
  }

  PRUnichar GetCompositeChar(PRUnichar aBaseChar) const;
};



inline PRUnichar
VirtualKey::GetCompositeChar(PRUint8 aShiftState, PRUnichar aBaseChar) const
{
  return mShiftStates[aShiftState].DeadKey.Table->GetCompositeChar(aBaseChar);
}

const DeadKeyTable*
VirtualKey::MatchingDeadKeyTable(const DeadKeyEntry* aDeadKeyArray,
                                 PRUint32 aEntries) const
{
  if (!mIsDeadKey) {
    return nsnull;
  }

  for (PRUint32 shiftState = 0; shiftState < 16; shiftState++) {
    if (!IsDeadKey(shiftState)) {
      continue;
    }
    const DeadKeyTable* dkt = mShiftStates[shiftState].DeadKey.Table;
    if (dkt && dkt->IsEqual(aDeadKeyArray, aEntries)) {
      return dkt;
    }
  }

  return nsnull;
}

void
VirtualKey::SetNormalChars(PRUint8 aShiftState,
                           const PRUnichar* aChars,
                           PRUint32 aNumOfChars)
{
  NS_ASSERTION(aShiftState < ArrayLength(mShiftStates), "invalid index");

  SetDeadKey(aShiftState, false);

  for (PRUint32 index = 0; index < aNumOfChars; index++) {
    // Ignore legacy non-printable control characters
    mShiftStates[aShiftState].Normal.Chars[index] =
      (aChars[index] >= 0x20) ? aChars[index] : 0;
  }

  PRUint32 len = ArrayLength(mShiftStates[aShiftState].Normal.Chars);
  for (PRUint32 index = aNumOfChars; index < len; index++) {
    mShiftStates[aShiftState].Normal.Chars[index] = 0;
  }
}

void
VirtualKey::SetDeadChar(PRUint8 aShiftState, PRUnichar aDeadChar)
{
  NS_ASSERTION(aShiftState < ArrayLength(mShiftStates), "invalid index");

  SetDeadKey(aShiftState, true);

  mShiftStates[aShiftState].DeadKey.DeadChar = aDeadChar;
  mShiftStates[aShiftState].DeadKey.Table = nsnull;
}

PRUint32
VirtualKey::GetUniChars(PRUint8 aShiftState,
                        PRUnichar* aUniChars,
                        PRUint8* aFinalShiftState) const
{
  *aFinalShiftState = aShiftState;
  PRUint32 numOfChars = GetNativeUniChars(aShiftState, aUniChars);

  if (!(aShiftState & (eAlt | eCtrl))) {
    return numOfChars;
  }

  PRUnichar unshiftedChars[5];
  PRUint32 numOfUnshiftedChars =
    GetNativeUniChars(aShiftState & ~(eAlt | eCtrl), unshiftedChars);

  if (!numOfChars) {
    if (!numOfUnshiftedChars) {
      return 0;
    }
    memcpy(aUniChars, unshiftedChars,
           numOfUnshiftedChars * sizeof(PRUnichar));
    return numOfUnshiftedChars;
  }

  if ((aShiftState & (eAlt | eCtrl)) == (eAlt | eCtrl)) {
    // Even if the shifted chars and the unshifted chars are same, we
    // should consume the Alt key state and the Ctrl key state when
    // AltGr key is pressed. Because if we don't consume them, the input
    // events are ignored on nsEditor. (I.e., Users cannot input the
    // characters with this key combination.)
    *aFinalShiftState &= ~(eAlt | eCtrl);
  } else if (!(numOfChars == numOfUnshiftedChars &&
               !memcmp(aUniChars, unshiftedChars,
                       numOfChars * sizeof(PRUnichar)))) {
    // Otherwise, we should consume the Alt key state and the Ctrl key state
    // only when the shifted chars and unshifted chars are different.
    *aFinalShiftState &= ~(eAlt | eCtrl);
  }
  return numOfChars;
}


PRUint32
VirtualKey::GetNativeUniChars(PRUint8 aShiftState,
                              PRUnichar* aUniChars) const
{
  if (IsDeadKey(aShiftState)) {
    if (aUniChars) {
      aUniChars[0] = mShiftStates[aShiftState].DeadKey.DeadChar;
    }
    return 1;
  }

  PRUint32 index;
  PRUint32 len = ArrayLength(mShiftStates[aShiftState].Normal.Chars);
  for (index = 0;
       index < len && mShiftStates[aShiftState].Normal.Chars[index]; index++) {
    if (aUniChars) {
      aUniChars[index] = mShiftStates[aShiftState].Normal.Chars[index];
    }
  }
  return index;
}

NativeKey::NativeKey(const KeyboardLayout& aKeyboardLayout,
                     nsWindow* aWindow,
                     const MSG& aKeyOrCharMessage) :
  mDOMKeyCode(0), mVirtualKeyCode(0), mOriginalVirtualKeyCode(0)
{
  mScanCode = WinUtils::GetScanCode(aKeyOrCharMessage.lParam);
  mIsExtended = WinUtils::IsExtendedScanCode(aKeyOrCharMessage.lParam);
  switch (aKeyOrCharMessage.message) {
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
      mOriginalVirtualKeyCode = static_cast<PRUint8>(aKeyOrCharMessage.wParam);
      switch (aKeyOrCharMessage.wParam) {
        case VK_CONTROL:
        case VK_MENU:
        case VK_SHIFT:
          mVirtualKeyCode = static_cast<PRUint8>(
            ::MapVirtualKeyEx(GetScanCodeWithExtendedFlag(),
                              MAPVK_VSC_TO_VK_EX, aKeyboardLayout.GetLayout()));
          break;
        case VK_PROCESSKEY:
          mVirtualKeyCode = mOriginalVirtualKeyCode =
            static_cast<PRUint8>(
              ::ImmGetVirtualKey(aWindow->GetWindowHandle()));
          break;
        default:
          mVirtualKeyCode = mOriginalVirtualKeyCode;
          break;
      }
      break;
    case WM_CHAR:
    case WM_UNICHAR:
    case WM_SYSCHAR:
      // We cannot compute the virtual key code from WM_CHAR message on WinXP
      // and 
      if (mIsExtended &&
          WinUtils::GetWindowsVersion() < WinUtils::VISTA_VERSION) {
        break;
      }
      mVirtualKeyCode = mOriginalVirtualKeyCode = static_cast<PRUint8>(
        ::MapVirtualKeyEx(GetScanCodeWithExtendedFlag(),
                          MAPVK_VSC_TO_VK_EX, aKeyboardLayout.GetLayout()));
      break;
    default:
      MOZ_NOT_REACHED("Unsupported message");
      break;
  }

  if (!mVirtualKeyCode) {
    mVirtualKeyCode = mOriginalVirtualKeyCode;
  }

  mDOMKeyCode =
    aKeyboardLayout.ConvertNativeKeyCodeToDOMKeyCode(mOriginalVirtualKeyCode);
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

PRUint32
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

    default:
      return nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD;
  }
}


KeyboardLayout::KeyboardLayout() :
  mKeyboardLayout(0)
{
  mDeadKeyTableListHead = nsnull;

  // Note: Don't call LoadLayout from here. Because an instance of this class
  // can be static. In that case, we cannot use any services in LoadLayout,
  // e.g., pref service.
}

KeyboardLayout::~KeyboardLayout()
{
  ReleaseDeadKeyTables();
}

bool
KeyboardLayout::IsPrintableCharKey(PRUint8 aVirtualKey)
{
  return GetKeyIndex(aVirtualKey) >= 0;
}

bool
KeyboardLayout::IsNumpadKey(PRUint8 aVirtualKey)
{
  return VK_NUMPAD0 <= aVirtualKey && aVirtualKey <= VK_DIVIDE;
}

void
KeyboardLayout::OnKeyDown(PRUint8 aVirtualKey)
{
  mLastVirtualKeyIndex = GetKeyIndex(aVirtualKey);

  if (mLastVirtualKeyIndex < 0) {
    // Does not produce any printable characters, but still preserves the
    // dead-key state.
    mNumOfChars = 0;
    return;
  }

  BYTE kbdState[256];
  if (!::GetKeyboardState(kbdState)) {
    return;
  }

  mLastShiftState = GetShiftState(kbdState);

  if (mVirtualKeys[mLastVirtualKeyIndex].IsDeadKey(mLastShiftState)) {
    if (mActiveDeadKey < 0) {
      // Dead-key state activated. No characters generated.
      mActiveDeadKey = aVirtualKey;
      mDeadKeyShiftState = mLastShiftState;
      mNumOfChars = 0;
      return;
    }

    // Dead-key followed by another dead-key. Reset dead-key state and
    // return both dead-key characters.
    PRInt32 activeDeadKeyIndex = GetKeyIndex(mActiveDeadKey);
    mVirtualKeys[activeDeadKeyIndex].GetUniChars(mDeadKeyShiftState,
                                                 mChars, mShiftStates);
    mVirtualKeys[mLastVirtualKeyIndex].GetUniChars(mLastShiftState,
                                                   &mChars[1],
                                                   &mShiftStates[1]);
    mNumOfChars = 2;
    DeactivateDeadKeyState();
    return;
  }

  PRUint8 finalShiftState;
  PRUnichar uniChars[5];
  PRUint32 numOfBaseChars =
    mVirtualKeys[mLastVirtualKeyIndex].GetUniChars(mLastShiftState, uniChars,
                                                   &finalShiftState);
  if (mActiveDeadKey < 0) {
    // No dead-keys are active. Just return the produced characters.
    memcpy(mChars, uniChars, numOfBaseChars * sizeof(PRUnichar));
    memset(mShiftStates, finalShiftState, numOfBaseChars);
    mNumOfChars = numOfBaseChars;
    return;
  }

  // Dead-key was active. See if pressed base character does produce
  // valid composite character.
  PRInt32 activeDeadKeyIndex = GetKeyIndex(mActiveDeadKey);
  PRUnichar compositeChar = (numOfBaseChars == 1 && uniChars[0]) ?
    mVirtualKeys[activeDeadKeyIndex].GetCompositeChar(mDeadKeyShiftState,
                                                      uniChars[0]) : 0;
  if (compositeChar) {
    // Active dead-key and base character does produce exactly one
    // composite character.
    mChars[0] = compositeChar;
    mShiftStates[0] = finalShiftState;
    mNumOfChars = 1;
  } else {
    // There is no valid dead-key and base character combination.
    // Return dead-key character followed by base character.
    mVirtualKeys[activeDeadKeyIndex].GetUniChars(mDeadKeyShiftState,
                                                 mChars, mShiftStates);
    memcpy(&mChars[1], uniChars, numOfBaseChars * sizeof(PRUnichar));
    memset(&mShiftStates[1], finalShiftState, numOfBaseChars);
    mNumOfChars = numOfBaseChars + 1;
  }

  DeactivateDeadKeyState();
}

PRUint32
KeyboardLayout::GetUniChars(PRUnichar* aUniChars,
                            PRUint8* aShiftStates,
                            PRUint32 aMaxChars) const
{
  PRUint32 chars = NS_MIN<PRUint32>(mNumOfChars, aMaxChars);

  memcpy(aUniChars, mChars, chars * sizeof(PRUnichar));
  memcpy(aShiftStates, mShiftStates, chars);

  return chars;
}

PRUint32
KeyboardLayout::GetUniCharsWithShiftState(PRUint8 aVirtualKey,
                                          PRUint8 aShiftStates,
                                          PRUnichar* aUniChars,
                                          PRUint32 aMaxChars) const
{
  PRInt32 key = GetKeyIndex(aVirtualKey);
  if (key < 0) {
    return 0;
  }
  PRUint8 finalShiftState;
  PRUnichar uniChars[5];
  PRUint32 numOfBaseChars =
    mVirtualKeys[key].GetUniChars(aShiftStates, uniChars, &finalShiftState);
  PRUint32 chars = NS_MIN(numOfBaseChars, aMaxChars);
  memcpy(aUniChars, uniChars, chars * sizeof(PRUnichar));
  return chars;
}

void
KeyboardLayout::LoadLayout(HKL aLayout)
{
  if (mKeyboardLayout == aLayout) {
    return;
  }

  mKeyboardLayout = aLayout;

  PRUint32 shiftState;

  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));

  BYTE originalKbdState[256];
  // Bitfield with all shift states that have at least one dead-key.
  PRUint16 shiftStatesWithDeadKeys = 0;
  // Bitfield with all shift states that produce any possible dead-key base
  // characters.
  PRUint16 shiftStatesWithBaseChars = 0;

  mActiveDeadKey = -1;
  mNumOfChars = 0;

  ReleaseDeadKeyTables();

  ::GetKeyboardState(originalKbdState);

  // For each shift state gather all printable characters that are produced
  // for normal case when no any dead-key is active.

  for (shiftState = 0; shiftState < 16; shiftState++) {
    SetShiftState(kbdState, shiftState);
    for (PRUint32 virtualKey = 0; virtualKey < 256; virtualKey++) {
      PRInt32 vki = GetKeyIndex(virtualKey);
      if (vki < 0) {
        continue;
      }
      NS_ASSERTION(PRUint32(vki) < ArrayLength(mVirtualKeys), "invalid index");
      PRUnichar uniChars[5];
      PRInt32 ret =
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
  for (shiftState = 0; shiftState < 16; shiftState++) {
    if (!(shiftStatesWithDeadKeys & (1 << shiftState))) {
      continue;
    }

    SetShiftState(kbdState, shiftState);

    for (PRUint32 virtualKey = 0; virtualKey < 256; virtualKey++) {
      PRInt32 vki = GetKeyIndex(virtualKey);
      if (vki >= 0 && mVirtualKeys[vki].IsDeadKey(shiftState)) {
        DeadKeyEntry deadKeyArray[256];
        PRInt32 n = GetDeadKeyCombinations(virtualKey, kbdState,
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


PRUint8
KeyboardLayout::GetShiftState(const PBYTE aKbdState)
{
  bool isShift = (aKbdState[VK_SHIFT] & 0x80) != 0;
  bool isCtrl  = (aKbdState[VK_CONTROL] & 0x80) != 0;
  bool isAlt   = (aKbdState[VK_MENU] & 0x80) != 0;
  bool isCaps  = (aKbdState[VK_CAPITAL] & 0x01) != 0;

  return ((isCaps << 3) | (isAlt << 2) | (isCtrl << 1) | isShift);
}

void
KeyboardLayout::SetShiftState(PBYTE aKbdState, PRUint8 aShiftState)
{
  NS_ASSERTION(aShiftState < 16, "aShiftState out of range");

  if (aShiftState & eShift) {
    aKbdState[VK_SHIFT] |= 0x80;
  } else {
    aKbdState[VK_SHIFT]  &= ~0x80;
    aKbdState[VK_LSHIFT] &= ~0x80;
    aKbdState[VK_RSHIFT] &= ~0x80;
  }

  if (aShiftState & eCtrl) {
    aKbdState[VK_CONTROL] |= 0x80;
  } else {
    aKbdState[VK_CONTROL]  &= ~0x80;
    aKbdState[VK_LCONTROL] &= ~0x80;
    aKbdState[VK_RCONTROL] &= ~0x80;
  }

  if (aShiftState & eAlt) {
    aKbdState[VK_MENU] |= 0x80;
  } else {
    aKbdState[VK_MENU]  &= ~0x80;
    aKbdState[VK_LMENU] &= ~0x80;
    aKbdState[VK_RMENU] &= ~0x80;
  }

  if (aShiftState & eCapsLock) {
    aKbdState[VK_CAPITAL] |= 0x01;
  } else {
    aKbdState[VK_CAPITAL] &= ~0x01;
  }
}

inline PRInt32
KeyboardLayout::GetKeyIndex(PRUint8 aVirtualKey)
{
// Currently these 54 (NS_NUM_OF_KEYS) virtual keys are assumed
// to produce visible representation:
// 0x20 - VK_SPACE          ' '
// 0x30..0x39               '0'..'9'
// 0x41..0x5A               'A'..'Z'
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

  static const PRInt8 xlat[256] =
  {
  // 0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
  //-----------------------------------------------------------------------
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 00
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 10
     0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 20
     1,  2,  3,  4,  5,  6,  7,  8,  9, 10, -1, -1, -1, -1, -1, -1,   // 30
    -1, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,   // 40
    26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, -1, -1, -1, -1, -1,   // 50
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 37, -1,   // 60
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 70
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 80
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // 90
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // A0
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 38, 39, 40, 41, 42, 43,   // B0
    44, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // C0
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 45, 46, 47, 48, 49,   // D0
    -1, 50, 51, 52, 53, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,   // E0
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
                                PRUint32 aEntries)
{
  DeadKeyTableListEntry* next = mDeadKeyTableListHead;

  const size_t bytes = offsetof(DeadKeyTableListEntry, data) +
    DeadKeyTable::SizeInBytes(aEntries);
  PRUint8* p = new PRUint8[bytes];

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
    PRUint8* p = reinterpret_cast<PRUint8*>(mDeadKeyTableListHead);
    mDeadKeyTableListHead = mDeadKeyTableListHead->next;

    delete [] p;
  }
}

bool
KeyboardLayout::EnsureDeadKeyActive(bool aIsActive,
                                    PRUint8 aDeadKey,
                                    const PBYTE aDeadKeyKbdState)
{
  PRInt32 ret;
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

  SetShiftState(kbdState, mDeadKeyShiftState);

  EnsureDeadKeyActive(false, mActiveDeadKey, kbdState);
  mActiveDeadKey = -1;
}

bool
KeyboardLayout::AddDeadKeyEntry(PRUnichar aBaseChar,
                                PRUnichar aCompositeChar,
                                DeadKeyEntry* aDeadKeyArray,
                                PRUint32 aEntries)
{
  for (PRUint32 index = 0; index < aEntries; index++) {
    if (aDeadKeyArray[index].BaseChar == aBaseChar) {
      return false;
    }
  }

  aDeadKeyArray[aEntries].BaseChar = aBaseChar;
  aDeadKeyArray[aEntries].CompositeChar = aCompositeChar;

  return true;
}

PRUint32
KeyboardLayout::GetDeadKeyCombinations(PRUint8 aDeadKey,
                                       const PBYTE aDeadKeyKbdState,
                                       PRUint16 aShiftStatesWithBaseChars,
                                       DeadKeyEntry* aDeadKeyArray,
                                       PRUint32 aMaxEntries)
{
  bool deadKeyActive = false;
  PRUint32 entries = 0;
  BYTE kbdState[256];
  memset(kbdState, 0, sizeof(kbdState));

  for (PRUint32 shiftState = 0; shiftState < 16; shiftState++) {
    if (!(aShiftStatesWithBaseChars & (1 << shiftState))) {
      continue;
    }

    SetShiftState(kbdState, shiftState);

    for (PRUint32 virtualKey = 0; virtualKey < 256; virtualKey++) {
      PRInt32 vki = GetKeyIndex(virtualKey);
      // Dead-key can pair only with such key that produces exactly one base
      // character.
      if (vki >= 0 && mVirtualKeys[vki].GetNativeUniChars(shiftState) == 1) {
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
        PRInt32 ret =
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
               CompareDeadKeyEntries, nsnull);
  return entries;
}

PRUint32
KeyboardLayout::ConvertNativeKeyCodeToDOMKeyCode(UINT aNativeKeyCode) const
{
  // Alphabet or Numeric or Numpad or Function keys
  if ((aNativeKeyCode >= 0x30 && aNativeKeyCode <= 0x39) ||
      (aNativeKeyCode >= 0x41 && aNativeKeyCode <= 0x5A) ||
      (aNativeKeyCode >= 0x60 && aNativeKeyCode <= 0x87)) {
    return static_cast<PRUint32>(aNativeKeyCode);
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
      return PRUint32(aNativeKeyCode);

    case VK_HELP:
      return NS_VK_HELP;

    // Windows key should be mapped to a Win keycode
    // They should be able to be distinguished by DOM3 KeyboardEvent.location
    case VK_LWIN:
    case VK_RWIN:
      return NS_VK_WIN;

    // Following keycodes are not defined in our DOM keycodes.
    case VK_BROWSER_BACK:
    case VK_BROWSER_FORWARD:
    case VK_BROWSER_REFRESH:
    case VK_BROWSER_STOP:
    case VK_BROWSER_SEARCH:
    case VK_BROWSER_FAVORITES:
    case VK_BROWSER_HOME:
    case VK_VOLUME_MUTE:
    case VK_VOLUME_DOWN:
    case VK_VOLUME_UP:
    case VK_MEDIA_NEXT_TRACK:
    case VK_MEDIA_STOP:
    case VK_MEDIA_PLAY_PAUSE:
    case VK_LAUNCH_MAIL:
    case VK_LAUNCH_MEDIA_SELECT:
    case VK_LAUNCH_APP1:
    case VK_LAUNCH_APP2:
    case VK_ATTN: // Attension key of IBM midrange computers, e.g., AS/400
    case VK_CRSEL: // Cursor Selection
    case VK_EXSEL: // Extend Selection
    case VK_EREOF: // Erase EOF key of IBM 3270 keyboard layout
    case VK_PLAY:
    case VK_ZOOM:
    case VK_PA1: // PA1 key of IBM 3270 keyboard layout
    case VK_OEM_CLEAR:
      return 0;

    // Following keycodes are only used by Nokia/Ericsson, we don't define them
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
    case 0xE1: // OEM specific
    case VK_OEM_102:
    case 0xE3: // OEM specific
    case 0xE4: // OEM specific
    {
      NS_ASSERTION(IsPrintableCharKey(aNativeKeyCode),
                   "The key must be printable");
      PRUnichar uniChars[5];
      PRUint32 numOfChars =
        GetUniCharsWithShiftState(aNativeKeyCode, 0,
                                  uniChars, ArrayLength(uniChars));
      if (numOfChars != 1 || uniChars[0] < ' ' || uniChars[0] > 0x7F) {
        numOfChars =
          GetUniCharsWithShiftState(aNativeKeyCode, eShift,
                                    uniChars, ArrayLength(uniChars));
        if (numOfChars != 1 || uniChars[0] < ' ' || uniChars[0] > 0x7F) {
          return 0;
        }
      }
      return WidgetUtils::ComputeKeyCodeFromChar(uniChars[0]);
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

PRUnichar
DeadKeyTable::GetCompositeChar(PRUnichar aBaseChar) const
{
  // Dead-key table is sorted by BaseChar in ascending order.
  // Usually they are too small to use binary search.

  for (PRUint32 index = 0; index < mEntries; index++) {
    if (mTable[index].BaseChar == aBaseChar) {
      return mTable[index].CompositeChar;
    }
    if (mTable[index].BaseChar > aBaseChar) {
      break;
    }
  }

  return 0;
}

} // namespace widget
} // namespace mozilla

