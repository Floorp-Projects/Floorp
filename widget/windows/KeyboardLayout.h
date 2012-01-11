/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Dainis Jonitis, <Dainis_Jonitis@exigengroup.lv>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef KeyboardLayout_h__
#define KeyboardLayout_h__

#include "nscore.h"
#include <windows.h>

#define NS_NUM_OF_KEYS          50

#define VK_OEM_1                0xBA   // ';:' for US
#define VK_OEM_PLUS             0xBB   // '+' any country
#define VK_OEM_MINUS            0xBD   // '-' any country

namespace mozilla {
namespace widget {

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

  HKL GetLayout() { return mKeyboardLayout; }
};

} // namespace widget
} // namespace mozilla

#endif
