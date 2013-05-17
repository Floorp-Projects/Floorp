/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Base implementation for console messages.
 */

#include "nsConsoleMessage.h"
#include "nsReadableUtils.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsConsoleMessage, nsIConsoleMessage)

nsConsoleMessage::nsConsoleMessage() 
{
}

nsConsoleMessage::nsConsoleMessage(const PRUnichar *message) 
{
	mMessage.Assign(message);
}

NS_IMETHODIMP
nsConsoleMessage::GetMessageMoz(PRUnichar **result) {
    *result = ToNewUnicode(mMessage);

    return NS_OK;
}

//  NS_IMETHODIMP
//  nsConsoleMessage::Init(const PRUnichar *message) {
//      nsAutoString newMessage(message);
//      mMessage = ToNewUnicode(newMessage);
//      return NS_OK;
//  }

