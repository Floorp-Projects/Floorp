/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef KeyboardLayout_h__
#define KeyboardLayout_h__

#include "nscore.h"
#include <windows.h>

#define NS_NUM_OF_KEYS          54

#define VK_OEM_1                0xBA   // ';:' for US
#define VK_OEM_PLUS             0xBB   // '+' any country
#define VK_OEM_COMMA            0xBC
#define VK_OEM_MINUS            0xBD   // '-' any country
#define VK_OEM_PERIOD           0xBE
#define VK_OEM_2                0xBF
#define VK_OEM_3                0xC0
#define VK_OEM_4                0xDB
#define VK_OEM_5                0xDC
#define VK_OEM_6                0xDD
#define VK_OEM_7                0xDE
#define VK_OEM_8                0xDF
#define VK_OEM_102              0xE2
#define VK_OEM_CLEAR            0xFE

class nsWindow;

namespace mozilla {
namespace widget {

class KeyboardLayout;

//  0 - Normal
//  1 - Shift
//  2 - Control
//  3 - Control + Shift
//  4 - Alt
//  5 - Alt + Shift
//  6 - Alt + Control (AltGr)
//  7 - Alt + Control + Shift (AltGr + Shift)
//  8 - CapsLock
//  9 - CapsLock + Shift
// 10 - CapsLock + Control
// 11 - CapsLock + Control + Shift
// 12 - CapsLock + Alt
// 13 - CapsLock + Alt + Shift
// 14 - CapsLock + Alt + Control (CapsLock + AltGr)
// 15 - CapsLock + Alt + Control + Shift (CapsLock + AltGr + Shift)

enum eKeyShiftFlags
{
  eShift    = 0x01,
  eCtrl     = 0x02,
  eAlt      = 0x04,
  eCapsLock = 0x08
};

struct DeadKeyEntry;
class DeadKeyTable;


class VirtualKey
{
  union KeyShiftState
  {
    struct
    {
      PRUnichar Chars[4];
    } Normal;
    struct
    {
      const DeadKeyTable* Table;
      PRUnichar DeadChar;
    } DeadKey;
  };

  KeyShiftState mShiftStates[16];
  PRUint16 mIsDeadKey;

  void SetDeadKey(PRUint8 aShiftState, bool aIsDeadKey)
  {
    if (aIsDeadKey) {
      mIsDeadKey |= 1 << aShiftState;
    } else {
      mIsDeadKey &= ~(1 << aShiftState);
    }
  }

public:
  bool IsDeadKey(PRUint8 aShiftState) const
  {
    return (mIsDeadKey & (1 << aShiftState)) != 0;
  }

  void AttachDeadKeyTable(PRUint8 aShiftState,
                          const DeadKeyTable* aDeadKeyTable)
  {
    mShiftStates[aShiftState].DeadKey.Table = aDeadKeyTable;
  }

  void SetNormalChars(PRUint8 aShiftState, const PRUnichar* aChars,
                      PRUint32 aNumOfChars);
  void SetDeadChar(PRUint8 aShiftState, PRUnichar aDeadChar);
  const DeadKeyTable* MatchingDeadKeyTable(const DeadKeyEntry* aDeadKeyArray,
                                           PRUint32 aEntries) const;
  inline PRUnichar GetCompositeChar(PRUint8 aShiftState,
                                    PRUnichar aBaseChar) const;
  PRUint32 GetNativeUniChars(PRUint8 aShiftState,
                             PRUnichar* aUniChars = nsnull) const;
  PRUint32 GetUniChars(PRUint8 aShiftState, PRUnichar* aUniChars,
                       PRUint8* aFinalShiftState) const;
};

class NativeKey {
public:
  NativeKey() :
    mDOMKeyCode(0), mVirtualKeyCode(0), mOriginalVirtualKeyCode(0),
    mScanCode(0), mIsExtended(false)
  {
  }

  NativeKey(const KeyboardLayout& aKeyboardLayout,
            nsWindow* aWindow,
            const MSG& aKeyOrCharMessage);

  PRUint32 GetDOMKeyCode() const { return mDOMKeyCode; }

  // The result is one of nsIDOMKeyEvent::DOM_KEY_LOCATION_*.
  PRUint32 GetKeyLocation() const;
  WORD GetScanCode() const { return mScanCode; }
  PRUint8 GetVirtualKeyCode() const { return mVirtualKeyCode; }
  PRUint8 GetOriginalVirtualKeyCode() const { return mOriginalVirtualKeyCode; }

private:
  PRUint32 mDOMKeyCode;
  // mVirtualKeyCode distinguishes left key or right key of modifier key.
  PRUint8 mVirtualKeyCode;
  // mOriginalVirtualKeyCode doesn't distinguish left key or right key of
  // modifier key.  However, if the given keycode is VK_PROCESS, it's resolved
  // to a keycode before it's handled by IME.
  PRUint8 mOriginalVirtualKeyCode;
  WORD    mScanCode;
  bool    mIsExtended;

  UINT GetScanCodeWithExtendedFlag() const;
};

class KeyboardLayout
{
  struct DeadKeyTableListEntry
  {
    DeadKeyTableListEntry* next;
    PRUint8 data[1];
  };

  HKL mKeyboardLayout;

  VirtualKey mVirtualKeys[NS_NUM_OF_KEYS];
  DeadKeyTableListEntry* mDeadKeyTableListHead;
  PRInt32 mActiveDeadKey;                 // -1 = no active dead-key
  PRUint8 mDeadKeyShiftState;
  PRInt32 mLastVirtualKeyIndex;
  PRUint8 mLastShiftState;
  PRUnichar mChars[5];                    // Dead-key + up to 4 characters
  PRUint8 mShiftStates[5];
  PRUint8 mNumOfChars;

  static PRUint8 GetShiftState(const PBYTE aKbdState);
  static void SetShiftState(PBYTE aKbdState, PRUint8 aShiftState);
  static inline PRInt32 GetKeyIndex(PRUint8 aVirtualKey);
  static int CompareDeadKeyEntries(const void* aArg1, const void* aArg2,
                                   void* aData);
  static bool AddDeadKeyEntry(PRUnichar aBaseChar, PRUnichar aCompositeChar,
                                DeadKeyEntry* aDeadKeyArray, PRUint32 aEntries);
  bool EnsureDeadKeyActive(bool aIsActive, PRUint8 aDeadKey,
                             const PBYTE aDeadKeyKbdState);
  PRUint32 GetDeadKeyCombinations(PRUint8 aDeadKey,
                                  const PBYTE aDeadKeyKbdState,
                                  PRUint16 aShiftStatesWithBaseChars,
                                  DeadKeyEntry* aDeadKeyArray,
                                  PRUint32 aMaxEntries);
  void DeactivateDeadKeyState();
  const DeadKeyTable* AddDeadKeyTable(const DeadKeyEntry* aDeadKeyArray,
                                      PRUint32 aEntries);
  void ReleaseDeadKeyTables();

public:
  KeyboardLayout();
  ~KeyboardLayout();

  static bool IsPrintableCharKey(PRUint8 aVirtualKey);
  static bool IsNumpadKey(PRUint8 aVirtualKey);

  bool IsDeadKey() const
  {
    return (mLastVirtualKeyIndex >= 0) ?
      mVirtualKeys[mLastVirtualKeyIndex].IsDeadKey(mLastShiftState) : false;
  }

  void LoadLayout(HKL aLayout);
  void OnKeyDown(PRUint8 aVirtualKey);
  PRUint32 GetUniChars(PRUnichar* aUniChars, PRUint8* aShiftStates,
                       PRUint32 aMaxChars) const;
  PRUint32 GetUniCharsWithShiftState(PRUint8 aVirtualKey, PRUint8 aShiftStates,
                                     PRUnichar* aUniChars,
                                     PRUint32 aMaxChars) const;
  PRUint32 ConvertNativeKeyCodeToDOMKeyCode(UINT aNativeKeyCode) const;

  HKL GetLayout() const { return mKeyboardLayout; }
};

} // namespace widget
} // namespace mozilla

#endif
