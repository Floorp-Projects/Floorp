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

#ifndef nsIPluginStream_h___
#define nsIPluginStream_h___

#ifndef NEW_PLUGIN_STREAM_API

#include "nsplugindefs.h"
#include "nsIOutputStream.h"

/**
 * The nsIPluginStream interface specifies the minimal set of operations that
 * must be implemented by a plugin stream in order to receive data from the
 * browser. When a nsIPluginManager::GetURL request is made, a subsequent
 * nsIPluginInstance::NewStream request will be made to instruct the plugin
 * instance to construct a new stream to receive the data. 
 */
class nsIPluginStream : public nsIOutputStream {
public:

    /**
     * Returns the stream type of a stream. 
     *
     * (Corresponds to NPP_NewStream's stype return parameter.)
     *
     * @param result - the resulting stream type
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetStreamType(nsPluginStreamType *result) = 0;

    /**
     * This operation passes to the plugin the name of the file which
     * contains the stream data.
     *
     * (Corresponds to NPP_StreamAsFile.)
     *
     * @param fname - the file name
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    AsFile(const char* fname) = 0;

};

#define NS_IPLUGINSTREAM_IID                         \
{ /* f287dd50-0199-11d2-815b-006008119d7a */         \
    0xf287dd50,                                      \
    0x0199,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

#endif // !NEW_PLUGIN_STREAM_API

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginStream_h___ */
