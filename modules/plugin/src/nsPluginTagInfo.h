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

#ifndef nsPluginTagInfo_h__
#define nsPluginTagInfo_h__

#include "nsIPluginTagInfo2.h"
#include "nsAgg.h"
#include "npglue.h"

class nsPluginTagInfo : public nsIPluginTagInfo2 {
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginTagInfo:

    // Get a ptr to the paired list of attribute names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    //
    NS_IMETHOD
    GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values);

    // Get the value for the named attribute.  Returns null
    // if the attribute was not set.
    NS_IMETHOD
    GetAttribute(const char* name, const char* *result);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginTagInfo2:

    // Get the type of the HTML tag that was used ot instantiate this
    // plugin.  Currently supported tags are EMBED, OBJECT and APPLET.
    NS_IMETHOD
    GetTagType(nsPluginTagType *result);

    // Get the complete text of the HTML tag that was
    // used to instantiate this plugin
    NS_IMETHOD
    GetTagText(const char * *result);

    // Get a ptr to the paired list of parameter names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    NS_IMETHOD
    GetParameters(PRUint16& n, const char*const*& names, const char*const*& values);

    // Get the value for the named parameter.  Returns null
    // if the parameter was not set.
    NS_IMETHOD
    GetParameter(const char* name, const char* *result);
    
    NS_IMETHOD
    GetDocumentBase(const char* *result);
    
    // Return an encoding whose name is specified in:
    // http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303
    NS_IMETHOD
    GetDocumentEncoding(const char* *result);
    
    NS_IMETHOD
    GetAlignment(const char* *result);
    
    NS_IMETHOD
    GetWidth(PRUint32 *result);
    
    NS_IMETHOD
    GetHeight(PRUint32 *result);
    
    NS_IMETHOD
    GetBorderVertSpace(PRUint32 *result);
    
    NS_IMETHOD
    GetBorderHorizSpace(PRUint32 *result);

    // Returns a unique id for the current document on which the
    // plugin is displayed.
    NS_IMETHOD
    GetUniqueID(PRUint32 *result);

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginTagInfo specific methods:
    
    nsPluginTagInfo(NPP npp);
    virtual ~nsPluginTagInfo(void);

    NS_DECL_AGGREGATED

protected:
    LO_CommonPluginStruct* GetLayoutElement(void)
    {
        np_instance* instance = (np_instance*) npp->ndata;
        NPEmbeddedApp* app = instance->app;
        np_data* ndata = (np_data*) app->np_data;
        return (LO_CommonPluginStruct*)ndata->lo_struct;
    }

    // aggregated interfaces:
    nsISupports*        fJVMPluginTagInfo;

    NPP                 npp;
    PRUint32            fUniqueID;
};

#endif // nsPluginTagInfo_h__
