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

#ifndef nsIMIMEMessage_h___
#define nsIMIMEMessage_h___

#include "nsISupports.h"
#include "nsString.h"
#include "nsIMIMEBodyPart.h"

//d69d9a40-7027-11d2-8dbc-00805f8a7ab6
#define NS_IMIME_MESSAGE_IID   \
{ 0xd69d9a40, 0x7027, 0x11d2,    \
{ 0x8d, 0xbc, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

typedef enum
{
  nsMIMEBodyType_empty = 0,
  nsMIMEBodyType_basicpart = 1,
  nsMIMEBodyType_multipart = 2,
  nsMIMEBodyType_messagepart = 3
} nsMIMEBodyType;

typedef enum
{
  nsMIMEEncoding_default            = 0,
  nsMIMEEncoding_quoted_printable   = 1,
  nsMIMEEncoding_base_64            = 2,
  nsMIMEEncoding_binary             = 3,
  nsMIMEEncoding_e7bit              = 4,
  nsMIMEEncoding_e8bit              = 5,
  nsMIMEEncoding_none               = 6
} nsMIMEEncoding;



class nsIMIMEMessage : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

  NS_IMETHOD AddHeader(nsString& aHeaderName, nsString& aHeaderValue) = 0;
  NS_IMETHOD RemoveHeader(nsString& aHeaderName) = 0;
  NS_IMETHOD GetHeader(nsString& aHeaderName, nsString& aHeaderValue) = 0;

  NS_IMETHOD AddAttachment(nsString& aAttachment, nsMIMEEncoding aMIMEEncoding = nsMIMEEncoding_default) = 0;
  NS_IMETHOD AddText(nsString& aText, nsMIMEEncoding aMIMEEncoding = nsMIMEEncoding_default) = 0;

  NS_IMETHOD AddBodyPart(nsIMIMEBodyPart& aBodyPart) = 0;

  NS_IMETHOD GetBodyType(nsMIMEBodyType& aBodyType) = 0;

  NS_IMETHOD Encode() = 0;

};

#endif /* nsIMIMEMessage_h___ */
