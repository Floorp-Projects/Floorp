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

#include "nsPluginTagInfo.h"
#ifdef OJI
#include "nsJVMPluginTagInfo.h"
#endif
#include "intl_csi.h"
#include "plstr.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID); 
static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kIPluginTagInfo2IID, NS_IPLUGINTAGINFO2_IID); 
static NS_DEFINE_IID(kIJVMPluginTagInfoIID, NS_IJVMPLUGINTAGINFO_IID); 

////////////////////////////////////////////////////////////////////////////////
// Plugin Tag Info Interface

nsPluginTagInfo::nsPluginTagInfo(NPP npp)
    : fJVMPluginTagInfo(NULL), npp(npp), fUniqueID(0)
{
    NS_INIT_AGGREGATED(NULL);
}

nsPluginTagInfo::~nsPluginTagInfo(void)
{
}

NS_IMPL_AGGREGATED(nsPluginTagInfo);

NS_METHOD
nsPluginTagInfo::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    } 
    if (aIID.Equals(kIPluginTagInfo2IID) ||
        aIID.Equals(kIPluginTagInfoIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*)(nsISupports*)this; 
        AddRef(); 
        return NS_OK; 
    } 
#ifdef OJI
    // Aggregates...
    if (fJVMPluginTagInfo == NULL)
        nsJVMPluginTagInfo::Create((nsISupports*)this, kISupportsIID,
                                   (void**)&fJVMPluginTagInfo, this);
    if (fJVMPluginTagInfo &&
        fJVMPluginTagInfo->QueryInterface(aIID, aInstancePtr) == NS_OK)
        return NS_OK;
#endif
    return NS_NOINTERFACE;
}

static char* empty_list[] = { "", NULL };

NS_METHOD
nsPluginTagInfo::GetAttributes(PRUint16& n, 
                               const char*const*& names, 
                               const char*const*& values)
{
    np_instance* instance = (np_instance*)npp->ndata;

#if 0
    // defense
    PR_ASSERT( 0 != names );
    PR_ASSERT( 0 != values );
    if( 0 == names || 0 == values )
        return 0;
#endif

    if (instance->type == NP_EMBED) {
        np_data* ndata = (np_data*)instance->app->np_data;

        names = (const char*const*)ndata->lo_struct->attributes.names;
        values = (const char*const*)ndata->lo_struct->attributes.values;
        n = (PRUint16)ndata->lo_struct->attributes.n;

        return NS_OK;
    } else {
        static char _name[] = "PALETTE";
        static char* _names[1];

        static char _value[] = "foreground";
        static char* _values[1];

        _names[0] = _name;
        _values[0] = _value;

        names = (const char*const*) _names;
        values = (const char*const*) _values;
        n = 1;

        return NS_OK;
    }

    // random, sun-spot induced error
    PR_ASSERT( 0 );

    n = 0;
    // const char* const* empty_list = { { '\0' } };
    names = values = (const char*const*)empty_list;

    return NS_ERROR_FAILURE;
}

NS_METHOD
nsPluginTagInfo::GetAttribute(const char* name, const char* *result) 
{
    PRUint16 nAttrs, i;
    const char*const* names;
    const char*const* values;

    nsresult rslt = GetAttributes(nAttrs, names, values);
    if (rslt != NS_OK)
        return rslt;
    
    *result = NULL;
    for( i = 0; i < nAttrs; i++ ) {
        if (PL_strcasecmp(name, names[i]) == 0) {
            *result = values[i];
            return NS_OK;
        }
    }

    return NS_ERROR_FAILURE;
}

NS_METHOD
nsPluginTagInfo::GetTagType(nsPluginTagType *result)
{
    *result = nsPluginTagType_Unknown;
    switch (GetLayoutElement()->type) {
      case LO_JAVA:
        *result = nsPluginTagType_Applet;
        return NS_OK;
      case LO_EMBED:
        *result = nsPluginTagType_Embed;
        return NS_OK;
      case LO_OBJECT:
        *result = nsPluginTagType_Object;
        return NS_OK;
      default:
        return NS_OK;
    }
}

NS_METHOD
nsPluginTagInfo::GetTagText(const char* *result)
{
    return NS_ERROR_NOT_IMPLEMENTED;    // XXX
}

NS_METHOD
nsPluginTagInfo::GetParameters(PRUint16& n, 
                               const char*const*& names, 
                               const char*const*& values)
{
    np_instance* instance = (np_instance*)npp->ndata;

    if (instance->type == NP_EMBED) {
        np_data* ndata = (np_data*)instance->app->np_data;

        names = (const char*const*)ndata->lo_struct->parameters.names;
        values = (const char*const*)ndata->lo_struct->parameters.values;
        n = (PRUint16)ndata->lo_struct->parameters.n;

        return NS_OK;
    } else {
        static char _name[] = "PALETTE";
        static char* _names[1];

        static char _value[] = "foreground";
        static char* _values[1];

        _names[0] = _name;
        _values[0] = _value;

        names = (const char*const*) _names;
        values = (const char*const*) _values;
        n = 1;

        return NS_OK;
    }

    // random, sun-spot induced error
    PR_ASSERT( 0 );

    n = 0;
    // static const char* const* empty_list = { { '\0' } };
    names = values = (const char*const*)empty_list;

    return NS_ERROR_FAILURE;
}

NS_METHOD
nsPluginTagInfo::GetParameter(const char* name, const char* *result) 
{
    PRUint16 nParams, i;
    const char*const* names;
    const char*const* values;

    nsresult rslt = GetParameters(nParams, names, values);
    if (rslt != NS_OK)
        return rslt;

    *result = NULL;
    for( i = 0; i < nParams; i++ ) {
        if (PL_strcasecmp(name, names[i]) == 0) {
            *result = values[i];
            return NS_OK;
        }
    }

    return NS_ERROR_FAILURE;
}

NS_METHOD
nsPluginTagInfo::GetDocumentBase(const char* *result)
{
    *result = (const char*)GetLayoutElement()->base_url;
    return NS_OK;
}

NS_METHOD
nsPluginTagInfo::GetDocumentEncoding(const char* *result)
{
    np_instance* instance = (np_instance*) npp->ndata;
    MWContext* cx = instance->cx;
    INTL_CharSetInfo info = LO_GetDocumentCharacterSetInfo(cx);
    int16 doc_csid = INTL_GetCSIWinCSID(info);
    *result = INTL_CharSetIDToJavaCharSetName(doc_csid);
    return NS_OK;
}

NS_METHOD
nsPluginTagInfo::GetAlignment(const char* *result)
{
    int alignment = GetLayoutElement()->alignment;

    const char* cp;
    switch (alignment) {
      case LO_ALIGN_CENTER:      cp = "abscenter"; break;
      case LO_ALIGN_LEFT:        cp = "left"; break;
      case LO_ALIGN_RIGHT:       cp = "right"; break;
      case LO_ALIGN_TOP:         cp = "texttop"; break;
      case LO_ALIGN_BOTTOM:      cp = "absbottom"; break;
      case LO_ALIGN_NCSA_CENTER: cp = "center"; break;
      case LO_ALIGN_NCSA_BOTTOM: cp = "bottom"; break;
      case LO_ALIGN_NCSA_TOP:    cp = "top"; break;
      default:                   cp = "baseline"; break;
    }
    *result = cp;
    return NS_OK;
}

NS_METHOD
nsPluginTagInfo::GetWidth(PRUint32 *result)
{
    LO_CommonPluginStruct* lo = GetLayoutElement();
    *result = lo->width ? lo->width : 50;
    return NS_OK;
}
    
NS_METHOD
nsPluginTagInfo::GetHeight(PRUint32 *result)
{
    LO_CommonPluginStruct* lo = GetLayoutElement();
    *result = lo->height ? lo->height : 50;
    return NS_OK;
}

NS_METHOD
nsPluginTagInfo::GetBorderVertSpace(PRUint32 *result)
{
    *result = GetLayoutElement()->border_vert_space;
    return NS_OK;
}
    
NS_METHOD
nsPluginTagInfo::GetBorderHorizSpace(PRUint32 *result)
{
    *result = GetLayoutElement()->border_horiz_space;
    return NS_OK;
}

NS_METHOD
nsPluginTagInfo::GetUniqueID(PRUint32 *result)
{
    if (fUniqueID == 0) {
        np_instance* instance = (np_instance*) npp->ndata;
        MWContext* cx = instance->cx;
        History_entry* history_element = SHIST_GetCurrent(&cx->hist);
        if (history_element) {
            fUniqueID = history_element->unique_id;
        } else {
            /*
            ** XXX What to do? This can happen for instance when printing a
            ** mail message that contains an applet.
            */
            static int32 unique_number;
            fUniqueID = --unique_number;
        }
        PR_ASSERT(fUniqueID != 0);
    }
    *result = fUniqueID;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
