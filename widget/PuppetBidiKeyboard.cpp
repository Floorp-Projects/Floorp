/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PuppetBidiKeyboard.h"
#include "nsIWidget.h"

using namespace mozilla::widget;

NS_IMPL_ISUPPORTS(PuppetBidiKeyboard, nsIBidiKeyboard)

PuppetBidiKeyboard::PuppetBidiKeyboard()
    : nsIBidiKeyboard(), mIsLangRTL(false), mHaveBidiKeyboards(false) {}

PuppetBidiKeyboard::~PuppetBidiKeyboard() = default;

NS_IMETHODIMP
PuppetBidiKeyboard::Reset() { return NS_OK; }

NS_IMETHODIMP
PuppetBidiKeyboard::IsLangRTL(bool* aIsRTL) {
  *aIsRTL = mIsLangRTL;
  return NS_OK;
}

void PuppetBidiKeyboard::SetBidiKeyboardInfo(bool aIsLangRTL,
                                             bool aHaveBidiKeyboards) {
  mIsLangRTL = aIsLangRTL;
  mHaveBidiKeyboards = aHaveBidiKeyboards;
}

NS_IMETHODIMP
PuppetBidiKeyboard::GetHaveBidiKeyboards(bool* aResult) {
  *aResult = mHaveBidiKeyboards;
  return NS_OK;
}

// static
already_AddRefed<nsIBidiKeyboard>
nsIWidget::CreateBidiKeyboardContentProcess() {
  return do_AddRef(new PuppetBidiKeyboard());
}
