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

#ifndef nsMessage_h___
#define nsMessage_h___

#include "nsIMessage.h"

class nsMessage : public nsIMessage
{
public:
  nsMessage();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() ;

  NS_IMETHOD SetSender(nsString& aSender);
  NS_IMETHOD AddRecipient(nsString& aRecipient);
  NS_IMETHOD SetSubject(nsString& aSubject);
  NS_IMETHOD SetBody(nsString& aBody);

  NS_IMETHOD GetSender(nsString& aSender);
  NS_IMETHOD GetRecipients(nsString& aRecipients);
  NS_IMETHOD GetSubject(nsString& aSubject);
  NS_IMETHOD GetBody(nsString& aBody);

protected:
  ~nsMessage();

private:
  nsString mSender;
  nsString mRecipients;
  nsString mSubject;
  nsString mBody;

};

#endif /* nsMessage_h___ */
