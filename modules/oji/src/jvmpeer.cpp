/* -*- Mode: C++; tb-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
// Plugin Manager Methods to support the JVM Plugin API
////////////////////////////////////////////////////////////////////////////////

#include "lo_ele.h"
#include "layout.h"
#include "shist.h"
#include "jvmmgr.h"

JVMInstancePeer::JVMInstancePeer(nsISupports* outer,
                                 MWContext* cx, LO_CommonPluginStruct* lo)
    : fContext(cx), fLayoutElement(lo), fUniqueID(0)
{
    NS_INIT_AGGREGATED(outer);
}

JVMInstancePeer::~JVMInstancePeer(void)
{
}

NS_IMPL_AGGREGATED(JVMInstancePeer);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIJVMPluginInstancePeerIID, NP_IJVMPLUGININSTANCEPEER_IID);
static NS_DEFINE_IID(kIPluginInstancePeerIID, NP_IPLUGININSTANCEPEER_IID);

NS_METHOD
JVMInstancePeer::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (aIID.Equals(kIJVMPluginInstancePeerIID) ||
        aIID.Equals(kIPluginInstancePeerIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = this;
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_METHOD
JVMInstancePeer::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr,
                        MWContext* cx, LO_CommonPluginStruct* lo)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?

    JVMInstancePeer* jvmInstPeer = new JVMInstancePeer(outer, cx, lo);
    if (jvmInstPeer == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    nsresult result = jvmInstPeer->QueryInterface(aIID, aInstancePtr);
    if (result != NS_OK) goto error;

    result = outer->QueryInterface(kIPluginInstancePeerIID,
                                   (void**)&jvmInstPeer->fPluginInstancePeer);
    if (result != NS_OK) goto error;
    outer->Release();   // no need to AddRef outer
    return result;

  error:
    delete jvmInstPeer;
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Delegated methods:

NS_METHOD_(NPIPlugin*)
JVMInstancePeer::GetClass(void)
{
    return fPluginInstancePeer->GetClass();
}

NS_METHOD_(nsMIMEType)
JVMInstancePeer::GetMIMEType(void)
{
    return fPluginInstancePeer->GetMIMEType();
}

NS_METHOD_(NPPluginType)
JVMInstancePeer::GetMode(void)
{
    return fPluginInstancePeer->GetMode();
}

NS_METHOD_(NPPluginError)
JVMInstancePeer::GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values)
{
    return fPluginInstancePeer->GetAttributes(n, names, values);
}

NS_METHOD_(const char*)
JVMInstancePeer::GetAttribute(const char* name)
{
    return fPluginInstancePeer->GetAttribute( name );
}

NS_METHOD_(NPPluginError)
JVMInstancePeer::GetParameters(PRUint16& n, const char*const*& names, const char*const*& values)
{
    return fPluginInstancePeer->GetParameters(n, names, values);
}

NS_METHOD_(const char*)
JVMInstancePeer::GetParameter(const char* name)
{
    return fPluginInstancePeer->GetParameter( name );
}

NS_METHOD_(NPTagType)
JVMInstancePeer::GetTagType(void)
{
    return fPluginInstancePeer->GetTagType();
}

NS_METHOD_(const char *)
JVMInstancePeer::GetTagText(void)
{
    return fPluginInstancePeer->GetTagText();
}

NS_METHOD_(NPIPluginManager*)
JVMInstancePeer::GetPluginManager(void)
{
    return fPluginInstancePeer->GetPluginManager();
}

NS_METHOD_(NPPluginError)
JVMInstancePeer::GetURL(const char* url, const char* target, void* notifyData,
                        const char* altHost, const char* referrer,
                        PRBool forceJSEnabled)
{
    return fPluginInstancePeer->GetURL(url, target, notifyData, altHost, referrer, forceJSEnabled);
}

NS_METHOD_(NPPluginError)
JVMInstancePeer::PostURL(const char* url, const char* target, PRUint32 bufLen, 
                         const char* buf, PRBool file, void* notifyData,
                         const char* altHost, const char* referrer,
                         PRBool forceJSEnabled,
                         PRUint32 postHeadersLength, const char* postHeaders)
{
    return fPluginInstancePeer->PostURL(url, target,
                                        bufLen, buf, file, notifyData,
                                        altHost, referrer, forceJSEnabled,
                                        postHeadersLength, postHeaders);
}

NS_METHOD_(NPPluginError)
JVMInstancePeer::NewStream(nsMIMEType type, const char* target,
                           NPIPluginManagerStream* *result)
{
    return fPluginInstancePeer->NewStream(type, target, result);
}

NS_METHOD_(void)
JVMInstancePeer::ShowStatus(const char* message)
{
    fPluginInstancePeer->ShowStatus(message);
}

NS_METHOD_(const char*)
JVMInstancePeer::UserAgent(void)
{
    return fPluginInstancePeer->UserAgent();
}

NS_METHOD_(NPPluginError)
JVMInstancePeer::GetValue(NPPluginManagerVariable variable, void *value)
{
    return fPluginInstancePeer->GetValue(variable, value);
}

NS_METHOD_(NPPluginError)
JVMInstancePeer::SetValue(NPPluginVariable variable, void *value)
{
    return fPluginInstancePeer->SetValue(variable, value);
}

NS_METHOD_(void)
JVMInstancePeer::InvalidateRect(nsRect *invalidRect)
{
    fPluginInstancePeer->InvalidateRect(invalidRect);
}

NS_METHOD_(void)
JVMInstancePeer::InvalidateRegion(nsRegion invalidRegion)
{
    fPluginInstancePeer->InvalidateRegion(invalidRegion);
}

NS_METHOD_(void)
JVMInstancePeer::ForceRedraw(void)
{
    fPluginInstancePeer->ForceRedraw();
}

NS_METHOD_(void)
JVMInstancePeer::RegisterWindow(void* window)
{
    fPluginInstancePeer->RegisterWindow(window);
}
    
NS_METHOD_(void)
JVMInstancePeer::UnregisterWindow(void* window)
{
    fPluginInstancePeer->UnregisterWindow(window);
}

NS_METHOD_(PRInt16)
JVMInstancePeer::AllocateMenuID(PRBool isSubmenu)
{
    return fPluginInstancePeer->AllocateMenuID(isSubmenu);
}

////////////////////////////////////////////////////////////////////////////////
// Non-delegated methods:

NS_METHOD_(PRUint32)
JVMInstancePeer::GetDocumentID(void)
{
    if (fUniqueID == 0) {
        History_entry* history_element = SHIST_GetCurrent(&fContext->hist);
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
    return fUniqueID;
}

NS_METHOD_(const char *) 
JVMInstancePeer::GetID(void)
{
    switch (fLayoutElement->type) {
      case LO_JAVA:
        return GetAttribute("name");
      default:
        return GetAttribute("id");
    }
}

NS_METHOD_(const char *) 
JVMInstancePeer::GetCode(void)
{
    return fPluginInstancePeer->GetAttribute("code");
}

NS_METHOD_(const char *) 
JVMInstancePeer::GetCodeBase()
{
    return fPluginInstancePeer->GetAttribute("codebase");
}

NS_METHOD_(const char *) 
JVMInstancePeer::GetArchive()
{
    return fPluginInstancePeer->GetAttribute("archive");
}

NS_METHOD_(PRBool) 
JVMInstancePeer::GetMayScript()
{
    return (fPluginInstancePeer->GetAttribute("mayscript") != NULL);
}

NS_METHOD_(const char*)
JVMInstancePeer::GetDocumentBase(void)
{
    return (const char*)fLayoutElement->base_url;
}

NS_METHOD_(INTL_CharSetInfo)
JVMInstancePeer::GetDocumentCharSetInfo(void)
{
    return LO_GetDocumentCharacterSetInfo(fContext);
}

NS_METHOD_(const char*)
JVMInstancePeer::GetAlignment(void)
{
    int alignment = fLayoutElement->alignment;

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
    return cp;
}

NS_METHOD_(PRUint32)
JVMInstancePeer::GetWidth(void)
{
    return fLayoutElement->width ? fLayoutElement->width : 50;
}
    
NS_METHOD_(PRUint32)
JVMInstancePeer::GetHeight(void)
{
    return fLayoutElement->height ? fLayoutElement->height : 50;
}

NS_METHOD_(PRUint32)
JVMInstancePeer::GetBorderVertSpace(void)
{
    return fLayoutElement->border_vert_space;
}
    
NS_METHOD_(PRUint32)
JVMInstancePeer::GetBorderHorizSpace(void)
{
    return fLayoutElement->border_horiz_space;
}

NS_METHOD_(PRBool)
JVMInstancePeer::Tickle(void)
{
#ifdef XP_MAC
    // XXX add something for the Mac...
#endif
    return PR_TRUE;
}
    
////////////////////////////////////////////////////////////////////////////////

