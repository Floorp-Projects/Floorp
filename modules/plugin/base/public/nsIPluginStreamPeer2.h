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

#ifndef nsIPluginStreamPeer2_h___
#define nsIPluginStreamPeer2_h___

#ifndef NEW_PLUGIN_STREAM_API

#include "nsIPluginStreamPeer.h"

////////////////////////////////////////////////////////////////////////////////
// Plugin Stream Peer Interface
// These extensions to nsIPluginStreamPeer are only available in Communicator 5.0.

class nsIPluginStreamPeer2 : public nsIPluginStreamPeer {
public:

    NS_IMETHOD
    GetContentLength(PRUint32 *result) = 0;

    NS_IMETHOD
    GetHeaderFieldCount(PRUint32 *result) = 0;

    NS_IMETHOD
    GetHeaderFieldKey(PRUint32 index, const char* *result) = 0;

    NS_IMETHOD
    GetHeaderField(PRUint32 index, const char* *result) = 0;

};

#define NS_IPLUGINSTREAMPEER2_IID                    \
{ /* 77083af0-019b-11d2-815b-006008119d7a */         \
    0x77083af0,                                      \
    0x019b,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif // !NEW_PLUGIN_STREAM_API

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginStreamPeer2_h___ */
