/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "nsBidiKeyboard.h"
#include "WidgetUtils.h"
#include "prmem.h"
#include <tchar.h>

NS_IMPL_ISUPPORTS(nsBidiKeyboard, nsIBidiKeyboard)

nsBidiKeyboard::nsBidiKeyboard() : nsIBidiKeyboard()
{
  Reset();
}

nsBidiKeyboard::~nsBidiKeyboard()
{
}

NS_IMETHODIMP nsBidiKeyboard::Reset()
{
  mInitialized = false;
  mHaveBidiKeyboards = false;
  mLTRKeyboard[0] = '\0';
  mRTLKeyboard[0] = '\0';
  mCurrentLocaleName[0] = '\0';
  return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::IsLangRTL(bool *aIsRTL)
{
  *aIsRTL = false;

  nsresult result = SetupBidiKeyboards();
  if (NS_FAILED(result))
    return result;

  HKL  currentLocale;
 
  currentLocale = ::GetKeyboardLayout(0);
  *aIsRTL = IsRTLLanguage(currentLocale);
  
  if (!::GetKeyboardLayoutNameW(mCurrentLocaleName))
    return NS_ERROR_FAILURE;

  NS_ASSERTION(*mCurrentLocaleName, 
    "GetKeyboardLayoutName return string length == 0");
  NS_ASSERTION((wcslen(mCurrentLocaleName) < KL_NAMELENGTH), 
    "GetKeyboardLayoutName return string length >= KL_NAMELENGTH");

  // The language set by the user overrides the default language for that direction
  if (*aIsRTL) {
    wcsncpy(mRTLKeyboard, mCurrentLocaleName, KL_NAMELENGTH);
    mRTLKeyboard[KL_NAMELENGTH-1] = '\0'; // null terminate
  } else {
    wcsncpy(mLTRKeyboard, mCurrentLocaleName, KL_NAMELENGTH);
    mLTRKeyboard[KL_NAMELENGTH-1] = '\0'; // null terminate
  }

  NS_ASSERTION((wcslen(mRTLKeyboard) < KL_NAMELENGTH), 
    "mLTRKeyboard has string length >= KL_NAMELENGTH");
  NS_ASSERTION((wcslen(mLTRKeyboard) < KL_NAMELENGTH), 
    "mRTLKeyboard has string length >= KL_NAMELENGTH");
  return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::GetHaveBidiKeyboards(bool* aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  nsresult result = SetupBidiKeyboards();
  if (NS_FAILED(result))
    return result;

  *aResult = mHaveBidiKeyboards;
  return NS_OK;
}


// Get the list of keyboard layouts available in the system
// Set mLTRKeyboard to the first LTR keyboard in the list and mRTLKeyboard to the first RTL keyboard in the list
// These defaults will be used unless the user explicitly sets something else.
nsresult nsBidiKeyboard::SetupBidiKeyboards()
{
  if (mInitialized)
    return mHaveBidiKeyboards ? NS_OK : NS_ERROR_FAILURE;

  int keyboards;
  HKL far* buf;
  HKL locale;
  wchar_t localeName[KL_NAMELENGTH];
  bool isLTRKeyboardSet = false;
  bool isRTLKeyboardSet = false;
  
  // GetKeyboardLayoutList with 0 as first parameter returns the number of keyboard layouts available
  keyboards = ::GetKeyboardLayoutList(0, nullptr);
  if (!keyboards)
    return NS_ERROR_FAILURE;

  // allocate a buffer to hold the list
  buf = (HKL far*) PR_Malloc(keyboards * sizeof(HKL));
  if (!buf)
    return NS_ERROR_OUT_OF_MEMORY;

  // Call again to fill the buffer
  if (::GetKeyboardLayoutList(keyboards, buf) != keyboards) {
    PR_Free(buf);
    return NS_ERROR_UNEXPECTED;
  }

  // Go through the list and pick a default LTR and RTL keyboard layout
  while (keyboards--) {
    locale = buf[keyboards];
    if (IsRTLLanguage(locale)) {
      _snwprintf(mRTLKeyboard, KL_NAMELENGTH, L"%.*x", KL_NAMELENGTH - 1,
                 LANGIDFROMLCID((DWORD_PTR)locale));
      isRTLKeyboardSet = true;
    }
    else {
      _snwprintf(mLTRKeyboard, KL_NAMELENGTH, L"%.*x", KL_NAMELENGTH - 1,
                 LANGIDFROMLCID((DWORD_PTR)locale));
      isLTRKeyboardSet = true;
    }
  }
  PR_Free(buf);
  mInitialized = true;

  // If there is not at least one keyboard of each directionality, Bidi
  // keyboard functionality will be disabled.
  mHaveBidiKeyboards = (isRTLKeyboardSet && isLTRKeyboardSet);
  if (!mHaveBidiKeyboards)
    return NS_ERROR_FAILURE;

  // Get the current keyboard layout and use it for either mRTLKeyboard or
  // mLTRKeyboard as appropriate. If the user has many keyboard layouts
  // installed this prevents us from arbitrarily resetting the current
  // layout (bug 80274)
  locale = ::GetKeyboardLayout(0);
  if (!::GetKeyboardLayoutNameW(localeName))
    return NS_ERROR_FAILURE;

  NS_ASSERTION(*localeName, 
    "GetKeyboardLayoutName return string length == 0");
  NS_ASSERTION((wcslen(localeName) < KL_NAMELENGTH), 
    "GetKeyboardLayout return string length >= KL_NAMELENGTH");

  if (IsRTLLanguage(locale)) {
    wcsncpy(mRTLKeyboard, localeName, KL_NAMELENGTH);
    mRTLKeyboard[KL_NAMELENGTH-1] = '\0'; // null terminate
  }
  else {
    wcsncpy(mLTRKeyboard, localeName, KL_NAMELENGTH);
    mLTRKeyboard[KL_NAMELENGTH-1] = '\0'; // null terminate
  }

  NS_ASSERTION(*mRTLKeyboard, 
    "mLTRKeyboard has string length == 0");
  NS_ASSERTION(*mLTRKeyboard, 
    "mLTRKeyboard has string length == 0");

  return NS_OK;
}

// Test whether the language represented by this locale identifier is a
//  right-to-left language, using bit 123 of the Unicode subset bitfield in
//  the LOCALESIGNATURE
// See http://msdn.microsoft.com/library/default.asp?url=/library/en-us/intl/unicode_63ub.asp
bool nsBidiKeyboard::IsRTLLanguage(HKL aLocale)
{
  LOCALESIGNATURE localesig;
  return (::GetLocaleInfoW(PRIMARYLANGID((DWORD_PTR)aLocale),
                           LOCALE_FONTSIGNATURE,
                           (LPWSTR)&localesig,
                           (sizeof(localesig)/sizeof(WCHAR))) &&
          (localesig.lsUsb[3] & 0x08000000));
}

//static
void
nsBidiKeyboard::OnLayoutChange()
{
  mozilla::widget::WidgetUtils::SendBidiKeyboardInfoToContent();
}
