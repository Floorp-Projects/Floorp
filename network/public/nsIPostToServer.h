/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
