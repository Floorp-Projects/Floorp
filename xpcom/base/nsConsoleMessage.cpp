/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

/*
 * Base implementation for console messages.
 */

#include "nsConsoleMessage.h"

NS_IMPL_THREADSAFE_ISUPPORTS(nsConsoleMessage, NS_GET_IID(nsIConsoleMessage));

nsConsoleMessage::nsConsoleMessage() 
{
	NS_INIT_ISUPPORTS();
}

nsConsoleMessage::nsConsoleMessage(const PRUnichar *message) 
{
	NS_INIT_ISUPPORTS();
	mMessage.Assign(message);
}

nsConsoleMessage::~nsConsoleMessage() {};

NS_IMETHODIMP
nsConsoleMessage::GetMessage(PRUnichar **result) {
    *result = mMessage.ToNewUnicode();

    return NS_OK;
}

//  NS_IMETHODIMP
//  nsConsoleMessage::Init(const PRUnichar *message) {
//      nsAutoString newMessage(message);
//      mMessage = newMessage.ToNewUnicode();
//      return NS_OK;
//  }

