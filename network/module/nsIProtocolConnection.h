/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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



struct nsIProtocolConnection : public nsISupports
{
    NS_IMETHOD InitializeURLInfo(URL_Struct_ *URL_s) = 0;
};

#endif /* nsIHttpUrl_h___ */
