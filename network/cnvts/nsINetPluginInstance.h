/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#ifndef nsINetPluginInstance_h___
#define nsINetPluginInstance_h___

#include "nsINetOStream.h"
#include "nsISupports.h"
#include "nsplugindefs.h"


class nsINetPluginInstance : public nsISupports {
public:

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

#define NS_INETPLUGININSTANCE_IID                       \
{ /* ebe00f40-0199-11d2-815b-006008219d7a */         \
    0xebe00f40,                                      \
    0x0199,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x21, 0x9d, 0x7a} \
}


////////////////////////////////////////////////////////////////////////////////

#endif /* nsINetPluginInstance_h___ */
