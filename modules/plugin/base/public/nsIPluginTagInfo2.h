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

#ifndef nsIPluginTagInfo2_h___
#define nsIPluginTagInfo2_h___

#include "nsplugindefs.h"

////////////////////////////////////////////////////////////////////////////////
// Plugin Tag Info Interface
// These extensions to nsIPluginTagInfo are only available in Communicator 5.0.

class nsIPluginTagInfo2 : public nsIPluginTagInfo {
public:

    // QueryInterface on nsIPluginInstancePeer to get this.

    // Get the type of the HTML tag that was used ot instantiate this
    // plugin.  Currently supported tags are EMBED, OBJECT and APPLET.
    NS_IMETHOD_(nsPluginTagType) 
    GetTagType(void) = 0;

    // Get the complete text of the HTML tag that was
    // used to instantiate this plugin
    NS_IMETHOD_(const char *)
    GetTagText(void) = 0;

    // Get a ptr to the paired list of parameter names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    NS_IMETHOD_(nsPluginError)
    GetParameters(PRUint16& n, const char*const*& names, const char*const*& values) = 0;

    // Get the value for the named parameter.  Returns null
    // if the parameter was not set.
    NS_IMETHOD_(const char*)
    GetParameter(const char* name) = 0;
    
    NS_IMETHOD_(const char*)
    GetDocumentBase(void) = 0;
    
    // Return an encoding whose name is specified in:
    // http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303
    NS_IMETHOD_(const char*)
    GetDocumentEncoding(void) = 0;
    
    NS_IMETHOD_(const char*)
    GetAlignment(void) = 0;
    
    NS_IMETHOD_(PRUint32)
    GetWidth(void) = 0;
    
    NS_IMETHOD_(PRUint32)
    GetHeight(void) = 0;
    
    NS_IMETHOD_(PRUint32)
    GetBorderVertSpace(void) = 0;
    
    NS_IMETHOD_(PRUint32)
    GetBorderHorizSpace(void) = 0;

    // Returns a unique id for the current document on which the
    // plugin is displayed.
    NS_IMETHOD_(PRUint32)
    GetUniqueID(void) = 0;

};

#define NS_IPLUGINTAGINFO2_IID                       \
{ /* 6a49c9a0-019b-11d2-815b-006008119d7a */         \
    0x6a49c9a0,                                      \
    0x019b,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginTagInfo2_h___ */
