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

#ifndef nsIMIMEBodyPart_h___
#define nsIMIMEBodyPart_h___

#include "nsISupports.h"
#include "nsString.h"

//4804d230-703a-11d2-8dbc-00805f8a7ab6
#define NS_IMIME_BODY_PART_IID   \
{ 0x4804d230, 0x703a, 0x11d2,    \
{ 0x8d, 0xbc, 0x00, 0x80, 0x5f, 0x8a, 0x7a, 0xb6 } }

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


class nsIMIMEBodyPart : public nsISupports
{

public:

  NS_IMETHOD Init() = 0;

};

#endif /* nsIMIMEBodyPart_h___ */
