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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nspr.h"
#include "nsHTTPHandler.h"
#include "nsHTTPChannel.h"

#include "plstr.h"
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsIURLParser.h"
#include "nsIChannel.h"
#include "nsISocketTransportService.h"
#include "nsISocketTransport.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIHTTPEventSink.h"
#include "nsIFileStreams.h" 
#include "nsIStringStream.h" 
#include "nsHTTPEncodeStream.h" 
#include "nsHTTPAtoms.h"
#include "nsFileSpec.h"
#include "nsIPref.h"
#include "nsIProtocolProxyService.h"

#include "nsILocalFile.h"
#include "nsNetUtil.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"

#ifdef XP_UNIX
#include <sys/utsname.h>
#endif /* XP_UNIX */

#ifdef XP_PC
#include <windows.h>
#endif

#if defined(PR_LOGGING)
//
// Log module for HTTP Protocol logging...
//
// To enable logging (see prlog.h for full details):
//
//    set NSPR_LOG_MODULES=nsHTTPProtocol:5
//    set NSPR_LOG_FILE=nspr.log
//
// this enables PR_LOG_ALWAYS level information and places all output in
// the file nspr.log
//
PRLogModuleInfo* gHTTPLog = nsnull;
#endif /* PR_LOGGING */

#define MAX_NUMBER_OF_OPEN_TRANSPORTS 8

static PRInt32 PR_CALLBACK HTTPPrefsCallback(const char* pref, void* instance);
static const char NETWORK_PREFS[] = "network.";

static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kAuthUrlParserCID, NS_AUTHORITYURLPARSER_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID); // remove now TODO
static NS_DEFINE_CID(kProtocolProxyServiceCID, NS_PROTOCOLPROXYSERVICE_CID);

NS_DEFINE_CID(kCategoryManagerCID, NS_CATEGORYMANAGER_CID);

NS_IMPL_ISUPPORTS2(nsHTTPHandler,
                   nsIHTTPProtocolHandler,
                   nsIProtocolHandler)

// nsIProtocolHandler methods
NS_IMETHODIMP
nsHTTPHandler::GetDefaultPort(PRInt32 *result)
{
    static const PRInt32 defaultPort = 80;
    *result = defaultPort;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetScheme(char * *o_Scheme)
{
    static const char* scheme = "http";
    *o_Scheme = nsCRT::strdup(scheme);
    return NS_OK;
}

/*
 * CategoryCreateService()
 * Given a category, this convenience functions enumerates the category and 
 * creates a service of every CID or ProgID registered under the category
 * @category: Input category
 * @return: returns error if any CID or ProgID registered failed to create.
 */
static nsresult
CategoryCreateService( const char *category )
{
    nsresult rv = NS_OK;

    int nFailed = 0; 
    nsCOMPtr<nsICategoryManager> categoryManager = 
        do_GetService("mozilla.categorymanager.1", &rv);
    if (!categoryManager) return rv;

    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = categoryManager->EnumerateCategory(category, 
            getter_AddRefs(enumerator));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISupports> entry;
    while (NS_SUCCEEDED(enumerator->GetNext(getter_AddRefs(entry))))
    {
        // From here on just skip any error we get.
        nsCOMPtr<nsISupportsString> catEntry = do_QueryInterface(entry, &rv);
        if (NS_FAILED(rv))
        {
            nFailed++;
            continue;
        }
        nsXPIDLCString cidString;
        rv = catEntry->GetData(getter_Copies(cidString));
        if (NS_FAILED(rv))
        {
            nFailed++;
            continue;
        }
        nsCID cid;
        rv = cid.Parse(cidString);
        if (NS_SUCCEEDED(rv))
        {
#ifdef DEBUG_dp
        printf("CategoryCreateInstance: Instantiating cid: %s \
                in category %s.\n",
               (const char *)cidString, category);
#endif /* DEBUG_dp */
            // Create a service from the cid
            nsCOMPtr<nsISupports> instance = do_GetService(cid, &rv);
        }
        else
        {
#ifdef DEBUG_dp
            printf("HTTP Handler: Instantiating progid %s \
                    in http startup category.\n", (const char *)cidString);
#endif /* DEBUG_dp */
            // This might be a progid. Try that too.
            nsCOMPtr<nsISupports> instance = do_GetService(cidString, &rv);
        }
        if (NS_FAILED(rv))
        {
            nFailed++;
        }
    }
    return (nFailed ? NS_ERROR_FAILURE : NS_OK);
}

NS_IMETHODIMP
nsHTTPHandler::NewURI(const char *aSpec, nsIURI *aBaseURI,
                      nsIURI **result)
{
    nsresult rv = NS_OK;

    nsCOMPtr<nsIURI> url;
    nsCOMPtr<nsIURLParser> urlparser; 
    if (aBaseURI)
    {
        rv = aBaseURI->Clone(getter_AddRefs(url));
        if (NS_FAILED(rv)) return rv;
        rv = url->SetRelativePath(aSpec);
    }
    else
    {
        rv = nsComponentManager::CreateInstance(kAuthUrlParserCID, 
                                    nsnull, NS_GET_IID(nsIURLParser),
                                    getter_AddRefs(urlparser));
        if (NS_FAILED(rv)) return rv;
        rv = nsComponentManager::CreateInstance(kStandardUrlCID, 
                                    nsnull, NS_GET_IID(nsIURI),
                                    getter_AddRefs(url));
        if (NS_FAILED(rv)) return rv;

        rv = url->SetURLParser(urlparser);
        if (NS_FAILED(rv)) return rv;
        rv = url->SetSpec((char*)aSpec);
    }
    if (NS_FAILED(rv)) return rv;
    *result = url.get();
    NS_ADDREF(*result);
    return rv;
}

NS_IMETHODIMP
nsHTTPHandler::NewChannel(const char* verb, nsIURI* i_URL,
                          nsILoadGroup* aLoadGroup,
                          nsIInterfaceRequestor* notificationCallbacks,
                          nsLoadFlags loadAttributes,
                          nsIURI* originalURI,
                          PRUint32 bufferSegmentSize,
                          PRUint32 bufferMaxSize,
                          nsIChannel **o_Instance)
{
    nsresult rv;
    nsHTTPChannel* pChannel = nsnull;
    nsXPIDLCString scheme;
    nsXPIDLCString handlerScheme;

    // Initial checks...
    if (!i_URL || !o_Instance) {
        return NS_ERROR_NULL_POINTER;
    }

    i_URL->GetScheme(getter_Copies(scheme));
    GetScheme(getter_Copies(handlerScheme));
    
    if (scheme != nsnull  && handlerScheme != nsnull  &&
        0 == PL_strcasecmp(scheme, handlerScheme)) 
    {
        nsCOMPtr<nsIURI> channelURI;
        PRUint32 count;
        PRInt32 index;

        //Check to see if an instance already exists in the active list
        mConnections->Count(&count);
        for (index=count-1; index >= 0; --index) {
            //switch to static_cast...
            pChannel = (nsHTTPChannel*)((nsIHTTPChannel*) 
                mConnections->ElementAt(index));
            //Do other checks here as well... TODO
            rv = pChannel->GetURI(getter_AddRefs(channelURI));
            if (NS_SUCCEEDED(rv) && (channelURI.get() == i_URL))
            {
                NS_ADDREF(pChannel);
                *o_Instance = pChannel;
                // TODO return NS_USING_EXISTING... 
                // or NS_DUPLICATE_REQUEST something like that.
                return NS_OK; 
            }
        }

        // Create one
        pChannel = new nsHTTPChannel(i_URL, 
                                     verb,
                                     originalURI,
                                     this,
                                     bufferSegmentSize,
                                     bufferMaxSize);
        if (pChannel) {
            PRBool checkForProxy = PR_FALSE;
            NS_ADDREF(pChannel);
            rv = pChannel->Init(aLoadGroup);
            if (NS_FAILED(rv)) goto done;
            rv = pChannel->SetLoadAttributes(loadAttributes);
            if (NS_FAILED(rv)) goto done;
            rv = pChannel->SetNotificationCallbacks(notificationCallbacks);
            if (NS_FAILED(rv)) goto done;

            rv = mProxySvc->GetProxyEnabled(&checkForProxy);
            if (checkForProxy)
            {
                rv = mProxySvc->ExamineForProxy(i_URL, pChannel);
                if (NS_FAILED(rv)) goto done;
            }

            rv = pChannel->QueryInterface(NS_GET_IID(nsIChannel), 
                                          (void**)o_Instance);
            // add this instance to the active list of connections
            // TODO!
          done:
            NS_RELEASE(pChannel);
        } else {
            rv = NS_ERROR_OUT_OF_MEMORY;
        }
        return rv;
    }

    NS_ERROR("Non-HTTP request coming to HTTP Handler!!!");
    //return NS_ERROR_MISMATCHED_URL;
    return NS_ERROR_FAILURE;
}


// nsIHTTPProtocolHandler methods
NS_IMETHODIMP
nsHTTPHandler::NewEncodeStream(nsIInputStream *rawStream, PRUint32 encodeFlags,
                               nsIInputStream **result)
{
    return nsHTTPEncodeStream::Create(rawStream, encodeFlags, result);
}

NS_IMETHODIMP
nsHTTPHandler::NewDecodeStream(nsIInputStream *encodedStream, 
                               PRUint32 decodeFlags,
                               nsIInputStream **result)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsHTTPHandler::NewPostDataStream(PRBool isFile, 
                                 const char *data, 
                                 PRUint32 encodeFlags,
                                 nsIInputStream **result)
{
    nsresult rv;
    if (isFile) {
        nsCOMPtr<nsILocalFile> file;
        rv = NS_NewLocalFile(data, getter_AddRefs(file));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsIInputStream> in;
        rv = NS_NewFileInputStream(file, getter_AddRefs(in));

        if (NS_FAILED(rv)) return rv;

        rv = NewEncodeStream(in,     
                             nsIHTTPProtocolHandler::ENCODE_NORMAL, 
                             result);
        return rv;
    }
    else {
        nsCOMPtr<nsISupports> in;
        rv = NS_NewStringInputStream(getter_AddRefs(in), data);
        if (NS_FAILED(rv)) return rv;

        rv = in->QueryInterface(NS_GET_IID(nsIInputStream), (void**)result);
        return rv;
    }
}

NS_IMETHODIMP
nsHTTPHandler::SetAcceptLanguages(const char* i_AcceptLanguages) 
{
    CRTFREEIF(mAcceptLanguages);
    if (i_AcceptLanguages)
    {
        mAcceptLanguages = nsCRT::strdup(i_AcceptLanguages);
        return (mAcceptLanguages == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetAcceptLanguages(char* *o_AcceptLanguages)
{
    if (!o_AcceptLanguages)
        return NS_ERROR_NULL_POINTER;
    if (mAcceptLanguages)
    {
        *o_AcceptLanguages = nsCRT::strdup(mAcceptLanguages);
        return (*o_AcceptLanguages == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        *o_AcceptLanguages = nsnull;
        return NS_OK;
    }
}

NS_IMETHODIMP
nsHTTPHandler::SetHttpVersion (unsigned int i_HttpVersion) 
{
	mHttpVersion = i_HttpVersion;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetHttpVersion (unsigned int * o_HttpVersion)
{
    if (!o_HttpVersion)
        return NS_ERROR_NULL_POINTER;

    *o_HttpVersion = mHttpVersion;
	return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetKeepAliveTimeout (unsigned int i_keepAliveTimeout) 
{
	mKeepAliveTimeout = i_keepAliveTimeout;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetKeepAliveTimeout (unsigned int * o_keepAliveTimeout)
{
    if (!o_keepAliveTimeout)
        return NS_ERROR_NULL_POINTER;

    *o_keepAliveTimeout = mKeepAliveTimeout;
	return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetDoKeepAlive (PRBool i_doKeepAlive) 
{
	mDoKeepAlive = i_doKeepAlive;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetDoKeepAlive (PRBool * o_doKeepAlive)
{
    if (!o_doKeepAlive)
        return NS_ERROR_NULL_POINTER;

    *o_doKeepAlive = mDoKeepAlive;
	return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetAuthEngine(nsAuthEngine** o_AuthEngine)
{
  *o_AuthEngine = &mAuthEngine;
  return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetAppName(PRUnichar* *aAppName)
{
    *aAppName = mAppName.ToNewUnicode();
    if (!*aAppName) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetAppVersion(PRUnichar* *aAppVersion) 
{
    *aAppVersion = mAppVersion.ToNewUnicode();
    if (!*aAppVersion) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetVendor(PRUnichar* *aVendor)
{
    *aVendor = mVendor.ToNewUnicode();
    if (!*aVendor) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetVendor(const PRUnichar* aVendor)
{
    mVendor = aVendor;
    return BuildUserAgent();
}

NS_IMETHODIMP
nsHTTPHandler::GetVendorSub(PRUnichar* *aVendorSub)
{
    *aVendorSub = mVendorSub.ToNewUnicode();
    if (!*aVendorSub) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetVendorSub(const PRUnichar* aVendorSub)
{
    mVendorSub = aVendorSub;
    return BuildUserAgent();
}

NS_IMETHODIMP
nsHTTPHandler::GetVendorComment(PRUnichar* *aComment)
{
    *aComment = mVendorComment.ToNewUnicode();
    if (!*aComment) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetVendorComment(const PRUnichar* aComment)
{
    mVendorComment = aComment;
    return BuildUserAgent();
}

NS_IMETHODIMP
nsHTTPHandler::GetProduct(PRUnichar* *aProduct)
{
    *aProduct = mProduct.ToNewUnicode();
    if (!*aProduct) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetProduct(const PRUnichar* aProduct)
{
    mProduct = aProduct;
    return BuildUserAgent();
}

NS_IMETHODIMP
nsHTTPHandler::GetProductSub(PRUnichar* *aProductSub)
{
    *aProductSub = mProductSub.ToNewUnicode();
    if (!*aProductSub) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetProductSub(const PRUnichar* aProductSub)
{
    mProductSub = aProductSub;
    return BuildUserAgent();
}

NS_IMETHODIMP
nsHTTPHandler::GetProductComment(PRUnichar* *aComment)
{
    *aComment = mProductComment.ToNewUnicode();
    if (!*aComment) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetProductComment(const PRUnichar* aComment)
{
    mProductComment = aComment;
    return BuildUserAgent();
}

NS_IMETHODIMP
nsHTTPHandler::GetLanguage(PRUnichar* *aLanguage)
{
    *aLanguage = mAppLanguage.ToNewUnicode();
    if (!*aLanguage) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetLanguage(const PRUnichar* aLanguage)
{
    mAppLanguage = aLanguage;
    return BuildUserAgent();
}

NS_IMETHODIMP
nsHTTPHandler::GetPlatform(PRUnichar* *aPlatform)
{
    *aPlatform = mAppPlatform.ToNewUnicode();
    if (!*aPlatform) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetUserAgent(PRUnichar* *aUserAgent)
{
    if (mAppUserAgent.IsEmpty()) return NS_ERROR_NOT_INITIALIZED;
    *aUserAgent = mAppUserAgent.ToNewUnicode();
    if (!*aUserAgent) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetMisc(PRUnichar* *aMisc)
{
    *aMisc = mAppMisc.ToNewUnicode();
    if (!*aMisc) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetMisc(const PRUnichar* aMisc)
{
    mAppMisc = (const PRUnichar*)aMisc;
    return BuildUserAgent();
}

nsHTTPHandler::nsHTTPHandler():
    mAcceptLanguages(nsnull),
	mHttpVersion(HTTP_ONE_ZERO),
    mDoKeepAlive(PR_FALSE),
    mKeepAliveTimeout(2*60),
    mReferrerLevel(0)
{
    NS_INIT_REFCNT();
}

#define UA_PREF_PREFIX "general.useragent."
nsresult
nsHTTPHandler::InitUserAgentComponents()
{
    nsresult rv = NS_OK;
    nsXPIDLCString UAPrefVal;

    // Gather vendor values.
    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "vendor",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mVendor = UAPrefVal;

    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "vendorSub",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mVendorSub = UAPrefVal;

    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "vendorComment",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mVendorComment = UAPrefVal;

    // Gather product values.
    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "product",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mProduct = UAPrefVal;

    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "productSub",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mProductSub = UAPrefVal;

    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "productComment",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mProductComment = UAPrefVal;

    // Gather misc value.
    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "misc",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mAppMisc = UAPrefVal;

    // Gather Application name and Version.
    mAppName = "Mozilla";
    mAppVersion = "5.0";

    mAppSecurity = "N"; // XXX needs to be set by HTTPS

    // Gather locale.
    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "locale", 
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mAppLanguage = (const char*)UAPrefVal;

    // Gather platform.
#if defined(XP_PC)
    mAppPlatform = "Windows";
#elif defined(RHAPSODY)
    mAppPlatform = "Macintosh";
#elif defined (XP_UNIX)
    mAppPlatform = "X11";
#else
    mAppPlatform = "Macintosh";
#endif

    // Gather OS/CPU.
#ifdef XP_PC
    OSVERSIONINFO info = { sizeof OSVERSIONINFO };
    if (GetVersionEx(&info)) {
        if ( info.dwPlatformId == VER_PLATFORM_WIN32_NT ) {
            if (info.dwMajorVersion      == 3) {
                mAppOSCPU = "WinNT3.51";
            }
            else if (info.dwMajorVersion == 4) {
                mAppOSCPU = "WinNT4.0";
            }
            else if (info.dwMajorVersion == 5) {
                mAppOSCPU = "Windows NT 5.0";
            }
            else {
                mAppOSCPU = "WinNT";
            }
        } else if (info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
            if (info.dwMinorVersion > 0)
                mAppOSCPU = "Win98";
            else
                mAppOSCPU = "Win95";
        }
    }
#elif defined (XP_UNIX)
    struct utsname name;
    
    int ret = uname(&name);
    if (ret >= 0) {
       mAppOSCPU =  (char*)name.sysname;
       mAppOSCPU += ' ';
       mAppOSCPU += (char*)name.release;
       mAppOSCPU += ' ';
       mAppOSCPU += (char*)name.machine;
    }
#elif defined (XP_MAC)
    mAppOSCPU = "PPC";
#elif defined (XP_BEOS)
    mAppOSCPU = "BeOS";
#endif

    // Finally, build up the user agent string.
    return BuildUserAgent();
}
nsresult
nsHTTPHandler::Init()
{
    nsresult rv = NS_OK;

    mProxySvc = do_GetService(kProtocolProxyServiceCID, &rv);
    mPrefs = do_GetService(kPrefServiceCID, &rv);
    if (!mPrefs)
        return NS_ERROR_OUT_OF_MEMORY;

    mPrefs->RegisterCallback(NETWORK_PREFS, 
                HTTPPrefsCallback, (void*)this);
    PrefsChanged();

    rv = InitUserAgentComponents();
    if (NS_FAILED(rv)) return rv;

#if defined (PR_LOGGING)
    gHTTPLog = PR_NewLogModule("nsHTTPProtocol");
#endif /* PR_LOGGING */

    mSessionStartTime = PR_Now();

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, ("Creating nsHTTPHandler [this=%x].\n", 
                this));

    // Initialize the Atoms used by the HTTP protocol...
    nsHTTPAtoms::AddRefAtoms();

    rv = NS_NewISupportsArray(getter_AddRefs(mConnections));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewISupportsArray(getter_AddRefs(mPendingChannelList));
    if (NS_FAILED(rv)) return rv;

    rv = NS_NewISupportsArray(getter_AddRefs(mTransportList));
    if (NS_FAILED(rv)) return rv;
    
    // At some later stage we could merge this with the transport
    // list and add a field to each transport to determine its 
    // state. 
    rv = NS_NewISupportsArray(getter_AddRefs(mIdleTransports));
    if (NS_FAILED(rv)) return rv;


    // Startup the http category
    // Bring alive the objects in the http-protocol-startup category
    CategoryCreateService(NS_HTTP_STARTUP_CATEGORY);
    // if creating the category service fails, that doesn't mean we should 
    // fail to create the http handler
    return NS_OK;
}

nsHTTPHandler::~nsHTTPHandler()
{
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("Deleting nsHTTPHandler [this=%x].\n", this));

    mConnections->Clear();
    mIdleTransports->Clear();
    mPendingChannelList->Clear();
    mTransportList->Clear();

    // Release the Atoms used by the HTTP protocol...
    nsHTTPAtoms::ReleaseAtoms();

    if (mPrefs)
        mPrefs->UnregisterCallback(NETWORK_PREFS, 
                HTTPPrefsCallback, (void*)this);

    CRTFREEIF(mAcceptLanguages);
}

nsresult nsHTTPHandler::RequestTransport(nsIURI* i_Uri, 
                                         nsHTTPChannel* i_Channel,
                                         PRUint32 bufferSegmentSize,
                                         PRUint32 bufferMaxSize,
                                         nsIChannel** o_pTrans)
{
    nsresult rv;
    PRUint32 count;

    *o_pTrans = nsnull;

    count = 0;
    mTransportList->Count(&count);
    if (count >= MAX_NUMBER_OF_OPEN_TRANSPORTS) {

        // XXX this method incorrectly returns a bool
        rv = mPendingChannelList->AppendElement(
                (nsISupports*)(nsIRequest*)i_Channel) 
            ? NS_OK : NS_ERROR_FAILURE;  
        NS_ASSERTION(NS_SUCCEEDED(rv), "AppendElement failed");

        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("nsHTTPHandler::RequestTransport."
                "\tAll socket transports are busy."
                "\tAdding nsHTTPChannel [%x] to pending list.\n",
                i_Channel));

        return NS_ERROR_BUSY;
    }

    PRInt32 port;
    nsXPIDLCString host;

    nsXPIDLCString proxy;
    PRInt32 proxyPort = -1;

    // Ask the channel for proxy info... since that overrides
    PRBool usingProxy = PR_FALSE;
    i_Channel -> GetUsingProxy (&usingProxy);

    if (usingProxy)
    {
        rv = i_Channel -> GetProxyHost (getter_Copies (proxy));
        if (NS_FAILED (rv)) return rv;
        
        rv = i_Channel -> GetProxyPort (&proxyPort);
        if (NS_FAILED (rv)) return rv;
    }
    else
    {
        rv = i_Uri -> GetHost (getter_Copies (host));
        if (NS_FAILED(rv)) return rv;
        
        rv = i_Uri -> GetPort (&port);
        if (NS_FAILED(rv)) return rv;
        
        if (port == -1)
            GetDefaultPort (&port);
    }

    nsIChannel* trans = nsnull;
    // Check in the idle transports for a host/port match
    count = 0;
    PRInt32 index = 0;
    if (mDoKeepAlive)
    {
        mIdleTransports -> Count (&count);

        // remove old and dead transports first

        for (index = count - 1; index >= 0; --index)
        {
            nsIChannel* cTrans = (nsIChannel*) mIdleTransports -> ElementAt (index);

            if (cTrans)
            {
                nsresult rv;
                nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface (cTrans, &rv);
                PRBool isAlive = PR_TRUE;

                if (NS_FAILED (rv) || NS_FAILED (sTrans -> IsAlive (mKeepAliveTimeout, &isAlive))
                    || !isAlive)
                    mIdleTransports -> RemoveElement (cTrans);
            }
        }

        mIdleTransports -> Count (&count);

        for (index = count - 1; index >= 0; --index)
        {
            nsCOMPtr<nsIURI> uri;
            nsIChannel* cTrans = (nsIChannel*) mIdleTransports -> ElementAt (index);
            
            if (cTrans && 
                    (NS_SUCCEEDED (cTrans -> GetURI (getter_AddRefs (uri)))))
            {
                nsXPIDLCString idlehost;
                if (NS_SUCCEEDED (uri -> GetHost (getter_Copies (idlehost))))
                {
                    if (!PL_strcasecmp (usingProxy ? proxy : host, idlehost))
                    {
                        PRInt32 idleport;
                        if (NS_SUCCEEDED (uri -> GetPort (&idleport)))
                        {
                            if (idleport == -1)
                                GetDefaultPort (&idleport);

                            if (idleport == usingProxy ? proxyPort : port)
                            {
                                // Addref it before removing it!
                                NS_ADDREF (cTrans);
                                // Remove it from the idle
                                mIdleTransports -> RemoveElement (cTrans);
                                trans = cTrans;
                                break;// break out of the for loop 
                            }
                        }
                    }
                }
            }
        } /* for */
    }
    // if we didn't find any from the keep-alive idlelist
    if (trans == nsnull)
    {
        if (usingProxy)
        {
            rv = CreateTransport(proxy, proxyPort, host,
                     bufferSegmentSize, bufferMaxSize, &trans);
        }
        else
        {
            rv = CreateTransport(host, port, host,
                         bufferSegmentSize, bufferMaxSize, &trans);
        }
        if (NS_FAILED(rv)) return rv;
    }

    // Put it in the table...
    // XXX this method incorrectly returns a bool
    rv = mTransportList->AppendElement(trans) ? 
        NS_OK : NS_ERROR_FAILURE;  
    if (NS_FAILED(rv)) return rv;

    *o_pTrans = trans;
    
    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPHandler::RequestTransport."
            "\tGot a socket transport for nsHTTPChannel [%x]. %d " 
            " Active transports.\n",
            i_Channel, count+1));

    return rv;
}

nsresult nsHTTPHandler::CreateTransport(const char* host, 
                                        PRInt32 port, 
                                        const char* aPrintHost,
                                        PRUint32 bufferSegmentSize,
                                        PRUint32 bufferMaxSize,
                                        nsIChannel** o_pTrans)
{
    nsresult rv;

    NS_WITH_SERVICE(nsISocketTransportService, sts, 
                    kSocketTransportServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    return sts->CreateTransport(host, port, aPrintHost,
                                bufferSegmentSize, bufferMaxSize,
                                o_pTrans);
}

nsresult nsHTTPHandler::ReleaseTransport(nsIChannel* i_pTrans)
{
    nsresult rv;
    PRUint32 count=0, transportsInUseCount = 0;

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPHandler::ReleaseTransport."
            "\tReleasing socket transport %x.\n",
            i_pTrans));
    //
    // Clear the EventSinkGetter for the transport...  This breaks the
    // circular reference between the HTTPChannel which holds a reference
    // to the transport and the transport which references the HTTPChannel
    // through the event sink...
    //
    rv = i_pTrans->SetNotificationCallbacks(nsnull);

    rv = mTransportList->RemoveElement(i_pTrans);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Transport not in table...");

    if (mDoKeepAlive)
    {
        nsresult rv;
        nsCOMPtr<nsISocketTransport> trans = do_QueryInterface (i_pTrans, &rv);

        if (NS_SUCCEEDED (rv))
        {
            PRBool alive = PR_FALSE;
            rv = trans -> IsAlive (0, &alive);
        
            if (NS_SUCCEEDED (rv) && alive)
            {
                PRBool added = mIdleTransports -> AppendElement (i_pTrans);
                NS_ASSERTION(added, 
                    "Failed to add a socket to idle transports list!");

                trans -> SetReuseConnection (PR_TRUE);
            }
        }
    }
    
    // Now trigger an additional one from the pending list
    while (1) {
        // There's no guarantee that a channel will re-request a transport once
        // it's taken off the pending list, so we loop until there are no
        // pending channels or all transports are in use

        mPendingChannelList->Count(&count);
        mTransportList->Count(&transportsInUseCount);
        if (!count || (transportsInUseCount >= MAX_NUMBER_OF_OPEN_TRANSPORTS))
            return NS_OK;

        nsCOMPtr<nsISupports> item;
        nsHTTPChannel* channel;

        rv = mPendingChannelList->GetElementAt(0, getter_AddRefs(item));
        if (NS_FAILED(rv)) return rv;

        mPendingChannelList->RemoveElement(item);
        channel = (nsHTTPChannel*)(nsIRequest*)(nsISupports*)item;

        PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
               ("nsHTTPHandler::ReleaseTransport."
                "\tRestarting nsHTTPChannel [%x]\n",
                channel));
        
        channel->Open();
    }

    return rv;
}

nsresult nsHTTPHandler::CancelPendingChannel(nsHTTPChannel* aChannel)
{
  PRBool ret;

  // XXX: RemoveElement *really* returns a PRBool :-(
  ret = (PRBool) mPendingChannelList->RemoveElement(
            (nsISupports*)(nsIRequest*)aChannel);

  PR_LOG(gHTTPLog, PR_LOG_ALWAYS, 
         ("nsHTTPHandler::CancelPendingChannel."
          "\tCancelling nsHTTPChannel [%x]\n",
          aChannel));

  return ret ? NS_OK : NS_ERROR_FAILURE;

}

nsresult
nsHTTPHandler::FollowRedirects(PRBool bFollow)
{
    //mFollowRedirects = bFollow;
    return NS_OK;
}

// This guy needs to be called each time one of it's comprising pieces changes.
nsresult
nsHTTPHandler::BuildUserAgent() {
    NS_ASSERTION((!mAppName.IsEmpty()
                  || !mAppVersion.IsEmpty()
                  || !mAppPlatform.IsEmpty()
                  || !mAppSecurity.IsEmpty()
                  || !mAppOSCPU.IsEmpty()),
                   "HTTP cannot send practical requests without this much");

    // Application portion
    mAppUserAgent = mAppName;
    mAppUserAgent += '/';
    mAppUserAgent += mAppVersion;
    mAppUserAgent += ' ';

    // Application comment
    mAppUserAgent += '(';
    mAppUserAgent += mAppPlatform;
    mAppUserAgent += "; ";
    mAppUserAgent += mAppSecurity;
    mAppUserAgent += "; ";
    mAppUserAgent += mAppOSCPU;
    if (!mAppLanguage.IsEmpty()) {
        mAppUserAgent += "; ";
        mAppUserAgent += mAppLanguage;
    }
    if (!mAppMisc.IsEmpty()) {
        mAppUserAgent += "; ";
        mAppUserAgent += mAppMisc;
    }
    mAppUserAgent += ')';

    // Product portion
    if (!mProduct.IsEmpty()) {
        mAppUserAgent += ' ';
        mAppUserAgent += mProduct;
        if (!mProductSub.IsEmpty()) {
            mAppUserAgent += '/';
            mAppUserAgent += mProductSub;
        }
        if (!mProductComment.IsEmpty()) {
            mAppUserAgent += " (";
            mAppUserAgent += mProductComment;
            mAppUserAgent += ')';
        }
    }

    // Vendor portion
    if (!mVendor.IsEmpty()) {
        mAppUserAgent += ' ';
        mAppUserAgent += mVendor;
        if (!mVendorSub.IsEmpty()) {
            mAppUserAgent += '/';
            mAppUserAgent += mVendorSub;
        }
        if (!mVendorComment.IsEmpty()) {
            mAppUserAgent += " (";
            mAppUserAgent += mVendorComment;
            mAppUserAgent += ')';
        }
    }
    return NS_OK;
}

void
nsHTTPHandler::PrefsChanged(const char* pref)
{
    PRBool bChangedAll = (pref) ? PR_FALSE : PR_TRUE;
    NS_ASSERTION(mPrefs, "No preference service available!");
    if (!mPrefs)
        return;

    nsresult rv = NS_OK;

    PRInt32 keepalive = -1;
    rv = mPrefs->GetIntPref("network.http.keep-alive", &keepalive);
    mDoKeepAlive = (keepalive == 1);

    rv = mPrefs->GetIntPref("network.http.keep-alive.timeout", &mKeepAliveTimeout);

    if (bChangedAll || !PL_strcmp(pref, "network.sendRefererHeader"))
    {
        PRInt32 referrerLevel = -1;
        rv = mPrefs->GetIntPref("network.sendRefererHeader",&referrerLevel);
        if (NS_SUCCEEDED(rv) && referrerLevel>0) 
           mReferrerLevel = referrerLevel; 
    }

	nsXPIDLCString httpVersion;
    rv = mPrefs -> CopyCharPref("network.http.version", 
            getter_Copies(httpVersion));
	if (NS_SUCCEEDED (rv) && httpVersion)
	{
		if (!PL_strcmp (httpVersion, "1.1"))
			mHttpVersion = HTTP_ONE_ONE;
		else
		if (!PL_strcmp (httpVersion, "0.9"))
			mHttpVersion = HTTP_ZERO_NINE;
	}
	else
		mHttpVersion = HTTP_ONE_ZERO;

	if (mHttpVersion == HTTP_ONE_ONE)
		mDoKeepAlive = PR_TRUE;

    // Things read only during initialization...
    if (bChangedAll) // intl.accept_languages
    {
        nsXPIDLCString acceptLanguages;
        rv = mPrefs->CopyCharPref("intl.accept_languages", 
                getter_Copies(acceptLanguages));
        if (NS_SUCCEEDED(rv))
            SetAcceptLanguages(acceptLanguages);
    }
}

PRInt32 PR_CALLBACK HTTPPrefsCallback(const char* pref, void* instance)
{
    nsHTTPHandler* pHandler = (nsHTTPHandler*) instance;
    NS_ASSERTION(nsnull != pHandler, "bad instance data");
    if (nsnull != pHandler)
        pHandler->PrefsChanged(pref);
    return 0;
}
