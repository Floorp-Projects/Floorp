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

////////////////////////////////////////////////////////////////////////////////
/**
 * <B>INTERFACE TO NETSCAPE COMMUNICATOR PLUGINS (NEW C++ API).</B>
 *
 * <P>This superscedes the old plugin API (npapi.h, npupp.h), and 
 * eliminates the need for glue files: npunix.c, npwin.cpp and npmac.cpp that
 * get linked with the plugin. You will however need to link with the "backward
 * adapter" (badapter.cpp) in order to allow your plugin to run in pre-5.0
 * browsers. 
 *
 * <P>See nsplugin.h for an overview of how this interface fits with the 
 * overall plugin architecture.
 */
////////////////////////////////////////////////////////////////////////////////

#ifndef nsIPluginStreamPeer_h___
#define nsIPluginStreamPeer_h___

#ifndef NEW_PLUGIN_STREAM_API

#include "nsplugindefs.h"
#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////
// Plugin Stream Peer Interface
// A plugin stream peer is passed to a plugin instance's NewStream call in 
// order to indicate that a new stream is to be created and be read by the
// plugin instance.

class nsIPluginStreamPeer : public nsISupports {
public:

    // (Corresponds to NPStream's url field.)
    NS_IMETHOD
    GetURL(const char* *result) = 0;

    // (Corresponds to NPStream's end field.)
    NS_IMETHOD
    GetEnd(PRUint32 *result) = 0;

    // (Corresponds to NPStream's lastmodified field.)
    NS_IMETHOD
    GetLastModified(PRUint32 *result) = 0;

    // (Corresponds to NPStream's notifyData field.)
    NS_IMETHOD
    GetNotifyData(void* *result) = 0;

    // (Corresponds to NPP_DestroyStream's reason argument.)
    NS_IMETHOD
    GetReason(nsPluginReason *result) = 0;

    // (Corresponds to NPP_NewStream's MIMEType argument.)
    NS_IMETHOD
    GetMIMEType(nsMIMEType *result) = 0;

};

#define NS_IPLUGINSTREAMPEER_IID                     \
{ /* 717b1e90-019b-11d2-815b-006008119d7a */         \
    0x717b1e90,                                      \
    0x019b,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif // !NEW_PLUGIN_STREAM_API

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginStreamPeer_h___ */
