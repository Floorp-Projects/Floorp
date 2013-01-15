/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsBidiKeyboard.h"
#include "nsCocoaUtils.h"
#include "TextInputHandler.h"

// This must be the last include:
#include "nsObjCExceptions.h"

using namespace mozilla::widget;

NS_IMPL_ISUPPORTS1(nsBidiKeyboard, nsIBidiKeyboard)

nsBidiKeyboard::nsBidiKeyboard() : nsIBidiKeyboard()
{
}

nsBidiKeyboard::~nsBidiKeyboard()
{
}

NS_IMETHODIMP nsBidiKeyboard::IsLangRTL(bool *aIsRTL)
{
  *aIsRTL = TISInputSourceWrapper::CurrentInputSource().IsForRTLLanguage();
  return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::SetLangFromBidiLevel(uint8_t aLevel)
{
  // XXX Insert platform specific code to set keyboard language
  return NS_OK;
}

NS_IMETHODIMP nsBidiKeyboard::GetHaveBidiKeyboards(bool* aResult)
{
  // not implemented yet
  return NS_ERROR_NOT_IMPLEMENTED;
}
