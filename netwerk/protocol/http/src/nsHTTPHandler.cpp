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

#include "nsIProxy.h"
#include "plstr.h" // For PL_strcasecmp maybe DEBUG only... TODO check
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsIURLParser.h"
#include "nsIChannel.h"
#include "nsISocketTransportService.h"
#include "nsIServiceManager.h"
#include "nsIInterfaceRequestor.h"
#include "nsIHttpEventSink.h"
#include "nsIFileStreams.h" 
#include "nsIStringStream.h" 
#include "nsHTTPEncodeStream.h" 
#include "nsHTTPAtoms.h"
#include "nsFileSpec.h"
#include "nsIPref.h" // preferences stuff
#ifdef DEBUG_gagan
#include "nsUnixColorPrintf.h"
#endif
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

static PRInt32 PR_CALLBACK ProxyPrefsCallback(const char* pref, void* instance);
static const char PROXY_PREFS[] = "network.proxy";

static NS_DEFINE_CID(kStandardUrlCID, NS_STANDARDURL_CID);
static NS_DEFINE_CID(kAuthUrlParserCID, NS_AUTHORITYURLPARSER_CID);
static NS_DEFINE_CID(kSocketTransportServiceCID, NS_SOCKETTRANSPORTSERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
NS_DEFINE_CID(kCategoryManagerCID, NS_CATEGORYMANAGER_CID);

NS_IMPL_ISUPPORTS3(nsHTTPHandler,
                   nsIHTTPProtocolHandler,
                   nsIProtocolHandler,
                   nsIProxy)

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
 * Given a category, this convenience functions enumerates the category and creates
 * a service of every CID or ProgID registered under the category
 * @category: Input category
 * @return: returns error if any CID or ProgID registered failed to create.
 */
static nsresult
CategoryCreateService( const char *category )
{
    nsresult rv = NS_OK;

    int nFailed = 0; 
    nsCOMPtr<nsICategoryManager> categoryManager = do_GetService("mozilla.categorymanager.1", &rv);
    if (!categoryManager) return rv;

    nsCOMPtr<nsISimpleEnumerator> enumerator;
    rv = categoryManager->EnumerateCategory(category, getter_AddRefs(enumerator));
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
            printf("CategoryCreateInstance: Instantiating cid: %s in category %s.\n",
                   (const char *)cidString, category);
#endif /* DEBUG_dp */
            // Create a service from the cid
            nsCOMPtr<nsISupports> instance = do_GetService(cid, &rv);
        }
        else
        {
#ifdef DEBUG_dp
            printf("HTTP Handler: Instantiating progid %s in http startup category.\n", (const char *)cidString);
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
        PRBool useProxy;

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
            NS_ADDREF(pChannel);
            rv = pChannel->Init(aLoadGroup);
            if (NS_FAILED(rv)) goto done;
            rv = pChannel->SetLoadAttributes(loadAttributes);
            if (NS_FAILED(rv)) goto done;
            rv = pChannel->SetNotificationCallbacks(notificationCallbacks);
            if (NS_FAILED(rv)) goto done;

            useProxy = mUseProxy && CanUseProxy(i_URL);
            if (useProxy)
            {
               rv = pChannel->SetProxyHost(mProxy);
               if (NS_FAILED(rv)) goto done;
               rv = pChannel->SetProxyPort(mProxyPort);
               if (NS_FAILED(rv)) goto done;
            }
            rv = pChannel->SetUsingProxy(useProxy);

           if (originalURI) 
           {
              // Referer - misspelled, but per the HTTP spec
              nsCOMPtr<nsIAtom> key = NS_NewAtom("referer");
              nsXPIDLCString spec;
              originalURI->GetSpec(getter_Copies(spec));
              if (spec && (0 == PL_strncasecmp((const char*)spec, 
                                           "http",4)))
              {
                pChannel->SetRequestHeader(key, spec);
              }
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
nsHTTPHandler::GetVendorName(PRUnichar* *aVendorName)
{
    *aVendorName = mVendorName.ToNewUnicode();
    if (!*aVendorName) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetVendorName(const PRUnichar* aVendorName)
{
    nsAutoString vendorName(aVendorName);
    char *vName = vendorName.ToNewCString();
    if (!vName) return NS_ERROR_OUT_OF_MEMORY;
    mVendorName = vName;
    nsAllocator::Free(vName);
    return BuildUserAgent();
}

NS_IMETHODIMP
nsHTTPHandler::GetVendorVersion(PRUnichar* *aVendorVersion)
{
    *aVendorVersion = mVendorVersion.ToNewUnicode();
    if (!*aVendorVersion) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetVendorVersion(const PRUnichar* aVendorVersion)
{
    nsAutoString vendorVersion(aVendorVersion);
    char *vVer = vendorVersion.ToNewCString();
    if (*vVer) return NS_ERROR_OUT_OF_MEMORY;
    mVendorVersion = vVer;
    nsAllocator::Free(vVer);
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
    nsAutoString language(aLanguage);
    char *lang = language.ToNewCString();
    if (!lang) return NS_ERROR_OUT_OF_MEMORY;
    mAppLanguage = lang;
    nsAllocator::Free(lang);
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
    nsAutoString miscStr(aMisc);
    char *misc = miscStr.ToNewCString();
    if (!misc) return NS_ERROR_OUT_OF_MEMORY;
    mAppMisc = misc;
    nsAllocator::Free(misc);
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
    nsAutoString commentStr(aComment);
    char *comment = commentStr.ToNewCString();
    if (!comment) return NS_ERROR_OUT_OF_MEMORY;
    mVendorComment = comment;
    nsAllocator::Free(comment);
    return BuildUserAgent();
}


// nsIProxy methods
NS_IMETHODIMP
nsHTTPHandler::GetProxyHost(char* *o_ProxyHost)
{
    if (!o_ProxyHost)
        return NS_ERROR_NULL_POINTER;
    if (mProxy)
    {
        *o_ProxyHost = nsCRT::strdup(mProxy);
        return (*o_ProxyHost == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    else
    {
        *o_ProxyHost = nsnull;
        return NS_OK;
    }
}

NS_IMETHODIMP
nsHTTPHandler::SetProxyHost(const char* i_ProxyHost) 
{
    CRTFREEIF(mProxy);
    if (i_ProxyHost)
    {
        mProxy = nsCRT::strdup(i_ProxyHost);
        return (mProxy == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::GetProxyPort(PRInt32 *aPort)
{
    *aPort = mProxyPort;
    return NS_OK;
}

NS_IMETHODIMP
nsHTTPHandler::SetProxyPort(PRInt32 i_ProxyPort) 
{
    mProxyPort = i_ProxyPort;
    return NS_OK;
}

nsHTTPHandler::nsHTTPHandler():
    mAcceptLanguages(nsnull),
    mDoKeepAlive(PR_FALSE),
    mNoProxyFor(0),
    mProxy(0),
    mProxyPort(-1),
    mUseProxy(PR_FALSE)
{
    NS_INIT_REFCNT();
}

nsresult
nsHTTPHandler::Init()
{
    nsresult rv = NS_OK;

    mPrefs = do_GetService(kPrefServiceCID, &rv);
    if (!mPrefs)
        return NS_ERROR_OUT_OF_MEMORY;

    mPrefs->RegisterCallback(PROXY_PREFS, 
                ProxyPrefsCallback, (void*)this);
    PrefsChanged();

    // initialize the version and app components
    mAppName = "Mozilla";
    mAppVersion = "5.0";
    mAppSecurity = "N";

    nsXPIDLCString locale;
    rv = mPrefs->CopyCharPref("general.useragent.locale", 
        getter_Copies(locale));
    if (NS_SUCCEEDED(rv)) {
        mAppLanguage = locale;
    }

    nsXPIDLCString milestone;
    rv = mPrefs->CopyCharPref("general.milestone",
        getter_Copies(milestone));
    if (NS_SUCCEEDED(rv)) {
        mAppVersion += milestone;
    }

    // Platform
#if defined(XP_PC)
    mAppPlatform = "Windows";
#elif defined (XP_UNIX)
    mAppPlatform = "X11";
#else
    mAppPlatform = "Macintosh";
#endif

    // OS/CPU
#ifdef XP_PC
    OSVERSIONINFO info = { sizeof OSVERSIONINFO };
    if (GetVersionEx(&info)) {
        if ( info.dwPlatformId == VER_PLATFORM_WIN32_NT ) {
            if (info.dwMajorVersion == 4)
                mAppOSCPU = "NT4.0";
            else if (info.dwMajorVersion == 3) {
                mAppOSCPU = "NT3.51";
            } else {
                mAppOSCPU = "NT";
            }
        } else if (info.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
            if (info.dwMinorVersion > 0)
                mAppOSCPU = "Win98";
            else
                mAppOSCPU = "Win95";
        } else {
            if (info.dwMajorVersion > 4)
                mAppOSCPU = "Win2k";
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

    rv = BuildUserAgent();
    if (NS_FAILED(rv)) return rv;

#if defined (PR_LOGGING)
    gHTTPLog = PR_NewLogModule("nsHTTPProtocol");
#endif /* PR_LOGGING */

    mSessionStartTime = PR_Now();

    PR_LOG(gHTTPLog, PR_LOG_ALWAYS, ("Creating nsHTTPHandler [this=%x].\n", this));

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
    // if creating the category service fails, that doesn't mean we should fail to create
    // the http handler
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
        mPrefs->UnregisterCallback(PROXY_PREFS, 
                ProxyPrefsCallback, (void*)this);

    CRTFREEIF(mAcceptLanguages);
    CRTFREEIF(mNoProxyFor);
    CRTFREEIF(mProxy);
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
        rv = mPendingChannelList->AppendElement((nsISupports*)(nsIRequest*)i_Channel) 
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

    rv = i_Uri->GetHost(getter_Copies(host));
    if (NS_FAILED(rv)) return rv;

    rv = i_Uri->GetPort(&port);
    if (NS_FAILED(rv)) return rv;

    if (port == -1)
        GetDefaultPort(&port);

    nsIChannel* trans;
    // Check in the idle transports for a host/port match
    count = 0;
    PRInt32 index = 0;
    if (mDoKeepAlive)
    {
        mIdleTransports->Count(&count);

        for (index=count-1; index >= 0; --index) 
        {
            nsCOMPtr<nsIURI> uri;
            trans = (nsIChannel*) mIdleTransports->ElementAt(index);
            if (trans && 
                    (NS_SUCCEEDED(trans->GetURI(getter_AddRefs(uri)))))
            {
                nsXPIDLCString idlehost;
                if (NS_SUCCEEDED(uri->GetHost(getter_Copies(idlehost))))
                {
                    if (0 == PL_strcasecmp(host, idlehost))
                    {
                        PRInt32 idleport;
                        if (NS_SUCCEEDED(uri->GetPort(&idleport)))
                        {
                            if (idleport == -1)
                                GetDefaultPort(&idleport);

#ifdef DEBUG_gagan
                            printf(STARTYELLOW "%s:%d\n", 
                                    (const char*)idlehost, idleport);
#endif

                            if (idleport == port)
                            {
                                // Addref it before removing it!
                                NS_ADDREF(trans);
#ifdef DEBUG_gagan
                                PRINTF_BLUE;
                                printf("Found a match in idle list!\n");
#endif
                                // Remove it from the idle
                                mIdleTransports->RemoveElement(trans);
                                //break;// break out of the for loop 
                            }
                        }
                    }
                }
            }
            // else delibrately ignored.
        }
    }
    // if we didn't find any from the keep-alive idlelist
    if (*o_pTrans == nsnull)
    {
        // Ask the channel for proxy info... since that overrides
        PRBool usingProxy;

        i_Channel->GetUsingProxy(&usingProxy);
        if (usingProxy)
        {
            nsXPIDLCString proxy;
            PRInt32 proxyPort = -1;

            rv = i_Channel->GetProxyHost(getter_Copies(proxy));
            if (NS_FAILED(rv)) return rv;

            rv = i_Channel->GetProxyPort(&proxyPort);
            if (NS_FAILED(rv)) return rv;

            rv = CreateTransport(proxy, proxyPort, host,
                     bufferSegmentSize, bufferMaxSize, &trans);
        }
        else
        {
            // Create a new one...
            if (!mProxy || !mUseProxy || !CanUseProxy(i_Uri))
            {
                rv = CreateTransport(host, port, host,
                             bufferSegmentSize, bufferMaxSize, &trans);
                // Update the proxy information on the channel
                i_Channel->SetProxyHost(mProxy);
                i_Channel->SetProxyPort(mProxyPort);
                i_Channel->SetUsingProxy(PR_FALSE);
            }
            else
            {
                rv = CreateTransport(mProxy, mProxyPort, host, 
                             bufferSegmentSize, bufferMaxSize, &trans);
                i_Channel->SetUsingProxy(PR_TRUE);
            }
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
            "\tGot a socket transport for nsHTTPChannel [%x]. %d Active transports.\n",
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
        // if this transport is to be kept alive 
        // then add it to mIdleTransports
        NS_WITH_SERVICE(nsISocketTransportService, sts, 
                kSocketTransportServiceCID, &rv);
        // don't bother about failure of this one...
        if (NS_SUCCEEDED(rv))
        {
            PRBool reuse = PR_FALSE;
            rv = sts->ReuseTransport(i_pTrans, &reuse);
            // Delibrately ignoring the return value of AppendElement 
            // if we can't append a transport we just won't have keep-alive
            // but life goes on... assert in debug though
            if (NS_SUCCEEDED(rv) && reuse)
            {
                PRBool added = mIdleTransports->AppendElement(i_pTrans);
                NS_ASSERTION(added, 
                    "Failed to add a socket to idle transports list!");
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
  ret = (PRBool) mPendingChannelList->RemoveElement((nsISupports*)(nsIRequest*)aChannel);

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
    // Mozilla portion
    mAppUserAgent = mAppName;
    mAppUserAgent += '/';
    mAppUserAgent += mAppVersion;
    mAppUserAgent += ' ';

    // Mozilla comment
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

    // Vendor portion
    if (!mVendorName.IsEmpty()) {
        mAppUserAgent += ' ';
        mAppUserAgent += mVendorName;
        if (!mVendorVersion.IsEmpty()) {
            mAppUserAgent += '/';
            mAppUserAgent += mVendorVersion;
        }
    }
    
    // Vendor comment
    if (!mVendorComment.IsEmpty()) {
        mAppUserAgent += " (";
        mAppUserAgent += mVendorComment;
        mAppUserAgent += ')';
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

#if 1   // only for keep alive TODO
        // This stuff only till Keep-Alive is not switched on by default
        PRInt32 keepalive = -1;
        rv = mPrefs->GetIntPref("network.http.keep-alive", &keepalive);
        mDoKeepAlive = (keepalive == 1);
#endif  // remove till here

    if (bChangedAll || !PL_strcmp(pref, "network.proxy.type"))
    {
        PRInt32 type = -1;
        rv = mPrefs->GetIntPref("network.proxy.type",&type);
        if (NS_SUCCEEDED(rv))
            mUseProxy = (type == 1); // type == 2 is autoconfig stuff
    }

    if (bChangedAll || !PL_strcmp(pref, "network.proxy.http"))
    {
        nsXPIDLCString proxyServer;
        rv = mPrefs->CopyCharPref("network.proxy.http", 
                getter_Copies(proxyServer));
        if (NS_SUCCEEDED(rv)) 
            SetProxyHost(proxyServer);
    }

    if (bChangedAll || !PL_strcmp(pref, "network.proxy.http_port"))
    {
        PRInt32 proxyPort = -1;
        rv = mPrefs->GetIntPref("network.proxy.http_port",&proxyPort);
        if (NS_SUCCEEDED(rv) && proxyPort>0) 
            SetProxyPort(proxyPort);
    }

    if (bChangedAll || !PL_strcmp(pref, "network.proxy.no_proxies_on"))
    {
        nsXPIDLCString noProxy;
        rv = mPrefs->CopyCharPref("network.proxy.no_proxies_on",
                getter_Copies(noProxy));
        if (NS_SUCCEEDED(rv))
            SetDontUseProxyFor(noProxy);
    }

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

PRInt32 PR_CALLBACK ProxyPrefsCallback(const char* pref, void* instance)
{
    nsHTTPHandler* pHandler = (nsHTTPHandler*) instance;
    NS_ASSERTION(nsnull != pHandler, "bad instance data");
    if (nsnull != pHandler)
        pHandler->PrefsChanged(pref);
    return 0;
}

nsresult
nsHTTPHandler::SetDontUseProxyFor(const char* i_hostlist)
{
    CRTFREEIF(mNoProxyFor);
    if (i_hostlist)
    {
        mNoProxyFor = nsCRT::strdup(i_hostlist);
        return (mNoProxyFor == nsnull) ? NS_ERROR_OUT_OF_MEMORY : NS_OK;
    }
    return NS_OK;
}

PRBool
nsHTTPHandler::CanUseProxy(nsIURI* i_Uri)
{

    NS_ASSERTION(mProxy, 
            "This shouldn't even get called if mProxy is null");

    PRBool rv = PR_TRUE;
    if (!mNoProxyFor || !*mNoProxyFor)
        return rv;

    PRInt32 port;
    char* host;

    rv = i_Uri->GetHost(&host);
    if (NS_FAILED(rv)) return rv;

    rv = i_Uri->GetPort(&port);
    if (NS_FAILED(rv)) 
    {
        nsCRT::free(host);
        return rv;
    }

    if (!host)
        return rv;

    // mNoProxyFor is of type "foo bar:8080, baz.com ..."
    char* brk = PL_strpbrk(mNoProxyFor, " ,:");
    char* noProxy = mNoProxyFor;
    PRInt32 noProxyPort = -1;
    int hostLen = PL_strlen(host);
    char* end = (char*)host+hostLen;
    char* nextbrk = end;
    int matchLen = 0;
    do 
    {
        if (brk)
        {
            if (*brk == ':')
            {
                noProxyPort = atoi(brk+1);
                nextbrk = PL_strpbrk(brk+1, " ,");
                if (!nextbrk)
                    nextbrk = end;
            }
            matchLen = brk-noProxy;
        }
        else
            matchLen = end-noProxy;

        if (matchLen <= hostLen) // match is smaller than host
        {
            if (((noProxyPort == -1) || (noProxyPort == port)) &&
                (0 == PL_strncasecmp(host+hostLen-matchLen, 
                             noProxy, matchLen)))
                {
                    nsCRT::free(host);
                    return PR_FALSE;
                }
        }

        noProxy = nextbrk;
        brk = PL_strpbrk(noProxy, " ,:");
    }
    while (brk);

    CRTFREEIF(host);
    return rv;
}
