/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EarlyHintPreloader.h"

#include "EarlyHintRegistrar.h"
#include "EarlyHintsService.h"
#include "ErrorList.h"
#include "HttpChannelParent.h"
#include "MainThreadUtils.h"
#include "NeckoCommon.h"
#include "mozilla/CORSMode.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ReferrerInfo.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/Logging.h"
#include "mozilla/net/EarlyHintRegistrar.h"
#include "mozilla/net/NeckoChannelParams.h"
#include "mozilla/StaticPrefs_network.h"
#include "nsAttrValue.h"
#include "nsAttrValueInlines.h"
#include "nsCOMPtr.h"
#include "nsContentSecurityManager.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsHttpChannel.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsICacheInfoChannel.h"
#include "nsIChannel.h"
#include "nsIHttpChannel.h"
#include "nsIInputStream.h"
#include "nsILoadInfo.h"
#include "nsIParentChannel.h"
#include "nsIReferrerInfo.h"
#include "nsIURI.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "nsStreamUtils.h"
#include "nsStringStream.h"
#include "ParentChannelListener.h"
#include "nsIChannel.h"

//
// To enable logging (see mozilla/Logging.h for full details):
//
//    set MOZ_LOG=EarlyHint:5
//    set MOZ_LOG_FILE=earlyhint.log
//
// this enables LogLevel::Debug level information and places all output in
// the file earlyhint.log
//
static mozilla::LazyLogModule gEarlyHintLog("EarlyHint");

#undef LOG
#define LOG(args) MOZ_LOG(gEarlyHintLog, mozilla::LogLevel::Debug, args)

#undef LOG_ENABLED
#define LOG_ENABLED() MOZ_LOG_TEST(gEarlyHintLog, mozilla::LogLevel::Debug)

namespace mozilla::net {

namespace {
// This id uniquely identifies each early hint preloader in the
// EarlyHintRegistrar. Must only be accessed from main thread.
static uint64_t gEarlyHintPreloaderId{0};
}  // namespace

//=============================================================================
// OngoingEarlyHints
//=============================================================================

void OngoingEarlyHints::CancelAllOngoingPreloads() {
  for (auto& preloader : mPreloaders) {
    preloader->CancelChannel(nsresult::NS_ERROR_ABORT);
  }
  mStartedPreloads.Clear();
}

bool OngoingEarlyHints::Contains(const PreloadHashKey& aKey) {
  return mStartedPreloads.Contains(aKey);
}

bool OngoingEarlyHints::Add(const PreloadHashKey& aKey,
                            RefPtr<EarlyHintPreloader> aPreloader) {
  if (!mStartedPreloads.Contains(aKey)) {
    mStartedPreloads.Insert(aKey);
    mPreloaders.AppendElement(aPreloader);
    return true;
  }
  return false;
}

void OngoingEarlyHints::RegisterLinksAndGetConnectArgs(
    nsTArray<EarlyHintConnectArgs>& aOutLinks) {
  // register all channels before returning
  for (auto& preload : mPreloaders) {
    aOutLinks.AppendElement(preload->Register());
  }
}

//=============================================================================
// EarlyHintPreloader
//=============================================================================

EarlyHintPreloader::EarlyHintPreloader() {
  AssertIsOnMainThread();
  mConnectArgs.earlyHintPreloaderId() = ++gEarlyHintPreloaderId;
};

/* static */
Maybe<PreloadHashKey> EarlyHintPreloader::GenerateHashKey(
    ASDestination aAs, nsIURI* aURI, nsIPrincipal* aPrincipal,
    CORSMode aCorsMode, const nsAString& aType) {
  if (aAs == ASDestination::DESTINATION_FONT) {
    return Some(PreloadHashKey::CreateAsFont(aURI, aCorsMode));
  }
  if (aAs == ASDestination::DESTINATION_IMAGE) {
    return Some(PreloadHashKey::CreateAsImage(aURI, aPrincipal, aCorsMode));
  }
  if (aAs == ASDestination::DESTINATION_SCRIPT &&
      !aType.LowerCaseEqualsASCII("module")) {
    return Some(PreloadHashKey::CreateAsScript(
        aURI, aCorsMode, JS::loader::ScriptKind::eClassic));
  }
  if (aAs == ASDestination::DESTINATION_STYLE) {
    return Some(PreloadHashKey::CreateAsStyle(
        aURI, aPrincipal, aCorsMode,
        css::SheetParsingMode::eAuthorSheetFeatures));
  }
  if (aAs == ASDestination::DESTINATION_FETCH) {
    return Some(PreloadHashKey::CreateAsFetch(aURI, aCorsMode));
  }
  return Nothing();
}

/* static */
nsSecurityFlags EarlyHintPreloader::ComputeSecurityFlags(CORSMode aCORSMode,
                                                         ASDestination aAs,
                                                         bool aIsModule) {
  if (aAs == ASDestination::DESTINATION_FONT) {
    return nsContentSecurityManager::ComputeSecurityFlags(
        CORSMode::CORS_NONE,
        nsContentSecurityManager::CORSSecurityMapping::REQUIRE_CORS_CHECKS);
  }
  if (aAs == ASDestination::DESTINATION_IMAGE) {
    return nsContentSecurityManager::ComputeSecurityFlags(
               aCORSMode, nsContentSecurityManager::CORSSecurityMapping::
                              CORS_NONE_MAPS_TO_INHERITED_CONTEXT) |
           nsILoadInfo::SEC_ALLOW_CHROME;
  }
  if (aAs == ASDestination::DESTINATION_SCRIPT) {
    if (aIsModule) {
      return nsContentSecurityManager::ComputeSecurityFlags(
                 aCORSMode, nsContentSecurityManager::CORSSecurityMapping::
                                REQUIRE_CORS_CHECKS) |
             nsILoadInfo::SEC_ALLOW_CHROME;
    }
    return nsContentSecurityManager::ComputeSecurityFlags(
               aCORSMode, nsContentSecurityManager::CORSSecurityMapping::
                              CORS_NONE_MAPS_TO_DISABLED_CORS_CHECKS) |
           nsILoadInfo::SEC_ALLOW_CHROME;
  }
  if (aAs == ASDestination::DESTINATION_STYLE) {
    return nsContentSecurityManager::ComputeSecurityFlags(
               aCORSMode, nsContentSecurityManager::CORSSecurityMapping::
                              CORS_NONE_MAPS_TO_INHERITED_CONTEXT) |
           nsILoadInfo::SEC_ALLOW_CHROME;
    ;
  }
  if (aAs == ASDestination::DESTINATION_FETCH) {
    return nsContentSecurityManager::ComputeSecurityFlags(
        aCORSMode, nsContentSecurityManager::CORSSecurityMapping::
                       CORS_NONE_MAPS_TO_DISABLED_CORS_CHECKS);
  }
  MOZ_ASSERT(false, "Unexpected ASDestination");
  return nsContentSecurityManager::ComputeSecurityFlags(
      CORSMode::CORS_NONE,
      nsContentSecurityManager::CORSSecurityMapping::REQUIRE_CORS_CHECKS);
}

// static
void EarlyHintPreloader::MaybeCreateAndInsertPreload(
    OngoingEarlyHints* aOngoingEarlyHints, const LinkHeader& aHeader,
    nsIURI* aBaseURI, nsIPrincipal* aPrincipal,
    nsICookieJarSettings* aCookieJarSettings) {
  if (!aHeader.mRel.LowerCaseEqualsASCII("preload")) {
    return;
  }

  nsAttrValue as;
  ParseAsValue(aHeader.mAs, as);

  ASDestination destination = static_cast<ASDestination>(as.GetEnumValue());
  CollectResourcesTypeTelemetry(destination);

  if (!StaticPrefs::network_early_hints_enabled()) {
    return;
  }

  if (as.GetEnumValue() == ASDestination::DESTINATION_INVALID) {
    // return early when it's definitly not an asset type we preload
    // would be caught later as well, e.g. when creating the PreloadHashKey
    return;
  }

  nsCOMPtr<nsIURI> uri;
  NS_ENSURE_SUCCESS_VOID(
      NS_NewURI(getter_AddRefs(uri), aHeader.mHref, nullptr, aBaseURI));
  // The link relation may apply to a different resource, specified
  // in the anchor parameter. For the link relations supported so far,
  // we simply abort if the link applies to a resource different to the
  // one we've loaded
  if (!nsContentUtils::LinkContextIsURI(aHeader.mAnchor, uri)) {
    return;
  }

  // only preload secure context urls
  if (!uri->SchemeIs("https")) {
    return;
  }

  CORSMode corsMode = dom::Element::StringToCORSMode(aHeader.mCrossOrigin);

  Maybe<PreloadHashKey> hashKey =
      GenerateHashKey(static_cast<ASDestination>(as.GetEnumValue()), uri,
                      aPrincipal, corsMode, aHeader.mType);
  if (!hashKey) {
    return;
  }

  if (aOngoingEarlyHints->Contains(*hashKey)) {
    return;
  }

  nsContentPolicyType contentPolicyType = AsValueToContentPolicy(as);
  if (contentPolicyType == nsContentPolicyType::TYPE_INVALID) {
    return;
  }

  dom::ReferrerPolicy referrerPolicy =
      dom::ReferrerInfo::ReferrerPolicyAttributeFromString(
          aHeader.mReferrerPolicy);

  nsCOMPtr<nsIReferrerInfo> referrerInfo =
      new dom::ReferrerInfo(aBaseURI, referrerPolicy);

  RefPtr<EarlyHintPreloader> earlyHintPreloader = new EarlyHintPreloader();

  nsSecurityFlags securityFlags = EarlyHintPreloader::ComputeSecurityFlags(
      corsMode, static_cast<ASDestination>(as.GetEnumValue()),
      aHeader.mType.LowerCaseEqualsASCII("module"));

  NS_ENSURE_SUCCESS_VOID(earlyHintPreloader->OpenChannel(
      uri, aPrincipal, securityFlags, contentPolicyType, referrerInfo,
      aCookieJarSettings));

  earlyHintPreloader->SetLinkHeader(aHeader);

  DebugOnly<bool> result =
      aOngoingEarlyHints->Add(*hashKey, earlyHintPreloader);
  MOZ_ASSERT(result);
}

nsresult EarlyHintPreloader::OpenChannel(
    nsIURI* aURI, nsIPrincipal* aPrincipal, nsSecurityFlags aSecurityFlags,
    nsContentPolicyType aContentPolicyType, nsIReferrerInfo* aReferrerInfo,
    nsICookieJarSettings* aCookieJarSettings) {
  MOZ_ASSERT(aContentPolicyType == nsContentPolicyType::TYPE_IMAGE ||
             aContentPolicyType ==
                 nsContentPolicyType::TYPE_INTERNAL_FETCH_PRELOAD ||
             aContentPolicyType == nsContentPolicyType::TYPE_SCRIPT ||
             aContentPolicyType == nsContentPolicyType::TYPE_STYLESHEET ||
             aContentPolicyType == nsContentPolicyType::TYPE_FONT);
  nsresult rv =
      NS_NewChannel(getter_AddRefs(mChannel), aURI, aPrincipal, aSecurityFlags,
                    aContentPolicyType, aCookieJarSettings,
                    /* aPerformanceStorage */ nullptr,
                    /* aLoadGroup */ nullptr,
                    /* aCallbacks */ this, nsIRequest::LOAD_NORMAL);

  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsHttpChannel> httpChannelObject = do_QueryObject(mChannel);
  if (!httpChannelObject) {
    mChannel = nullptr;
    return NS_ERROR_ABORT;
  }

  // configure HTTP specific stuff
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(mChannel);
  if (!httpChannel) {
    mChannel = nullptr;
    return NS_ERROR_ABORT;
  }
  DebugOnly<nsresult> success = httpChannel->SetReferrerInfo(aReferrerInfo);
  MOZ_ASSERT(NS_SUCCEEDED(success));
  success = httpChannel->SetRequestHeader("X-Moz"_ns, "early hint"_ns, false);
  MOZ_ASSERT(NS_SUCCEEDED(success));

  mParentListener = new ParentChannelListener(this, nullptr, false);

  rv = mChannel->AsyncOpen(mParentListener);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

void EarlyHintPreloader::SetLinkHeader(const LinkHeader& aLinkHeader) {
  mConnectArgs.link() = aLinkHeader;
}

EarlyHintConnectArgs EarlyHintPreloader::Register() {
  // Create an entry in the redirect channel registrar to
  // allocate an identifier for this load.
  RefPtr<EarlyHintRegistrar> registrar = EarlyHintRegistrar::GetOrCreate();
  registrar->RegisterEarlyHint(mConnectArgs.earlyHintPreloaderId(), this);
  return mConnectArgs;
}

nsresult EarlyHintPreloader::CancelChannel(nsresult aStatus) {
  // clear redirect channel in case this channel is cleared between the call of
  // EarlyHintPreloader::AsyncOnChannelRedirect and
  // EarlyHintPreloader::OnRedirectResult
  mRedirectChannel = nullptr;
  if (mChannel) {
    mChannel->Cancel(aStatus);
    mChannel = nullptr;
  }
  return NS_OK;
}

void EarlyHintPreloader::OnParentReady(nsIParentChannel* aParent,
                                       uint64_t aChannelId) {
  AssertIsOnMainThread();
  MOZ_ASSERT(aParent);
  LOG(("EarlyHintPreloader::OnParentReady [this=%p]\n", this));

  mParent = aParent;
  mChannelId = aChannelId;

  RefPtr<EarlyHintRegistrar> registrar = EarlyHintRegistrar::GetOrCreate();
  registrar->DeleteEntry(mConnectArgs.earlyHintPreloaderId());

  if (mSuspended) {
    SetParentChannel();
    InvokeStreamListenerFunctions();
  }
}

void EarlyHintPreloader::SetParentChannel() {
  RefPtr<HttpBaseChannel> channel = do_QueryObject(mChannel);
  RefPtr<HttpChannelParent> parent = do_QueryObject(mParent);
  parent->SetHttpChannelFromEarlyHintPreloader(channel);
}

// Adapted from
// https://searchfox.org/mozilla-central/rev/b4150d1c6fae0c51c522df2d2c939cf5ad331d4c/netwerk/ipc/DocumentLoadListener.cpp#1311
bool EarlyHintPreloader::InvokeStreamListenerFunctions() {
  AssertIsOnMainThread();

  LOG(("EarlyHintPreloader::RedirectToParent [this=%p parent=%p]\n", this,
       mParent.get()));

  if (nsCOMPtr<nsIIdentChannel> channel = do_QueryInterface(mChannel)) {
    MOZ_ASSERT(mChannelId);
    DebugOnly<nsresult> rv = channel->SetChannelId(mChannelId);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
  }

  // If we failed to suspend the channel, then we might have received
  // some messages while the redirected was being handled.
  // Manually send them on now.
  if (!mIsFinished) {
    // This is safe to do, because OnStartRequest/OnStopRequest/OnDataAvailable
    // are all called on the main thread. They can't be called until we worked
    // through all functions in the streamListnerFunctions array.
    mParentListener->SetListenerAfterRedirect(mParent);
  }
  nsTArray<StreamListenerFunction> streamListenerFunctions =
      std::move(mStreamListenerFunctions);

  ForwardStreamListenerFunctions(streamListenerFunctions, mParent);

  // We don't expect to get new stream listener functions added
  // via re-entrancy. If this ever happens, we should understand
  // exactly why before allowing it.
  NS_ASSERTION(mStreamListenerFunctions.IsEmpty(),
               "Should not have added new stream listener function!");

  if (mChannel && mSuspended) {
    mChannel->Resume();
  }
  mChannel = nullptr;
  mParent = nullptr;
  mParentListener = nullptr;
  return true;
}

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(EarlyHintPreloader, nsIRequestObserver, nsIStreamListener,
                  nsIChannelEventSink, nsIInterfaceRequestor,
                  nsIRedirectResultListener, nsIMultiPartChannelListener);

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsIStreamListener
//-----------------------------------------------------------------------------

// Implementation copied and adapted from DocumentLoadListener::OnStartRequest
// https://searchfox.org/mozilla-central/rev/380fc5571b039fd453b45bbb64ed13146fe9b066/netwerk/ipc/DocumentLoadListener.cpp#2317-2508
NS_IMETHODIMP
EarlyHintPreloader::OnStartRequest(nsIRequest* aRequest) {
  LOG(("EarlyHintPreloader::OnStartRequest [this=%p]\n", this));
  AssertIsOnMainThread();

  nsCOMPtr<nsIMultiPartChannel> multiPartChannel = do_QueryInterface(aRequest);
  if (multiPartChannel) {
    multiPartChannel->GetBaseChannel(getter_AddRefs(mChannel));
  } else {
    mChannel = do_QueryInterface(aRequest);
  }
  MOZ_DIAGNOSTIC_ASSERT(mChannel);

  nsCOMPtr<nsICacheInfoChannel> cacheInfoChannel = do_QueryInterface(aRequest);
  if (!cacheInfoChannel) {
    return NS_ERROR_ABORT;
  }

  if (mParent) {
    SetParentChannel();
    mParent->OnStartRequest(aRequest);
    InvokeStreamListenerFunctions();
  } else {
    mStreamListenerFunctions.AppendElement(
        AsVariant(OnStartRequestParams{aRequest}));
    mChannel->Suspend();
    mSuspended = true;
  }

  return NS_OK;
}

// Implementation copied from DocumentLoadListener::OnStopRequest
// https://searchfox.org/mozilla-central/rev/380fc5571b039fd453b45bbb64ed13146fe9b066/netwerk/ipc/DocumentLoadListener.cpp#2510-2528
NS_IMETHODIMP
EarlyHintPreloader::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  AssertIsOnMainThread();
  LOG(("EarlyHintPreloader::OnStopRequest [this=%p]\n", this));
  mStreamListenerFunctions.AppendElement(
      AsVariant(OnStopRequestParams{aRequest, aStatusCode}));

  // If we're not a multi-part channel, then we're finished and we don't
  // expect any further events. If we are, then this might be called again,
  // so wait for OnAfterLastPart instead.
  nsCOMPtr<nsIMultiPartChannel> multiPartChannel = do_QueryInterface(aRequest);
  if (!multiPartChannel) {
    mIsFinished = true;
  }

  return NS_OK;
}

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsIStreamListener
//-----------------------------------------------------------------------------

// Implementation copied from DocumentLoadListener::OnDataAvailable
// https://searchfox.org/mozilla-central/rev/380fc5571b039fd453b45bbb64ed13146fe9b066/netwerk/ipc/DocumentLoadListener.cpp#2530-2549
NS_IMETHODIMP
EarlyHintPreloader::OnDataAvailable(nsIRequest* aRequest,
                                    nsIInputStream* aInputStream,
                                    uint64_t aOffset, uint32_t aCount) {
  AssertIsOnMainThread();
  LOG(("EarlyHintPreloader::OnDataAvailable [this=%p]\n", this));
  // This isn't supposed to happen, since we suspended the channel, but
  // sometimes Suspend just doesn't work. This can happen when we're routing
  // through nsUnknownDecoder to sniff the content type, and it doesn't handle
  // being suspended. Let's just store the data and manually forward it to our
  // redirected channel when it's ready.
  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(aInputStream, data, aCount);
  NS_ENSURE_SUCCESS(rv, rv);

  mStreamListenerFunctions.AppendElement(
      AsVariant(OnDataAvailableParams{aRequest, data, aOffset, aCount}));

  return NS_OK;
}

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsIMultiPartChannelListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EarlyHintPreloader::OnAfterLastPart(nsresult aStatus) {
  LOG(("EarlyHintPreloader::OnAfterLastPart [this=%p]", this));
  mStreamListenerFunctions.AppendElement(
      AsVariant(OnAfterLastPartParams{aStatus}));
  mIsFinished = true;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsIChannelEventSink
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EarlyHintPreloader::AsyncOnChannelRedirect(
    nsIChannel* aOldChannel, nsIChannel* aNewChannel, uint32_t aFlags,
    nsIAsyncVerifyRedirectCallback* callback) {
  LOG(("EarlyHintPreloader::AsyncOnChannelRedirect [this=%p]", this));
  nsCOMPtr<nsIURI> newURI;
  nsresult rv = NS_GetFinalChannelURI(aNewChannel, getter_AddRefs(newURI));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = aNewChannel->GetURI(getter_AddRefs(newURI));
  if (NS_FAILED(rv)) {
    callback->OnRedirectVerifyCallback(rv);
    return NS_OK;
  }

  // HTTP request headers are not automatically forwarded to the new channel.
  nsCOMPtr<nsIHttpChannel> httpChannel = do_QueryInterface(aNewChannel);
  NS_ENSURE_STATE(httpChannel);

  rv = httpChannel->SetRequestHeader("X-Moz"_ns, "early hint"_ns, false);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  // Assign to mChannel after we get notification about success of the
  // redirect in OnRedirectResult.
  mRedirectChannel = aNewChannel;

  callback->OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsIRedirectResultListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EarlyHintPreloader::OnRedirectResult(nsresult aStatus) {
  LOG(("EarlyHintPreloader::OnRedirectResult [this=%p] aProceeding=0x%" PRIx32,
       this, static_cast<uint32_t>(aStatus)));
  if (NS_SUCCEEDED(aStatus) && mRedirectChannel) {
    mChannel = mRedirectChannel;
  }

  mRedirectChannel = nullptr;

  return NS_OK;
}

//-----------------------------------------------------------------------------
// EarlyHintPreloader::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
EarlyHintPreloader::GetInterface(const nsIID& aIID, void** aResult) {
  if (aIID.Equals(NS_GET_IID(nsIChannelEventSink))) {
    NS_ADDREF_THIS();
    *aResult = static_cast<nsIChannelEventSink*>(this);
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIRedirectResultListener))) {
    NS_ADDREF_THIS();
    *aResult = static_cast<nsIRedirectResultListener*>(this);
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

void EarlyHintPreloader::CollectResourcesTypeTelemetry(
    ASDestination aASDestination) {
  if (aASDestination == ASDestination::DESTINATION_FONT) {
    glean::netwerk::early_hints.Get("font"_ns).Add(1);
  } else if (aASDestination == ASDestination::DESTINATION_SCRIPT) {
    glean::netwerk::early_hints.Get("script"_ns).Add(1);
  } else if (aASDestination == ASDestination::DESTINATION_STYLE) {
    glean::netwerk::early_hints.Get("stylesheet"_ns).Add(1);
  } else if (aASDestination == ASDestination::DESTINATION_IMAGE) {
    glean::netwerk::early_hints.Get("image"_ns).Add(1);
  } else if (aASDestination == ASDestination::DESTINATION_FETCH) {
    glean::netwerk::early_hints.Get("fetch"_ns).Add(1);
  } else {
    glean::netwerk::early_hints.Get("other"_ns).Add(1);
  }
}
}  // namespace mozilla::net
