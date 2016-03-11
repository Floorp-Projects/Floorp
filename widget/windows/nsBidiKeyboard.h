/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsBidiKeyboard
#define __nsBidiKeyboard
#include "nsIBidiKeyboard.h"
#include <windows.h>

class nsBidiKeyboard : public nsIBidiKeyboard
{
  virtual ~nsBidiKeyboard();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBIDIKEYBOARD

  nsBidiKeyboard();

  static void OnLayoutChange();

protected:

  nsresult SetupBidiKeyboards();
  bool IsRTLLanguage(HKL aLocale);

  bool mInitialized;
  bool mHaveBidiKeyboards;
  wchar_t  mLTRKeyboard[KL_NAMELENGTH];
  wchar_t  mRTLKeyboard[KL_NAMELENGTH];
  wchar_t  mCurrentLocaleName[KL_NAMELENGTH];
};


#endif // __nsBidiKeyboard
