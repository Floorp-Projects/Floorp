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

#include "nsHTTPRequest.h"

#ifdef XP_UNIX
#include <sys/utsname.h>
#endif /* XP_UNIX */

#if defined(XP_PC) && !defined(XP_OS2)
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
nsHTTPHandler::NewChannel(nsIURI* i_URL, nsIChannel **o_Instance)
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
        pChannel = new nsHTTPChannel(i_URL, this);
        if (pChannel) {
            PRBool checkForProxy = PR_FALSE;
            NS_ADDREF(pChannel);
            rv = pChannel->Init();
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
        rv = NS_NewLocalFileInputStream(getter_AddRefs(in), file);

        if (NS_FAILED(rv)) return rv;

        rv = NewEncodeStream(in,     
                             nsIHTTPProtocolHandler::ENCODE_NORMAL, 
                             result);
        return rv;
    }
    else {
        nsCOMPtr<nsISupports> in;
        rv = NS_NewCStringInputStream(getter_AddRefs(in), data);
        if (NS_FAILED(rv)) return rv;

        rv = in->QueryInterface(NS_GET_IID(nsIInputStream),(void**)result);
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
nsHTTPHandler::SetAcceptEncodings(const char* i_AcceptEncodings) 
{
    CRTFREEIF (mAcceptEncodings);
    if (i_AcceptEncodings)
    {
        mAcceptEncodings = nsCRT::strdup(i_AcceptEncodings);
        return (mAcceptEncodings == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetAcceptEncodings(char* *o_AcceptEncodings)
{
    if (!o_AcceptEncodings)
        return NS_ERROR_NULL_POINTER;
    
    if (mAcceptEncodings)
    {
        *o_AcceptEncodings = nsCRT::strdup(mAcceptEncodings);
        return (*o_AcceptEncodings == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        *o_AcceptEncodings = nsnull;
        return NS_OK;
    }
}

NS_IMETHODIMP
nsHTTPHandler::SetHttpVersion(unsigned int i_HttpVersion) 
{
	mHttpVersion = i_HttpVersion;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetHttpVersion(unsigned int * o_HttpVersion)
{
    if (!o_HttpVersion)
        return NS_ERROR_NULL_POINTER;

    *o_HttpVersion = mHttpVersion;
	return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetCapabilities(PRUint32 * o_Capabilities)
{
    if (!o_Capabilities)
        return NS_ERROR_NULL_POINTER;

    *o_Capabilities = mCapabilities;
	return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetKeepAliveTimeout(unsigned int * o_keepAliveTimeout)
{
    if (!o_keepAliveTimeout)
        return NS_ERROR_NULL_POINTER;

    *o_keepAliveTimeout = mKeepAliveTimeout;
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
    mVendor.AssignWithConversion(aVendor);
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
    mVendorSub.AssignWithConversion(aVendorSub);
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
    mVendorComment.AssignWithConversion(aComment);
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
    mProduct.AssignWithConversion(aProduct);
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
    mProductSub.AssignWithConversion(aProductSub);
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
    mProductComment.AssignWithConversion(aComment);
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
    mAppLanguage.AssignWithConversion(aLanguage);
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
nsHTTPHandler::GetOscpu(PRUnichar* *aOSCPU)
{
    *aOSCPU = mAppOSCPU.ToNewUnicode();
    if (!*aOSCPU) return NS_ERROR_OUT_OF_MEMORY;
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
    mAppMisc.AssignWithConversion(NS_STATIC_CAST(const PRUnichar*, aMisc));
    return BuildUserAgent();
}

nsHTTPHandler::nsHTTPHandler():
    mAcceptLanguages  (nsnull),
    mAcceptEncodings  (nsnull),
	mHttpVersion      (HTTP_ONE_ONE),
    mCapabilities     (DEFAULT_ALLOWED_CAPABILITIES ),
    mKeepAliveTimeout (DEFAULT_KEEP_ALIVE_TIMEOUT),
    mMaxConnections   (MAX_NUMBER_OF_OPEN_TRANSPORTS),
    mReferrerLevel  (0),
    mRequestTimeout (DEFAULT_HTTP_REQUEST_TIMEOUT),
    mConnectTimeout (DEFAULT_HTTP_CONNECT_TIMEOUT),
    mMaxAllowedKeepAlives (DEFAULT_MAX_ALLOWED_KEEPALIVES),
    mMaxAllowedKeepAlivesPerServer (DEFAULT_MAX_ALLOWED_KEEPALIVES_PER_SERVER)
{
    NS_INIT_REFCNT ();
    SetAcceptEncodings (DEFAULT_ACCEPT_ENCODINGS);
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
        mVendor.Assign(UAPrefVal);

    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "vendorSub",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mVendorSub.Assign(UAPrefVal);

    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "vendorComment",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mVendorComment.Assign(UAPrefVal);

    // Gather product values.
    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "product",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mProduct.Assign(UAPrefVal);

    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "productSub",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mProductSub.Assign(UAPrefVal);

    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "productComment",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mProductComment.Assign(UAPrefVal);

    // Gather misc value.
    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "misc",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mAppMisc.Assign(UAPrefVal);

    // Gather Application name and Version.
    mAppName = "Mozilla";
    mAppVersion = "5.0";

    // Get Security level supported
    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "security",
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mAppSecurity.Assign(NS_STATIC_CAST(const char*, UAPrefVal));
    else
        mAppSecurity = "N";

    // Gather locale.
    rv = mPrefs->CopyCharPref(UA_PREF_PREFIX "locale", 
        getter_Copies(UAPrefVal));
    if (NS_SUCCEEDED(rv))
        mAppLanguage = (const char*)UAPrefVal;

    // Gather platform.
#if defined(XP_OS2)
    mAppPlatform = "OS/2";
#elif defined(XP_PC)
    mAppPlatform = "Windows";
#elif defined(RHAPSODY)
    mAppPlatform = "Macintosh";
#elif defined (XP_UNIX)
    mAppPlatform = "X11";
#else
    mAppPlatform = "Macintosh";
#endif

    // Gather OS/CPU.
#if defined(XP_OS2)
    ULONG os2ver = 0;
    DosQuerySysInfo(QSV_VERSION_MINOR, QSV_VERSION_MINOR,
                    &os2ver, sizeof(os2ver));
    if (os2ver == 11)
        mAppOSCPU = "2.11";
    else if (os2ver == 30)
        mAppOSCPU = "Warp 3";
    else if (os2ver == 40)
        mAppOSCPU = "Warp 4";
    else if (os2ver == 45)
        mAppOSCPU = "Warp 4.5";

#elif defined(XP_PC)
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

    rv = NS_NewISupportsArray(getter_AddRefs(mPipelinedRequests));
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

    mIdleTransports    ->Clear();
    mTransportList     ->Clear();
    mPipelinedRequests ->Clear();
    mPendingChannelList->Clear();
    mConnections       ->Clear();

    // Release the Atoms used by the HTTP protocol...
    nsHTTPAtoms::ReleaseAtoms();

    if (mPrefs)
        mPrefs->UnregisterCallback(NETWORK_PREFS, 
                HTTPPrefsCallback, (void*)this);

    CRTFREEIF (mAcceptLanguages);
    CRTFREEIF (mAcceptEncodings);
}

nsresult nsHTTPHandler::RequestTransport(nsIURI* i_Uri,
                                         nsHTTPChannel* i_Channel,
                                         PRUint32 bufferSegmentSize,
                                         PRUint32 bufferMaxSize,
                                         nsIChannel** o_pTrans,
                                         PRUint32 flags)
{
    nsresult rv;
    *o_pTrans = nsnull;
    PRUint32 count = 0;

    PRInt32 port;
    nsXPIDLCString host;

    nsXPIDLCString proxy;
    PRInt32 proxyPort = -1;

    // Ask the channel for proxy info... since that overrides
    PRBool usingProxy = PR_FALSE;
    i_Channel->GetUsingProxy(&usingProxy);

    if (usingProxy)
    {
        rv = i_Channel->GetProxyHost(getter_Copies(proxy));
        if (NS_FAILED (rv)) return rv;
        
        rv = i_Channel->GetProxyPort(&proxyPort);
        if (NS_FAILED (rv)) return rv;
    }

    rv = i_Uri->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;

    rv = i_Uri->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    if (port == -1)
        GetDefaultPort (&port);

    nsIChannel* trans = nsnull;
    // Check in the idle transports for a host/port match
    count = 0;
    PRInt32 index = 0;
    if ((mCapabilities & ALLOW_KEEPALIVE) && (flags & TRANSPORT_REUSE_ALIVE))
    {
        mIdleTransports->Count(&count);

        // remove old and dead transports first

        if (count > 0)
        {
            for (index = count - 1; index >= 0; --index)
            {
                nsCOMPtr<nsIChannel> cTrans = 
                    dont_AddRef ((nsIChannel*) mIdleTransports->ElementAt(index));

                if (cTrans)
                {
                    nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface (cTrans, &rv);
                    PRBool isAlive = PR_TRUE;

                    if (NS_FAILED (rv) || NS_FAILED (sTrans->IsAlive(mKeepAliveTimeout, &isAlive))
                        || !isAlive)
                        mIdleTransports->RemoveElement(cTrans);
                }
            }
        }

        mIdleTransports->Count(&count);

        if (count > 0)
        {
            for (index = count - 1; index >= 0; --index)
            {
                nsCOMPtr<nsIURI> uri;
                nsCOMPtr<nsIChannel> cTrans = dont_AddRef ((nsIChannel*) mIdleTransports->ElementAt(index) );
            
                if (cTrans && 
                        (NS_SUCCEEDED (cTrans->GetURI(getter_AddRefs(uri)))))
                {
                    nsXPIDLCString idlehost;
                    if (NS_SUCCEEDED (uri->GetHost(getter_Copies(idlehost))))
                    {
                        if (!PL_strcasecmp (usingProxy ? proxy : host, idlehost))
                        {
                            PRInt32 idleport;
                            if (NS_SUCCEEDED (uri->GetPort(&idleport)))
                            {
                                if (idleport == -1)
                                    GetDefaultPort (&idleport);

                                if (idleport == usingProxy ? proxyPort : port)
                                {
                                    // Addref it before removing it!
                                    trans = cTrans;
                                    NS_ADDREF (trans);
                                    // Remove it from the idle
                                    mIdleTransports->RemoveElement(trans);
                                    break;// break out of the for loop 
                                }
                            }
                        }
                    }
                }
            } /* for */
        } /* count > 0 */
    }
    // if we didn't find any from the keep-alive idlelist
    if (trans == nsnull)
    {
        if (! (flags & TRANSPORT_OPEN_ALWAYS) )
        {
            count = 0;
            mTransportList->Count(&count);
            if (count >= (PRUint32) mMaxConnections)
            {

                // XXX this method incorrectly returns a bool
                rv = mPendingChannelList->AppendElement((nsISupports*)(nsIRequest*)i_Channel)
                        ? NS_OK : NS_ERROR_FAILURE;  
                NS_ASSERTION (NS_SUCCEEDED(rv), "AppendElement failed");

                PR_LOG (gHTTPLog, PR_LOG_ALWAYS, 
                       ("nsHTTPHandler::RequestTransport.""\tAll socket transports are busy."
                        "\tAdding nsHTTPChannel [%x] to pending list.\n",
                        i_Channel));
                return NS_ERROR_BUSY;
            }
        }

        if (usingProxy)
            rv = CreateTransport(proxy, proxyPort, host, bufferSegmentSize, 
                    bufferMaxSize, &trans);
        else
            rv = CreateTransport(host, port, host, bufferSegmentSize, 
                    bufferMaxSize, &trans);

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

    NS_WITH_SERVICE(nsISocketTransportService, sts, kSocketTransportServiceCID, &rv);
    if (NS_FAILED (rv))
        return rv;

    rv = sts->CreateTransport(host, port, aPrintHost, bufferSegmentSize, 
            bufferMaxSize, o_pTrans);
    if (NS_SUCCEEDED (rv))
    {
        nsCOMPtr<nsISocketTransport> trans = do_QueryInterface (*o_pTrans, &rv);
        if (NS_SUCCEEDED (rv))
        {
            trans->SetSocketTimeout(mRequestTimeout);
            trans->SetSocketConnectTimeout(mConnectTimeout);
        }
    }
    return rv;
}

static  PRUint32 sMaxKeepAlives = 0;

nsresult
nsHTTPHandler::ReleaseTransport (nsIChannel* i_pTrans  ,
                                 PRUint32 aCapabilities, 
                                 PRBool aDontRestartChannels,
                                 PRUint32 aKeepAliveTimeout,
                                 PRInt32  aKeepAliveMaxCon
                                )
{
    nsresult rv;
    PRUint32 count = 0, transportsInUseCount = 0;

    PRUint32 capabilities = (mCapabilities & aCapabilities);

    PR_LOG (gHTTPLog, PR_LOG_DEBUG, ("nsHTTPHandler::ReleaseTransport [this=%x] "
                "i_pTrans=%x, aCapabilites=%o, aKeepAliveTimeout=%d, aKeepAliveMaxCon=%d, "
                "maxKeepAlives=%d\n",
                this, i_pTrans, aCapabilities, 
                aKeepAliveTimeout, aKeepAliveMaxCon, sMaxKeepAlives));

    if (aKeepAliveMaxCon == -1)
        aKeepAliveMaxCon = mMaxAllowedKeepAlivesPerServer;
    else
    if (aKeepAliveMaxCon > mMaxAllowedKeepAlivesPerServer)
        aKeepAliveMaxCon = mMaxAllowedKeepAlivesPerServer;

    if (aKeepAliveTimeout > (PRUint32)mKeepAliveTimeout)
        aKeepAliveTimeout = (PRUint32)mKeepAliveTimeout;

    PR_LOG (gHTTPLog, PR_LOG_ALWAYS, 
           ("nsHTTPHandler::ReleaseTransport."
            "\tReleasing socket transport %x.\n",
            i_pTrans));

    nsCOMPtr<nsIURI> uri;
    PRInt32 port    = -1;
    nsXPIDLCString  host;

    i_pTrans->GetURI (getter_AddRefs (uri));

    if (uri)
    {
        uri->GetHost (getter_Copies (host));
        uri->GetPort (&port);
    }
    if (port == -1)
        GetDefaultPort (&port);

    // ruslan: now update capabilities for this specific host
    if (! (aCapabilities & DONTRECORD_CAPABILITIES) )
        SetServerCapabilities (host, port, aCapabilities);

    
    // remove old dead transports
    
    mIdleTransports->Count (&count);
    if (count > 0)
    {
        PRInt32 keepAliveMaxCon = 0;
        PRInt32 index;

        for (index = count - 1; index >= 0; --index)
        {
            nsCOMPtr<nsIChannel> cTrans = dont_AddRef ((nsIChannel*) mIdleTransports->ElementAt (index) );

            if (cTrans)
            {
                nsCOMPtr<nsISocketTransport> sTrans = do_QueryInterface (cTrans, &rv);
                PRBool isAlive = PR_TRUE;

                if (NS_FAILED (rv) || NS_FAILED (sTrans->IsAlive (mKeepAliveTimeout, &isAlive))
                    || !isAlive)
                    mIdleTransports->RemoveElement (cTrans);
                else
                if (capabilities & (ALLOW_KEEPALIVE|ALLOW_PROXY_KEEPALIVE))
                {        
                    cTrans->GetURI (getter_AddRefs (uri));
                    if (uri)
                    {
                        PRInt32 lPort    = -1;
                        nsXPIDLCString  lHost;

                        uri->GetHost (getter_Copies (lHost));
                        uri->GetPort (&lPort);
                        if (lPort == -1)
                            GetDefaultPort (&lPort);

                        if (lHost && !PL_strcasecmp (lHost, host) && lPort == port)
                            keepAliveMaxCon++;

                        if (keepAliveMaxCon >= aKeepAliveMaxCon)
                            mIdleTransports->RemoveElement (cTrans);
                    }
                } /* IsAlive */
            } /* cTrans */
        } /* for */
    } /* count > 0 */

    if (capabilities & (ALLOW_KEEPALIVE|ALLOW_PROXY_KEEPALIVE))
    {
        nsCOMPtr<nsISocketTransport> trans = do_QueryInterface (i_pTrans, &rv);

        if (NS_SUCCEEDED (rv))
        {
            PRBool alive = PR_FALSE;
            rv = trans->IsAlive(0, &alive);
        
            if (NS_SUCCEEDED (rv) && alive)
            {
                // remove the oldest connection when we hit the maximum
                mIdleTransports->Count(&count);

                if (count >= (PRUint32) mMaxAllowedKeepAlives)
                    mIdleTransports->RemoveElementAt(0);

                PRBool added = mIdleTransports->AppendElement(i_pTrans);
                NS_ASSERTION(added, 
                    "Failed to add a socket to idle transports list!");

                trans->SetReuseConnection(PR_TRUE);
                trans->SetIdleTimeout(aKeepAliveTimeout);

                mIdleTransports->Count(&count);
                if (count > sMaxKeepAlives)
                    sMaxKeepAlives = count;
            }
        }
    }    

    //
    // Clear the EventSinkGetter for the transport...  This breaks the
    // circular reference between the HTTPChannel which holds a reference
    // to the transport and the transport which references the HTTPChannel
    // through the event sink...
    //
    rv = i_pTrans->SetNotificationCallbacks(nsnull);

    rv = mTransportList->RemoveElement(i_pTrans);
    NS_ASSERTION(NS_SUCCEEDED(rv), "Transport not in table...");

    // Now trigger an additional one from the pending list

    while (!aDontRestartChannels)
    {
        // There's no guarantee that a channel will re-request a transport once
        // it's taken off the pending list, so we loop until there are no
        // pending channels or all transports are in use

        count = 0;
        mPendingChannelList->Count(&count);
        mTransportList->Count(&transportsInUseCount);

        PR_LOG (gHTTPLog, PR_LOG_ALWAYS, ("nsHTTPHandler::ReleaseTransport():"
                "pendingChannels=%d, InUseCount=%d\n", count, transportsInUseCount));

        if (!count || (transportsInUseCount >= (PRUint32) mMaxConnections))
            return NS_OK;

        nsCOMPtr<nsISupports> item;
        nsHTTPChannel* channel;

        rv = mPendingChannelList->GetElementAt(0, getter_AddRefs(item));
        if (NS_FAILED(rv)) return rv;

        mPendingChannelList->RemoveElement(item);
        channel = (nsHTTPChannel*)(nsIRequest*)(nsISupports*)item;

        PR_LOG (gHTTPLog, PR_LOG_ALWAYS, ("nsHTTPHandler::ReleaseTransport."
                "\tRestarting nsHTTPChannel [%x]\n", channel));

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

    mPrefs->GetIntPref("network.http.keep-alive.timeout", &mKeepAliveTimeout);
    mPrefs->GetIntPref("network.http.max-connections"   ,   &mMaxConnections);

    if (bChangedAll || !PL_strcmp(pref, "network.sendRefererHeader"))
    {
        PRInt32 referrerLevel = -1;
        rv = mPrefs->GetIntPref("network.sendRefererHeader",&referrerLevel);
        if (NS_SUCCEEDED(rv) && referrerLevel>0) 
           mReferrerLevel = referrerLevel; 
    }

	nsXPIDLCString httpVersion;
    rv = mPrefs->CopyCharPref("network.http.version", getter_Copies(httpVersion));
	
    if (NS_SUCCEEDED (rv) && httpVersion)
	{
		if (!PL_strcmp (httpVersion, "1.1"))
			mHttpVersion = HTTP_ONE_ONE;
		else
		if (!PL_strcmp (httpVersion, "0.9"))
			mHttpVersion = HTTP_ZERO_NINE;
    	else
	    	mHttpVersion = HTTP_ONE_ZERO;
	}

	if (mHttpVersion == HTTP_ONE_ONE)
        mCapabilities = ALLOW_KEEPALIVE|ALLOW_PROXY_KEEPALIVE;
    else
        mCapabilities = 0;

    PRInt32 cVar = 0;
    rv = mPrefs->GetIntPref("network.http.keep-alive", &cVar);
    if (NS_SUCCEEDED (rv))
    {
        if (cVar)
            mCapabilities |=  ALLOW_KEEPALIVE;
        else
            mCapabilities &= ~ALLOW_KEEPALIVE;
    }

    cVar = 0;
    rv = mPrefs->GetIntPref("network.http.proxy.keep-alive", &cVar);
    if (NS_SUCCEEDED (rv))
    {
        if (cVar)
            mCapabilities |=  ALLOW_PROXY_KEEPALIVE;
        else
            mCapabilities &= ~ALLOW_PROXY_KEEPALIVE;
    }

    cVar = 0;
    rv = mPrefs->GetIntPref("network.http.pipelining", &cVar);
    if (NS_SUCCEEDED (rv))
    {
        if (cVar)
            mCapabilities |=  ALLOW_PIPELINING;
        else
            mCapabilities &= ~ALLOW_PIPELINING;
    }

    cVar = 0;
    rv = mPrefs->GetIntPref("network.http.proxy.pipelining", &cVar);
    if (NS_SUCCEEDED (rv))
    {
        if (cVar)
            mCapabilities |=  ALLOW_PROXY_PIPELINING;
        else
            mCapabilities &= ~ALLOW_PROXY_PIPELINING;
    }

    mPrefs->GetIntPref("network.http.connect.timeout", &mConnectTimeout);
    mPrefs->GetIntPref("network.http.request.timeout", &mRequestTimeout);
    mPrefs->GetIntPref("network.http.keep-alive.max-connections", &mMaxAllowedKeepAlives);
    mPrefs->GetIntPref("network.http.keep-alive.max-connections-per-server",
                &mMaxAllowedKeepAlivesPerServer);

    // Things read only during initialization...
    if (bChangedAll) // intl.accept_languages
    {
        nsXPIDLCString acceptLanguages;
        rv = mPrefs->CopyCharPref("intl.accept_languages", 
                getter_Copies(acceptLanguages));
        if (NS_SUCCEEDED(rv))
            SetAcceptLanguages(acceptLanguages);
    }

    nsXPIDLCString acceptEncodings;
    rv = mPrefs->CopyCharPref("network.http.accept-encoding", getter_Copies(acceptEncodings));
    if (NS_SUCCEEDED (rv))
        SetAcceptEncodings (acceptEncodings);

}

PRInt32 PR_CALLBACK HTTPPrefsCallback(const char* pref, void* instance)
{
    nsHTTPHandler* pHandler = (nsHTTPHandler*) instance;
    NS_ASSERTION(nsnull != pHandler, "bad instance data");
    if (nsnull != pHandler)
        pHandler->PrefsChanged(pref);
    return 0;
}

NS_IMETHODIMP
nsHTTPHandler::SetServerCapabilities(
        const char *host, 
        PRInt32 port,
        PRUint32 aCapabilities)
{
    if (host)
    {
        nsCString hStr(host);
        hStr.Append( ':');
        hStr.AppendInt(port);

        nsStringKey key(hStr);
        mCapTable.Put(&key, (void *)aCapabilities);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetServerCapabilities (
        const char *host, 
        PRInt32 port, 
        PRUint32 defCap, 
        PRUint32 *o_Cap)
{
    if (o_Cap == nsnull)
        return NS_ERROR_NULL_POINTER;

    PRUint32 capabilities = (mCapabilities & defCap);

    nsCString hStr (host);
    hStr.Append ( ':');
    hStr.AppendInt (port);

    nsStringKey key (hStr);
    void * p = mCapTable.Get (&key);

    if (p != NULL)
        capabilities = mCapabilities & ((PRUint32) p);

    *o_Cap = capabilities;
    return NS_OK;
}


nsresult
nsHTTPHandler::GetPipelinedRequest(nsIHTTPChannel* i_Channel, nsHTTPPipelinedRequest ** o_Req)
{
    if (o_Req == NULL)
        return NS_ERROR_NULL_POINTER;

    nsCOMPtr<nsIURI> uri;
    nsXPIDLCString  host;
    PRInt32         port = -1;
    nsresult rv = i_Channel->GetURI(getter_AddRefs(uri));

    if (NS_SUCCEEDED (rv))
    {
        rv = uri->GetHost(getter_Copies(host));
        if (NS_SUCCEEDED (rv) && host)
        {
            uri->GetPort(&port);
            if (port == -1)
                GetDefaultPort (&port);
        }
    }

    PRUint32 count = 0;
    PRUint32 index = 0;
    mPipelinedRequests->Count(&count);
    
    nsHTTPPipelinedRequest *pReq = nsnull;

    for (index = 0; index < count; index++, pReq = nsnull)
    {
        pReq = (nsHTTPPipelinedRequest *)mPipelinedRequests->ElementAt(index);
        if (pReq != NULL)
        {
            PRBool same = PR_TRUE;
            pReq->GetSameRequest(host, port, &same);
            if (same)
            {
                PRBool commit = PR_FALSE;
                pReq->GetMustCommit(&commit);

                if (!commit)
                    break;
            }
            NS_RELEASE (pReq);
        }
    } /* for */

    if (pReq == nsnull)
    {
        PRBool usingProxy = PR_FALSE;
        i_Channel->GetUsingProxy(&usingProxy);
        PRUint32 capabilities;

        if (usingProxy)
            GetServerCapabilities (host, port, DEFAULT_PROXY_CAPABILITIES , &capabilities);
        else
            GetServerCapabilities (host, port, DEFAULT_SERVER_CAPABILITIES, &capabilities);

        pReq = new nsHTTPPipelinedRequest (this, host, port, capabilities);
        NS_ADDREF (pReq);

        mPipelinedRequests->AppendElement(pReq);
    }
    *o_Req = pReq;

    return NS_OK;
}

nsresult
nsHTTPHandler::AddPipelinedRequest(nsHTTPPipelinedRequest *pReq)
{
    if (pReq == NULL)
        return NS_ERROR_NULL_POINTER;

    mPipelinedRequests->AppendElement(pReq);

    return NS_OK;
}

nsresult
nsHTTPHandler::ReleasePipelinedRequest(nsHTTPPipelinedRequest *pReq)
{
    if (pReq == NULL)
        return NS_ERROR_NULL_POINTER;

    mPipelinedRequests->RemoveElement(pReq);

    return NS_OK;
}

static BrokenServersTable brokenServers_well_known [] =
{
    { "Netscape-Enterprise 3.6", nsHTTPHandler::BAD_SERVERS_MATCH_EXACT , 0},   // 3.6 but not 3.6 SPx - keep-alive is broken
    { "Apache 1.2"             , nsHTTPHandler::BAD_SERVERS_MATCH_ALL   , nsIHTTPProtocolHandler::DONTALLOW_HTTP11}  // chunk-encoding returns garbage sometimes
};

NS_IMETHODIMP
nsHTTPHandler::Check4BrokenHTTPServers(const char * a_Server, PRUint32 * a_Capabilities)
{
    if (a_Capabilities == NULL)
        return NS_ERROR_NULL_POINTER;
    
    for (int i = 0; i < sizeof (brokenServers_well_known) / sizeof (BrokenServersTable); i++)
    {
        BrokenServersTable *tP = &brokenServers_well_known[i];
        if (tP->matchFlags == BAD_SERVERS_MATCH_EXACT && !PL_strcmp(tP->serverHeader, a_Server))
        {
            *a_Capabilities = tP->capabilities;
            break;
        }
        else
        if (tP->matchFlags == BAD_SERVERS_MATCH_ALL && !PL_strncmp(tP->serverHeader, a_Server, strlen(tP->serverHeader)))
        {
            *a_Capabilities = tP->capabilities;
            break;
        }
    }
    return NS_OK;
}
