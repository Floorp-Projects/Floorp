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

#ifndef nsISeekablePluginStreamPeer_h___
#define nsISeekablePluginStreamPeer_h___

#ifndef NEW_PLUGIN_STREAM_API

#include "nsplugindefs.h"
#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////
// Seekable Plugin Stream Peer Interface
// The browser implements this subclass of plugin stream peer if a stream
// is seekable. Plugins can query interface for this type, and call the 
// RequestRead method to seek to a particular position in the stream.

class nsISeekablePluginStreamPeer : public nsISupports {
public:

    // QueryInterface for this class corresponds to NPP_NewStream's 
    // seekable argument.

    // (Corresponds to NPN_RequestRead.)
    NS_IMETHOD
    RequestRead(nsByteRange* rangeList) = 0;

};

#define NS_ISEEKABLEPLUGINSTREAMPEER_IID             \
{ /* 7e028d20-019b-11d2-815b-006008119d7a */         \
    0x7e028d20,                                      \
    0x019b,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif // !NEW_PLUGIN_STREAM_API

////////////////////////////////////////////////////////////////////////////////

#endif /* nsISeekablePluginStreamPeer_h___ */
