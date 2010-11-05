/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 sts=4 et cin: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
 *   Gagan Saksena <gagan@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Christopher Blizzard <blizzard@mozilla.org>
 *   Adrian Havill <havill@redhat.com>
 *   Gervase Markham <gerv@gerv.net>
 *   Bradley Baetz <bbaetz@netscape.com>
 *   Benjamin Smedberg <bsmedberg@covad.net>
 *   Josh Aas <josh@mozilla.com>
 *   Dão Gottwald <dao@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHttp.h"
#include "nsHttpHandler.h"
#include "nsHttpChannel.h"
#include "nsHttpConnection.h"
#include "nsHttpResponseHead.h"
#include "nsHttpTransaction.h"
#include "nsHttpAuthCache.h"
#include "nsStandardURL.h"
#include "nsIHttpChannel.h"
#include "nsIURL.h"
#include "nsIStandardURL.h"
#include "nsICacheService.h"
#include "nsICategoryManager.h"
#include "nsCategoryManagerUtils.h"
#include "nsICacheService.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"
#include "nsIPrefLocalizedString.h"
#include "nsISocketProviderService.h"
#include "nsISocketProvider.h"
#include "nsPrintfCString.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "nsAutoLock.h"
#include "prprf.h"
#include "nsReadableUtils.h"
#include "nsQuickSort.h"
#include "nsNetUtil.h"
#include "nsIOService.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsSocketTransportService2.h"

#include "nsIXULAppInfo.h"

#ifdef MOZ_IPC
#include "mozilla/net/NeckoChild.h"
#endif 

#if defined(XP_UNIX) || defined(XP_BEOS)
#include <sys/utsname.h>
#endif

#if defined(XP_WIN)
#include <windows.h>
#endif

#if defined(XP_MACOSX)
#include <CoreServices/CoreServices.h>
#endif

#if defined(XP_OS2)
#define INCL_DOSMISC
#include <os2.h>
#endif

//-----------------------------------------------------------------------------
using namespace mozilla::net;
#ifdef MOZ_IPC
#include "mozilla/net/HttpChannelChild.h"
#endif 

#include "mozilla/FunctionTimer.h"

#ifdef DEBUG
// defined by the socket transport service while active
extern PRThread *gSocketThread;
#endif

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_CID(kStreamConverterServiceCID, NS_STREAMCONVERTERSERVICE_CID);
static NS_DEFINE_CID(kCookieServiceCID, NS_COOKIESERVICE_CID);
static NS_DEFINE_CID(kCacheServiceCID, NS_CACHESERVICE_CID);
static NS_DEFINE_CID(kSocketProviderServiceCID, NS_SOCKETPROVIDERSERVICE_CID);

#define UA_PREF_PREFIX          "general.useragent."
#ifdef XP_WIN
#define UA_SPARE_PLATFORM
#endif

#define HTTP_PREF_PREFIX        "network.http."
#define INTL_ACCEPT_LANGUAGES   "intl.accept_languages"
#define INTL_ACCEPT_CHARSET     "intl.charset.default"
#define NETWORK_ENABLEIDN       "network.enableIDN"
#define BROWSER_PREF_PREFIX     "browser.cache."

#define UA_PREF(_pref) UA_PREF_PREFIX _pref
#define HTTP_PREF(_pref) HTTP_PREF_PREFIX _pref
#define BROWSER_PREF(_pref) BROWSER_PREF_PREFIX _pref

#define NS_HTTP_PROTOCOL_FLAGS (URI_STD | ALLOWS_PROXY | ALLOWS_PROXY_HTTP | URI_LOADABLE_BY_ANYONE)

//-----------------------------------------------------------------------------

static nsresult
NewURI(const nsACString &aSpec,
       const char *aCharset,
       nsIURI *aBaseURI,
       PRInt32 aDefaultPort,
       nsIURI **aURI)
{
    nsStandardURL *url = new nsStandardURL();
    if (!url)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(url);

    nsresult rv = url->Init(nsIStandardURL::URLTYPE_AUTHORITY,
                            aDefaultPort, aSpec, aCharset, aBaseURI);
    if (NS_FAILED(rv)) {
        NS_RELEASE(url);
        return rv;
    }

    *aURI = url; // no QI needed
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler <public>
//-----------------------------------------------------------------------------

nsHttpHandler *gHttpHandler = nsnull;

nsHttpHandler::nsHttpHandler()
    : mConnMgr(nsnull)
    , mHttpVersion(NS_HTTP_VERSION_1_1)
    , mProxyHttpVersion(NS_HTTP_VERSION_1_1)
    , mCapabilities(NS_HTTP_ALLOW_KEEPALIVE)
    , mProxyCapabilities(NS_HTTP_ALLOW_KEEPALIVE)
    , mReferrerLevel(0xff) // by default we always send a referrer
    , mIdleTimeout(10)
    , mMaxRequestAttempts(10)
    , mMaxRequestDelay(10)
    , mMaxConnections(24)
    , mMaxConnectionsPerServer(8)
    , mMaxPersistentConnectionsPerServer(2)
    , mMaxPersistentConnectionsPerProxy(4)
    , mMaxPipelinedRequests(2)
    , mRedirectionLimit(10)
    , mInPrivateBrowsingMode(PR_FALSE)
    , mPhishyUserPassLength(1)
    , mQoSBits(0x00)
    , mPipeliningOverSSL(PR_FALSE)
    , mLastUniqueID(NowInSeconds())
    , mSessionStartTime(0)
    , mLegacyAppName("Mozilla")
    , mLegacyAppVersion("5.0")
    , mProduct("Gecko")
    , mUserAgentIsDirty(PR_TRUE)
    , mUseCache(PR_TRUE)
    , mPromptTempRedirect(PR_TRUE)
    , mSendSecureXSiteReferrer(PR_TRUE)
    , mEnablePersistentHttpsCaching(PR_FALSE)
{
#if defined(PR_LOGGING)
    gHttpLog = PR_NewLogModule("nsHttp");
#endif

    LOG(("Creating nsHttpHandler [this=%x].\n", this));

    NS_ASSERTION(!gHttpHandler, "HTTP handler already created!");
    gHttpHandler = this;
}

nsHttpHandler::~nsHttpHandler()
{
    LOG(("Deleting nsHttpHandler [this=%x]\n", this));

    // make sure the connection manager is shutdown
    if (mConnMgr) {
        mConnMgr->Shutdown();
        NS_RELEASE(mConnMgr);
    }

    // Note: don't call NeckoChild::DestroyNeckoChild() here, as it's too late
    // and it'll segfault.  NeckoChild will get cleaned up by process exit.

    nsHttp::DestroyAtomTable();

    gHttpHandler = nsnull;
}

nsresult
nsHttpHandler::Init()
{
    NS_TIME_FUNCTION;

    nsresult rv;

    LOG(("nsHttpHandler::Init\n"));

    rv = nsHttp::CreateAtomTable();
    if (NS_FAILED(rv))
        return rv;

    mIOService = do_GetService(NS_IOSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
        NS_WARNING("unable to continue without io service");
        return rv;
    }

#ifdef MOZ_IPC
    if (IsNeckoChild())
        NeckoChild::InitNeckoChild();
#endif // MOZ_IPC

    // figure out if we're starting in private browsing mode
    nsCOMPtr<nsIPrivateBrowsingService> pbs =
      do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
    if (pbs)
      pbs->GetPrivateBrowsingEnabled(&mInPrivateBrowsingMode);

    InitUserAgentComponents();

    // monitor some preference changes
    nsCOMPtr<nsIPrefBranch2> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefBranch) {
        prefBranch->AddObserver(HTTP_PREF_PREFIX, this, PR_TRUE);
        prefBranch->AddObserver(UA_PREF_PREFIX, this, PR_TRUE);
        prefBranch->AddObserver(INTL_ACCEPT_LANGUAGES, this, PR_TRUE); 
        prefBranch->AddObserver(INTL_ACCEPT_CHARSET, this, PR_TRUE);
        prefBranch->AddObserver(NETWORK_ENABLEIDN, this, PR_TRUE);
        prefBranch->AddObserver(BROWSER_PREF("disk_cache_ssl"), this, PR_TRUE);

        PrefsChanged(prefBranch, nsnull);
    }

    mMisc.AssignLiteral("rv:" MOZILLA_VERSION);

    nsCOMPtr<nsIXULAppInfo> appInfo =
        do_GetService("@mozilla.org/xre/app-info;1");

    mAppName.AssignLiteral(MOZ_APP_UA_NAME);
    if (mAppName.Length() == 0 && appInfo) {
        appInfo->GetName(mAppName);
        appInfo->GetVersion(mAppVersion);
        mAppName.StripChars(" ()<>@,;:\\\"/[]?={}");
    } else {
        mAppVersion.AssignLiteral(MOZ_APP_UA_VERSION);
    }

#if DEBUG
    // dump user agent prefs
    LOG(("> legacy-app-name = %s\n", mLegacyAppName.get()));
    LOG(("> legacy-app-version = %s\n", mLegacyAppVersion.get()));
    LOG(("> platform = %s\n", mPlatform.get()));
    LOG(("> oscpu = %s\n", mOscpu.get()));
    LOG(("> language = %s\n", mLanguage.get()));
    LOG(("> misc = %s\n", mMisc.get()));
    LOG(("> product = %s\n", mProduct.get()));
    LOG(("> product-sub = %s\n", mProductSub.get()));
    LOG(("> app-name = %s\n", mAppName.get()));
    LOG(("> app-version = %s\n", mAppVersion.get()));
    LOG(("> compat-firefox = %s\n", mCompatFirefox.get()));
    LOG(("> user-agent = %s\n", UserAgent().get()));
#endif

    mSessionStartTime = NowInSeconds();

    rv = mAuthCache.Init();
    if (NS_FAILED(rv)) return rv;

    rv = InitConnectionMgr();
    if (NS_FAILED(rv)) return rv;

    mProductSub.AssignLiteral(MOZ_UA_BUILDID);
    if (mProductSub.IsEmpty() && appInfo)
        appInfo->GetPlatformBuildID(mProductSub);
    if (mProductSub.Length() > 8)
        mProductSub.SetLength(8);

    // Startup the http category
    // Bring alive the objects in the http-protocol-startup category
    NS_CreateServicesFromCategory(NS_HTTP_STARTUP_CATEGORY,
                                  static_cast<nsISupports*>(static_cast<void*>(this)),
                                  NS_HTTP_STARTUP_TOPIC);    
    
    mObserverService = mozilla::services::GetObserverService();
    if (mObserverService) {
        mObserverService->AddObserver(this, "profile-change-net-teardown", PR_TRUE);
        mObserverService->AddObserver(this, "profile-change-net-restore", PR_TRUE);
        mObserverService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_TRUE);
        mObserverService->AddObserver(this, "net:clear-active-logins", PR_TRUE);
        mObserverService->AddObserver(this, NS_PRIVATE_BROWSING_SWITCH_TOPIC, PR_TRUE);
        mObserverService->AddObserver(this, "net:prune-dead-connections", PR_TRUE);
    }
 
    return NS_OK;
}

nsresult
nsHttpHandler::InitConnectionMgr()
{
    NS_TIME_FUNCTION;

    nsresult rv;

    if (!mConnMgr) {
        mConnMgr = new nsHttpConnectionMgr();
        if (!mConnMgr)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mConnMgr);
    }

    rv = mConnMgr->Init(mMaxConnections,
                        mMaxConnectionsPerServer,
                        mMaxConnectionsPerServer,
                        mMaxPersistentConnectionsPerServer,
                        mMaxPersistentConnectionsPerProxy,
                        mMaxRequestDelay,
                        mMaxPipelinedRequests);
    return rv;
}

nsresult
nsHttpHandler::AddStandardRequestHeaders(nsHttpHeaderArray *request,
                                         PRUint8 caps,
                                         PRBool useProxy)
{
    nsresult rv;

    // Add the "User-Agent" header
    rv = request->SetHeader(nsHttp::User_Agent, UserAgent());
    if (NS_FAILED(rv)) return rv;

    // MIME based content negotiation lives!
    // Add the "Accept" header
    rv = request->SetHeader(nsHttp::Accept, mAccept);
    if (NS_FAILED(rv)) return rv;

    // Add the "Accept-Language" header
    if (!mAcceptLanguages.IsEmpty()) {
        // Add the "Accept-Language" header
        rv = request->SetHeader(nsHttp::Accept_Language, mAcceptLanguages);
        if (NS_FAILED(rv)) return rv;
    }

    // Add the "Accept-Encoding" header
    rv = request->SetHeader(nsHttp::Accept_Encoding, mAcceptEncodings);
    if (NS_FAILED(rv)) return rv;

    // Add the "Accept-Charset" header
    rv = request->SetHeader(nsHttp::Accept_Charset, mAcceptCharsets);
    if (NS_FAILED(rv)) return rv;

    // RFC2616 section 19.6.2 states that the "Connection: keep-alive"
    // and "Keep-alive" request headers should not be sent by HTTP/1.1
    // user-agents.  Otherwise, problems with proxy servers (especially
    // transparent proxies) can result.
    //
    // However, we need to send something so that we can use keepalive
    // with HTTP/1.0 servers/proxies. We use "Proxy-Connection:" when 
    // we're talking to an http proxy, and "Connection:" otherwise
    
    NS_NAMED_LITERAL_CSTRING(close, "close");
    NS_NAMED_LITERAL_CSTRING(keepAlive, "keep-alive");

    const nsACString *connectionType = &close;
    if (caps & NS_HTTP_ALLOW_KEEPALIVE) {
        rv = request->SetHeader(nsHttp::Keep_Alive, nsPrintfCString("%u", mIdleTimeout));
        if (NS_FAILED(rv)) return rv;
        connectionType = &keepAlive;
    } else if (useProxy) {
        // Bug 92006
        request->SetHeader(nsHttp::Connection, close);
    }

    const nsHttpAtom &header = useProxy ? nsHttp::Proxy_Connection
                                        : nsHttp::Connection;
    return request->SetHeader(header, *connectionType);
}

PRBool
nsHttpHandler::IsAcceptableEncoding(const char *enc)
{
    if (!enc)
        return PR_FALSE;

    // HTTP 1.1 allows servers to send x-gzip and x-compress instead
    // of gzip and compress, for example.  So, we'll always strip off
    // an "x-" prefix before matching the encoding to one we claim
    // to accept.
    if (!PL_strncasecmp(enc, "x-", 2))
        enc += 2;
    
    return nsHttp::FindToken(mAcceptEncodings.get(), enc, HTTP_LWS ",") != nsnull;
}

nsresult
nsHttpHandler::GetCacheSession(nsCacheStoragePolicy storagePolicy,
                               nsICacheSession **result)
{
    nsresult rv;

    // Skip cache if disabled in preferences
    if (!mUseCache)
        return NS_ERROR_NOT_AVAILABLE;

    // We want to get the pointer to the cache service each time we're called,
    // because it's possible for some add-ons (such as Google Gears) to swap
    // in new cache services on the fly, and we want to pick them up as
    // appropriate.
    nsCOMPtr<nsICacheService> serv = do_GetService(NS_CACHESERVICE_CONTRACTID,
                                                   &rv);
    if (NS_FAILED(rv)) return rv;

    const char *sessionName = "HTTP";
    switch (storagePolicy) {
    case nsICache::STORE_IN_MEMORY:
        sessionName = "HTTP-memory-only";
        break;
    case nsICache::STORE_OFFLINE:
        sessionName = "HTTP-offline";
        break;
    default:
        break;
    }

    nsCOMPtr<nsICacheSession> cacheSession;
    rv = serv->CreateSession(sessionName,
                             storagePolicy,
                             nsICache::STREAM_BASED,
                             getter_AddRefs(cacheSession));
    if (NS_FAILED(rv)) return rv;

    rv = cacheSession->SetDoomEntriesIfExpired(PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = cacheSession);

    return NS_OK;
}

nsresult
nsHttpHandler::GetStreamConverterService(nsIStreamConverterService **result)
{
    if (!mStreamConvSvc) {
        nsresult rv;
        mStreamConvSvc = do_GetService(NS_STREAMCONVERTERSERVICE_CONTRACTID, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    *result = mStreamConvSvc;
    NS_ADDREF(*result);
    return NS_OK;
}

nsIStrictTransportSecurityService*
nsHttpHandler::GetSTSService()
{
    if (!mSTSService)
      mSTSService = do_GetService(NS_STSSERVICE_CONTRACTID);
    return mSTSService;
}

nsICookieService *
nsHttpHandler::GetCookieService()
{
    if (!mCookieService)
        mCookieService = do_GetService(NS_COOKIESERVICE_CONTRACTID);
    return mCookieService;
}

nsresult 
nsHttpHandler::GetIOService(nsIIOService** result)
{
    NS_ADDREF(*result = mIOService);
    return NS_OK;
}


void
nsHttpHandler::NotifyObservers(nsIHttpChannel *chan, const char *event)
{
    LOG(("nsHttpHandler::NotifyObservers [chan=%x event=\"%s\"]\n", chan, event));
    if (mObserverService)
        mObserverService->NotifyObservers(chan, event, nsnull);
}

nsresult
nsHttpHandler::AsyncOnChannelRedirect(nsIChannel* oldChan, nsIChannel* newChan,
                                 PRUint32 flags)
{
    // TODO E10S This helper has to be initialized on the other process
    nsRefPtr<nsAsyncRedirectVerifyHelper> redirectCallbackHelper =
        new nsAsyncRedirectVerifyHelper();

    return redirectCallbackHelper->Init(oldChan, newChan, flags);
}

/* static */ nsresult
nsHttpHandler::GenerateHostPort(const nsCString& host, PRInt32 port,
                                nsCString& hostLine)
{
    if (strchr(host.get(), ':')) {
        // host is an IPv6 address literal and must be encapsulated in []'s
        hostLine.Assign('[');
        // scope id is not needed for Host header.
        int scopeIdPos = host.FindChar('%');
        if (scopeIdPos == kNotFound)
            hostLine.Append(host);
        else if (scopeIdPos > 0)
            hostLine.Append(Substring(host, 0, scopeIdPos));
        else
          return NS_ERROR_MALFORMED_URI;
        hostLine.Append(']');
    }
    else
        hostLine.Assign(host);
    if (port != -1) {
        hostLine.Append(':');
        hostLine.AppendInt(port);
    }
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler <private>
//-----------------------------------------------------------------------------

const nsAFlatCString &
nsHttpHandler::UserAgent()
{
    if (mUserAgentOverride) {
        LOG(("using general.useragent.override : %s\n", mUserAgentOverride.get()));
        return mUserAgentOverride;
    }

    if (mUserAgentIsDirty) {
        BuildUserAgent();
        mUserAgentIsDirty = PR_FALSE;
    }

    return mUserAgent;
}

void
nsHttpHandler::BuildUserAgent()
{
    LOG(("nsHttpHandler::BuildUserAgent\n"));

    NS_ASSERTION(!mLegacyAppName.IsEmpty() &&
                 !mLegacyAppVersion.IsEmpty() &&
                 !mPlatform.IsEmpty() &&
                 !mOscpu.IsEmpty(),
                 "HTTP cannot send practical requests without this much");

    // preallocate to worst-case size, which should always be better
    // than if we didn't preallocate at all.
    mUserAgent.SetCapacity(mLegacyAppName.Length() + 
                           mLegacyAppVersion.Length() + 
#ifndef UA_SPARE_PLATFORM
                           mPlatform.Length() + 
#endif
                           mOscpu.Length() +
                           mMisc.Length() +
                           mProduct.Length() +
                           mProductSub.Length() +
                           mAppName.Length() +
                           mAppVersion.Length() +
                           mCompatFirefox.Length() +
                           13);

    // Application portion
    mUserAgent.Assign(mLegacyAppName);
    mUserAgent += '/';
    mUserAgent += mLegacyAppVersion;
    mUserAgent += ' ';

    // Application comment
    mUserAgent += '(';
#ifndef UA_SPARE_PLATFORM
    mUserAgent += mPlatform;
    mUserAgent.AppendLiteral("; ");
#endif
    mUserAgent += mOscpu;
    mUserAgent.AppendLiteral("; ");
    mUserAgent += mMisc;
    mUserAgent += ')';

    // Product portion
    mUserAgent += ' ';
    mUserAgent += mProduct;
    mUserAgent += '/';
    mUserAgent += mProductSub;

    // "Firefox/x.y.z" compatibility token
    if (!mCompatFirefox.IsEmpty()) {
        mUserAgent += ' ';
        mUserAgent += mCompatFirefox;
    }

    // App portion
    mUserAgent += ' ';
    mUserAgent += mAppName;
    mUserAgent += '/';
    mUserAgent += mAppVersion;
}

#ifdef XP_WIN
typedef BOOL (WINAPI *IsWow64ProcessP) (HANDLE, PBOOL);

#define WNT_BASE "Windows NT %ld.%ld"
#define W64_PREFIX "; Win64"
#endif

void
nsHttpHandler::InitUserAgentComponents()
{

      // Gather platform.
    mPlatform.AssignLiteral(
#if defined(ANDROID)
    "Android"
#elif defined(XP_OS2)
    "OS/2"
#elif defined(XP_WIN)
    "Windows"
#elif defined(XP_MACOSX)
    "Macintosh"
#elif defined(XP_BEOS)
    "BeOS"
#elif defined(MOZ_PLATFORM_MAEMO)
    "Maemo"
#elif defined(MOZ_X11)
    "X11"
#else
    "?"
#endif
    );

    // Gather OS/CPU.
#if defined(XP_OS2)
    ULONG os2ver = 0;
    DosQuerySysInfo(QSV_VERSION_MINOR, QSV_VERSION_MINOR,
                    &os2ver, sizeof(os2ver));
    if (os2ver == 11)
        mOscpu.AssignLiteral("2.11");
    else if (os2ver == 30)
        mOscpu.AssignLiteral("Warp 3");
    else if (os2ver == 40)
        mOscpu.AssignLiteral("Warp 4");
    else if (os2ver == 45)
        mOscpu.AssignLiteral("Warp 4.5");

#elif defined(WINCE) || defined(XP_WIN)
    OSVERSIONINFO info = { sizeof(OSVERSIONINFO) };
    if (GetVersionEx(&info)) {
        const char *format;
#ifdef WINCE
        format = "WindowsCE %ld.%ld";
#elif defined _M_IA64
        format = WNT_BASE W64_PREFIX "; IA64";
#elif defined _M_X64 || defined _M_AMD64
        format = WNT_BASE W64_PREFIX "; x64";
#else
        BOOL isWow64 = FALSE;
        IsWow64ProcessP fnIsWow64Process = (IsWow64ProcessP)
          GetProcAddress(GetModuleHandleW(L"kernel32"), "IsWow64Process");
        if (fnIsWow64Process &&
            !fnIsWow64Process(GetCurrentProcess(), &isWow64)) {
            isWow64 = FALSE;
        }
        format = isWow64
          ? WNT_BASE "; WOW64"
          : WNT_BASE;
#endif
        char *buf = PR_smprintf(format,
                                info.dwMajorVersion,
                                info.dwMinorVersion);
        if (buf) {
            mOscpu = buf;
            PR_smprintf_free(buf);
        }
    }
#elif defined (XP_MACOSX)
#if defined(__ppc__)
    mOscpu.AssignLiteral("PPC Mac OS X");
#elif defined(__i386__) || defined(__x86_64__)
    mOscpu.AssignLiteral("Intel Mac OS X");
#endif
    SInt32 majorVersion, minorVersion;
    if ((::Gestalt(gestaltSystemVersionMajor, &majorVersion) == noErr) &&
        (::Gestalt(gestaltSystemVersionMinor, &minorVersion) == noErr)) {
        mOscpu += nsPrintfCString(" %d.%d", majorVersion, minorVersion);
    }
#elif defined (XP_UNIX) || defined (XP_BEOS)
    struct utsname name;
    
    int ret = uname(&name);
    if (ret >= 0) {
        nsCAutoString buf;
        buf =  (char*)name.sysname;

        if (strcmp(name.machine, "x86_64") == 0 &&
            sizeof(void *) == sizeof(PRInt32)) {
            // We're running 32-bit code on x86_64. Make this browser
            // look like it's running on i686 hardware, but append "
            // (x86_64)" to the end of the oscpu identifier to be able
            // to differentiate this from someone running 64-bit code
            // on x86_64..

            buf += " i686 on x86_64";
        } else {
            buf += ' ';

#ifdef AIX
            // AIX uname returns machine specific info in the uname.machine
            // field and does not return the cpu type like other platforms.
            // We use the AIX version and release numbers instead.
            buf += (char*)name.version;
            buf += '.';
            buf += (char*)name.release;
#else
            buf += (char*)name.machine;
#endif
        }

        mOscpu.Assign(buf);
    }
#endif

    mUserAgentIsDirty = PR_TRUE;
}

void
nsHttpHandler::PrefsChanged(nsIPrefBranch *prefs, const char *pref)
{
    nsresult rv = NS_OK;
    PRInt32 val;

    LOG(("nsHttpHandler::PrefsChanged [pref=%s]\n", pref));

#define PREF_CHANGED(p) ((pref == nsnull) || !PL_strcmp(pref, p))
#define MULTI_PREF_CHANGED(p) \
  ((pref == nsnull) || !PL_strncmp(pref, p, sizeof(p) - 1))

    //
    // UA components
    //

    PRBool cVar = PR_FALSE;

    if (PREF_CHANGED(UA_PREF("compatMode.firefox"))) {
        rv = prefs->GetBoolPref(UA_PREF("compatMode.firefox"), &cVar);
        if (NS_SUCCEEDED(rv) && cVar) {
            mCompatFirefox.AssignLiteral("Firefox/" MOZ_UA_FIREFOX_VERSION);
        } else {
            mCompatFirefox.Truncate();
        }
        mUserAgentIsDirty = PR_TRUE;
    }

    // Gather locale.
    if (PREF_CHANGED(UA_PREF("locale"))) {
        nsCOMPtr<nsIPrefLocalizedString> pls;
        prefs->GetComplexValue(UA_PREF("locale"),
                               NS_GET_IID(nsIPrefLocalizedString),
                               getter_AddRefs(pls));
        if (pls) {
            nsXPIDLString uval;
            pls->ToString(getter_Copies(uval));
            if (uval)
                CopyUTF16toUTF8(uval, mLanguage);
        }
        else {
            nsXPIDLCString cval;
            rv = prefs->GetCharPref(UA_PREF("locale"), getter_Copies(cval));
            if (cval)
                mLanguage.Assign(cval);
        }

        mUserAgentIsDirty = PR_TRUE;
    }

    // general.useragent.override
    if (PREF_CHANGED(UA_PREF("override"))) {
        prefs->GetCharPref(UA_PREF("override"),
                            getter_Copies(mUserAgentOverride));
        mUserAgentIsDirty = PR_TRUE;
    }

    //
    // HTTP options
    //

    if (PREF_CHANGED(HTTP_PREF("keep-alive.timeout"))) {
        rv = prefs->GetIntPref(HTTP_PREF("keep-alive.timeout"), &val);
        if (NS_SUCCEEDED(rv))
            mIdleTimeout = (PRUint16) NS_CLAMP(val, 1, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("request.max-attempts"))) {
        rv = prefs->GetIntPref(HTTP_PREF("request.max-attempts"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxRequestAttempts = (PRUint16) NS_CLAMP(val, 1, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("request.max-start-delay"))) {
        rv = prefs->GetIntPref(HTTP_PREF("request.max-start-delay"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxRequestDelay = (PRUint16) NS_CLAMP(val, 0, 0xffff);
            if (mConnMgr)
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_REQUEST_DELAY,
                                      mMaxRequestDelay);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("max-connections"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-connections"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxConnections = (PRUint16) NS_CLAMP(val, 1, NS_SOCKET_MAX_COUNT);
            if (mConnMgr)
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_CONNECTIONS,
                                      mMaxConnections);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("max-connections-per-server"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-connections-per-server"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxConnectionsPerServer = (PRUint8) NS_CLAMP(val, 1, 0xff);
            if (mConnMgr) {
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_CONNECTIONS_PER_HOST,
                                      mMaxConnectionsPerServer);
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_CONNECTIONS_PER_PROXY,
                                      mMaxConnectionsPerServer);
            }
        }
    }

    if (PREF_CHANGED(HTTP_PREF("max-persistent-connections-per-server"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-persistent-connections-per-server"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxPersistentConnectionsPerServer = (PRUint8) NS_CLAMP(val, 1, 0xff);
            if (mConnMgr)
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_PERSISTENT_CONNECTIONS_PER_HOST,
                                      mMaxPersistentConnectionsPerServer);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("max-persistent-connections-per-proxy"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-persistent-connections-per-proxy"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxPersistentConnectionsPerProxy = (PRUint8) NS_CLAMP(val, 1, 0xff);
            if (mConnMgr)
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_PERSISTENT_CONNECTIONS_PER_PROXY,
                                      mMaxPersistentConnectionsPerProxy);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("sendRefererHeader"))) {
        rv = prefs->GetIntPref(HTTP_PREF("sendRefererHeader"), &val);
        if (NS_SUCCEEDED(rv))
            mReferrerLevel = (PRUint8) NS_CLAMP(val, 0, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("redirection-limit"))) {
        rv = prefs->GetIntPref(HTTP_PREF("redirection-limit"), &val);
        if (NS_SUCCEEDED(rv))
            mRedirectionLimit = (PRUint8) NS_CLAMP(val, 0, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("version"))) {
        nsXPIDLCString httpVersion;
        prefs->GetCharPref(HTTP_PREF("version"), getter_Copies(httpVersion));
        if (httpVersion) {
            if (!PL_strcmp(httpVersion, "1.1"))
                mHttpVersion = NS_HTTP_VERSION_1_1;
            else if (!PL_strcmp(httpVersion, "0.9"))
                mHttpVersion = NS_HTTP_VERSION_0_9;
            else
                mHttpVersion = NS_HTTP_VERSION_1_0;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("proxy.version"))) {
        nsXPIDLCString httpVersion;
        prefs->GetCharPref(HTTP_PREF("proxy.version"), getter_Copies(httpVersion));
        if (httpVersion) {
            if (!PL_strcmp(httpVersion, "1.1"))
                mProxyHttpVersion = NS_HTTP_VERSION_1_1;
            else
                mProxyHttpVersion = NS_HTTP_VERSION_1_0;
            // it does not make sense to issue a HTTP/0.9 request to a proxy server
        }
    }

    if (PREF_CHANGED(HTTP_PREF("keep-alive"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("keep-alive"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mCapabilities |= NS_HTTP_ALLOW_KEEPALIVE;
            else
                mCapabilities &= ~NS_HTTP_ALLOW_KEEPALIVE;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("proxy.keep-alive"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("proxy.keep-alive"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mProxyCapabilities |= NS_HTTP_ALLOW_KEEPALIVE;
            else
                mProxyCapabilities &= ~NS_HTTP_ALLOW_KEEPALIVE;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("pipelining"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("pipelining"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mCapabilities |=  NS_HTTP_ALLOW_PIPELINING;
            else
                mCapabilities &= ~NS_HTTP_ALLOW_PIPELINING;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("pipelining.maxrequests"))) {
        rv = prefs->GetIntPref(HTTP_PREF("pipelining.maxrequests"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxPipelinedRequests = NS_CLAMP(val, 1, NS_HTTP_MAX_PIPELINED_REQUESTS);
            if (mConnMgr)
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_PIPELINED_REQUESTS,
                                      mMaxPipelinedRequests);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("pipelining.ssl"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("pipelining.ssl"), &cVar);
        if (NS_SUCCEEDED(rv))
            mPipeliningOverSSL = cVar;
    }

    if (PREF_CHANGED(HTTP_PREF("proxy.pipelining"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("proxy.pipelining"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            if (cVar)
                mProxyCapabilities |=  NS_HTTP_ALLOW_PIPELINING;
            else
                mProxyCapabilities &= ~NS_HTTP_ALLOW_PIPELINING;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("qos"))) {
        rv = prefs->GetIntPref(HTTP_PREF("qos"), &val);
        if (NS_SUCCEEDED(rv))
            mQoSBits = (PRUint8) NS_CLAMP(val, 0, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("sendSecureXSiteReferrer"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("sendSecureXSiteReferrer"), &cVar);
        if (NS_SUCCEEDED(rv))
            mSendSecureXSiteReferrer = cVar;
    }

    if (PREF_CHANGED(HTTP_PREF("accept.default"))) {
        nsXPIDLCString accept;
        rv = prefs->GetCharPref(HTTP_PREF("accept.default"),
                                  getter_Copies(accept));
        if (NS_SUCCEEDED(rv))
            SetAccept(accept);
    }
    
    if (PREF_CHANGED(HTTP_PREF("accept-encoding"))) {
        nsXPIDLCString acceptEncodings;
        rv = prefs->GetCharPref(HTTP_PREF("accept-encoding"),
                                  getter_Copies(acceptEncodings));
        if (NS_SUCCEEDED(rv))
            SetAcceptEncodings(acceptEncodings);
    }

    if (PREF_CHANGED(HTTP_PREF("use-cache"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("use-cache"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            mUseCache = cVar;
        }
    }

    if (PREF_CHANGED(HTTP_PREF("default-socket-type"))) {
        nsXPIDLCString sval;
        rv = prefs->GetCharPref(HTTP_PREF("default-socket-type"),
                                getter_Copies(sval));
        if (NS_SUCCEEDED(rv)) {
            if (sval.IsEmpty())
                mDefaultSocketType.Adopt(0);
            else {
                // verify that this socket type is actually valid
                nsCOMPtr<nsISocketProviderService> sps(
                        do_GetService(NS_SOCKETPROVIDERSERVICE_CONTRACTID));
                if (sps) {
                    nsCOMPtr<nsISocketProvider> sp;
                    rv = sps->GetSocketProvider(sval, getter_AddRefs(sp));
                    if (NS_SUCCEEDED(rv)) {
                        // OK, this looks like a valid socket provider.
                        mDefaultSocketType.Assign(sval);
                    }
                }
            }
        }
    }

    if (PREF_CHANGED(HTTP_PREF("prompt-temp-redirect"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("prompt-temp-redirect"), &cVar);
        if (NS_SUCCEEDED(rv)) {
            mPromptTempRedirect = cVar;
        }
    }

    // enable Persistent caching for HTTPS - bug#205921    
    if (PREF_CHANGED(BROWSER_PREF("disk_cache_ssl"))) {
        cVar = PR_FALSE;
        rv = prefs->GetBoolPref(BROWSER_PREF("disk_cache_ssl"), &cVar);
        if (NS_SUCCEEDED(rv))
            mEnablePersistentHttpsCaching = cVar;
    }

    if (PREF_CHANGED(HTTP_PREF("phishy-userpass-length"))) {
        rv = prefs->GetIntPref(HTTP_PREF("phishy-userpass-length"), &val);
        if (NS_SUCCEEDED(rv))
            mPhishyUserPassLength = (PRUint8) NS_CLAMP(val, 0, 0xff);
    }

    //
    // INTL options
    //

    if (PREF_CHANGED(INTL_ACCEPT_LANGUAGES)) {
        nsCOMPtr<nsIPrefLocalizedString> pls;
        prefs->GetComplexValue(INTL_ACCEPT_LANGUAGES,
                                NS_GET_IID(nsIPrefLocalizedString),
                                getter_AddRefs(pls));
        if (pls) {
            nsXPIDLString uval;
            pls->ToString(getter_Copies(uval));
            if (uval)
                SetAcceptLanguages(NS_ConvertUTF16toUTF8(uval).get());
        } 
    }

    if (PREF_CHANGED(INTL_ACCEPT_CHARSET)) {
        nsCOMPtr<nsIPrefLocalizedString> pls;
        prefs->GetComplexValue(INTL_ACCEPT_CHARSET,
                                NS_GET_IID(nsIPrefLocalizedString),
                                getter_AddRefs(pls));
        if (pls) {
            nsXPIDLString uval;
            pls->ToString(getter_Copies(uval));
            if (uval)
                SetAcceptCharsets(NS_ConvertUTF16toUTF8(uval).get());
        } 
    }

    //
    // IDN options
    //

    if (PREF_CHANGED(NETWORK_ENABLEIDN)) {
        PRBool enableIDN = PR_FALSE;
        prefs->GetBoolPref(NETWORK_ENABLEIDN, &enableIDN);
        // No locking is required here since this method runs in the main
        // UI thread, and so do all the methods in nsHttpChannel.cpp
        // (mIDNConverter is used by nsHttpChannel)
        if (enableIDN && !mIDNConverter) {
            mIDNConverter = do_GetService(NS_IDNSERVICE_CONTRACTID);
            NS_ASSERTION(mIDNConverter, "idnSDK not installed");
        }
        else if (!enableIDN && mIDNConverter)
            mIDNConverter = nsnull;
    }

#undef PREF_CHANGED
#undef MULTI_PREF_CHANGED
}

/**
 *  Allocates a C string into that contains a ISO 639 language list
 *  notated with HTTP "q" values for output with a HTTP Accept-Language
 *  header. Previous q values will be stripped because the order of
 *  the langs imply the q value. The q values are calculated by dividing
 *  1.0 amongst the number of languages present.
 *
 *  Ex: passing: "en, ja"
 *      returns: "en,ja;q=0.5"
 *
 *      passing: "en, ja, fr_CA"
 *      returns: "en,ja;q=0.7,fr_CA;q=0.3"
 */
static nsresult
PrepareAcceptLanguages(const char *i_AcceptLanguages, nsACString &o_AcceptLanguages)
{
    if (!i_AcceptLanguages)
        return NS_OK;

    PRUint32 n, size, wrote;
    double q, dec;
    char *p, *p2, *token, *q_Accept, *o_Accept;
    const char *comma;
    PRInt32 available;

    o_Accept = nsCRT::strdup(i_AcceptLanguages);
    if (!o_Accept)
        return NS_ERROR_OUT_OF_MEMORY;
    for (p = o_Accept, n = size = 0; '\0' != *p; p++) {
        if (*p == ',') n++;
            size++;
    }

    available = size + ++n * 11 + 1;
    q_Accept = new char[available];
    if (!q_Accept) {
        nsCRT::free(o_Accept);
        return NS_ERROR_OUT_OF_MEMORY;
    }
    *q_Accept = '\0';
    q = 1.0;
    dec = q / (double) n;
    n = 0;
    p2 = q_Accept;
    for (token = nsCRT::strtok(o_Accept, ",", &p);
         token != (char *) 0;
         token = nsCRT::strtok(p, ",", &p))
    {
        token = net_FindCharNotInSet(token, HTTP_LWS);
        char* trim;
        trim = net_FindCharInSet(token, ";" HTTP_LWS);
        if (trim != (char*)0)  // remove "; q=..." if present
            *trim = '\0';

        if (*token != '\0') {
            comma = n++ != 0 ? "," : ""; // delimiter if not first item
            PRUint32 u = QVAL_TO_UINT(q);
            if (u < 10)
                wrote = PR_snprintf(p2, available, "%s%s;q=0.%u", comma, token, u);
            else
                wrote = PR_snprintf(p2, available, "%s%s", comma, token);
            q -= dec;
            p2 += wrote;
            available -= wrote;
            NS_ASSERTION(available > 0, "allocated string not long enough");
        }
    }
    nsCRT::free(o_Accept);

    o_AcceptLanguages.Assign((const char *) q_Accept);
    delete [] q_Accept;

    return NS_OK;
}

nsresult
nsHttpHandler::SetAcceptLanguages(const char *aAcceptLanguages) 
{
    nsCAutoString buf;
    nsresult rv = PrepareAcceptLanguages(aAcceptLanguages, buf);
    if (NS_SUCCEEDED(rv))
        mAcceptLanguages.Assign(buf);
    return rv;
}

/**
 *  Allocates a C string into that contains a character set/encoding list
 *  notated with HTTP "q" values for output with a HTTP Accept-Charset
 *  header. If the UTF-8 character set is not present, it will be added.
 *  If a wildcard catch-all is not present, it will be added. If more than
 *  one charset is set (as of 2001-02-07, only one is used), they will be
 *  comma delimited and with q values set for each charset in decending order.
 *
 *  Ex: passing: "euc-jp"
 *      returns: "euc-jp,utf-8;q=0.6,*;q=0.6"
 *
 *      passing: "UTF-8"
 *      returns: "UTF-8, *"
 */
static nsresult
PrepareAcceptCharsets(const char *i_AcceptCharset, nsACString &o_AcceptCharset)
{
    PRUint32 n, size, wrote, u;
    PRInt32 available;
    double q, dec;
    char *p, *p2, *token, *q_Accept, *o_Accept;
    const char *acceptable, *comma;
    PRBool add_utf = PR_FALSE;
    PRBool add_asterisk = PR_FALSE;

    if (!i_AcceptCharset)
        acceptable = "";
    else
        acceptable = i_AcceptCharset;
    o_Accept = nsCRT::strdup(acceptable);
    if (nsnull == o_Accept)
        return NS_ERROR_OUT_OF_MEMORY;
    for (p = o_Accept, n = size = 0; '\0' != *p; p++) {
        if (*p == ',') n++;
            size++;
    }

    // only add "utf-8" and "*" to the list if they aren't
    // already specified.

    if (PL_strcasestr(acceptable, "utf-8") == NULL) {
        n++;
        add_utf = PR_TRUE;
    }
    if (PL_strchr(acceptable, '*') == NULL) {
        n++;
        add_asterisk = PR_TRUE;
    }

    available = size + ++n * 11 + 1;
    q_Accept = new char[available];
    if ((char *) 0 == q_Accept)
        return NS_ERROR_OUT_OF_MEMORY;
    *q_Accept = '\0';
    q = 1.0;
    dec = q / (double) n;
    n = 0;
    p2 = q_Accept;
    for (token = nsCRT::strtok(o_Accept, ",", &p);
         token != (char *) 0;
         token = nsCRT::strtok(p, ",", &p)) {
        token = net_FindCharNotInSet(token, HTTP_LWS);
        char* trim;
        trim = net_FindCharInSet(token, ";" HTTP_LWS);
        if (trim != (char*)0)  // remove "; q=..." if present
            *trim = '\0';

        if (*token != '\0') {
            comma = n++ != 0 ? "," : ""; // delimiter if not first item
            u = QVAL_TO_UINT(q);
            if (u < 10)
                wrote = PR_snprintf(p2, available, "%s%s;q=0.%u", comma, token, u);
            else
                wrote = PR_snprintf(p2, available, "%s%s", comma, token);
            q -= dec;
            p2 += wrote;
            available -= wrote;
            NS_ASSERTION(available > 0, "allocated string not long enough");
        }
    }
    if (add_utf) {
        comma = n++ != 0 ? "," : ""; // delimiter if not first item
        u = QVAL_TO_UINT(q);
        if (u < 10)
            wrote = PR_snprintf(p2, available, "%sutf-8;q=0.%u", comma, u);
        else
            wrote = PR_snprintf(p2, available, "%sutf-8", comma);
        q -= dec;
        p2 += wrote;
        available -= wrote;
        NS_ASSERTION(available > 0, "allocated string not long enough");
    }
    if (add_asterisk) {
        comma = n++ != 0 ? "," : ""; // delimiter if not first item

        // keep q of "*" equal to the lowest q value
        // in the event of a tie between the q of "*" and a non-wildcard
        // the non-wildcard always receives preference.

        q += dec;
        u = QVAL_TO_UINT(q);
        if (u < 10)
            wrote = PR_snprintf(p2, available, "%s*;q=0.%u", comma, u);
        else
            wrote = PR_snprintf(p2, available, "%s*", comma);
        available -= wrote;
        p2 += wrote;
        NS_ASSERTION(available > 0, "allocated string not long enough");
    }
    nsCRT::free(o_Accept);

    // change alloc from C++ new/delete to nsCRT::strdup's way
    o_AcceptCharset.Assign(q_Accept);
#if defined DEBUG_havill
    printf("Accept-Charset: %s\n", q_Accept);
#endif
    delete [] q_Accept;
    return NS_OK;
}

nsresult
nsHttpHandler::SetAcceptCharsets(const char *aAcceptCharsets) 
{
    nsCString buf;
    nsresult rv = PrepareAcceptCharsets(aAcceptCharsets, buf);
    if (NS_SUCCEEDED(rv))
        mAcceptCharsets.Assign(buf);
    return rv;
}

nsresult
nsHttpHandler::SetAccept(const char *aAccept) 
{
    mAccept = aAccept;
    return NS_OK;
}

nsresult
nsHttpHandler::SetAcceptEncodings(const char *aAcceptEncodings) 
{
    mAcceptEncodings = aAcceptEncodings;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS5(nsHttpHandler,
                              nsIHttpProtocolHandler,
                              nsIProxiedProtocolHandler,
                              nsIProtocolHandler,
                              nsIObserver,
                              nsISupportsWeakReference)

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::GetScheme(nsACString &aScheme)
{
    aScheme.AssignLiteral("http");
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetDefaultPort(PRInt32 *result)
{
    *result = NS_HTTP_DEFAULT_PORT;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProtocolFlags(PRUint32 *result)
{
    *result = NS_HTTP_PROTOCOL_FLAGS;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::NewURI(const nsACString &aSpec,
                      const char *aCharset,
                      nsIURI *aBaseURI,
                      nsIURI **aURI)
{
    LOG(("nsHttpHandler::NewURI\n"));
    return ::NewURI(aSpec, aCharset, aBaseURI, NS_HTTP_DEFAULT_PORT, aURI);
}

NS_IMETHODIMP
nsHttpHandler::NewChannel(nsIURI *uri, nsIChannel **result)
{
    LOG(("nsHttpHandler::NewChannel\n"));

    NS_ENSURE_ARG_POINTER(uri);
    NS_ENSURE_ARG_POINTER(result);

    PRBool isHttp = PR_FALSE, isHttps = PR_FALSE;

    // Verify that we have been given a valid scheme
    nsresult rv = uri->SchemeIs("http", &isHttp);
    if (NS_FAILED(rv)) return rv;
    if (!isHttp) {
        rv = uri->SchemeIs("https", &isHttps);
        if (NS_FAILED(rv)) return rv;
        if (!isHttps) {
            NS_WARNING("Invalid URI scheme");
            return NS_ERROR_UNEXPECTED;
        }
    }
    
    return NewProxiedChannel(uri, nsnull, result);
}

NS_IMETHODIMP 
nsHttpHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIProxiedProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::NewProxiedChannel(nsIURI *uri,
                                 nsIProxyInfo* givenProxyInfo,
                                 nsIChannel **result)
{
    nsRefPtr<HttpBaseChannel> httpChannel;

    LOG(("nsHttpHandler::NewProxiedChannel [proxyInfo=%p]\n",
        givenProxyInfo));
    
    nsCOMPtr<nsProxyInfo> proxyInfo;
    if (givenProxyInfo) {
        proxyInfo = do_QueryInterface(givenProxyInfo);
        NS_ENSURE_ARG(proxyInfo);
    }

    PRBool https;
    nsresult rv = uri->SchemeIs("https", &https);
    if (NS_FAILED(rv))
        return rv;

#ifdef MOZ_IPC
    if (IsNeckoChild()) {
        httpChannel = new HttpChannelChild();
    } else
#endif
    {
        httpChannel = new nsHttpChannel();
    }

    // select proxy caps if using a non-transparent proxy.  SSL tunneling
    // should not use proxy settings.
    PRInt8 caps;
    if (proxyInfo && !nsCRT::strcmp(proxyInfo->Type(), "http") && !https)
        caps = mProxyCapabilities;
    else
        caps = mCapabilities;

    if (https) {
        // enable pipelining over SSL if requested
        if (mPipeliningOverSSL)
            caps |= NS_HTTP_ALLOW_PIPELINING;

#ifdef MOZ_IPC
        if (!IsNeckoChild()) 
#endif
        {
            // HACK: make sure PSM gets initialized on the main thread.
            nsCOMPtr<nsISocketProviderService> spserv =
                    do_GetService(NS_SOCKETPROVIDERSERVICE_CONTRACTID);
            if (spserv) {
                nsCOMPtr<nsISocketProvider> provider;
                spserv->GetSocketProvider("ssl", getter_AddRefs(provider));
            }
        }
    }

    rv = httpChannel->Init(uri, caps, proxyInfo);
    if (NS_FAILED(rv))
        return rv;

    httpChannel.forget(result);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIHttpProtocolHandler
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::GetUserAgent(nsACString &value)
{
    value = UserAgent();
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetAppName(nsACString &value)
{
    value = mLegacyAppName;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetAppVersion(nsACString &value)
{
    value = mLegacyAppVersion;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProduct(nsACString &value)
{
    value = mProduct;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetProductSub(nsACString &value)
{
    value = mProductSub;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetPlatform(nsACString &value)
{
    value = mPlatform;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetOscpu(nsACString &value)
{
    value = mOscpu;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetLanguage(nsACString &value)
{
    value = mLanguage;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpHandler::GetMisc(nsACString &value)
{
    value = mMisc;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpHandler::nsIObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsHttpHandler::Observe(nsISupports *subject,
                       const char *topic,
                       const PRUnichar *data)
{
    LOG(("nsHttpHandler::Observe [topic=\"%s\"]\n", topic));

    if (strcmp(topic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0) {
        nsCOMPtr<nsIPrefBranch> prefBranch = do_QueryInterface(subject);
        if (prefBranch)
            PrefsChanged(prefBranch, NS_ConvertUTF16toUTF8(data).get());
    }
    else if (strcmp(topic, "profile-change-net-teardown")    == 0 ||
             strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID)    == 0) {

        // clear cache of all authentication credentials.
        mAuthCache.ClearAll();

        // ensure connection manager is shutdown
        if (mConnMgr)
            mConnMgr->Shutdown();

        // need to reset the session start time since cache validation may
        // depend on this value.
        mSessionStartTime = NowInSeconds();
    }
    else if (strcmp(topic, "profile-change-net-restore") == 0) {
        // initialize connection manager
        InitConnectionMgr();
    }
    else if (strcmp(topic, "net:clear-active-logins") == 0) {
        mAuthCache.ClearAll();
    }
    else if (strcmp(topic, NS_PRIVATE_BROWSING_SWITCH_TOPIC) == 0) {
        if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_ENTER).Equals(data))
            mInPrivateBrowsingMode = PR_TRUE;
        else if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).Equals(data))
            mInPrivateBrowsingMode = PR_FALSE;
    }
    else if (strcmp(topic, "net:prune-dead-connections") == 0) {
        if (mConnMgr) {
            mConnMgr->PruneDeadConnections();
        }
    }
  
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsHttpsHandler implementation
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS4(nsHttpsHandler,
                              nsIHttpProtocolHandler,
                              nsIProxiedProtocolHandler,
                              nsIProtocolHandler,
                              nsISupportsWeakReference)

nsresult
nsHttpsHandler::Init()
{
    nsCOMPtr<nsIProtocolHandler> httpHandler(
            do_GetService(NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "http"));
    NS_ASSERTION(httpHandler.get() != nsnull, "no http handler?");
    return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetScheme(nsACString &aScheme)
{
    aScheme.AssignLiteral("https");
    return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetDefaultPort(PRInt32 *aPort)
{
    *aPort = NS_HTTPS_DEFAULT_PORT;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::GetProtocolFlags(PRUint32 *aProtocolFlags)
{
    *aProtocolFlags = NS_HTTP_PROTOCOL_FLAGS;
    return NS_OK;
}

NS_IMETHODIMP
nsHttpsHandler::NewURI(const nsACString &aSpec,
                       const char *aOriginCharset,
                       nsIURI *aBaseURI,
                       nsIURI **_retval)
{
    return ::NewURI(aSpec, aOriginCharset, aBaseURI, NS_HTTPS_DEFAULT_PORT, _retval);
}

NS_IMETHODIMP
nsHttpsHandler::NewChannel(nsIURI *aURI, nsIChannel **_retval)
{
    NS_ABORT_IF_FALSE(gHttpHandler, "Should have a HTTP handler by now.");
    if (!gHttpHandler)
      return NS_ERROR_UNEXPECTED;
    return gHttpHandler->NewChannel(aURI, _retval);
}

NS_IMETHODIMP
nsHttpsHandler::AllowPort(PRInt32 aPort, const char *aScheme, PRBool *_retval)
{
    // don't override anything.  
    *_retval = PR_FALSE;
    return NS_OK;
}
