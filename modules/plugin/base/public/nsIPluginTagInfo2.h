/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

#include "nsIPluginTagInfo.h"

////////////////////////////////////////////////////////////////////////////////
// Plugin Tag Info Interface
// These extensions to nsIPluginTagInfo are only available in Communicator 5.0.

enum nsPluginTagType {
    nsPluginTagType_Unknown,
    nsPluginTagType_Embed,
    nsPluginTagType_Object,
    nsPluginTagType_Applet
};

#define NS_IPLUGINTAGINFO2_IID                       \
{ /* 6a49c9a0-019b-11d2-815b-006008119d7a */         \
    0x6a49c9a0,                                      \
    0x019b,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

class nsIPluginTagInfo2 : public nsIPluginTagInfo {
public:
	NS_DEFINE_STATIC_IID_ACCESSOR(NS_IPLUGINTAGINFO2_IID)

    // QueryInterface on nsIPluginInstancePeer to get this.

    // Get the type of the HTML tag that was used ot instantiate this
    // plugin.  Currently supported tags are EMBED, OBJECT and APPLET.
    NS_IMETHOD
    GetTagType(nsPluginTagType *result) = 0;

    // Get the complete text of the HTML tag that was
    // used to instantiate this plugin
    NS_IMETHOD
    GetTagText(const char* *result) = 0;

    // Get a ptr to the paired list of parameter names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    NS_IMETHOD
    GetParameters(PRUint16& n, const char*const*& names, const char*const*& values) = 0;

    // Get the value for the named parameter.  Returns null
    // if the parameter was not set.
    // @result - NS_OK if this operation was successful, NS_ERROR_FAILURE if
    // this operation failed. result is set to NULL if the attribute is not found
    // else to the found value.
    NS_IMETHOD
    GetParameter(const char* name, const char* *result) = 0;
    
    NS_IMETHOD
    GetDocumentBase(const char* *result) = 0;
    
    // Return an encoding whose name is specified in:
    // http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303
    NS_IMETHOD
    GetDocumentEncoding(const char* *result) = 0;
    
    NS_IMETHOD
    GetAlignment(const char* *result) = 0;
    
    NS_IMETHOD
    GetWidth(PRUint32 *result) = 0;
    
    NS_IMETHOD
    GetHeight(PRUint32 *result) = 0;
    
    NS_IMETHOD
    GetBorderVertSpace(PRUint32 *result) = 0;
    
    NS_IMETHOD
    GetBorderHorizSpace(PRUint32 *result) = 0;

    // Returns a unique id for the current document on which the
    // plugin is displayed.
    NS_IMETHOD
    GetUniqueID(PRUint32 *result) = 0;

    /**
     * Returns the DOM element corresponding to the tag which references
     * this plugin in the document.
     *
     * REMIND: do we need to expose as an nsISupports * to avoid
     * introducing runtime dependencies on XPCOM?
     *
     * @param result - resulting DOM element
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetDOMElement(nsIDOMElement* *result) = 0;
};

////////////////////////////////////////////////////////////////////////////////

#endif /* nsIPluginTagInfo2_h___ */
