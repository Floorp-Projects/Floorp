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

#ifndef nsMIMEBasicBodyPart_h___
#define nsMIMEBasicBodyPart_h___

#include "nsMIMEBodyPart.h"
#include "nsString.h"
#include "mime.h"

class nsMIMEBasicBodyPart : public nsMIMEBodyPart
{
public:
  nsMIMEBasicBodyPart();

  NS_DECL_ISUPPORTS

  NS_IMETHOD Init() ;
  NS_IMETHOD SetBody(nsString& aBody) ;

protected:
  ~nsMIMEBasicBodyPart();

public:
  mime_basicPart_t * mMimeBasicPart ;

};

#endif /* nsMIMEBasicBodyPart_h___ */
