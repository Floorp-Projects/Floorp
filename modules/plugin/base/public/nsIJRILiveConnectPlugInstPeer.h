/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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
