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

#ifndef nsIProtocolConnection_h___
#define nsIProtocolConnection_h___

#include "nscore.h"
#include "nsISupports.h"


/* forward declaration */
struct URL_Struct_;

/* 121CC0F1-EA26-11d1-BEAE-00805F8A66DC */
#define NS_IPROTOCOLCONNECTION_IID                      \
{ 0x121cc0f1, 0xea26, 0x11d1,                           \
  { 0xbe, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0xdc } }



class nsIProtocolConnection : public nsISupports
{
public:
  NS_IMETHOD InitializeURLInfo(URL_Struct_ *URL_s) = 0;
  NS_IMETHOD GetURLInfo(URL_Struct_ **aResult)     = 0;
};

#endif /* nsIIProtocolConnection_h___ */
