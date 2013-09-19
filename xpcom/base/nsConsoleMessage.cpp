/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base implementation for console messages.
 */

#include "nsConsoleMessage.h"
#include "nsReadableUtils.h"
#include "jsapi.h"

NS_IMPL_ISUPPORTS1(nsConsoleMessage, nsIConsoleMessage)

nsConsoleMessage::nsConsoleMessage()
    :  mTimeStamp(0),
       mMessage()
{
}

nsConsoleMessage::nsConsoleMessage(const PRUnichar *message)
{
  mTimeStamp = JS_Now() / 1000;
  mMessage.Assign(message);
}

NS_IMETHODIMP
nsConsoleMessage::GetMessageMoz(PRUnichar **result)
{
  *result = ToNewUnicode(mMessage);

  return NS_OK;
}

NS_IMETHODIMP
nsConsoleMessage::GetTimeStamp(int64_t *aTimeStamp)
{
  *aTimeStamp = mTimeStamp;
  return NS_OK;
}

NS_IMETHODIMP
nsConsoleMessage::ToString(nsACString& /*UTF8*/ aResult)
{
  CopyUTF16toUTF8(mMessage, aResult);

  return NS_OK;
}
