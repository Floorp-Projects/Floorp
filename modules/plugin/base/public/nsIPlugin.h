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

#ifndef nsIPlugin_h___
#define nsIPlugin_h___

#include "nsplugindefs.h"
#include "nsIFactory.h"

// {df773070-0199-11d2-815b-006008119d7a}
#define NS_IPLUGIN_IID \
{ 0xdf773070, 0x0199, 0x11d2, { 0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a } }

// {ffc63200-cf09-11d2-a5a0-bc8f7ad21dfc}
#define NS_PLUGIN_CID \
{ 0xffc63200, 0xcf09, 0x11d2, { 0xa5, 0xa0, 0xbc, 0x8f, 0x7a, 0xd2, 0x1d, 0xfc } }

// Prefix for ContractID of all plugins
#define NS_INLINE_PLUGIN_CONTRACTID_PREFIX "@mozilla.org/inline-plugin/"

/**
 * The nsIPlugin interface is the minimum interface plugin developers need to 
 * support in order to implement a plugin. The plugin manager may QueryInterface 
 * for more specific plugin types, e.g. nsILiveConnectPlugin.
 *
 * The old NPP_New plugin operation is now subsumed by two operations:
 *
 * CreateInstance -- called once, after the plugin instance is created. This 
 * method is used to initialize the new plugin instance (although the actual
 * plugin instance object will be created by the plugin manager).
 *
 * nsIPluginInstance::Start -- called when the plugin instance is to be
 * started. This happens in two circumstances: (1) after the plugin instance
 * is first initialized, and (2) after a plugin instance is returned to
 * (e.g. by going back in the window history) after previously being stopped
 * by the Stop method. 
 */
class nsIPlugin : public nsIFactory {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGIN_IID)

	/**
	 * Creates a new plugin instance, based on a MIME type. This
	 * allows different impelementations to be created depending on
	 * the specified MIME type.
	 */
    NS_IMETHOD CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, 
                                    const char* aPluginMIMEType,
                                    void **aResult) = 0;

    /**
     * Initializes the plugin and will be called before any new instances are
     * created. It is passed browserInterfaces on which QueryInterface
     * may be used to obtain an nsIPluginManager, and other interfaces.
     *
     * @param browserInterfaces - an object that allows access to other browser
     * interfaces via QueryInterface
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Initialize() = 0;

    /**
     * Called when the browser is done with the plugin factory, or when
     * the plugin is disabled by the user.
     *
     * (Corresponds to NPP_Shutdown.)
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Shutdown(void) = 0;

    /**
     * Returns the MIME description for the plugin. The MIME description 
     * is a colon-separated string containg the plugin MIME type, plugin
     * data file extension, and plugin name, e.g.:
     *
     * "application/x-simple-plugin:smp:Simple LiveConnect Sample Plug-in"
     *
     * (Corresponds to NPP_GetMIMEDescription.)
     *
     * @param resultingDesc - the resulting MIME description 
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetMIMEDescription(const char* *resultingDesc) = 0;

    /**
     * Returns the value of a variable associated with the plugin.
     *
     * (Corresponds to NPP_GetValue.)
     *
     * @param variable - the plugin variable to get
     * @param value - the address of where to store the resulting value
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetValue(nsPluginVariable variable, void *value) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPlugin_h___ */
