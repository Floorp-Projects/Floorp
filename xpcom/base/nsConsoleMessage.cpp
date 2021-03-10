/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base implementation for console messages.
 */

#include "nsConsoleMessage.h"
#include "jsapi.h"

NS_IMPL_ISUPPORTS(nsConsoleMessage, nsIConsoleMessage)

nsConsoleMessage::nsConsoleMessage() : mTimeStamp(0), mMessage() {}

nsConsoleMessage::nsConsoleMessage(const char16_t* aMessage) {
  mTimeStamp = JS_Now() / 1000;
  mMessage.Assign(aMessage);
  mIsForwardedFromContentProcess = false;
}

NS_IMETHODIMP
nsConsoleMessage::GetMessageMoz(nsAString& aMessage) {
  aMessage = mMessage;
  return NS_OK;
}

NS_IMETHODIMP
nsConsoleMessage::GetLogLevel(uint32_t* aLogLevel) {
  *aLogLevel = nsConsoleMessage::info;
  return NS_OK;
}

NS_IMETHODIMP
nsConsoleMessage::GetTimeStamp(int64_t* aTimeStamp) {
  *aTimeStamp = mTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
nsConsoleMessage::ToString(nsACString& /*UTF8*/ aResult) {
  CopyUTF16toUTF8(mMessage, aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsConsoleMessage::GetIsForwardedFromContentProcess(
    bool* aIsForwardedFromContentProcess) {
  *aIsForwardedFromContentProcess = mIsForwardedFromContentProcess;
  return NS_OK;
}

NS_IMETHODIMP
nsConsoleMessage::SetIsForwardedFromContentProcess(
    bool aIsForwardedFromContentProcess) {
  mIsForwardedFromContentProcess = aIsForwardedFromContentProcess;
  return NS_OK;
}
