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
// Backward Adapter
// This acts as a adapter layer to allow 5.0 plugins work with the 4.0/3.0 
// browser.
////////////////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// SECTION 1 - Includes
////////////////////////////////////////////////////////////////////////////////
#include "npapi.h"
#include "nsIPlug.h"
#include "Debug.h"

// Include the old API glue file.
/*#ifdef XP_WIN
	// XXX - This needs to be fixed.
	#include "npwin.cpp"
#elif XP_UNIX
    #include "npunix.c"
#elif XP_MAC
    #include "npmac.cpp"
#endif //XP_WIN
*/

////////////////////////////////////////////////////////////////////////////////
// SECTION 3 - Classes
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// CPluginManager
//
// This is the dummy plugin manager that interacts with the 5.0 plugin.
//
class CPluginManager : public nsIPluginManager {

public:

    CPluginManager(void);
    virtual ~CPluginManager(void);

    NS_DECL_ISUPPORTS
    
    NS_IMETHOD_(void)
    ReloadPlugins(PRBool reloadPages);
    
    NS_IMETHOD_(void* )
    MemAlloc(PRUint32 size);

    NS_IMETHOD_(void)
    MemFree(void* ptr);

    NS_IMETHOD_(PRUint32)
    MemFlush(PRUint32 size);

    // (Corresponds to NPN_GetValue.)
    NS_IMETHOD_(nsPluginError)
    GetValue(nsPluginManagerVariable variable, void *value);

    // (Corresponds to NPN_SetValue.)
    NS_IMETHOD_(nsPluginError)
    SetValue(nsPluginManagerVariable variable, void *value);

    // (Corresponds to NPN_UserAgent.)
    NS_IMETHOD_(const char*)
    UserAgent(void);

    // (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
    //   notifyData: When present, URLNotify is called passing the notifyData back
    //          to the client. When NULL, this call behaves like NPN_GetURL.
    // New arguments:
    //   peer:  A plugin instance peer. The peer's window will be used to display
    //          progress information. If NULL, the load happens in the background.
    //   altHost: An IP-address string that will be used instead of the host
    //          specified in the URL. This is used to prevent DNS-spoofing attacks.
    //          Can be defaulted to NULL meaning use the host in the URL.
    //   referrer: 
    //   forceJSEnabled: Forces JavaScript to be enabled for 'javascript:' URLs,
    //          even if the user currently has JavaScript disabled. 
    NS_IMETHOD_(nsPluginError)
    GetURL(nsISupports* peer, const char* url, const char* target, void* notifyData = NULL,
           const char* altHost = NULL, const char* referrer = NULL,
           PRBool forceJSEnabled = PR_FALSE);

    // (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
    //   notifyData: When present, URLNotify is called passing the notifyData back
    //          to the client. When NULL, this call behaves like NPN_GetURL.
    // New arguments:
    //   peer:  A plugin instance peer. The peer's window will be used to display
    //          progress information. If NULL, the load happens in the background.
    //   altHost: An IP-address string that will be used instead of the host
    //          specified in the URL. This is used to prevent DNS-spoofing attacks.
    //          Can be defaulted to NULL meaning use the host in the URL.
    //   referrer: 
    //   forceJSEnabled: Forces JavaScript to be enabled for 'javascript:' URLs,
    //          even if the user currently has JavaScript disabled. 
    //   postHeaders: A string containing post headers.
    //   postHeadersLength: The length of the post headers string.
    NS_IMETHOD_(nsPluginError)
    PostURL(nsISupports* peer, const char* url, const char* target, PRUint32 bufLen, 
            const char* buf, PRBool file, void* notifyData = NULL,
            const char* altHost = NULL, const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, const char* postHeaders = NULL);
};

////////////////////////////////////////////////////////////////////////////////
//
// CPluginManagerStream
//
// This is the dummy plugin manager stream that interacts with the 5.0 plugin.
//
class CPluginManagerStream : public nsIOutputStream {

public:

    CPluginManagerStream(NPP npp, NPStream* pstr);
    virtual ~CPluginManagerStream(void);

    NS_DECL_ISUPPORTS

    //////////////////////////////////////////////////////////////////////////
    //
    // Taken from nsIStream
    //
    
    // The Release function in nsISupport will take care of destroying the stream.

    NS_IMETHOD_(PRInt32)
    Write (const char* buffer, PRInt32 offset, PRInt32 len, nsresult* error);
    // XXX - Hmm?  Why is there an extra parameter offset?
    // (NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer);

    //////////////////////////////////////////////////////////////////////////
    //
    // Specific methods to nsIPluginManagerStream.
    //
    
    // Corresponds to NPStream's url field.
    NS_IMETHOD_(const char* )
    GetURL(void);

    // Corresponds to NPStream's end field.
    NS_IMETHOD_(PRUint32)
    GetEnd(void);

    // Corresponds to NPStream's lastmodfied field.
    NS_IMETHOD_(PRUint32)
    GetLastModified(void);

    // Corresponds to NPStream's notifyData field.
    NS_IMETHOD_(void* )
    GetNotifyData(void);

    // Corresponds to NPStream's url field.
    NS_IMETHOD Close(void);

protected:

    // npp
    // The plugin instance that the manager stream belongs to.
    NPP npp;

    // pstream
    // The stream the class is using.
    NPStream* pstream;

};

////////////////////////////////////////////////////////////////////////////////
//
// CPluginInstancePeer
//
// This is the dummy instance peer that interacts with the 5.0 plugin.
// In order to do LiveConnect, the class subclasses nsILiveConnectPluginInstancePeer.
//
class CPluginInstancePeer : public nsIPluginInstancePeer, public nsIPluginTagInfo {

public:

    // XXX - I add parameters to the constructor because I wasn't sure if
    // XXX - the 4.0 browser had the npp_instance struct implemented.
    // XXX - If so, then I can access npp_instance through npp->ndata.
    CPluginInstancePeer(NPP npp, nsMIMEType typeString, nsPluginType type,
        PRUint16 attribute_cnt, const char** attribute_list, const char** values_list);
    virtual ~CPluginInstancePeer(void);

    NS_DECL_ISUPPORTS

    // Corresponds to NPP_New's MIMEType argument.
    NS_IMETHOD_(nsMIMEType)
    GetMIMEType(void);

    // Corresponds to NPP_New's mode argument.
    NS_IMETHOD_(nsPluginType)
    GetMode(void);

    // Get a ptr to the paired list of attribute names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    NS_IMETHOD_(nsPluginError)
    GetAttributes(PRUint16& n, const char* const*& names, const char* const*& values);

    // Get the value for the named attribute.  Returns null
    // if the attribute was not set.
    NS_IMETHOD_(const char*)
    GetAttribute(const char* name);

    // Corresponds to NPN_NewStream.
    NS_IMETHOD_(nsPluginError)
    NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result);

    // Corresponds to NPN_ShowStatus.
    NS_IMETHOD_(void)
    ShowStatus(const char* message);

    // XXX - Where did this go?
    NS_IMETHOD_(void)
    Version(int* plugin_major, int* plugin_minor,
                      int* netscape_major, int* netscape_minor);

	NPP GetNPPInstance(void)   {
		return npp;
	}
protected:

    NPP npp;
    // XXX - The next five variables may need to be here since I
    // XXX - don't think np_instance is available in 4.0X.
    nsMIMEType typeString;
	nsPluginType type;
	PRUint16 attribute_cnt;
	char** attribute_list;
	char** values_list;
};

////////////////////////////////////////////////////////////////////////////////
//
// CPluginStreamPeer
//
// This is the dummy stream peer that interacts with the 5.0 plugin.
//
class CPluginStreamPeer : public nsISeekablePluginStreamPeer, public nsIPluginStreamPeer {

public:
    
    CPluginStreamPeer(nsMIMEType type, NPStream* npStream,
		PRBool seekable, PRUint16* stype);
    virtual ~CPluginStreamPeer();

    NS_DECL_ISUPPORTS

    // (Corresponds to NPStream's url field.)
    NS_IMETHOD_(const char*)
    GetURL(void);

    // (Corresponds to NPStream's end field.)
    NS_IMETHOD_(PRUint32)
    GetEnd(void);

    // (Corresponds to NPStream's lastmodified field.)
    NS_IMETHOD_(PRUint32)
    GetLastModified(void);

    // (Corresponds to NPStream's notifyData field.)
    NS_IMETHOD_(void*)
    GetNotifyData(void);

	//////////////////////////////////////////////////////////////////////////
    //
    // From nsIPluginStreamPeer
    //

    // Corresponds to NPP_DestroyStream's reason argument.
    NS_IMETHOD_(nsPluginReason)
    GetReason(void);

    // Corresponds to NPP_NewStream's MIMEType argument.
    NS_IMETHOD_(nsMIMEType)
    GetMIMEType(void);

    //////////////////////////////////////////////////////////////////////////
    //
    // From nsISeekablePluginStreamPeer
    //

    // Corresponds to NPN_RequestRead.
    NS_IMETHOD_(nsPluginError)
    RequestRead(nsByteRange* rangeList);

protected:

	nsMIMEType type;
	NPStream* npStream;
	PRBool seekable;
	PRUint16* stype;
    nsPluginReason reason;

};

//////////////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
} /* extern "C" */
#endif
