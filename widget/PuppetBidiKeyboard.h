/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_PuppetBidiKeyboard_h_
#define mozilla_widget_PuppetBidiKeyboard_h_

#include "nsIBidiKeyboard.h"

namespace mozilla {
namespace widget {

class PuppetBidiKeyboard final : public nsIBidiKeyboard
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBIDIKEYBOARD

  PuppetBidiKeyboard();

  void SetBidiKeyboardInfo(bool aIsLangRTL, bool aHaveBidiKeyboards);

private:
  ~PuppetBidiKeyboard();

  bool mIsLangRTL;
  bool mHaveBidiKeyboards;
};

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_PuppetBidiKeyboard_h_
