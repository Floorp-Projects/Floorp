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

#ifndef nsINetPluginInstance_h___
#define nsINetPluginInstance_h___

#include "nsINetOStream.h"
#include "nsISupports.h"
#include "nsplugindefs.h"


#define NS_INETPLUGININSTANCE_IID                       \
{ /* ebe00f40-0199-11d2-815b-006008219d7a */         \
    0xebe00f40,                                      \
    0x0199,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x21, 0x9d, 0x7a} \
}

class nsINetPluginInstance : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_INETPLUGININSTANCE_IID; return iid; }

    NS_IMETHOD
    Initialize(nsINetOStream* out_stream, const char *stream_name) = 0;

    NS_IMETHOD
    GetMIMEOutput(const char* *result, const char *stream_name) = 0;

    NS_IMETHOD
    Start(void) = 0;

    NS_IMETHOD
    Stop(void) = 0;

    NS_IMETHOD
    Destroy(void) = 0;

};


////////////////////////////////////////////////////////////////////////////////

#endif /* nsINetPluginInstance_h___ */
