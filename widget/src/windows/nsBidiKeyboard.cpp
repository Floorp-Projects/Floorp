/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is IBM code.
 * 
 * The Initial Developer of the Original Code is IBM.
 * Portions created by IBM are
 * Copyright (C) International Business Machines
 * Corporation, 2000.  All Rights Reserved.
 * 
 * Contributor(s): Simon Montagu
 */

#include "nsBidiKeyboard.h"
#include "prmem.h"

NS_IMPL_ISUPPORTS(nsBidiKeyboard, NS_GET_IID(nsIBidiKeyboard))

nsBidiKeyboard::nsBidiKeyboard() : nsIBidiKeyboard()
{
  NS_INIT_REFCNT();
#ifdef IBMBIDI
  mDefaultsSet = PR_FALSE;
#endif
}

nsBidiKeyboard::~nsBidiKeyboard()
{
}

NS_IMETHODIMP nsBidiKeyboard::SetLangFromBidiLevel(PRUint8 aLevel)
{
#ifdef IBMBIDI      
  if (!mDefaultsSet) {
    nsresult result = EnumerateKeyboards();
    if (NS_SUCCEEDED(result))
      mDefaultsSet = PR_TRUE;
    else
      return result;
  }

  // call LoadKeyboardLayout() only if the target keyboard layout is different from the current
  char currentLocaleName[KL_NAMELENGTH];
  strcpy(currentLocaleName, (aLevel & 1) ? mRTLKeyboard : mLTRKeyboard);
  if (strcmp(mCurrentLocaleName, currentLocaleName)) {
    if (!::LoadKeyboardLayout(currentLocaleName, KLF_ACTIVATE | KLF_SUBSTITUTE_OK)) {
      return NS_ERROR_FAILURE;
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::IsLangRTL(PRBool *aIsRTL)
{
  *aIsRTL = PR_FALSE;
#ifdef IBMBIDI
  HKL  currentLocale;
 
  currentLocale = ::GetKeyboardLayout(0);
  *aIsRTL = IsRTLLanguage(currentLocale);
  
  if (!::GetKeyboardLayoutName(mCurrentLocaleName))
    return NS_ERROR_FAILURE;

  // The language set by the user overrides the default language for that direction
  if (*aIsRTL)
    strcpy(mRTLKeyboard, mCurrentLocaleName);
  else
    strcpy(mLTRKeyboard, mCurrentLocaleName);
#endif
  return NS_OK;
}

#ifdef IBMBIDI

// Get the list of keyboard layouts available in the system
// Set mLTRKeyboard to the first LTR keyboard in the list and mRTLKeyboard to the first RTL keyboard in the list
// These defaults will be used unless the user explicitly sets something else.
nsresult nsBidiKeyboard::EnumerateKeyboards()
{
  int keyboards;
  HKL far* buf;
  HKL locale;
  char localeName[KL_NAMELENGTH];
  
  // GetKeyboardLayoutList with 0 as first parameter returns the number of keyboard layouts available
  keyboards = ::GetKeyboardLayoutList(0, nsnull);
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
    if (IsRTLLanguage(locale))
      sprintf(mRTLKeyboard, "%.*x", KL_NAMELENGTH - 1, LANGIDFROMLCID(locale));
    else
      sprintf(mLTRKeyboard, "%.*x", KL_NAMELENGTH - 1, LANGIDFROMLCID(locale));
  }
  PR_Free(buf);

  // Get the current keyboard layout and use it for either mRTLKeyboard or
  // mLTRKeyboard as appropriate. If the user has many keyboard layouts
  // installed this prevents us from arbitrarily resetting the current
  // layout (bug 80274)
  locale = ::GetKeyboardLayout(0);
  if (!::GetKeyboardLayoutName(localeName))
    return NS_ERROR_FAILURE;

  if (IsRTLLanguage(locale)) {
    strcpy(mRTLKeyboard, localeName);
  }
  else {
    strcpy(mLTRKeyboard, localeName);
  }

  return NS_OK;
}

// Test whether the language represented by this locale identifier is a right-to-left language
PRBool nsBidiKeyboard::IsRTLLanguage(HKL aLocale)
{
  // This macro extracts the primary language id (low 10 bits) from the locale id
  switch (PRIMARYLANGID(aLocale)){
    case LANG_ARABIC:
    case LANG_FARSI:
    case LANG_HEBREW:
      return PR_TRUE;
      break;

    default:
      return PR_FALSE;
  }
}
#endif // IBMBIDI
