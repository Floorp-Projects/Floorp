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

#ifndef nsIPostToServer_h___
#define nsIPostToServer_h___

#include "nscore.h"
#include "nsISupports.h"

#include "nspr.h"

/* EADF7B41-EBC0-11d1-BEAE-00805F8A66DC */
#define NS_IPOSTTOSERVER_IID                            \
{ 0xeadf7b41, 0xebc0, 0x11d1,                           \
    { 0xbe, 0xae, 0x0, 0x80, 0x5f, 0x8a, 0x66, 0xdc } }



struct nsIPostToServer : public nsISupports
{
	static const nsIID& GetIID() { static nsIID iid = NS_IPOSTTOSERVER_IID; return iid; }

    NS_IMETHOD SendFile(const char *aFile) = 0;
    NS_IMETHOD SendData(const char *aBuffer, PRUint32 aLength) = 0;
    NS_IMETHOD SendDataFromFile(const char *aFile) = 0;
};

#define NS_IPOSTTOSERVER_ALREADY_POSTING  NS_ERROR_GENERATE_SUCCESS(NS_ERROR_MODULE_NETWORK, 1)


#endif /* nsIPostToServer_h___ */
