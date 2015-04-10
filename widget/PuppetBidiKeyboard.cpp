/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PuppetBidiKeyboard.h"

using namespace mozilla::widget;

NS_IMPL_ISUPPORTS(PuppetBidiKeyboard, nsIBidiKeyboard)

PuppetBidiKeyboard::PuppetBidiKeyboard() : nsIBidiKeyboard()
{
}

PuppetBidiKeyboard::~PuppetBidiKeyboard()
{
}

NS_IMETHODIMP
PuppetBidiKeyboard::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
PuppetBidiKeyboard::IsLangRTL(bool* aIsRTL)
{
  *aIsRTL = mIsLangRTL;
  return NS_OK;
}

void
PuppetBidiKeyboard::SetIsLangRTL(bool aIsLangRTL)
{
  mIsLangRTL = aIsLangRTL;
}

NS_IMETHODIMP
PuppetBidiKeyboard::GetHaveBidiKeyboards(bool* aResult)
{
  // not implemented yet
  return NS_ERROR_NOT_IMPLEMENTED;
}
