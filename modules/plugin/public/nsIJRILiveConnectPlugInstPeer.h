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

#ifndef nsIJRILiveConnectPluginInstancePeer_h__
#define nsIJRILiveConnectPluginInstancePeer_h__

#include "nsplugindefs.h"
#include "nsISupports.h"

////////////////////////////////////////////////////////////////////////////////
// JRI-Based LiveConnect Classes
////////////////////////////////////////////////////////////////////////////////
//
// This stuff is here so that the browser can support older JRI-based
// LiveConnected plugins (when using old plugin to new C++-style plugin
// adapter code). 
//
// Warning: Don't use this anymore, unless you're sure that you have to!
////////////////////////////////////////////////////////////////////////////////

#include "jri.h"

#define NS_IJRILIVECONNECTPLUGININSTANCEPEER_IID     \
{ /* 25b63f40-f773-11d1-815b-006008119d7a */         \
    0x25b63f40,                                      \
    0xf773,                                          \
    0x11d1,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

// QueryInterface for this IID on nsIPluginManager to get a JRIEnv
// XXX change this
#define NS_IJRIENV_IID                               \
{ /* f9d4ea00-a1bc-11d1-85b1-00805f0e4dfe */         \
    0xf9d4ea00,                                      \
    0xa1bc,                                          \
    0x11d1,                                          \
    {0x85, 0xb1, 0x00, 0x80, 0x5f, 0x0e, 0x4d, 0xfe} \
}

/**
 * The nsIJRILiveConnectPluginInstancePeer interface is implemented by browsers
 * that support JRI-based LiveConnect. Note that for 5.0, LiveConnect support
 * has become JNI-based, so this interface is effectively deprecated.
 *
 * To obtain: QueryInterface on nsIPluginInstancePeer
 */
class nsIJRILiveConnectPluginInstancePeer : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IJRILIVECONNECTPLUGININSTANCEPEER_IID)

	/**
	 * Returns a JRI env corresponding to the current Java thread of the
	 * browser.
     *
     * (Corresponds to NPN_GetJavaEnv.)
     *
     * @result - NS_OK if this operation was successful
	 */
	NS_IMETHOD
	GetJavaEnv(JRIEnv* *resultingEnv) = 0;

    /**
     * Returns a JRI reference to the Java peer object associated with the
     * plugin instance. This object is an instance of the class specified
     * by nsIJRILiveConnectPlugin::GetJavaClass.
     *
     * (Corresponds to NPN_GetJavaPeer.)
     *
     * @param resultingJavaPeer - a resulting reference to the Java instance
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetJavaPeer(jref *resultingJavaPeer) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIJRILiveConnectPluginInstancePeer_h__ */
