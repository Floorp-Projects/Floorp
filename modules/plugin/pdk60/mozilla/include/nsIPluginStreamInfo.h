/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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


#ifndef nsIPluginStreamInfo_h___
#define nsIPluginStreamInfo_h___

#include "nsplugindefs.h"
#include "nsISupports.h"

class nsIPluginStreamInfo : public nsISupports {
public:

	NS_IMETHOD
	GetContentType(nsMIMEType* result) = 0;

	NS_IMETHOD
	IsSeekable(PRBool* result) = 0;

	NS_IMETHOD
	GetLength(PRUint32* result) = 0;

	NS_IMETHOD
	GetLastModified(PRUint32* result) = 0;

	NS_IMETHOD
	GetURL(const char** result) = 0;

	NS_IMETHOD
	RequestRead(nsByteRange* rangeList) = 0;
};

#define NS_IPLUGINSTREAMINFO_IID                 \
{ /* ed7d4ca0-b005-11d2-adaa-00805f6dec49 */         \
    0xed7d4ca0,                                      \
    0xb005,                                          \
    0x11d2,                                          \
    {0xad, 0xaa, 0x00, 0x80, 0x5f, 0x6d, 0xec, 0x49} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginStreamInfo_h___ */
