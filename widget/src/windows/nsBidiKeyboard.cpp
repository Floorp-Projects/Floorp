/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is IBM code.
 *
 * The Initial Developer of the Original Code is
 * IBM. Portions created by IBM are Copyright (C) International Business Machines Corporation, 2000.  All Rights Reserved.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Simon Montagu
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <stdio.h>
#include "nsBidiKeyboard.h"
#include "prmem.h"

NS_IMPL_ISUPPORTS1(nsBidiKeyboard, nsIBidiKeyboard)

nsBidiKeyboard::nsBidiKeyboard() : nsIBidiKeyboard()
{
  mDefaultsSet = PR_FALSE;
  mLTRKeyboard[0] = '\0';
  mRTLKeyboard[0] = '\0';
  mCurrentLocaleName[0] = '\0';
}

nsBidiKeyboard::~nsBidiKeyboard()
{
}

NS_IMETHODIMP nsBidiKeyboard::SetLangFromBidiLevel(PRUint8 aLevel)
{
  if (!mDefaultsSet) {
    nsresult result = EnumerateKeyboards();
    if (NS_SUCCEEDED(result))
      mDefaultsSet = PR_TRUE;
    else
      return result;
  }

  // call LoadKeyboardLayout() only if the target keyboard layout is different from the current
  char currentLocaleName[KL_NAMELENGTH];
  strncpy(currentLocaleName, (aLevel & 1) ? mRTLKeyboard : mLTRKeyboard, KL_NAMELENGTH);
  currentLocaleName[KL_NAMELENGTH-1] = '\0'; // null terminate

  NS_ASSERTION(*currentLocaleName, 
    "currentLocaleName has string length == 0");

#if 0
  /* This implementation of automatic keyboard layout switching is too buggy to be useful
     and the feature itself is inconsistent with Windows. See Bug 162242 */
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
  HKL  currentLocale;
 
  currentLocale = ::GetKeyboardLayout(0);
  *aIsRTL = IsRTLLanguage(currentLocale);
  
  if (!::GetKeyboardLayoutName(mCurrentLocaleName))
    return NS_ERROR_FAILURE;

  NS_ASSERTION(*mCurrentLocaleName, 
    "GetKeyboardLayoutName return string length == 0");
  NS_ASSERTION((strlen(mCurrentLocaleName) < KL_NAMELENGTH), 
    "GetKeyboardLayoutName return string length >= KL_NAMELENGTH");

  // The language set by the user overrides the default language for that direction
  if (*aIsRTL) {
    strncpy(mRTLKeyboard, mCurrentLocaleName, KL_NAMELENGTH);
    mRTLKeyboard[KL_NAMELENGTH-1] = '\0'; // null terminate
  } else {
    strncpy(mLTRKeyboard, mCurrentLocaleName, KL_NAMELENGTH);
    mLTRKeyboard[KL_NAMELENGTH-1] = '\0'; // null terminate
  }

  NS_ASSERTION((strlen(mRTLKeyboard) < KL_NAMELENGTH), 
    "mLTRKeyboard has string length >= KL_NAMELENGTH");
  NS_ASSERTION((strlen(mLTRKeyboard) < KL_NAMELENGTH), 
    "mRTLKeyboard has string length >= KL_NAMELENGTH");
  return NS_OK;
}


// Get the list of keyboard layouts available in the system
// Set mLTRKeyboard to the first LTR keyboard in the list and mRTLKeyboard to the first RTL keyboard in the list
// These defaults will be used unless the user explicitly sets something else.
nsresult nsBidiKeyboard::EnumerateKeyboards()
{
  int keyboards;
  HKL far* buf;
  HKL locale;
  char localeName[KL_NAMELENGTH];
  PRBool isLTRKeyboardSet = PR_FALSE;
  PRBool isRTLKeyboardSet = PR_FALSE;
  
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
    if (IsRTLLanguage(locale)) {
      sprintf(mRTLKeyboard, "%.*x", KL_NAMELENGTH - 1, LANGIDFROMLCID(locale));
      isRTLKeyboardSet = PR_TRUE;
    }
    else {
      sprintf(mLTRKeyboard, "%.*x", KL_NAMELENGTH - 1, LANGIDFROMLCID(locale));
      isLTRKeyboardSet = PR_TRUE;
    }
  }
  PR_Free(buf);

  // Get the current keyboard layout and use it for either mRTLKeyboard or
  // mLTRKeyboard as appropriate. If the user has many keyboard layouts
  // installed this prevents us from arbitrarily resetting the current
  // layout (bug 80274)

  // If one or other keyboard is still not initialized, copy the
  // initialized keyboard to the uninitialized (bug 85813)
  locale = ::GetKeyboardLayout(0);
  if (!::GetKeyboardLayoutName(localeName))
    return NS_ERROR_FAILURE;

  NS_ASSERTION(*localeName, 
    "GetKeyboardLayoutName return string length == 0");
  NS_ASSERTION((strlen(localeName) < KL_NAMELENGTH), 
    "GetKeyboardLayout return string length >= KL_NAMELENGTH");

  if (IsRTLLanguage(locale)) {
    strncpy(mRTLKeyboard, localeName, KL_NAMELENGTH);
    mRTLKeyboard[KL_NAMELENGTH-1] = '\0'; // null terminate
    if (! isLTRKeyboardSet) {
      strncpy(mLTRKeyboard, localeName, KL_NAMELENGTH);
      mLTRKeyboard[KL_NAMELENGTH-1] = '\0'; // null terminate
    }
  }
  else {
    strncpy(mLTRKeyboard, localeName, KL_NAMELENGTH);
    mLTRKeyboard[KL_NAMELENGTH-1] = '\0'; // null terminate
    if (! isRTLKeyboardSet) {
      strncpy(mRTLKeyboard, localeName, KL_NAMELENGTH);
      mRTLKeyboard[KL_NAMELENGTH-1] = '\0'; // null terminate
    }
  }

  NS_ASSERTION(*mRTLKeyboard, 
    "mLTRKeyboard has string length == 0");
  NS_ASSERTION(*mLTRKeyboard, 
    "mLTRKeyboard has string length == 0");

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
