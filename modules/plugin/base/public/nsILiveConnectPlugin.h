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

#ifndef nsILiveConnectPlugin_h__
#define nsILiveConnectPlugin_h__

#include "nsIPlugin.h"
#include "jni.h"        // standard JVM API

#define NS_ILIVECONNECTPLUGIN_IID                    \
{ /* cf134df0-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xcf134df0,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

/** 
 * The nsILiveConnectPlugin interface defines additional entry points that a
 * plugin developer needs to implement in order for the plugin to support 
 * JNI-based LiveConnect (new in 5.0).
 *
 * Plugin developers requiring this capability should implement this interface
 * in addition to the basic nsIPlugin interface.
 */
class nsILiveConnectPlugin : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_ILIVECONNECTPLUGIN_IID)

    /**
     * Returns the class of the Java instance to be associated with the
     * plugin.
     *
     * (New JNI-based entry point, roughly corresponds to NPP_GetJavaClass.)
     *
     * @param resultingClass - a resulting reference to the Java class
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetJavaClass(JNIEnv* env, jclass *resultingClass) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsILiveConnectPlugin_h__ */
