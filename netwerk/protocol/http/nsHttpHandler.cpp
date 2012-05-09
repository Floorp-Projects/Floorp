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
#include "nsIPrefBranch.h"
#include "nsIPrefLocalizedString.h"
#include "nsISocketProviderService.h"
#include "nsISocketProvider.h"
#include "nsPrintfCString.h"
#include "nsCOMPtr.h"
#include "nsNetCID.h"
#include "prprf.h"
#include "nsReadableUtils.h"
#include "nsQuickSort.h"
#include "nsNetUtil.h"
#include "nsIOService.h"
#include "nsAsyncRedirectVerifyHelper.h"
#include "nsSocketTransportService2.h"
#include "nsAlgorithm.h"
#include "SpdySession.h"

#include "nsIXULAppInfo.h"

#include "mozilla/net/NeckoChild.h"

#if defined(XP_UNIX)
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
using namespace mozilla;
using namespace mozilla::net;
#include "mozilla/net/HttpChannelChild.h"

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
#define NETWORK_ENABLEIDN       "network.enableIDN"
#define BROWSER_PREF_PREFIX     "browser.cache."
#define DONOTTRACK_HEADER_ENABLED "privacy.donottrackheader.enabled"
#define TELEMETRY_ENABLED        "toolkit.telemetry.enabled"
#define ALLOW_EXPERIMENTS        "network.allow-experiments"

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
    , mFastFallbackToIPv4(false)
    , mIdleTimeout(PR_SecondsToInterval(10))
    , mSpdyTimeout(PR_SecondsToInterval(180))
    , mMaxRequestAttempts(10)
    , mMaxRequestDelay(10)
    , mIdleSynTimeout(250)
    , mMaxConnections(24)
    , mMaxConnectionsPerServer(8)
    , mMaxPersistentConnectionsPerServer(2)
    , mMaxPersistentConnectionsPerProxy(4)
    , mMaxPipelinedRequests(32)
    , mMaxOptimisticPipelinedRequests(4)
    , mPipelineAggressive(false)
    , mMaxPipelineObjectSize(300000)
    , mPipelineRescheduleOnTimeout(true)
    , mPipelineRescheduleTimeout(PR_MillisecondsToInterval(1500))
    , mPipelineReadTimeout(PR_MillisecondsToInterval(30000))
    , mRedirectionLimit(10)
    , mPhishyUserPassLength(1)
    , mQoSBits(0x00)
    , mPipeliningOverSSL(false)
    , mEnforceAssocReq(false)
    , mInPrivateBrowsingMode(PRIVATE_BROWSING_UNKNOWN)
    , mLastUniqueID(NowInSeconds())
    , mSessionStartTime(0)
    , mLegacyAppName("Mozilla")
    , mLegacyAppVersion("5.0")
    , mProduct("Gecko")
    , mUserAgentIsDirty(true)
    , mUseCache(true)
    , mPromptTempRedirect(true)
    , mSendSecureXSiteReferrer(true)
    , mEnablePersistentHttpsCaching(false)
    , mDoNotTrackEnabled(false)
    , mTelemetryEnabled(false)
    , mAllowExperiments(true)
    , mEnableSpdy(false)
    , mCoalesceSpdy(true)
    , mUseAlternateProtocol(false)
    , mSpdySendingChunkSize(SpdySession::kSendingChunkSize)
    , mSpdyPingThreshold(PR_SecondsToInterval(44))
    , mSpdyPingTimeout(PR_SecondsToInterval(8))
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

    if (IsNeckoChild())
        NeckoChild::InitNeckoChild();

    InitUserAgentComponents();

    // monitor some preference changes
    nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
    if (prefBranch) {
        prefBranch->AddObserver(HTTP_PREF_PREFIX, this, true);
        prefBranch->AddObserver(UA_PREF_PREFIX, this, true);
        prefBranch->AddObserver(INTL_ACCEPT_LANGUAGES, this, true); 
        prefBranch->AddObserver(NETWORK_ENABLEIDN, this, true);
        prefBranch->AddObserver(BROWSER_PREF("disk_cache_ssl"), this, true);
        prefBranch->AddObserver(DONOTTRACK_HEADER_ENABLED, this, true);
        prefBranch->AddObserver(TELEMETRY_ENABLED, this, true);

        PrefsChanged(prefBranch, nsnull);
    }

    mMisc.AssignLiteral("rv:" MOZILLA_UAVERSION);

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

    mProductSub.AssignLiteral(MOZILLA_UAVERSION);

    // Startup the http category
    // Bring alive the objects in the http-protocol-startup category
    NS_CreateServicesFromCategory(NS_HTTP_STARTUP_CATEGORY,
                                  static_cast<nsISupports*>(static_cast<void*>(this)),
                                  NS_HTTP_STARTUP_TOPIC);    
    
    mObserverService = mozilla::services::GetObserverService();
    if (mObserverService) {
        mObserverService->AddObserver(this, "profile-change-net-teardown", true);
        mObserverService->AddObserver(this, "profile-change-net-restore", true);
        mObserverService->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);
        mObserverService->AddObserver(this, "net:clear-active-logins", true);
        mObserverService->AddObserver(this, NS_PRIVATE_BROWSING_SWITCH_TOPIC, true);
        mObserverService->AddObserver(this, "net:prune-dead-connections", true);
        mObserverService->AddObserver(this, "net:failed-to-process-uri-content", true);
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
                        mMaxPipelinedRequests,
                        mMaxOptimisticPipelinedRequests);
    return rv;
}

nsresult
nsHttpHandler::AddStandardRequestHeaders(nsHttpHeaderArray *request,
                                         PRUint8 caps,
                                         bool useProxy)
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

    // RFC2616 section 19.6.2 states that the "Connection: keep-alive"
    // and "Keep-alive" request headers should not be sent by HTTP/1.1
    // user-agents.  Otherwise, problems with proxy servers (especially
    // transparent proxies) can result.
    //
    // However, we need to send something so that we can use keepalive
    // with HTTP/1.0 servers/proxies. We use "Proxy-Connection:" when 
    // we're talking to an http proxy, and "Connection:" otherwise.
    // We no longer send the Keep-Alive request header.
    
    NS_NAMED_LITERAL_CSTRING(close, "close");
    NS_NAMED_LITERAL_CSTRING(keepAlive, "keep-alive");

    const nsACString *connectionType = &close;
    if (caps & NS_HTTP_ALLOW_KEEPALIVE) {
        connectionType = &keepAlive;
    } else if (useProxy) {
        // Bug 92006
        request->SetHeader(nsHttp::Connection, close);
    }

    // Add the "Do-Not-Track" header
    if (mDoNotTrackEnabled) {
      rv = request->SetHeader(nsHttp::DoNotTrack,
                              NS_LITERAL_CSTRING("1"));
      if (NS_FAILED(rv)) return rv;
    }

    const nsHttpAtom &header = useProxy ? nsHttp::Proxy_Connection
                                        : nsHttp::Connection;
    return request->SetHeader(header, *connectionType);
}

bool
nsHttpHandler::IsAcceptableEncoding(const char *enc)
{
    if (!enc)
        return false;

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

    rv = cacheSession->SetDoomEntriesIfExpired(false);
    if (NS_FAILED(rv)) return rv;

    NS_ADDREF(*result = cacheSession);

    return NS_OK;
}

bool
nsHttpHandler::InPrivateBrowsingMode()
{
    if (PRIVATE_BROWSING_UNKNOWN == mInPrivateBrowsingMode) {
        // figure out if we're starting in private browsing mode
        nsCOMPtr<nsIPrivateBrowsingService> pbs =
            do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
        if (!pbs)
            return PRIVATE_BROWSING_OFF;

        bool p = false;
        pbs->GetPrivateBrowsingEnabled(&p);
        mInPrivateBrowsingMode = p ? PRIVATE_BROWSING_ON : PRIVATE_BROWSING_OFF;
    }
    return PRIVATE_BROWSING_ON == mInPrivateBrowsingMode;
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

PRUint32
nsHttpHandler::Get32BitsOfPseudoRandom()
{
    // only confirm rand seeding on socket thread
    NS_ABORT_IF_FALSE(PR_GetCurrentThread() == gSocketThread, "wrong thread");

    // rand() provides different amounts of PRNG on different platforms.
    // 15 or 31 bits are common amounts.

    PR_STATIC_ASSERT(RAND_MAX >= 0xfff);
    
#if RAND_MAX < 0xffffU
    return ((PRUint16) rand() << 20) |
            (((PRUint16) rand() & 0xfff) << 8) |
            ((PRUint16) rand() & 0xff);
#elif RAND_MAX < 0xffffffffU
    return ((PRUint16) rand() << 16) | ((PRUint16) rand() & 0xffff);
#else
    return (PRUint32) rand();
#endif
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
    return NS_GenerateHostPort(host, port, hostLine);
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
        mUserAgentIsDirty = false;
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
                           mCompatDevice.Length() +
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
#ifdef ANDROID
    if (!mCompatDevice.IsEmpty()) {
        mUserAgent += mCompatDevice;
        mUserAgent.AppendLiteral("; ");
    }
#else
    mUserAgent += mOscpu;
    mUserAgent.AppendLiteral("; ");
#endif
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
#elif defined(MOZ_PLATFORM_MAEMO)
    "Maemo"
#elif defined(MOZ_X11)
    "X11"
#else
    "?"
#endif
    );

#if defined(ANDROID)
    nsCOMPtr<nsIPropertyBag2> infoService = do_GetService("@mozilla.org/system-info;1");
    NS_ASSERTION(infoService, "Could not find a system info service");

    bool isTablet = false;
    infoService->GetPropertyAsBool(NS_LITERAL_STRING("tablet"), &isTablet);
    if (isTablet)
        mCompatDevice.AssignLiteral("Tablet");
    else
        mCompatDevice.AssignLiteral("Mobile");
#endif

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

#elif defined(XP_WIN)
    OSVERSIONINFO info = { sizeof(OSVERSIONINFO) };
    if (GetVersionEx(&info)) {
        const char *format;
#if defined _M_IA64
        format = WNT_BASE W64_PREFIX "; IA64";
#elif defined _M_X64 || defined _M_AMD64
        format = WNT_BASE W64_PREFIX "; x64";
#else
        BOOL isWow64 = FALSE;
        if (!IsWow64Process(GetCurrentProcess(), &isWow64)) {
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
#elif defined (XP_UNIX)
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

    mUserAgentIsDirty = true;
}

PRUint32
nsHttpHandler::MaxSocketCount()
{
    PR_CallOnce(&nsSocketTransportService::gMaxCountInitOnce,
                nsSocketTransportService::DiscoverMaxCount);
    // Don't use the full max count because sockets can be held in
    // the persistent connection pool for a long time and that could
    // starve other users.

    PRUint32 maxCount = nsSocketTransportService::gMaxCount;
    if (maxCount <= 8)
        maxCount = 1;
    else
        maxCount -= 8;

    return maxCount;
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

    bool cVar = false;

    if (PREF_CHANGED(UA_PREF("compatMode.firefox"))) {
        rv = prefs->GetBoolPref(UA_PREF("compatMode.firefox"), &cVar);
        if (NS_SUCCEEDED(rv) && cVar) {
            mCompatFirefox.AssignLiteral("Firefox/" MOZ_UA_FIREFOX_VERSION);
        } else {
            mCompatFirefox.Truncate();
        }
        mUserAgentIsDirty = true;
    }

    // general.useragent.override
    if (PREF_CHANGED(UA_PREF("override"))) {
        prefs->GetCharPref(UA_PREF("override"),
                            getter_Copies(mUserAgentOverride));
        mUserAgentIsDirty = true;
    }

    //
    // HTTP options
    //

    if (PREF_CHANGED(HTTP_PREF("keep-alive.timeout"))) {
        rv = prefs->GetIntPref(HTTP_PREF("keep-alive.timeout"), &val);
        if (NS_SUCCEEDED(rv))
            mIdleTimeout = PR_SecondsToInterval(clamped(val, 1, 0xffff));
    }

    if (PREF_CHANGED(HTTP_PREF("request.max-attempts"))) {
        rv = prefs->GetIntPref(HTTP_PREF("request.max-attempts"), &val);
        if (NS_SUCCEEDED(rv))
            mMaxRequestAttempts = (PRUint16) clamped(val, 1, 0xffff);
    }

    if (PREF_CHANGED(HTTP_PREF("request.max-start-delay"))) {
        rv = prefs->GetIntPref(HTTP_PREF("request.max-start-delay"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxRequestDelay = (PRUint16) clamped(val, 0, 0xffff);
            if (mConnMgr)
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_REQUEST_DELAY,
                                      mMaxRequestDelay);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("max-connections"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-connections"), &val);
        if (NS_SUCCEEDED(rv)) {

            mMaxConnections = (PRUint16) clamped((PRUint32)val,
                                                 (PRUint32)1, MaxSocketCount());

            if (mConnMgr)
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_CONNECTIONS,
                                      mMaxConnections);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("max-connections-per-server"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-connections-per-server"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxConnectionsPerServer = (PRUint8) clamped(val, 1, 0xff);
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
            mMaxPersistentConnectionsPerServer = (PRUint8) clamped(val, 1, 0xff);
            if (mConnMgr)
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_PERSISTENT_CONNECTIONS_PER_HOST,
                                      mMaxPersistentConnectionsPerServer);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("max-persistent-connections-per-proxy"))) {
        rv = prefs->GetIntPref(HTTP_PREF("max-persistent-connections-per-proxy"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxPersistentConnectionsPerProxy = (PRUint8) clamped(val, 1, 0xff);
            if (mConnMgr)
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_PERSISTENT_CONNECTIONS_PER_PROXY,
                                      mMaxPersistentConnectionsPerProxy);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("sendRefererHeader"))) {
        rv = prefs->GetIntPref(HTTP_PREF("sendRefererHeader"), &val);
        if (NS_SUCCEEDED(rv))
            mReferrerLevel = (PRUint8) clamped(val, 0, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("redirection-limit"))) {
        rv = prefs->GetIntPref(HTTP_PREF("redirection-limit"), &val);
        if (NS_SUCCEEDED(rv))
            mRedirectionLimit = (PRUint8) clamped(val, 0, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("connection-retry-timeout"))) {
        rv = prefs->GetIntPref(HTTP_PREF("connection-retry-timeout"), &val);
        if (NS_SUCCEEDED(rv))
            mIdleSynTimeout = (PRUint16) clamped(val, 0, 3000);
    }

    if (PREF_CHANGED(HTTP_PREF("fast-fallback-to-IPv4"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("fast-fallback-to-IPv4"), &cVar);
        if (NS_SUCCEEDED(rv))
            mFastFallbackToIPv4 = cVar;
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
            mMaxPipelinedRequests = clamped(val, 1, 0xffff);
            if (mConnMgr)
                mConnMgr->UpdateParam(nsHttpConnectionMgr::MAX_PIPELINED_REQUESTS,
                                      mMaxPipelinedRequests);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("pipelining.max-optimistic-requests"))) {
        rv = prefs->
            GetIntPref(HTTP_PREF("pipelining.max-optimistic-requests"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxOptimisticPipelinedRequests = clamped(val, 1, 0xffff);
            if (mConnMgr)
                mConnMgr->UpdateParam
                    (nsHttpConnectionMgr::MAX_OPTIMISTIC_PIPELINED_REQUESTS,
                     mMaxOptimisticPipelinedRequests);
        }
    }

    if (PREF_CHANGED(HTTP_PREF("pipelining.aggressive"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("pipelining.aggressive"), &cVar);
        if (NS_SUCCEEDED(rv))
            mPipelineAggressive = cVar;
    }

    if (PREF_CHANGED(HTTP_PREF("pipelining.maxsize"))) {
        rv = prefs->GetIntPref(HTTP_PREF("pipelining.maxsize"), &val);
        if (NS_SUCCEEDED(rv)) {
            mMaxPipelineObjectSize =
                static_cast<PRInt64>(clamped(val, 1000, 100000000));
        }
    }

    // Determines whether or not to actually reschedule after the
    // reschedule-timeout has expired
    if (PREF_CHANGED(HTTP_PREF("pipelining.reschedule-on-timeout"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("pipelining.reschedule-on-timeout"),
                                &cVar);
        if (NS_SUCCEEDED(rv))
            mPipelineRescheduleOnTimeout = cVar;
    }

    // The amount of time head of line blocking is allowed (in ms)
    // before the blocked transactions are moved to another pipeline
    if (PREF_CHANGED(HTTP_PREF("pipelining.reschedule-timeout"))) {
        rv = prefs->GetIntPref(HTTP_PREF("pipelining.reschedule-timeout"),
                               &val);
        if (NS_SUCCEEDED(rv)) {
            mPipelineRescheduleTimeout =
                PR_MillisecondsToInterval((PRUint16) clamped(val, 500, 0xffff));
        }
    }

    // The amount of time a pipelined transaction is allowed to wait before
    // being canceled and retried in a non-pipeline connection
    if (PREF_CHANGED(HTTP_PREF("pipelining.read-timeout"))) {
        rv = prefs->GetIntPref(HTTP_PREF("pipelining.read-timeout"), &val);
        if (NS_SUCCEEDED(rv)) {
            mPipelineReadTimeout =
                PR_MillisecondsToInterval((PRUint16) clamped(val, 5000,
                                                             0xffff));
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
            mQoSBits = (PRUint8) clamped(val, 0, 0xff);
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

    if (PREF_CHANGED(HTTP_PREF("assoc-req.enforce"))) {
        cVar = false;
        rv = prefs->GetBoolPref(HTTP_PREF("assoc-req.enforce"), &cVar);
        if (NS_SUCCEEDED(rv))
            mEnforceAssocReq = cVar;
    }

    // enable Persistent caching for HTTPS - bug#205921    
    if (PREF_CHANGED(BROWSER_PREF("disk_cache_ssl"))) {
        cVar = false;
        rv = prefs->GetBoolPref(BROWSER_PREF("disk_cache_ssl"), &cVar);
        if (NS_SUCCEEDED(rv))
            mEnablePersistentHttpsCaching = cVar;
    }

    if (PREF_CHANGED(HTTP_PREF("phishy-userpass-length"))) {
        rv = prefs->GetIntPref(HTTP_PREF("phishy-userpass-length"), &val);
        if (NS_SUCCEEDED(rv))
            mPhishyUserPassLength = (PRUint8) clamped(val, 0, 0xff);
    }

    if (PREF_CHANGED(HTTP_PREF("spdy.enabled"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("spdy.enabled"), &cVar);
        if (NS_SUCCEEDED(rv))
            mEnableSpdy = cVar;
    }

    if (PREF_CHANGED(HTTP_PREF("spdy.coalesce-hostnames"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("spdy.coalesce-hostnames"), &cVar);
        if (NS_SUCCEEDED(rv))
            mCoalesceSpdy = cVar;
    }

    if (PREF_CHANGED(HTTP_PREF("spdy.use-alternate-protocol"))) {
        rv = prefs->GetBoolPref(HTTP_PREF("spdy.use-alternate-protocol"),
                                &cVar);
        if (NS_SUCCEEDED(rv))
            mUseAlternateProtocol = cVar;
    }

    if (PREF_CHANGED(HTTP_PREF("spdy.timeout"))) {
        rv = prefs->GetIntPref(HTTP_PREF("spdy.timeout"), &val);
        if (NS_SUCCEEDED(rv))
            mSpdyTimeout = PR_SecondsToInterval(clamped(val, 1, 0xffff));
    }

    if (PREF_CHANGED(HTTP_PREF("spdy.chunk-size"))) {
        rv = prefs->GetIntPref(HTTP_PREF("spdy.chunk-size"), &val);
        if (NS_SUCCEEDED(rv))
            mSpdySendingChunkSize = (PRUint32) clamped(val, 1, 0x7fffffff);
    }

    // The amount of idle seconds on a spdy connection before initiating a
    // server ping. 0 will disable.
    if (PREF_CHANGED(HTTP_PREF("spdy.ping-threshold"))) {
        rv = prefs->GetIntPref(HTTP_PREF("spdy.ping-threshold"), &val);
        if (NS_SUCCEEDED(rv))
            mSpdyPingThreshold =
                PR_SecondsToInterval((PRUint16) clamped(val, 0, 0x7fffffff));
    }

    // The amount of seconds to wait for a spdy ping response before
    // closing the session.
    if (PREF_CHANGED(HTTP_PREF("spdy.ping-timeout"))) {
        rv = prefs->GetIntPref(HTTP_PREF("spdy.ping-timeout"), &val);
        if (NS_SUCCEEDED(rv))
            mSpdyPingTimeout =
                PR_SecondsToInterval((PRUint16) clamped(val, 0, 0x7fffffff));
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

    //
    // IDN options
    //

    if (PREF_CHANGED(NETWORK_ENABLEIDN)) {
        bool enableIDN = false;
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

    //
    // Tracking options
    //

    if (PREF_CHANGED(DONOTTRACK_HEADER_ENABLED)) {
        cVar = false;
        rv = prefs->GetBoolPref(DONOTTRACK_HEADER_ENABLED, &cVar);
        if (NS_SUCCEEDED(rv)) {
            mDoNotTrackEnabled = cVar;
        }
    }

    //
    // Telemetry
    //

    if (PREF_CHANGED(TELEMETRY_ENABLED)) {
        cVar = false;
        rv = prefs->GetBoolPref(TELEMETRY_ENABLED, &cVar);
        if (NS_SUCCEEDED(rv)) {
            mTelemetryEnabled = cVar;
        }
    }

    //
    // network.allow-experiments
    //

    if (PREF_CHANGED(ALLOW_EXPERIMENTS)) {
        cVar = true;
        rv = prefs->GetBoolPref(ALLOW_EXPERIMENTS, &cVar);
        if (NS_SUCCEEDED(rv)) {
            mAllowExperiments = cVar;
        }
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

NS_IMPL_THREADSAFE_ISUPPORTS6(nsHttpHandler,
                              nsIHttpProtocolHandler,
                              nsIProxiedProtocolHandler,
                              nsIProtocolHandler,
                              nsIObserver,
                              nsISupportsWeakReference,
                              nsISpeculativeConnect)

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

    bool isHttp = false, isHttps = false;

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
nsHttpHandler::AllowPort(PRInt32 port, const char *scheme, bool *_retval)
{
    // don't override anything.  
    *_retval = false;
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

    bool https;
    nsresult rv = uri->SchemeIs("https", &https);
    if (NS_FAILED(rv))
        return rv;

    if (IsNeckoChild()) {
        httpChannel = new HttpChannelChild();
    } else {
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

        if (!IsNeckoChild()) {
            // HACK: make sure PSM gets initialized on the main thread.
            net_EnsurePSMInit();
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
            mInPrivateBrowsingMode = PRIVATE_BROWSING_ON;
        else if (NS_LITERAL_STRING(NS_PRIVATE_BROWSING_LEAVE).Equals(data))
            mInPrivateBrowsingMode = PRIVATE_BROWSING_OFF;
        if (mConnMgr)
            mConnMgr->ClosePersistentConnections();
    }
    else if (strcmp(topic, "net:prune-dead-connections") == 0) {
        if (mConnMgr) {
            mConnMgr->PruneDeadConnections();
        }
    }
    else if (strcmp(topic, "net:failed-to-process-uri-content") == 0) {
        nsCOMPtr<nsIURI> uri = do_QueryInterface(subject);
        if (uri && mConnMgr)
            mConnMgr->ReportFailedToProcess(uri);
    }
  
    return NS_OK;
}

// nsISpeculativeConnect

NS_IMETHODIMP
nsHttpHandler::SpeculativeConnect(nsIURI *aURI,
                                  nsIInterfaceRequestor *aCallbacks,
                                  nsIEventTarget *aTarget)
{
    nsIStrictTransportSecurityService* stss = gHttpHandler->GetSTSService();
    bool isStsHost = false;
    if (!stss)
        return NS_OK;
    
    nsCOMPtr<nsIURI> clone;
    if (NS_SUCCEEDED(stss->IsStsURI(aURI, &isStsHost)) && isStsHost) {
        if (NS_SUCCEEDED(aURI->Clone(getter_AddRefs(clone)))) {
            clone->SetScheme(NS_LITERAL_CSTRING("https"));
            aURI = clone.get();
        }
    }

    nsCAutoString scheme;
    nsresult rv = aURI->GetScheme(scheme);
    if (NS_FAILED(rv))
        return rv;

    // If this is HTTPS, make sure PSM is initialized as the channel
    // creation path may have been bypassed
    if (scheme.EqualsLiteral("https")) {
        if (!IsNeckoChild()) {
            // make sure PSM gets initialized on the main thread.
            net_EnsurePSMInit();
        }
    }
    // Ensure that this is HTTP or HTTPS, otherwise we don't do preconnect here
    else if (!scheme.EqualsLiteral("http"))
        return NS_ERROR_UNEXPECTED;

    // Construct connection info object
    bool usingSSL = false;
    rv = aURI->SchemeIs("https", &usingSSL);
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString host;
    rv = aURI->GetAsciiHost(host);
    if (NS_FAILED(rv))
        return rv;

    PRInt32 port = -1;
    rv = aURI->GetPort(&port);
    if (NS_FAILED(rv))
        return rv;

    nsHttpConnectionInfo *ci =
        new nsHttpConnectionInfo(host, port, nsnull, usingSSL);

    return SpeculativeConnect(ci, aCallbacks, aTarget);
}

//-----------------------------------------------------------------------------
// nsHttpsHandler implementation
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS5(nsHttpsHandler,
                              nsIHttpProtocolHandler,
                              nsIProxiedProtocolHandler,
                              nsIProtocolHandler,
                              nsISupportsWeakReference,
                              nsISpeculativeConnect)

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
nsHttpsHandler::AllowPort(PRInt32 aPort, const char *aScheme, bool *_retval)
{
    // don't override anything.  
    *_retval = false;
    return NS_OK;
}
