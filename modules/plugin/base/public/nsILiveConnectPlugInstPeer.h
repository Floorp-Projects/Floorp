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

#ifndef nsILiveConnectPluginInstancePeer_h___
#define nsILiveConnectPluginInstancePeer_h___

#include "nsplugindefs.h"
#include "nsISupports.h"
#include "jni.h"        // standard JVM API

/**
 * The nsILiveConnectPluginInstancePeer interface is implemented by browsers
 * that support LiveConnect, i.e. scriptability via JavaScript. Note that this
 * LiveConnect interface is now JNI-based (since 5.0).
 *
 * To obtain: QueryInterface on nsIPluginInstancePeer
 */
class nsILiveConnectPluginInstancePeer : public nsISupports {
public:

    /**
     * Returns a JNI reference to the Java peer object associated with the
     * plugin instance. This object is an instance of the class specified
     * by nsIJRILiveConnectPlugin::GetJavaClass.
     *
     * (New JNI-based entry point, roughly corresponds to NPN_GetJavaPeer.)
     *
     * @param resultingJavaPeer - a resulting reference to the Java instance
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetJavaPeer(jobject *resultingJavaPeer) = 0;

};

#define NS_ILIVECONNECTPLUGININSTANCEPEER_IID        \
{ /* 1e3502a0-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x1e3502a0,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

// QueryInterface for this IID on nsIPluginManager to get a JNIEnv
// XXX change this
#define NS_IJNIENV_IID                               \
{ /* 04610650-a1bd-11d1-85b1-00805f0e4dfe */         \
    0x04610650,                                      \
    0xa1bd,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsILiveConnectPluginInstancePeer_h___ */
