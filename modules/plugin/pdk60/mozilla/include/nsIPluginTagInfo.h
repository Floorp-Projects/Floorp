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

#ifndef nsIPluginTagInfo_h___
#define nsIPluginTagInfo_h___

#include "nsplugindefs.h"
#include "nsISupports.h"

class nsIDOMElement;

#define NS_IPLUGINTAGINFO_IID                        \
{ /* 5f1ec1d0-019b-11d2-815b-006008119d7a */         \
    0x5f1ec1d0,                                      \
    0x019b,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Tag Info Interface
// This interface provides information about the HTML tag on the page.
// Some day this might get superseded by a DOM API.

class nsIPluginTagInfo : public nsISupports {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGINTAGINFO_IID)
	
    // QueryInterface on nsIPluginInstancePeer to get this.

    // (Corresponds to NPP_New's argc, argn, and argv arguments.)
    // Get a ptr to the paired list of attribute names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    NS_IMETHOD
    GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values) = 0;

    /**
     * Gets the value for the named attribute.
     *
     * @param name - the name of the attribute to find
     * @param result - the resulting attribute
     * @result - NS_OK if this operation was successful, NS_ERROR_FAILURE if
     * this operation failed. result is set to NULL if the attribute is not found
     * else to the found value.
     */
    NS_IMETHOD
    GetAttribute(const char* name, const char* *result) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginTagInfo_h___ */
