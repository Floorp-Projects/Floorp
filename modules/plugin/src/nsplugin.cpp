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
// IMPLEMENTATION OF NETSCAPE COMMUNICATOR PLUGINS (NEW C++ API).
//
// This superscedes the old plugin API (npapi.h, npupp.h), and 
// eliminates the need for glue files: npunix.c, npwin.cpp and npmac.cpp. 
// Correspondences to the old API are shown throughout the file.
////////////////////////////////////////////////////////////////////////////////

#include "npglue.h" 

#ifdef OJI
#include "nsIPlug.h"
#include "jvmmgr.h" 
#endif
#include "plstr.h" /* PL_strcasecmp */

#include "xp_mem.h"
#include "xpassert.h" 

#ifdef XP_MAC
#include "MacMemAllocator.h"
#include "asyncCursors.h"
#endif

#include "intl_csi.h"

static NS_DEFINE_IID(kIJRIEnvIID, NS_IJRIENV_IID); 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginManager2IID, NS_IPLUGINMANAGER2_IID);
static NS_DEFINE_IID(kIJNIEnvIID, NS_IJNIENV_IID); 
static NS_DEFINE_IID(kILiveConnectPluginInstancePeerIID, NS_ILIVECONNECTPLUGININSTANCEPEER_IID); 
static NS_DEFINE_IID(kPluginInstancePeerCID, NS_PLUGININSTANCEPEER_CID);
static NS_DEFINE_IID(kIPluginInstancePeerIID, NS_IPLUGININSTANCEPEER_IID); 
static NS_DEFINE_IID(kIPluginInstancePeer2IID, NS_IPLUGININSTANCEPEER2_IID); 
static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kIPluginTagInfo2IID, NS_IPLUGINTAGINFO2_IID); 
static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);
static NS_DEFINE_IID(kISeekablePluginStreamPeerIID, NS_ISEEKABLEPLUGINSTREAMPEER_IID);
static NS_DEFINE_IID(kIPluginStreamPeerIID, NS_IPLUGINSTREAMPEER_IID);
static NS_DEFINE_IID(kIPluginStreamPeer2IID, NS_IPLUGINSTREAMPEER2_IID);
static NS_DEFINE_IID(kIFileUtilitiesIID, NS_IFILEUTILITIES_IID);

////////////////////////////////////////////////////////////////////////////////
// THINGS IMPLEMENTED BY THE BROWSER...
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Interface
// This interface defines the minimum set of functionality that a plugin
// manager will support if it implements plugins. Plugin implementations can
// QueryInterface to determine if a plugin manager implements more specific 
// APIs for the plugin to use.

nsPluginManager* thePluginManager = NULL;

nsPluginManager::nsPluginManager(nsISupports* outer)
    : fJVMMgr(NULL)
{
    NS_INIT_AGGREGATED(outer);
}

nsPluginManager::~nsPluginManager(void)
{
    fJVMMgr->Release();
    fJVMMgr = NULL;
}

NS_IMPL_AGGREGATED(nsPluginManager);

#include "nsRepository.h"

NS_METHOD
nsPluginManager::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?
    nsPluginManager* mgr = new nsPluginManager(outer);
    nsresult result = mgr->QueryInterface(aIID, aInstancePtr);
    if (result != NS_OK) {
        delete mgr;
    }
    return result;
}

NS_METHOD_(void)
nsPluginManager::ReloadPlugins(PRBool reloadPages)
{
    npn_reloadplugins(reloadPages);
}

NS_METHOD_(void*)
nsPluginManager::MemAlloc(PRUint32 size)
{
    return XP_ALLOC(size);
}

NS_METHOD_(void)
nsPluginManager::MemFree(void* ptr)
{
    (void)XP_FREE(ptr);
}

NS_METHOD_(PRUint32)
nsPluginManager::MemFlush(PRUint32 size)
{
#ifdef XP_MAC
    /* Try to free some memory and return the amount we freed. */
    if (CallCacheFlushers(size))
        return size;
#endif
    return 0;
}

NS_METHOD_(const char*)
nsPluginManager::UserAgent(void)
{
    return npn_useragent(NULL); // we don't really need an npp here
}

NS_METHOD
nsPluginManager::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
    if (aIID.Equals(kIPluginManagerIID) || 
        aIID.Equals(kIPluginManager2IID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    } 
#if 0   // Browser interface should be JNI from now on. Hence all new code
        // should be written using JNI. sudu.
    if (aIID.Equals(kIJRIEnvIID)) {
        // XXX Need to implement ISupports for JRIEnv
        *aInstancePtr = (void*) ((nsISupports*)npn_getJavaEnv()); 
//        AddRef();     // XXX should the plugin instance peer and the env be linked?
        return NS_OK; 
    } 
#endif 
    if (aIID.Equals(kIJNIEnvIID)) {
        // XXX Need to implement ISupports for JNIEnv
        *aInstancePtr = (void*) ((nsISupports*)npn_getJavaEnv(NULL));       //=-= Fix this to return a Interface XXX need JNI version
//        AddRef();     // XXX should the plugin instance peer and the env be linked?
        return NS_OK; 
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)this); 
        AddRef(); 
        return NS_OK; 
    } 
    // Aggregates...
    nsIJVMManager* jvmMgr = GetJVMMgr(aIID);
    if (jvmMgr) {
        *aInstancePtr = (void*) ((nsISupports*)jvmMgr);
        return NS_OK; 
    }
    return NS_NOINTERFACE;
}

nsIJVMManager*
nsPluginManager::GetJVMMgr(const nsIID& aIID)
{
    nsIJVMManager* result = NULL;
#ifdef OJI
    if (fJVMMgr == NULL) {
        // The plugin manager is the outer of the JVM manager
        if (nsJVMMgr::Create(this, kISupportsIID, (void**)&fJVMMgr) != NS_OK)
            return NULL;
    }
    if (fJVMMgr->QueryInterface(aIID, (void**)&result) != NS_OK)
        return NULL;
#endif
    return result;
}

NS_METHOD_(nsPluginError)
nsPluginManager::GetURL(nsISupports* peer, const char* url, const char* target, void* notifyData,
                        const char* altHost, const char* referrer,
                        PRBool forceJSEnabled)
{
    nsPluginError rslt;
    nsPluginInstancePeer* instPeer;
    NPP npp = NULL;
    if (peer->QueryInterface(kPluginInstancePeerCID, (void**)&instPeer) == NS_OK)
        npp = instPeer->GetNPP();
    rslt = (nsPluginError)np_geturlinternal(npp, url, target, altHost, referrer,
                                            forceJSEnabled, notifyData != NULL, notifyData);
    if (npp)
        instPeer->Release();
    return rslt;
}

NS_METHOD_(nsPluginError)
nsPluginManager::PostURL(nsISupports* peer, const char* url, const char* target, PRUint32 bufLen, 
                         const char* buf, PRBool file, void* notifyData,
                         const char* altHost, const char* referrer,
                         PRBool forceJSEnabled,
                         PRUint32 postHeadersLength, const char* postHeaders)
{
    nsPluginError rslt;
    nsPluginInstancePeer* instPeer;
    NPP npp = NULL;
    if (peer->QueryInterface(kPluginInstancePeerCID, (void**)&instPeer) == NS_OK)
        npp = instPeer->GetNPP();
    rslt = (nsPluginError)np_posturlinternal(npp, url, target, altHost, referrer, forceJSEnabled,
                                             bufLen, buf, file, notifyData != NULL, notifyData);
    if (npp)
        instPeer->Release();
    return rslt;
}

NS_METHOD_(void)
nsPluginManager::BeginWaitCursor(void)
{
    if (fWaiting == 0) {
#ifdef XP_PC
        fOldCursor = (void*)GetCursor();
        HCURSOR newCursor = LoadCursor(NULL, IDC_WAIT);
        if (newCursor)
            SetCursor(newCursor);
#endif
#ifdef XP_MAC
        startAsyncCursors();
#endif
    }
    fWaiting++;
}

NS_METHOD_(void)
nsPluginManager::EndWaitCursor(void)
{
    fWaiting--;
    if (fWaiting == 0) {
#ifdef XP_PC
        if (fOldCursor)
            SetCursor((HCURSOR)fOldCursor);
#endif
#ifdef XP_MAC
        stopAsyncCursors();
#endif
        fOldCursor = NULL;
    }
}

NS_METHOD_(PRBool)
nsPluginManager::SupportsURLProtocol(const char* protocol)
{
    int type = NET_URL_Type(protocol);
    return (PRBool)(type != 0);
}

////////////////////////////////////////////////////////////////////////////////
// File Utilities Interface

nsFileUtilities::nsFileUtilities(nsISupports* outer)
    : fProgramPath(NULL)
{
    NS_INIT_AGGREGATED(outer);
}

nsFileUtilities::~nsFileUtilities(void)
{
}

NS_IMPL_AGGREGATED(nsFileUtilities);

NS_METHOD
nsFileUtilities::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER;
    }
    if (aIID.Equals(kIFileUtilitiesIID) || 
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE;
}


NS_METHOD_(const char*)
nsFileUtilities::GetProgramPath(void)
{
    return fProgramPath;
}

NS_METHOD_(const char*)
nsFileUtilities::GetTempDirPath(void)
{
    // XXX I don't need a static really, the browser holds the tempDir name
    // as a static string -- it's just the XP_TempDirName that strdups it.
    static const char* tempDirName = NULL;
    if (tempDirName == NULL)
        tempDirName = XP_TempDirName();
    return tempDirName;
}

NS_METHOD
nsFileUtilities::GetFileName(const char* fn, FileNameType type,
                             char* resultBuf, PRUint32 bufLen)
{
    // XXX This should be rewritten so that we don't have to malloc the name.
    XP_FileType filetype;

    if (type == SIGNED_APPLET_DBNAME)
        filetype = xpSignedAppletDB;
    else if (type == TEMP_FILENAME)
        filetype = xpTemporary;
    else 
        return NS_ERROR_ILLEGAL_VALUE;

    char* tempName = WH_FileName(fn, filetype);
    if (tempName == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    XP_STRNCPY_SAFE(resultBuf, tempName, bufLen);
    XP_FREE(tempName);
    return NS_OK;
}

NS_METHOD
nsFileUtilities::NewTempFileName(const char* prefix, char* resultBuf, PRUint32 bufLen)
{
    // XXX This should be rewritten so that we don't have to malloc the name.
    char* tempName = WH_TempName(xpTemporary, prefix);
    if (tempName == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    XP_STRNCPY_SAFE(resultBuf, tempName, bufLen);
    XP_FREE(tempName);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Instance Peer Interface

nsPluginInstancePeer::nsPluginInstancePeer(NPP npp)
    : npp(npp), userInst(NULL)
{
    NS_INIT_AGGREGATED(NULL);
    tagInfo = new nsPluginTagInfo(npp);
    tagInfo->AddRef();
}

nsPluginInstancePeer::~nsPluginInstancePeer(void)
{
    tagInfo->Release();
    tagInfo = NULL;
}

NS_IMPL_AGGREGATED(nsPluginInstancePeer);

NS_METHOD
nsPluginInstancePeer::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
    if (aIID.Equals(kILiveConnectPluginInstancePeerIID) ||
        aIID.Equals(kIPluginInstancePeer2IID) ||
        aIID.Equals(kIPluginInstancePeerIID) ||
        aIID.Equals(kPluginInstancePeerCID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*)(nsISupports*)(nsIPluginInstancePeer*)this; 
        AddRef(); 
        return NS_OK; 
    }
    return tagInfo->QueryInterface(aIID, aInstancePtr);
}

NS_METHOD_(nsMIMEType)
nsPluginInstancePeer::GetMIMEType(void)
{
    np_instance* instance = (np_instance*)npp->ndata;
    return instance->typeString;
}

NS_METHOD_(nsPluginType)
nsPluginInstancePeer::GetMode(void)
{
    np_instance* instance = (np_instance*)npp->ndata;
    return (nsPluginType)instance->type;
}

NS_METHOD_(nsPluginError)
nsPluginInstancePeer::NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result)
{
    NPStream* pstream;
    nsPluginError err = (nsPluginError)
        npn_newstream(npp, (char*)type, (char*)target, &pstream);
    if (err != nsPluginError_NoError)
        return err;
    *result = new nsPluginManagerStream(npp, pstream);
    return nsPluginError_NoError;
}

NS_METHOD_(void)
nsPluginInstancePeer::ShowStatus(const char* message)
{
    npn_status(npp, message);
}

NS_METHOD_(nsPluginError)
nsPluginInstancePeer::GetValue(nsPluginManagerVariable variable, void *value)
{
    return (nsPluginError)npn_getvalue(npp, (NPNVariable)variable, value);
}

NS_METHOD_(nsPluginError)
nsPluginInstancePeer::SetValue(nsPluginVariable variable, void *value)
{
    return (nsPluginError)npn_setvalue(npp, (NPPVariable)variable, value);
}

NS_METHOD_(void)
nsPluginInstancePeer::InvalidateRect(nsRect *invalidRect)
{
    npn_invalidaterect(npp, (NPRect*)invalidRect);
}

NS_METHOD_(void)
nsPluginInstancePeer::InvalidateRegion(nsRegion invalidRegion)
{
    npn_invalidateregion(npp, invalidRegion);
}

NS_METHOD_(void)
nsPluginInstancePeer::ForceRedraw(void)
{
    npn_forceredraw(npp);
}

NS_METHOD_(void)
nsPluginInstancePeer::RegisterWindow(void* window)
{
    npn_registerwindow(npp, window);
}

NS_METHOD_(void)
nsPluginInstancePeer::UnregisterWindow(void* window)
{
    npn_unregisterwindow(npp, window);
}

NS_METHOD_(PRInt16)
nsPluginInstancePeer::AllocateMenuID(PRBool isSubmenu)
{
#ifdef XP_MAC
    return npn_allocateMenuID(npp, isSubmenu);
#else
    return -1;
#endif
}

NS_METHOD_(PRBool)
nsPluginInstancePeer::Tickle(void)
{
#ifdef XP_MAC
    // XXX add something for the Mac...
#endif
    return PR_TRUE;
}
    
NS_METHOD_(jref)
nsPluginInstancePeer::GetJavaPeer(void)
{
    return npn_getJavaPeer(npp);
}

extern "C" JSContext *lm_crippled_context; /* XXX kill me */

JSContext *
nsPluginInstancePeer::GetJSContext(void)
{
    MWContext *pMWCX = GetMWContext();
    JSContext *pJSCX = NULL;
    if (!pMWCX || (pMWCX->type != MWContextBrowser && pMWCX->type != MWContextPane))
    {
       return 0;
    }
    if( pMWCX->mocha_context != NULL)
    {
      pJSCX = pMWCX->mocha_context;
    }
    else
    {
       pJSCX = lm_crippled_context;
    }
    return pJSCX;
}

MWContext *
nsPluginInstancePeer::GetMWContext(void)
{
    np_instance* instance = (np_instance*) npp->ndata;
    MWContext *pMWCX = instance->cx;

    return pMWCX;
}


////////////////////////////////////////////////////////////////////////////////
// Plugin Tag Info Interface

nsPluginTagInfo::nsPluginTagInfo(NPP npp)
    : npp(npp), fUniqueID(0), fJVMPluginTagInfo(NULL)
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
    if (fJVMPluginTagInfo == NULL) {
        np_instance* instance = (np_instance*) npp->ndata;
        MWContext* cx = instance->cx;
        nsresult result =
            nsJVMPluginTagInfo::Create((nsISupports*)this, kISupportsIID,
                                       (void**)&fJVMPluginTagInfo, this);
        if (result != NS_OK) return result;
    }
#endif
    return fJVMPluginTagInfo->QueryInterface(aIID, aInstancePtr);
}

static char* empty_list[] = { "", NULL };

NS_METHOD_(nsPluginError)
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

#ifdef OJI
        names = (const char*const*)ndata->lo_struct->attributes.names;
        values = (const char*const*)ndata->lo_struct->attributes.values;
        n = (PRUint16)ndata->lo_struct->attributes.n;
#else
        // XXX I think all the parameters and attributes just get
        // munged together under MOZ_JAVA...
        names = (const char*const*) ndata->lo_struct->attribute_list;
        values = (const char*const*) ndata->lo_struct->value_list;
        n = (PRUint16) ndata->lo_struct->attribute_cnt;
#endif

        return nsPluginError_NoError;
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

        return nsPluginError_NoError;
    }

    // random, sun-spot induced error
    PR_ASSERT( 0 );

    n = 0;
    // const char* const* empty_list = { { '\0' } };
    names = values = (const char*const*)empty_list;

    return nsPluginError_GenericError;
}

NS_METHOD_(const char*)
nsPluginTagInfo::GetAttribute(const char* name) 
{
    PRUint16 nAttrs, i;
    const char*const* names;
    const char*const* values;

    if( NPCallFailed( GetAttributes( nAttrs, names, values )) )
        return 0;

    for( i = 0; i < nAttrs; i++ ) {
        if( PL_strcasecmp( name, names[i] ) == 0 )
            return values[i];
    }

    return 0;
}

NS_METHOD_(nsPluginTagType) 
nsPluginTagInfo::GetTagType(void)
{
#ifdef XXX
    switch (GetLayoutElement()->lo_element.type) {
      case LO_JAVA:
        return nsPluginTagType_Applet;
      case LO_EMBED:
        return nsPluginTagType_Embed;
      case LO_OBJECT:
        return nsPluginTagType_Object;

      default:
        return nsPluginTagType_Unknown;
    }
#endif
    return nsPluginTagType_Unknown;
}

NS_METHOD_(const char *) 
nsPluginTagInfo::GetTagText(void)
{
    // XXX
    return NULL;
}

NS_METHOD_(nsPluginError)
nsPluginTagInfo::GetParameters(PRUint16& n, 
                               const char*const*& names, 
                               const char*const*& values)
{
    np_instance* instance = (np_instance*)npp->ndata;

    if (instance->type == NP_EMBED) {
        np_data* ndata = (np_data*)instance->app->np_data;

#ifdef OJI
        names = (const char*const*)ndata->lo_struct->parameters.names;
        values = (const char*const*)ndata->lo_struct->parameters.values;
        n = (PRUint16)ndata->lo_struct->parameters.n;
#else
        // XXX I think all the parameters and attributes just get
        // munged together under MOZ_JAVA...
        names = (const char*const*) ndata->lo_struct->attribute_list;
        values = (const char*const*) ndata->lo_struct->value_list;
        n = (PRUint16)ndata->lo_struct->attribute_cnt;
#endif

        return nsPluginError_NoError;
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

        return nsPluginError_NoError;
    }

    // random, sun-spot induced error
    PR_ASSERT( 0 );

    n = 0;
    // static const char* const* empty_list = { { '\0' } };
    names = values = (const char*const*)empty_list;

    return nsPluginError_GenericError;
}

NS_METHOD_(const char*)
nsPluginTagInfo::GetParameter(const char* name) 
{
    PRUint16 nParams, i;
    const char*const* names;
    const char*const* values;

    if( NPCallFailed( GetParameters( nParams, names, values )) )
        return 0;

    for( i = 0; i < nParams; i++ ) {
        if( PL_strcasecmp( name, names[i] ) == 0 )
            return values[i];
    }

    return 0;
}

NS_METHOD_(const char*)
nsPluginTagInfo::GetDocumentBase(void)
{
    return (const char*)GetLayoutElement()->base_url;
}

NS_METHOD_(const char*)
nsPluginTagInfo::GetDocumentEncoding(void)
{
    np_instance* instance = (np_instance*) npp->ndata;
    MWContext* cx = instance->cx;
    INTL_CharSetInfo info = LO_GetDocumentCharacterSetInfo(cx);
    int16 doc_csid = INTL_GetCSIWinCSID(info);
    return INTL_CharSetIDToJavaCharSetName(doc_csid);
}

NS_METHOD_(const char*)
nsPluginTagInfo::GetAlignment(void)
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
    return cp;
}

NS_METHOD_(PRUint32)
nsPluginTagInfo::GetWidth(void)
{
    LO_CommonPluginStruct* lo = GetLayoutElement();
    return lo->width ? lo->width : 50;
}
    
NS_METHOD_(PRUint32)
nsPluginTagInfo::GetHeight(void)
{
    LO_CommonPluginStruct* lo = GetLayoutElement();
    return lo->height ? lo->height : 50;
}

NS_METHOD_(PRUint32)
nsPluginTagInfo::GetBorderVertSpace(void)
{
    return GetLayoutElement()->border_vert_space;
}
    
NS_METHOD_(PRUint32)
nsPluginTagInfo::GetBorderHorizSpace(void)
{
    return GetLayoutElement()->border_horiz_space;
}

NS_METHOD_(PRUint32)
nsPluginTagInfo::GetUniqueID(void)
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
    return fUniqueID;
}

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Stream Interface

nsPluginManagerStream::nsPluginManagerStream(NPP npp, NPStream* pstr)
    : npp(npp), pstream(pstr)
{
    NS_INIT_REFCNT();
    // XXX get rid of the npp instance variable if this is true
    PR_ASSERT(npp == ((np_stream*)pstr->ndata)->instance->npp);
}

nsPluginManagerStream::~nsPluginManagerStream(void)
{
}

NS_METHOD
nsPluginManagerStream::Close(void)
{
    NPError err = npn_destroystream(npp, pstream, nsPluginReason_Done);
    return (nsresult)err;
}

NS_METHOD_(PRInt32)
nsPluginManagerStream::Write(const char* aBuf, PRInt32 aOffset, PRInt32 aCount,
                             nsresult *errorResult)
{
    PRInt32 rslt = npn_write(npp, pstream, aCount, (void*)(aBuf + aOffset));
    if (rslt == -1) 
        *errorResult = NS_ERROR_FAILURE;       // XXX what should this be?
    return rslt;
}

NS_IMPL_QUERY_INTERFACE(nsPluginManagerStream, kIOutputStreamIID);
NS_IMPL_ADDREF(nsPluginManagerStream);
NS_IMPL_RELEASE(nsPluginManagerStream);

////////////////////////////////////////////////////////////////////////////////
// Plugin Stream Peer Interface

nsPluginStreamPeer::nsPluginStreamPeer(URL_Struct *urls, np_stream *stream)
    : userStream(NULL), urls(urls), stream(stream), 
      reason(nsPluginReason_NoReason)
{
    NS_INIT_REFCNT();
}

nsPluginStreamPeer::~nsPluginStreamPeer(void)
{
#if 0
    NPError err = npn_destroystream(stream->instance->npp, stream->pstream, reason);
    PR_ASSERT(err == nsPluginError_NoError);
#endif
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetURL(void)
{
    return stream->pstream->url;
}

NS_METHOD_(PRUint32)
nsPluginStreamPeer::GetEnd(void)
{
    return stream->pstream->end;
}

NS_METHOD_(PRUint32)
nsPluginStreamPeer::GetLastModified(void)
{
    return stream->pstream->lastmodified;
}

NS_METHOD_(void*)
nsPluginStreamPeer::GetNotifyData(void)
{
    return stream->pstream->notifyData;
}

NS_METHOD_(nsPluginReason)
nsPluginStreamPeer::GetReason(void)
{
    return reason;
}

NS_METHOD_(nsMIMEType)
nsPluginStreamPeer::GetMIMEType(void)
{
    return (nsMIMEType)urls->content_type;
}

NS_METHOD_(PRUint32)
nsPluginStreamPeer::GetContentLength(void)
{
    return urls->content_length;
}
#if 0
NS_METHOD_(const char*)
nsPluginStreamPeer::GetContentEncoding(void)
{
    return urls->content_encoding;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetCharSet(void)
{
    return urls->charset;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetBoundary(void)
{
    return urls->boundary;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetContentName(void)
{
    return urls->content_name;
}

NS_METHOD_(time_t)
nsPluginStreamPeer::GetExpires(void)
{
    return urls->expires;
}

NS_METHOD_(time_t)
nsPluginStreamPeer::GetLastModified(void)
{
    return urls->last_modified;
}

NS_METHOD_(time_t)
nsPluginStreamPeer::GetServerDate(void)
{
    return urls->server_date;
}

NS_METHOD_(NPServerStatus)
nsPluginStreamPeer::GetServerStatus(void)
{
    return urls->server_status;
}
#endif
NS_METHOD_(PRUint32)
nsPluginStreamPeer::GetHeaderFieldCount(void)
{
    return urls->all_headers.empty_index;
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetHeaderFieldKey(PRUint32 index)
{
    return urls->all_headers.key[index];
}

NS_METHOD_(const char*)
nsPluginStreamPeer::GetHeaderField(PRUint32 index)
{
    return urls->all_headers.value[index];
}

NS_METHOD_(nsPluginError)
nsPluginStreamPeer::RequestRead(nsByteRange* rangeList)
{
    return (nsPluginError)npn_requestread(stream->pstream,
                                          (NPByteRange*)rangeList);
}

NS_IMPL_ADDREF(nsPluginStreamPeer);
NS_IMPL_RELEASE(nsPluginStreamPeer);

NS_METHOD
nsPluginStreamPeer::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if ((stream->seekable && aIID.Equals(kISeekablePluginStreamPeerIID)) ||
		aIID.Equals(kIPluginStreamPeer2IID) ||
        aIID.Equals(kIPluginStreamPeerIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*)(nsISupports*)(nsIPluginStreamPeer2*)this; 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
} 

////////////////////////////////////////////////////////////////////////////////
