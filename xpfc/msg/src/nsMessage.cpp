/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsMessage.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kMessageIID,       NS_IMESSAGE_IID);

nsMessage :: nsMessage()
{
  NS_INIT_REFCNT();
}

nsMessage :: ~nsMessage()  
{
}

NS_IMPL_ADDREF(nsMessage)
NS_IMPL_RELEASE(nsMessage)
NS_IMPL_QUERY_INTERFACE(nsMessage, kMessageIID)

nsresult nsMessage::Init()
{
  return NS_OK;
}

nsresult nsMessage::SetSender(nsString& aSender)
{
  return NS_OK;
}

nsresult nsMessage::AddRecipient(nsString& aRecipient)
{
  return NS_OK;
}

nsresult nsMessage::SetSubject(nsString& aSubject)
{
  return NS_OK;
}

nsresult nsMessage::SetBody(nsString& aBody)
{
  return NS_OK;
}

nsresult nsMessage::GetSender(nsString& aSender)
{
  return NS_OK;
}

nsresult nsMessage::GetReciepients(nsString& aRecipients)
{
  return NS_OK;
}

nsresult nsMessage::GetSubject(nsString& aSubject)
{
  return NS_OK;
}

nsresult nsMessage::GetBody(nsString& aBody)
{
  return NS_OK;
}



