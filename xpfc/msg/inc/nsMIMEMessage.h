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

#ifndef nsMIMEMessage_h___
#define nsMIMEMessage_h___

#include "nsMessage.h"
#include "nsIMIMEMessage.h"
#include "nsIMIMEBodyPart.h"
#include "mime.h"

class nsMIMEMessage : public nsMessage,
                      public nsIMIMEMessage
{
public:
  nsMIMEMessage();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() ;

  NS_IMETHOD AddHeader(nsString& aHeaderName, nsString& aHeaderValue);
  NS_IMETHOD RemoveHeader(nsString& aHeaderName);
  NS_IMETHOD GetHeader(nsString& aHeaderName, nsString& aHeaderValue);

  NS_IMETHOD AddAttachment(nsString& aAttachment, nsMIMEEncoding aMIMEEncoding = nsMIMEEncoding_default);
  NS_IMETHOD AddText(nsString& aText, nsMIMEEncoding aMIMEEncoding = nsMIMEEncoding_default);
  NS_IMETHOD SetBody(nsString& aBody);

  NS_IMETHOD AddBodyPart(nsIMIMEBodyPart& aBodyPart);

  NS_IMETHOD GetBodyType(nsMIMEBodyType& aBodyType) ;
  NS_IMETHOD Encode();

protected:
  ~nsMIMEMessage();

private:
  nsMIMEBodyType mBodyType;
  nsIMIMEBodyPart * mBodyPart;

public:
  mime_message_t * mMimeMessageT;
  nsmail_outputstream_t * mMimeMessageStreamT;

};

#endif /* nsMIMEMessage_h___ */
