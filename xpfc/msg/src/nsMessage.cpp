/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s): 
 */

#include "nsMessage.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kISupportsIID,     NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kMessageIID,       NS_IMESSAGE_IID);

nsMessage :: nsMessage()
{
  NS_INIT_REFCNT();

  mSender = "";
  mRecipients = "";
  mSubject = "";
  mBody = "";

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
  mSender = aSender;
  return NS_OK;
}

nsresult nsMessage::AddRecipient(nsString& aRecipient)
{
  if (mRecipients.Length() > 0)
    mRecipients += ",";
  mRecipients += aRecipient;

  return NS_OK;
}

nsresult nsMessage::SetSubject(nsString& aSubject)
{
  mSubject = aSubject;
  return NS_OK;
}

nsresult nsMessage::SetBody(nsString& aBody)
{
  mBody = aBody;
  return NS_OK;
}

nsresult nsMessage::GetSender(nsString& aSender)
{
  aSender = mSender;
  return NS_OK;
}

nsresult nsMessage::GetRecipients(nsString& aRecipients)
{
  aRecipients = mRecipients;
  return NS_OK;
}

nsresult nsMessage::GetSubject(nsString& aSubject)
{
  aSubject = mSubject;
  return NS_OK;
}

nsresult nsMessage::GetBody(nsString& aBody)
{
  aBody = mBody;
  return NS_OK;
}



