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

#ifndef nsIHttpURL_h___
#define nsIHttpURL_h___

#include "nscore.h"
#include "nsISupports.h"
#include "nspr.h"

/* 1A0B6FA1-EA25-11d1-BEAE-00805F8A66DC */

#define NS_IHTTPURL_IID                                 \
{ 0x1a0b6fa1, 0xea25, 0x11d1,                           \
    { 0xbe, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0xdc } }

struct nsIHttpURL : public nsISupports
{
    /**
      * Parse the mime header into the url struct.
      * This method is intended to be used when an HTML META tag is encoutered
      * with the type http-equiv. In this case, the http-equiv header should
      * be added to the url in netlib, immediately after the http-equiv meta
      * tag is encoutered.
      * 
      * @param shell The shell loading this url.
      * @param url The url to parse the mime header into.
      * @param name The name of the mime header.
      * @param value The value of the mime header.
      */
    NS_IMETHOD AddMimeHeader(const char *name, const char *value) = 0;
};

/** Create a new HttpURL. */
extern "C" NS_NET nsresult NS_NewHttpURL(nsISupports** aInstancePtrResult,
                                         nsISupports* aOuter);

#endif /* nsIHttpURL_h___ */
