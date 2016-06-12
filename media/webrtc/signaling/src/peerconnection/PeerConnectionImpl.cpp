/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <cerrno>
#include <deque>
#include <set>
#include <sstream>
#include <vector>

#include "CSFLog.h"
#include "timecard.h"

#include "jsapi.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"

#include "nsNetCID.h"
#include "nsIProperty.h"
#include "nsIPropertyBag2.h"
#include "nsIServiceManager.h"
#include "nsISimpleEnumerator.h"
#include "nsServiceManagerUtils.h"
#include "nsISocketTransportService.h"
#include "nsIConsoleService.h"
#include "nsThreadUtils.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsProxyRelease.h"
#include "nsQueryObject.h"
#include "prtime.h"

#include "AudioConduit.h"
#include "VideoConduit.h"
#include "runnable_utils.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "nsDOMDataChannelDeclarations.h"
#include "dtlsidentity.h"
#include "signaling/src/sdp/SdpAttribute.h"

#include "signaling/src/jsep/JsepTrack.h"
#include "signaling/src/jsep/JsepSession.h"
#include "signaling/src/jsep/JsepSessionImpl.h"

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#ifdef XP_WIN
// We need to undef the MS macro for nsIDocument::CreateEvent
#ifdef CreateEvent
#undef CreateEvent
#endif
#endif // XP_WIN

#include "nsIDocument.h"
#include "nsGlobalWindow.h"
#include "nsDOMDataChannel.h"
#include "mozilla/dom/Performance.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Preferences.h"
#include "mozilla/PublicSSL.h"
#include "nsXULAppAPI.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsIScriptError.h"
#include "nsPrintfCString.h"
#include "nsURLHelper.h"
#include "nsNetUtil.h"
#include "nsIURLParser.h"
#include "nsIDOMDataChannel.h"
#include "nsIDOMLocation.h"
#include "nsNullPrincipal.h"
#include "mozilla/PeerIdentity.h"
#include "mozilla/dom/RTCCertificate.h"
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "mozilla/dom/RTCRtpSenderBinding.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/dom/PeerConnectionImplBinding.h"
#include "mozilla/dom/DataChannelBinding.h"
#include "mozilla/dom/PerformanceTiming.h"
#include "mozilla/dom/PluginCrashedEvent.h"
#include "MediaStreamList.h"
#include "MediaStreamTrack.h"
#include "AudioStreamTrack.h"
#include "VideoStreamTrack.h"
#include "nsIScriptGlobalObject.h"
#include "MediaStreamGraph.h"
#include "DOMMediaStream.h"
#include "rlogringbuffer.h"
#include "WebrtcGlobalInformation.h"
#include "mozilla/dom/Event.h"
#include "nsIDOMCustomEvent.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/net/DataChannelProtocol.h"
#endif

#ifndef USE_FAKE_MEDIA_STREAMS
#include "MediaStreamGraphImpl.h"
#endif

#ifdef XP_WIN
// We need to undef the MS macro again in case the windows include file
// got imported after we included nsIDocument.h
#ifdef CreateEvent
#undef CreateEvent
#endif
#endif // XP_WIN

#ifndef USE_FAKE_MEDIA_STREAMS
#include "MediaSegment.h"
#endif

#ifdef USE_FAKE_PCOBSERVER
#include "FakePCObserver.h"
#else
#include "mozilla/dom/PeerConnectionObserverBinding.h"
#endif
#include "mozilla/dom/PeerConnectionObserverEnumsBinding.h"

#ifdef MOZ_WEBRTC_OMX
#include "OMXVideoCodec.h"
#include "OMXCodecWrapper.h"
#endif

#define ICE_PARSING "In RTCConfiguration passed to RTCPeerConnection constructor"

using namespace mozilla;
using namespace mozilla::dom;

typedef PCObserverString ObString;

static const char* logTag = "PeerConnectionImpl";


// Getting exceptions back down from PCObserver is generally not harmful.
namespace {
class JSErrorResult : public ErrorResult
{
public:
  ~JSErrorResult()
  {
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
    SuppressException();
#endif
  }
};

// The WrapRunnable() macros copy passed-in args and passes them to the function
// later on the other thread. ErrorResult cannot be passed like this because it
// disallows copy-semantics.
//
// This WrappableJSErrorResult hack solves this by not actually copying the
// ErrorResult, but creating a new one instead, which works because we don't
// care about the result.
//
// Since this is for JS-calls, these can only be dispatched to the main thread.

class WrappableJSErrorResult {
public:
  WrappableJSErrorResult() : isCopy(false) {}
  WrappableJSErrorResult(const WrappableJSErrorResult &other) : mRv(), isCopy(true) {}
  ~WrappableJSErrorResult() {
    if (isCopy) {
      MOZ_ASSERT(NS_IsMainThread());
    }
  }
  operator JSErrorResult &() { return mRv; }
private:
  JSErrorResult mRv;
  bool isCopy;
};
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
static nsresult InitNSSInContent()
{
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_NOT_SAME_THREAD);

  if (!XRE_IsContentProcess()) {
    MOZ_ASSERT_UNREACHABLE("Must be called in content process");
    return NS_ERROR_FAILURE;
  }

  static bool nssStarted = false;
  if (nssStarted) {
    return NS_OK;
  }

  if (NSS_NoDB_Init(nullptr) != SECSuccess) {
    CSFLogError(logTag, "NSS_NoDB_Init failed.");
    return NS_ERROR_FAILURE;
  }

  if (NS_FAILED(mozilla::psm::InitializeCipherSuite())) {
    CSFLogError(logTag, "Fail to set up nss cipher suite.");
    return NS_ERROR_FAILURE;
  }

  mozilla::psm::DisableMD5();

  nssStarted = true;

  return NS_OK;
}
#endif // MOZILLA_INTERNAL_API

namespace mozilla {
  class DataChannel;
}

class nsIDOMDataChannel;

PRLogModuleInfo *signalingLogInfo() {
  static PRLogModuleInfo *logModuleInfo = nullptr;
  if (!logModuleInfo) {
    logModuleInfo = PR_NewLogModule("signaling");
  }
  return logModuleInfo;
}

// XXX Workaround for bug 998092 to maintain the existing broken semantics
template<>
struct nsISupportsWeakReference::COMTypeInfo<nsSupportsWeakReference, void> {
  static const nsIID kIID;
};
const nsIID nsISupportsWeakReference::COMTypeInfo<nsSupportsWeakReference, void>::kIID = NS_ISUPPORTSWEAKREFERENCE_IID;

namespace mozilla {

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
RTCStatsQuery::RTCStatsQuery(bool internal) :
  failed(false),
  internalStats(internal),
  grabAllLevels(false) {
}

RTCStatsQuery::~RTCStatsQuery() {
  MOZ_ASSERT(NS_IsMainThread());
}

#endif

NS_IMPL_ISUPPORTS0(PeerConnectionImpl)

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
bool
PeerConnectionImpl::WrapObject(JSContext* aCx,
                               JS::Handle<JSObject*> aGivenProto,
                               JS::MutableHandle<JSObject*> aReflector)
{
  return PeerConnectionImplBinding::Wrap(aCx, this, aGivenProto, aReflector);
}
#endif

bool PCUuidGenerator::Generate(std::string* idp) {
  nsresult rv;

  if(!mGenerator) {
    mGenerator = do_GetService("@mozilla.org/uuid-generator;1", &rv);
    if (NS_FAILED(rv)) {
      return false;
    }
    if (!mGenerator) {
      return false;
    }
  }

  nsID id;
  rv = mGenerator->GenerateUUIDInPlace(&id);
  if (NS_FAILED(rv)) {
    return false;
  }
  char buffer[NSID_LENGTH];
  id.ToProvidedString(buffer);
  idp->assign(buffer);

  return true;
}

bool IsPrivateBrowsing(nsPIDOMWindowInner* aWindow)
{
#if defined(MOZILLA_EXTERNAL_LINKAGE)
  return false;
#else
  if (!aWindow) {
    return false;
  }

  nsIDocument *doc = aWindow->GetExtantDoc();
  if (!doc) {
    return false;
  }

  nsILoadContext *loadContext = doc->GetLoadContext();
  return loadContext && loadContext->UsePrivateBrowsing();
#endif
}

PeerConnectionImpl::PeerConnectionImpl(const GlobalObject* aGlobal)
: mTimeCard(MOZ_LOG_TEST(signalingLogInfo(),LogLevel::Error) ?
            create_timecard() : nullptr)
  , mSignalingState(PCImplSignalingState::SignalingStable)
  , mIceConnectionState(PCImplIceConnectionState::New)
  , mIceGatheringState(PCImplIceGatheringState::New)
  , mDtlsConnected(false)
  , mWindow(nullptr)
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  , mCertificate(nullptr)
#else
  , mIdentity(nullptr)
#endif
  , mPrivacyRequested(false)
  , mIsLoop(false)
  , mSTSThread(nullptr)
  , mAllowIceLoopback(false)
  , mAllowIceLinkLocal(false)
  , mMedia(nullptr)
  , mUuidGen(MakeUnique<PCUuidGenerator>())
  , mNumAudioStreams(0)
  , mNumVideoStreams(0)
  , mHaveConfiguredCodecs(false)
  , mHaveDataStream(false)
  , mAddCandidateErrorCount(0)
  , mTrickle(true) // TODO(ekr@rtfm.com): Use pref
  , mNegotiationNeeded(false)
  , mPrivateWindow(false)
{
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  MOZ_ASSERT(NS_IsMainThread());
  auto log = RLogRingBuffer::CreateInstance();
  if (aGlobal) {
    mWindow = do_QueryInterface(aGlobal->GetAsSupports());
    if (IsPrivateBrowsing(mWindow)) {
      mPrivateWindow = true;
      log->EnterPrivateMode();
    }
  }
#endif
  CSFLogInfo(logTag, "%s: PeerConnectionImpl constructor for %s",
             __FUNCTION__, mHandle.c_str());
  STAMP_TIMECARD(mTimeCard, "Constructor Completed");
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  mAllowIceLoopback = Preferences::GetBool(
    "media.peerconnection.ice.loopback", false);
  mAllowIceLinkLocal = Preferences::GetBool(
    "media.peerconnection.ice.link_local", false);
#endif
  memset(mMaxReceiving, 0, sizeof(mMaxReceiving));
  memset(mMaxSending, 0, sizeof(mMaxSending));
}

PeerConnectionImpl::~PeerConnectionImpl()
{
  if (mTimeCard) {
    STAMP_TIMECARD(mTimeCard, "Destructor Invoked");
    print_timecard(mTimeCard);
    destroy_timecard(mTimeCard);
    mTimeCard = nullptr;
  }
  // This aborts if not on main thread (in Debug builds)
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (mPrivateWindow) {
    auto * log = RLogRingBuffer::GetInstance();
    if (log) {
      log->ExitPrivateMode();
    }
    mPrivateWindow = false;
  }
#endif
  if (PeerConnectionCtx::isActive()) {
    PeerConnectionCtx::GetInstance()->mPeerConnections.erase(mHandle);
  } else {
    CSFLogError(logTag, "PeerConnectionCtx is already gone. Ignoring...");
  }

  CSFLogInfo(logTag, "%s: PeerConnectionImpl destructor invoked for %s",
             __FUNCTION__, mHandle.c_str());

  Close();

  // Since this and Initialize() occur on MainThread, they can't both be
  // running at once

  // Right now, we delete PeerConnectionCtx at XPCOM shutdown only, but we
  // probably want to shut it down more aggressively to save memory.  We
  // could shut down here when there are no uses.  It might be more optimal
  // to release off a timer (and XPCOM Shutdown) to avoid churn
}

already_AddRefed<DOMMediaStream>
PeerConnectionImpl::MakeMediaStream()
{
  MediaStreamGraph* graph =
    MediaStreamGraph::GetInstance(MediaStreamGraph::AUDIO_THREAD_DRIVER,
                                  AudioChannel::Normal);

  RefPtr<DOMMediaStream> stream =
    DOMMediaStream::CreateSourceStreamAsInput(GetWindow(), graph);

  CSFLogDebug(logTag, "Created media stream %p, inner: %p", stream.get(), stream->GetInputStream());

  return stream.forget();
}

nsresult
PeerConnectionImpl::CreateRemoteSourceStreamInfo(RefPtr<RemoteSourceStreamInfo>*
                                                 aInfo,
                                                 const std::string& aStreamID)
{
  MOZ_ASSERT(aInfo);
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  RefPtr<DOMMediaStream> stream = MakeMediaStream();
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  RefPtr<RemoteSourceStreamInfo> remote;
  remote = new RemoteSourceStreamInfo(stream.forget(), mMedia, aStreamID);
  *aInfo = remote;

  return NS_OK;
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
/**
 * In JS, an RTCConfiguration looks like this:
 *
 * { "iceServers": [ { url:"stun:stun.example.org" },
 *                   { url:"turn:turn.example.org?transport=udp",
 *                     username: "jib", credential:"mypass"} ] }
 *
 * This function converts that into an internal PeerConnectionConfiguration
 * object.
 */
nsresult
PeerConnectionConfiguration::Init(const RTCConfiguration& aSrc)
{
  if (aSrc.mIceServers.WasPassed()) {
    for (size_t i = 0; i < aSrc.mIceServers.Value().Length(); i++) {
      nsresult rv = AddIceServer(aSrc.mIceServers.Value()[i]);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  switch (aSrc.mBundlePolicy) {
    case dom::RTCBundlePolicy::Balanced:
      setBundlePolicy(kBundleBalanced);
      break;
    case dom::RTCBundlePolicy::Max_compat:
      setBundlePolicy(kBundleMaxCompat);
      break;
    case dom::RTCBundlePolicy::Max_bundle:
      setBundlePolicy(kBundleMaxBundle);
      break;
    default:
      MOZ_CRASH();
  }

  switch (aSrc.mIceTransportPolicy) {
    case dom::RTCIceTransportPolicy::None:
      setIceTransportPolicy(NrIceCtx::ICE_POLICY_NONE);
      break;
    case dom::RTCIceTransportPolicy::Relay:
      setIceTransportPolicy(NrIceCtx::ICE_POLICY_RELAY);
      break;
    case dom::RTCIceTransportPolicy::All:
      setIceTransportPolicy(NrIceCtx::ICE_POLICY_ALL);
      break;
    default:
      MOZ_CRASH();
  }
  return NS_OK;
}

nsresult
PeerConnectionConfiguration::AddIceServer(const RTCIceServer &aServer)
{
  NS_ENSURE_STATE(aServer.mUrls.WasPassed());
  NS_ENSURE_STATE(aServer.mUrls.Value().IsStringSequence());
  auto &urls = aServer.mUrls.Value().GetAsStringSequence();
  for (size_t i = 0; i < urls.Length(); i++) {
    // Without STUN/TURN handlers, NS_NewURI returns nsSimpleURI rather than
    // nsStandardURL. To parse STUN/TURN URI's to spec
    // http://tools.ietf.org/html/draft-nandakumar-rtcweb-stun-uri-02#section-3
    // http://tools.ietf.org/html/draft-petithuguenin-behave-turn-uri-03#section-3
    // we parse out the query-string, and use ParseAuthority() on the rest
    RefPtr<nsIURI> url;
    nsresult rv = NS_NewURI(getter_AddRefs(url), urls[i]);
    NS_ENSURE_SUCCESS(rv, rv);
    bool isStun = false, isStuns = false, isTurn = false, isTurns = false;
    url->SchemeIs("stun", &isStun);
    url->SchemeIs("stuns", &isStuns);
    url->SchemeIs("turn", &isTurn);
    url->SchemeIs("turns", &isTurns);
    if (!(isStun || isStuns || isTurn || isTurns)) {
      return NS_ERROR_FAILURE;
    }
    if (isTurns || isStuns) {
      continue; // TODO: Support TURNS and STUNS (Bug 1056934)
    }
    nsAutoCString spec;
    rv = url->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);

    // TODO(jib@mozilla.com): Revisit once nsURI supports STUN/TURN (Bug 833509)
    int32_t port;
    nsAutoCString host;
    nsAutoCString transport;
    {
      uint32_t hostPos;
      int32_t hostLen;
      nsAutoCString path;
      rv = url->GetPath(path);
      NS_ENSURE_SUCCESS(rv, rv);

      // Tolerate query-string + parse 'transport=[udp|tcp]' by hand.
      int32_t questionmark = path.FindChar('?');
      if (questionmark >= 0) {
        const nsCString match = NS_LITERAL_CSTRING("transport=");

        for (int32_t i = questionmark, endPos; i >= 0; i = endPos) {
          endPos = path.FindCharInSet("&", i + 1);
          const nsDependentCSubstring fieldvaluepair = Substring(path, i + 1,
                                                                 endPos);
          if (StringBeginsWith(fieldvaluepair, match)) {
            transport = Substring(fieldvaluepair, match.Length());
            ToLowerCase(transport);
          }
        }
        path.SetLength(questionmark);
      }

      rv = net_GetAuthURLParser()->ParseAuthority(path.get(), path.Length(),
                                                  nullptr,  nullptr,
                                                  nullptr,  nullptr,
                                                  &hostPos,  &hostLen, &port);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!hostLen) {
        return NS_ERROR_FAILURE;
      }
      if (hostPos > 1)  /* The username was removed */
        return NS_ERROR_FAILURE;
      path.Mid(host, hostPos, hostLen);
    }
    if (port == -1)
      port = (isStuns || isTurns)? 5349 : 3478;

    if (isTurn || isTurns) {
      NS_ConvertUTF16toUTF8 credential(aServer.mCredential.Value());
      NS_ConvertUTF16toUTF8 username(aServer.mUsername.Value());

      if (!addTurnServer(host.get(), port,
                         username.get(),
                         credential.get(),
                         (transport.IsEmpty() ?
                          kNrIceTransportUdp : transport.get()))) {
        return NS_ERROR_FAILURE;
      }
    } else {
      if (!addStunServer(host.get(), port, (transport.IsEmpty() ?
                         kNrIceTransportUdp : transport.get()))) {
        return NS_ERROR_FAILURE;
      }
    }
  }
  return NS_OK;
}
#endif

nsresult
PeerConnectionImpl::Initialize(PeerConnectionObserver& aObserver,
                               nsGlobalWindow* aWindow,
                               const PeerConnectionConfiguration& aConfiguration,
                               nsISupports* aThread)
{
  nsresult res;

  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aThread);
  if (!mThread) {
    mThread = do_QueryInterface(aThread);
    MOZ_ASSERT(mThread);
  }
  CheckThread();

  mPCObserver = do_GetWeakReference(&aObserver);

  // Find the STS thread

  mSTSThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
  MOZ_ASSERT(mSTSThread);
#if !defined(MOZILLA_EXTERNAL_LINKAGE)

  // Initialize NSS if we are in content process. For chrome process, NSS should already
  // been initialized.
  if (XRE_IsParentProcess()) {
    // This code interferes with the C++ unit test startup code.
    nsCOMPtr<nsISupports> nssDummy = do_GetService("@mozilla.org/psm;1", &res);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    NS_ENSURE_SUCCESS(res = InitNSSInContent(), res);
  }

  // Currently no standalone unit tests for DataChannel,
  // which is the user of mWindow
  MOZ_ASSERT(aWindow);
  mWindow = aWindow->AsInner();
  NS_ENSURE_STATE(mWindow);
#endif // MOZILLA_INTERNAL_API

  PRTime timestamp = PR_Now();
  // Ok if we truncate this.
  char temp[128];

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  nsAutoCString locationCStr;

  if (nsCOMPtr<nsIDOMLocation> location = mWindow->GetLocation()) {
    nsAutoString locationAStr;
    location->ToString(locationAStr);

    CopyUTF16toUTF8(locationAStr, locationCStr);
#define HELLO_CLICKER_URL_START "https://hello.firefox.com/"
#define HELLO_INITIATOR_URL_START "about:loop"
    mIsLoop = (strncmp(HELLO_CLICKER_URL_START, locationCStr.get(),
                       strlen(HELLO_CLICKER_URL_START)) == 0) ||
              (strncmp(HELLO_INITIATOR_URL_START, locationCStr.get(),
                       strlen(HELLO_INITIATOR_URL_START)) == 0);
  }

  PR_snprintf(
      temp,
      sizeof(temp),
      "%llu (id=%llu url=%s)",
      static_cast<unsigned long long>(timestamp),
      static_cast<unsigned long long>(mWindow ? mWindow->WindowID() : 0),
      locationCStr.get() ? locationCStr.get() : "NULL");

#else
  PR_snprintf(temp, sizeof(temp), "%llu", (unsigned long long)timestamp);
#endif // MOZILLA_INTERNAL_API

  mName = temp;

  // Generate a random handle
  unsigned char handle_bin[8];
  SECStatus rv;
  rv = PK11_GenerateRandom(handle_bin, sizeof(handle_bin));
  if (rv != SECSuccess) {
    MOZ_CRASH();
    return NS_ERROR_UNEXPECTED;
  }

  char hex[17];
  PR_snprintf(hex,sizeof(hex),"%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
    handle_bin[0],
    handle_bin[1],
    handle_bin[2],
    handle_bin[3],
    handle_bin[4],
    handle_bin[5],
    handle_bin[6],
    handle_bin[7]);

  mHandle = hex;

  STAMP_TIMECARD(mTimeCard, "Initializing PC Ctx");
  res = PeerConnectionCtx::InitializeGlobal(mThread, mSTSThread);
  NS_ENSURE_SUCCESS(res, res);

  mMedia = new PeerConnectionMedia(this);

  // Connect ICE slots.
  mMedia->SignalIceGatheringStateChange.connect(
      this,
      &PeerConnectionImpl::IceGatheringStateChange);
  mMedia->SignalUpdateDefaultCandidate.connect(
      this,
      &PeerConnectionImpl::UpdateDefaultCandidate);
  mMedia->SignalEndOfLocalCandidates.connect(
      this,
      &PeerConnectionImpl::EndOfLocalCandidates);
  mMedia->SignalIceConnectionStateChange.connect(
      this,
      &PeerConnectionImpl::IceConnectionStateChange);

  mMedia->SignalCandidate.connect(this, &PeerConnectionImpl::CandidateReady);

  // Initialize the media object.
  res = mMedia->Init(aConfiguration.getStunServers(),
                     aConfiguration.getTurnServers(),
                     aConfiguration.getIceTransportPolicy());
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Couldn't initialize media object", __FUNCTION__);
    return res;
  }

  PeerConnectionCtx::GetInstance()->mPeerConnections[mHandle] = this;

  mJsepSession = MakeUnique<JsepSessionImpl>(mName,
                                             MakeUnique<PCUuidGenerator>());

  res = mJsepSession->Init();
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Couldn't init JSEP Session, res=%u",
                        __FUNCTION__,
                        static_cast<unsigned>(res));
    return res;
  }

  res = mJsepSession->SetIceCredentials(mMedia->ice_ctx()->ufrag(),
                                        mMedia->ice_ctx()->pwd());
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Couldn't set ICE credentials, res=%u",
                         __FUNCTION__,
                         static_cast<unsigned>(res));
    return res;
  }

#if defined(MOZILLA_EXTERNAL_LINKAGE)
  {
    mIdentity = DtlsIdentity::Generate();
    if (!mIdentity) {
      return NS_ERROR_FAILURE;
    }

    std::vector<uint8_t> fingerprint;
    res = CalculateFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM,
                               &fingerprint);
    NS_ENSURE_SUCCESS(res, res);

    res = mJsepSession->AddDtlsFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM,
                                           fingerprint);
    NS_ENSURE_SUCCESS(res, res);
  }
#endif

  res = mJsepSession->SetBundlePolicy(aConfiguration.getBundlePolicy());
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Couldn't set bundle policy, res=%u, error=%s",
                        __FUNCTION__,
                        static_cast<unsigned>(res),
                        mJsepSession->GetLastError().c_str());
    return res;
  }

  return NS_OK;
}

#ifndef MOZILLA_EXTERNAL_LINKAGE
void
PeerConnectionImpl::Initialize(PeerConnectionObserver& aObserver,
                               nsGlobalWindow& aWindow,
                               const RTCConfiguration& aConfiguration,
                               nsISupports* aThread,
                               ErrorResult &rv)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aThread);
  mThread = do_QueryInterface(aThread);

  PeerConnectionConfiguration converted;
  nsresult res = converted.Init(aConfiguration);
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Invalid RTCConfiguration", __FUNCTION__);
    rv.Throw(res);
    return;
  }

  res = Initialize(aObserver, &aWindow, converted, aThread);
  if (NS_FAILED(res)) {
    rv.Throw(res);
  }

  if (!aConfiguration.mPeerIdentity.IsEmpty()) {
    mPeerIdentity = new PeerIdentity(aConfiguration.mPeerIdentity);
    mPrivacyRequested = true;
  }
}
#endif

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
void
PeerConnectionImpl::SetCertificate(mozilla::dom::RTCCertificate& aCertificate)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(!mCertificate, "This can only be called once");
  mCertificate = &aCertificate;

  std::vector<uint8_t> fingerprint;
  nsresult rv = CalculateFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM,
                                     &fingerprint);
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Couldn't calculate fingerprint, rv=%u",
                __FUNCTION__, static_cast<unsigned>(rv));
    mCertificate = nullptr;
    return;
  }
  rv = mJsepSession->AddDtlsFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM,
                                        fingerprint);
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Couldn't set DTLS credentials, rv=%u",
                __FUNCTION__, static_cast<unsigned>(rv));
    mCertificate = nullptr;
  }
}

const RefPtr<mozilla::dom::RTCCertificate>&
PeerConnectionImpl::Certificate() const
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mCertificate;
}
#endif

RefPtr<DtlsIdentity>
PeerConnectionImpl::Identity() const
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  MOZ_ASSERT(mCertificate);
  return mCertificate->CreateDtlsIdentity();
#else
  RefPtr<DtlsIdentity> id = mIdentity;
  return id;
#endif
}

class CompareCodecPriority {
  public:
    void SetPreferredCodec(int32_t preferredCodec) {
      // This pref really ought to be a string, preferably something like
      // "H264" or "VP8" instead of a payload type.
      // Bug 1101259.
      std::ostringstream os;
      os << preferredCodec;
      mPreferredCodec = os.str();
    }

    bool operator()(JsepCodecDescription* lhs,
                    JsepCodecDescription* rhs) const {
      if (!mPreferredCodec.empty() &&
          lhs->mDefaultPt == mPreferredCodec &&
          rhs->mDefaultPt != mPreferredCodec) {
        return true;
      }

      if (lhs->mStronglyPreferred && !rhs->mStronglyPreferred) {
        return true;
      }

      return false;
    }

  private:
    std::string mPreferredCodec;
};

class ConfigureCodec {
  public:
    explicit ConfigureCodec(nsCOMPtr<nsIPrefBranch>& branch) :
      mHardwareH264Enabled(false),
      mHardwareH264Supported(false),
      mSoftwareH264Enabled(false),
      mH264Enabled(false),
      mVP9Enabled(false),
      mH264Level(13), // minimum suggested for WebRTC spec
      mH264MaxBr(0), // Unlimited
      mH264MaxMbps(0), // Unlimited
      mVP8MaxFs(0),
      mVP8MaxFr(0),
      mUseTmmbr(false),
      mUseRemb(false),
      mUseAudioFec(false)
    {
#ifdef MOZ_WEBRTC_OMX
      // Check to see if what HW codecs are available (not in use) at this moment.
      // Note that streaming video decode can reserve a decoder

      // XXX See bug 1018791 Implement W3 codec reservation policy
      // Note that currently, OMXCodecReservation needs to be held by an sp<> because it puts
      // 'this' into an sp<EventListener> to talk to the resource reservation code

      // This pref is a misnomer; it is solely for h264 _hardware_ support.
      branch->GetBoolPref("media.peerconnection.video.h264_enabled",
          &mHardwareH264Enabled);

      if (mHardwareH264Enabled) {
        // Ok, it is preffed on. Can we actually do it?
        android::sp<android::OMXCodecReservation> encode = new android::OMXCodecReservation(true);
        android::sp<android::OMXCodecReservation> decode = new android::OMXCodecReservation(false);

        // Currently we just check if they're available right now, which will fail if we're
        // trying to call ourself, for example.  It will work for most real-world cases, like
        // if we try to add a person to a 2-way call to make a 3-way mesh call
        if (encode->ReserveOMXCodec() && decode->ReserveOMXCodec()) {
          CSFLogDebug( logTag, "%s: H264 hardware codec available", __FUNCTION__);
          mHardwareH264Supported = true;
        }
      }

#endif // MOZ_WEBRTC_OMX

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
      mSoftwareH264Enabled = PeerConnectionCtx::GetInstance()->gmpHasH264();
#else
      // For unit-tests
      mSoftwareH264Enabled = true;
#endif

      mH264Enabled = mHardwareH264Supported || mSoftwareH264Enabled;

      branch->GetIntPref("media.navigator.video.h264.level", &mH264Level);
      mH264Level &= 0xFF;

      branch->GetIntPref("media.navigator.video.h264.max_br", &mH264MaxBr);

#ifdef MOZ_WEBRTC_OMX
      // Level 1.2; but let's allow CIF@30 or QVGA@30+ by default
      mH264MaxMbps = 11880;
#endif

      branch->GetIntPref("media.navigator.video.h264.max_mbps", &mH264MaxMbps);

      branch->GetBoolPref("media.peerconnection.video.vp9_enabled",
          &mVP9Enabled);

      branch->GetIntPref("media.navigator.video.max_fs", &mVP8MaxFs);
      if (mVP8MaxFs <= 0) {
        mVP8MaxFs = 12288; // We must specify something other than 0
      }

      branch->GetIntPref("media.navigator.video.max_fr", &mVP8MaxFr);
      if (mVP8MaxFr <= 0) {
        mVP8MaxFr = 60; // We must specify something other than 0
      }

      // TMMBR is enabled from a pref in about:config
      branch->GetBoolPref("media.navigator.video.use_tmmbr", &mUseTmmbr);

      // REMB is enabled by default, but can be disabled from about:config
      branch->GetBoolPref("media.navigator.video.use_remb", &mUseRemb);

      branch->GetBoolPref("media.navigator.audio.use_fec", &mUseAudioFec);
    }

    void operator()(JsepCodecDescription* codec) const
    {
      switch (codec->mType) {
        case SdpMediaSection::kAudio:
          {
            JsepAudioCodecDescription& audioCodec =
              static_cast<JsepAudioCodecDescription&>(*codec);
            if (audioCodec.mName == "opus") {
              audioCodec.mFECEnabled = mUseAudioFec;
            }
          }
          break;
        case SdpMediaSection::kVideo:
          {
            JsepVideoCodecDescription& videoCodec =
              static_cast<JsepVideoCodecDescription&>(*codec);

            if (videoCodec.mName == "H264") {
              // Override level
              videoCodec.mProfileLevelId &= 0xFFFF00;
              videoCodec.mProfileLevelId |= mH264Level;

              videoCodec.mConstraints.maxBr = mH264MaxBr;

              videoCodec.mConstraints.maxMbps = mH264MaxMbps;

              // Might disable it, but we set up other params anyway
              videoCodec.mEnabled = mH264Enabled;

              if (videoCodec.mPacketizationMode == 0 && !mSoftwareH264Enabled) {
                // We're assuming packetization mode 0 is unsupported by
                // hardware.
                videoCodec.mEnabled = false;
              }

              if (mHardwareH264Supported) {
                videoCodec.mStronglyPreferred = true;
              }
            } else if (videoCodec.mName == "VP8" || videoCodec.mName == "VP9") {
              if (videoCodec.mName == "VP9" && !mVP9Enabled) {
                videoCodec.mEnabled = false;
                break;
              }
              videoCodec.mConstraints.maxFs = mVP8MaxFs;
              videoCodec.mConstraints.maxFps = mVP8MaxFr;
            }

            if (mUseTmmbr) {
              videoCodec.EnableTmmbr();
            }
            if (mUseRemb) {
              videoCodec.EnableRemb();
            }
          }
          break;
        case SdpMediaSection::kText:
        case SdpMediaSection::kApplication:
        case SdpMediaSection::kMessage:
          {} // Nothing to configure for these.
      }
    }

  private:
    bool mHardwareH264Enabled;
    bool mHardwareH264Supported;
    bool mSoftwareH264Enabled;
    bool mH264Enabled;
    bool mVP9Enabled;
    int32_t mH264Level;
    int32_t mH264MaxBr;
    int32_t mH264MaxMbps;
    int32_t mVP8MaxFs;
    int32_t mVP8MaxFr;
    bool mUseTmmbr;
    bool mUseRemb;
    bool mUseAudioFec;
};

nsresult
PeerConnectionImpl::ConfigureJsepSessionCodecs() {
  nsresult res;
  nsCOMPtr<nsIPrefService> prefs =
    do_GetService("@mozilla.org/preferences-service;1", &res);

  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Couldn't get prefs service, res=%u",
        __FUNCTION__,
        static_cast<unsigned>(res));
    return res;
  }

  nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);
  if (!branch) {
    CSFLogError(logTag, "%s: Couldn't get prefs branch", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  ConfigureCodec configurer(branch);
  mJsepSession->ForEachCodec(configurer);

  // We use this to sort the list of codecs once everything is configured
  CompareCodecPriority comparator;

  // Sort by priority
  int32_t preferredCodec = 0;
  branch->GetIntPref("media.navigator.video.preferred_codec",
                     &preferredCodec);

  if (preferredCodec) {
    comparator.SetPreferredCodec(preferredCodec);
  }

  mJsepSession->SortCodecs(comparator);
  return NS_OK;
}

// Data channels won't work without a window, so in order for the C++ unit
// tests to work (it doesn't have a window available) we ifdef the following
// two implementations.
NS_IMETHODIMP
PeerConnectionImpl::EnsureDataConnection(uint16_t aNumstreams)
{
  PC_AUTO_ENTER_API_CALL(false);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (mDataConnection) {
    CSFLogDebug(logTag,"%s DataConnection already connected",__FUNCTION__);
    // Ignore the request to connect when already connected.  This entire
    // implementation is temporary.  Ignore aNumstreams as it's merely advisory
    // and we increase the number of streams dynamically as needed.
    return NS_OK;
  }
  mDataConnection = new DataChannelConnection(this);
  if (!mDataConnection->Init(5000, aNumstreams, true)) {
    CSFLogError(logTag,"%s DataConnection Init Failed",__FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  CSFLogDebug(logTag,"%s DataChannelConnection %p attached to %s",
              __FUNCTION__, (void*) mDataConnection.get(), mHandle.c_str());
#endif
  return NS_OK;
}

nsresult
PeerConnectionImpl::GetDatachannelParameters(
    const mozilla::JsepApplicationCodecDescription** datachannelCodec,
    uint16_t* level) const {

  auto trackPairs = mJsepSession->GetNegotiatedTrackPairs();
  for (auto j = trackPairs.begin(); j != trackPairs.end(); ++j) {
    JsepTrackPair& trackPair = *j;

    bool sendDataChannel =
      trackPair.mSending &&
      trackPair.mSending->GetMediaType() == SdpMediaSection::kApplication;
    bool recvDataChannel =
      trackPair.mReceiving &&
      trackPair.mReceiving->GetMediaType() == SdpMediaSection::kApplication;
    (void)recvDataChannel;
    MOZ_ASSERT(sendDataChannel == recvDataChannel);

    if (sendDataChannel) {
      // This will release assert if there is no such index, and that's ok
      const JsepTrackEncoding& encoding =
        trackPair.mSending->GetNegotiatedDetails()->GetEncoding(0);

      if (encoding.GetCodecs().empty()) {
        CSFLogError(logTag, "%s: Negotiated m=application with no codec. "
                            "This is likely to be broken.",
                            __FUNCTION__);
        return NS_ERROR_FAILURE;
      }

      for (const JsepCodecDescription* codec : encoding.GetCodecs()) {
        if (codec->mType != SdpMediaSection::kApplication) {
          CSFLogError(logTag, "%s: Codec type for m=application was %u, this "
                              "is a bug.",
                              __FUNCTION__,
                              static_cast<unsigned>(codec->mType));
          MOZ_ASSERT(false, "Codec for m=application was not \"application\"");
          return NS_ERROR_FAILURE;
        }

        if (codec->mName != "webrtc-datachannel") {
          CSFLogWarn(logTag, "%s: Codec for m=application was not "
                             "webrtc-datachannel (was instead %s). ",
                             __FUNCTION__,
                             codec->mName.c_str());
          continue;
        }

        *datachannelCodec =
          static_cast<const JsepApplicationCodecDescription*>(codec);
        if (trackPair.mBundleLevel.isSome()) {
          *level = static_cast<uint16_t>(*trackPair.mBundleLevel);
        } else {
          *level = static_cast<uint16_t>(trackPair.mLevel);
        }
        return NS_OK;
      }
    }
  }

  *datachannelCodec = nullptr;
  *level = 0;
  return NS_OK;
}

/* static */
void
PeerConnectionImpl::DeferredAddTrackToJsepSession(
    const std::string& pcHandle,
    SdpMediaSection::MediaType type,
    const std::string& streamId,
    const std::string& trackId)
{
  PeerConnectionWrapper wrapper(pcHandle);

  if (wrapper.impl()) {
    if (!PeerConnectionCtx::GetInstance()->isReady()) {
      MOZ_CRASH("Why is DeferredAddTrackToJsepSession being executed when the "
                "PeerConnectionCtx isn't ready?");
    }
    wrapper.impl()->AddTrackToJsepSession(type, streamId, trackId);
  }
}

nsresult
PeerConnectionImpl::AddTrackToJsepSession(SdpMediaSection::MediaType type,
                                          const std::string& streamId,
                                          const std::string& trackId)
{
  nsresult res = ConfigureJsepSessionCodecs();
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "Failed to configure codecs");
    return res;
  }

  res = mJsepSession->AddTrack(
      new JsepTrack(type, streamId, trackId, sdp::kSend));

  if (NS_FAILED(res)) {
    std::string errorString = mJsepSession->GetLastError();
    CSFLogError(logTag, "%s (%s) : pc = %s, error = %s",
                __FUNCTION__,
                type == SdpMediaSection::kAudio ? "audio" : "video",
                mHandle.c_str(),
                errorString.c_str());
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
PeerConnectionImpl::InitializeDataChannel()
{
  PC_AUTO_ENTER_API_CALL(false);
  CSFLogDebug(logTag, "%s", __FUNCTION__);

  const JsepApplicationCodecDescription* codec;
  uint16_t level;
  nsresult rv = GetDatachannelParameters(&codec, &level);

  NS_ENSURE_SUCCESS(rv, rv);

  if (!codec) {
    CSFLogDebug(logTag, "%s: We did not negotiate datachannel", __FUNCTION__);
    return NS_OK;
  }

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  uint32_t channels = codec->mChannels;
  if (channels > MAX_NUM_STREAMS) {
    channels = MAX_NUM_STREAMS;
  }

  rv = EnsureDataConnection(channels);
  if (NS_SUCCEEDED(rv)) {
    uint16_t localport = 5000;
    uint16_t remoteport = 0;
    // The logic that reflects the remote payload type is what sets the remote
    // port here.
    if (!codec->GetPtAsInt(&remoteport)) {
      return NS_ERROR_FAILURE;
    }

    // use the specified TransportFlow
    RefPtr<TransportFlow> flow = mMedia->GetTransportFlow(level, false).get();
    CSFLogDebug(logTag, "Transportflow[%u] = %p",
                        static_cast<unsigned>(level), flow.get());
    if (flow) {
      if (mDataConnection->ConnectViaTransportFlow(flow,
                                                   localport,
                                                   remoteport)) {
        return NS_OK;
      }
    }
    // If we inited the DataConnection, call Destroy() before releasing it
    mDataConnection->Destroy();
  }
  mDataConnection = nullptr;
#endif
  return NS_ERROR_FAILURE;
}

already_AddRefed<nsDOMDataChannel>
PeerConnectionImpl::CreateDataChannel(const nsAString& aLabel,
                                      const nsAString& aProtocol,
                                      uint16_t aType,
                                      bool outOfOrderAllowed,
                                      uint16_t aMaxTime,
                                      uint16_t aMaxNum,
                                      bool aExternalNegotiated,
                                      uint16_t aStream,
                                      ErrorResult &rv)
{
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  RefPtr<nsDOMDataChannel> result;
  rv = CreateDataChannel(aLabel, aProtocol, aType, outOfOrderAllowed,
                         aMaxTime, aMaxNum, aExternalNegotiated,
                         aStream, getter_AddRefs(result));
  return result.forget();
#else
  return nullptr;
#endif
}

NS_IMETHODIMP
PeerConnectionImpl::CreateDataChannel(const nsAString& aLabel,
                                      const nsAString& aProtocol,
                                      uint16_t aType,
                                      bool outOfOrderAllowed,
                                      uint16_t aMaxTime,
                                      uint16_t aMaxNum,
                                      bool aExternalNegotiated,
                                      uint16_t aStream,
                                      nsDOMDataChannel** aRetval)
{
  PC_AUTO_ENTER_API_CALL(false);
  MOZ_ASSERT(aRetval);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  RefPtr<DataChannel> dataChannel;
  DataChannelConnection::Type theType =
    static_cast<DataChannelConnection::Type>(aType);

  nsresult rv = EnsureDataConnection(WEBRTC_DATACHANNEL_STREAMS_DEFAULT);
  if (NS_FAILED(rv)) {
    return rv;
  }
  dataChannel = mDataConnection->Open(
    NS_ConvertUTF16toUTF8(aLabel), NS_ConvertUTF16toUTF8(aProtocol), theType,
    !outOfOrderAllowed,
    aType == DataChannelConnection::PARTIAL_RELIABLE_REXMIT ? aMaxNum :
    (aType == DataChannelConnection::PARTIAL_RELIABLE_TIMED ? aMaxTime : 0),
    nullptr, nullptr, aExternalNegotiated, aStream
  );
  NS_ENSURE_TRUE(dataChannel,NS_ERROR_FAILURE);

  CSFLogDebug(logTag, "%s: making DOMDataChannel", __FUNCTION__);

  if (!mHaveDataStream) {

    std::string streamId;
    std::string trackId;

    // Generate random ids because these aren't linked to any local streams.
    if (!mUuidGen->Generate(&streamId)) {
      return NS_ERROR_FAILURE;
    }
    if (!mUuidGen->Generate(&trackId)) {
      return NS_ERROR_FAILURE;
    }

    RefPtr<JsepTrack> track(new JsepTrack(
          mozilla::SdpMediaSection::kApplication,
          streamId,
          trackId,
          sdp::kSend));

    rv = mJsepSession->AddTrack(track);
    if (NS_FAILED(rv)) {
      CSFLogError(logTag, "%s: Failed to add application track.",
                          __FUNCTION__);
      return rv;
    }
    mHaveDataStream = true;
    OnNegotiationNeeded();
  }
  nsIDOMDataChannel *retval;
  rv = NS_NewDOMDataChannel(dataChannel.forget(), mWindow, &retval);
  if (NS_FAILED(rv)) {
    return rv;
  }
  *aRetval = static_cast<nsDOMDataChannel*>(retval);
#endif
  return NS_OK;
}

// do_QueryObjectReferent() - Helps get PeerConnectionObserver from nsWeakPtr.
//
// nsWeakPtr deals in XPCOM interfaces, while webidl bindings are concrete objs.
// TODO: Turn this into a central (template) function somewhere (Bug 939178)
//
// Without it, each weak-ref call in this file would look like this:
//
//  nsCOMPtr<nsISupportsWeakReference> tmp = do_QueryReferent(mPCObserver);
//  if (!tmp) {
//    return;
//  }
//  RefPtr<nsSupportsWeakReference> tmp2 = do_QueryObject(tmp);
//  RefPtr<PeerConnectionObserver> pco = static_cast<PeerConnectionObserver*>(&*tmp2);

static already_AddRefed<PeerConnectionObserver>
do_QueryObjectReferent(nsIWeakReference* aRawPtr) {
  nsCOMPtr<nsISupportsWeakReference> tmp = do_QueryReferent(aRawPtr);
  if (!tmp) {
    return nullptr;
  }
  RefPtr<nsSupportsWeakReference> tmp2 = do_QueryObject(tmp);
  RefPtr<PeerConnectionObserver> tmp3 = static_cast<PeerConnectionObserver*>(&*tmp2);
  return tmp3.forget();
}


#if !defined(MOZILLA_EXTERNAL_LINKAGE)
// Not a member function so that we don't need to keep the PC live.
static void NotifyDataChannel_m(RefPtr<nsIDOMDataChannel> aChannel,
                                RefPtr<PeerConnectionObserver> aObserver)
{
  MOZ_ASSERT(NS_IsMainThread());
  JSErrorResult rv;
  RefPtr<nsDOMDataChannel> channel = static_cast<nsDOMDataChannel*>(&*aChannel);
  aObserver->NotifyDataChannel(*channel, rv);
  NS_DataChannelAppReady(aChannel);
}
#endif

void
PeerConnectionImpl::NotifyDataChannel(already_AddRefed<DataChannel> aChannel)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  // XXXkhuey this is completely fucked up.  We can't use RefPtr<DataChannel>
  // here because DataChannel's AddRef/Release are non-virtual and not visible
  // if !MOZILLA_INTERNAL_API, but this function leaks the DataChannel if
  // !MOZILLA_INTERNAL_API because it never transfers the ref to
  // NS_NewDOMDataChannel.
  DataChannel* channel = aChannel.take();
  MOZ_ASSERT(channel);

  CSFLogDebug(logTag, "%s: channel: %p", __FUNCTION__, channel);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  nsCOMPtr<nsIDOMDataChannel> domchannel;
  nsresult rv = NS_NewDOMDataChannel(already_AddRefed<DataChannel>(channel),
                                     mWindow, getter_AddRefs(domchannel));
  NS_ENSURE_SUCCESS_VOID(rv);

  mHaveDataStream = true;

  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }

  RUN_ON_THREAD(mThread,
                WrapRunnableNM(NotifyDataChannel_m,
                               domchannel.get(),
                               pco),
                NS_DISPATCH_NORMAL);
#endif
}

NS_IMETHODIMP
PeerConnectionImpl::CreateOffer(const RTCOfferOptions& aOptions)
{
  JsepOfferOptions options;
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // convert the RTCOfferOptions to JsepOfferOptions
  if (aOptions.mOfferToReceiveAudio.WasPassed()) {
    options.mOfferToReceiveAudio =
      mozilla::Some(size_t(aOptions.mOfferToReceiveAudio.Value()));
  }

  if (aOptions.mOfferToReceiveVideo.WasPassed()) {
    options.mOfferToReceiveVideo =
        mozilla::Some(size_t(aOptions.mOfferToReceiveVideo.Value()));
  }

  options.mIceRestart = mozilla::Some(aOptions.mIceRestart);

  if (aOptions.mMozDontOfferDataChannel.WasPassed()) {
    options.mDontOfferDataChannel =
      mozilla::Some(aOptions.mMozDontOfferDataChannel.Value());
  }
#endif
  return CreateOffer(options);
}

static void DeferredCreateOffer(const std::string& aPcHandle,
                                const JsepOfferOptions& aOptions) {
  PeerConnectionWrapper wrapper(aPcHandle);

  if (wrapper.impl()) {
    if (!PeerConnectionCtx::GetInstance()->isReady()) {
      MOZ_CRASH("Why is DeferredCreateOffer being executed when the "
                "PeerConnectionCtx isn't ready?");
    }
    wrapper.impl()->CreateOffer(aOptions);
  }
}

// Used by unit tests and the IDL CreateOffer.
NS_IMETHODIMP
PeerConnectionImpl::CreateOffer(const JsepOfferOptions& aOptions)
{
  PC_AUTO_ENTER_API_CALL(true);
  bool restartIce = aOptions.mIceRestart.isSome() && *(aOptions.mIceRestart);
  if (!restartIce &&
      mMedia->GetIceRestartState() ==
          PeerConnectionMedia::ICE_RESTART_PROVISIONAL) {
    RollbackIceRestart();
  }

  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }

  if (!PeerConnectionCtx::GetInstance()->isReady()) {
    // Uh oh. We're not ready yet. Enqueue this operation.
    PeerConnectionCtx::GetInstance()->queueJSEPOperation(
        WrapRunnableNM(DeferredCreateOffer, mHandle, aOptions));
    STAMP_TIMECARD(mTimeCard, "Deferring CreateOffer (not ready)");
    return NS_OK;
  }

  CSFLogDebug(logTag, "CreateOffer()");

  nsresult nrv;
  if (restartIce && !mJsepSession->GetLocalDescription().empty()) {
    // If restart is requested and a restart is already in progress, we
    // need to make room for the restart request so we either rollback
    // or finalize to "clear" the previous restart.
    if (mMedia->GetIceRestartState() ==
            PeerConnectionMedia::ICE_RESTART_PROVISIONAL) {
      // we're mid-restart and can rollback
      RollbackIceRestart();
    } else if (mMedia->GetIceRestartState() ==
                   PeerConnectionMedia::ICE_RESTART_COMMITTED) {
      // we're mid-restart and can't rollback, finalize restart even
      // though we're not really ready yet
      FinalizeIceRestart();
    }

    CSFLogInfo(logTag, "Offerer restarting ice");
    nrv = SetupIceRestart();
    if (NS_FAILED(nrv)) {
      CSFLogError(logTag, "%s: SetupIceRestart failed, res=%u",
                           __FUNCTION__,
                           static_cast<unsigned>(nrv));
      return nrv;
    }
  }

  nrv = ConfigureJsepSessionCodecs();
  if (NS_FAILED(nrv)) {
    CSFLogError(logTag, "Failed to configure codecs");
    return nrv;
  }

  STAMP_TIMECARD(mTimeCard, "Create Offer");

  std::string offer;

  nrv = mJsepSession->CreateOffer(aOptions, &offer);
  JSErrorResult rv;
  if (NS_FAILED(nrv)) {
    Error error;
    switch (nrv) {
      case NS_ERROR_UNEXPECTED:
        error = kInvalidState;
        break;
      default:
        error = kInternalError;
    }
    std::string errorString = mJsepSession->GetLastError();

    CSFLogError(logTag, "%s: pc = %s, error = %s",
                __FUNCTION__, mHandle.c_str(), errorString.c_str());
    pco->OnCreateOfferError(error, ObString(errorString.c_str()), rv);
  } else {
    pco->OnCreateOfferSuccess(ObString(offer.c_str()), rv);
  }

  UpdateSignalingState();
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CreateAnswer()
{
  PC_AUTO_ENTER_API_CALL(true);

  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }

  CSFLogDebug(logTag, "CreateAnswer()");

  nsresult nrv;
  if (mJsepSession->RemoteIceIsRestarting()) {
    if (mMedia->GetIceRestartState() ==
            PeerConnectionMedia::ICE_RESTART_COMMITTED) {
      FinalizeIceRestart();
    } else if (!mMedia->IsIceRestarting()) {
      CSFLogInfo(logTag, "Answerer restarting ice");
      nrv = SetupIceRestart();
      if (NS_FAILED(nrv)) {
        CSFLogError(logTag, "%s: SetupIceRestart failed, res=%u",
                             __FUNCTION__,
                             static_cast<unsigned>(nrv));
        return nrv;
      }
    }
  }

  STAMP_TIMECARD(mTimeCard, "Create Answer");
  // TODO(bug 1098015): Once RTCAnswerOptions is standardized, we'll need to
  // add it as a param to CreateAnswer, and convert it here.
  JsepAnswerOptions options;
  std::string answer;

  nrv = mJsepSession->CreateAnswer(options, &answer);
  JSErrorResult rv;
  if (NS_FAILED(nrv)) {
    Error error;
    switch (nrv) {
      case NS_ERROR_UNEXPECTED:
        error = kInvalidState;
        break;
      default:
        error = kInternalError;
    }
    std::string errorString = mJsepSession->GetLastError();

    CSFLogError(logTag, "%s: pc = %s, error = %s",
                __FUNCTION__, mHandle.c_str(), errorString.c_str());
    pco->OnCreateAnswerError(error, ObString(errorString.c_str()), rv);
  } else {
    pco->OnCreateAnswerSuccess(ObString(answer.c_str()), rv);
  }

  UpdateSignalingState();

  return NS_OK;
}

nsresult
PeerConnectionImpl::SetupIceRestart()
{
  if (mMedia->IsIceRestarting()) {
    CSFLogError(logTag, "%s: ICE already restarting",
                         __FUNCTION__);
    return NS_ERROR_UNEXPECTED;
  }

  std::string ufrag = mMedia->ice_ctx()->GetNewUfrag();
  std::string pwd = mMedia->ice_ctx()->GetNewPwd();
  if (ufrag.empty() || pwd.empty()) {
    CSFLogError(logTag, "%s: Bad ICE credentials (ufrag:'%s'/pwd:'%s')",
                         __FUNCTION__,
                         ufrag.c_str(), pwd.c_str());
    return NS_ERROR_UNEXPECTED;
  }

  // hold on to the current ice creds in case of rollback
  mPreviousIceUfrag = mJsepSession->GetUfrag();
  mPreviousIcePwd = mJsepSession->GetPwd();
  mMedia->BeginIceRestart(ufrag, pwd);

  nsresult nrv = mJsepSession->SetIceCredentials(ufrag, pwd);
  if (NS_FAILED(nrv)) {
    CSFLogError(logTag, "%s: Couldn't set ICE credentials, res=%u",
                         __FUNCTION__,
                         static_cast<unsigned>(nrv));
    return nrv;
  }

  return NS_OK;
}

nsresult
PeerConnectionImpl::RollbackIceRestart()
{
  mMedia->RollbackIceRestart();
  // put back the previous ice creds
  nsresult nrv = mJsepSession->SetIceCredentials(mPreviousIceUfrag,
                                                 mPreviousIcePwd);
  if (NS_FAILED(nrv)) {
    CSFLogError(logTag, "%s: Couldn't set ICE credentials, res=%u",
                         __FUNCTION__,
                         static_cast<unsigned>(nrv));
    return nrv;
  }
  mPreviousIceUfrag = "";
  mPreviousIcePwd = "";

  return NS_OK;
}

void
PeerConnectionImpl::FinalizeIceRestart()
{
  mMedia->FinalizeIceRestart();
  // clear the previous ice creds since they are no longer needed
  mPreviousIceUfrag = "";
  mPreviousIcePwd = "";
}

NS_IMETHODIMP
PeerConnectionImpl::SetLocalDescription(int32_t aAction, const char* aSDP)
{
  PC_AUTO_ENTER_API_CALL(true);

  if (!aSDP) {
    CSFLogError(logTag, "%s - aSDP is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSErrorResult rv;
  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }

  STAMP_TIMECARD(mTimeCard, "Set Local Description");

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  bool isolated = mMedia->AnyLocalTrackHasPeerIdentity();
  mPrivacyRequested = mPrivacyRequested || isolated;
#endif

  mLocalRequestedSDP = aSDP;

  JsepSdpType sdpType;
  switch (aAction) {
    case IPeerConnection::kActionOffer:
      sdpType = mozilla::kJsepSdpOffer;
      break;
    case IPeerConnection::kActionAnswer:
      sdpType = mozilla::kJsepSdpAnswer;
      break;
    case IPeerConnection::kActionPRAnswer:
      sdpType = mozilla::kJsepSdpPranswer;
      break;
    case IPeerConnection::kActionRollback:
      sdpType = mozilla::kJsepSdpRollback;
      break;
    default:
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;

  }
  nsresult nrv = mJsepSession->SetLocalDescription(sdpType,
                                                   mLocalRequestedSDP);
  if (NS_FAILED(nrv)) {
    Error error;
    switch (nrv) {
      case NS_ERROR_INVALID_ARG:
        error = kInvalidSessionDescription;
        break;
      case NS_ERROR_UNEXPECTED:
        error = kInvalidState;
        break;
      default:
        error = kInternalError;
    }

    std::string errorString = mJsepSession->GetLastError();
    CSFLogError(logTag, "%s: pc = %s, error = %s",
                __FUNCTION__, mHandle.c_str(), errorString.c_str());
    pco->OnSetLocalDescriptionError(error, ObString(errorString.c_str()), rv);
  } else {
    pco->OnSetLocalDescriptionSuccess(rv);
  }

  UpdateSignalingState(sdpType == mozilla::kJsepSdpRollback);
  return NS_OK;
}

static void DeferredSetRemote(const std::string& aPcHandle,
                              int32_t aAction,
                              const std::string& aSdp) {
  PeerConnectionWrapper wrapper(aPcHandle);

  if (wrapper.impl()) {
    if (!PeerConnectionCtx::GetInstance()->isReady()) {
      MOZ_CRASH("Why is DeferredSetRemote being executed when the "
                "PeerConnectionCtx isn't ready?");
    }
    wrapper.impl()->SetRemoteDescription(aAction, aSdp.c_str());
  }
}

static void StartTrack(MediaStream* aSource,
                       TrackID aTrackId,
                       nsAutoPtr<MediaSegment>&& aSegment) {
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  class Message : public ControlMessage {
   public:
    Message(MediaStream* aStream,
            TrackID aTrack,
            nsAutoPtr<MediaSegment>&& aSegment)
      : ControlMessage(aStream),
        track_id_(aTrack),
        segment_(aSegment) {}

    virtual void Run() override {
      TrackRate track_rate = segment_->GetType() == MediaSegment::AUDIO ?
        WEBRTC_DEFAULT_SAMPLE_RATE : mStream->GraphRate();
      StreamTime current_end = mStream->GetTracksEnd();
      TrackTicks current_ticks =
        mStream->TimeToTicksRoundUp(track_rate, current_end);

      // Add a track 'now' to avoid possible underrun, especially if we add
      // a track "later".

      if (current_end != 0L) {
        CSFLogDebug(logTag, "added track @ %u -> %f",
                    static_cast<unsigned>(current_end),
                    mStream->StreamTimeToSeconds(current_end));
      }

      // To avoid assertions, we need to insert a dummy segment that covers up
      // to the "start" time for the track
      segment_->AppendNullData(current_ticks);
      if (segment_->GetType() == MediaSegment::AUDIO) {
        mStream->AsSourceStream()->AddAudioTrack(
            track_id_,
            WEBRTC_DEFAULT_SAMPLE_RATE,
            0,
            static_cast<AudioSegment*>(segment_.forget()));
      } else {
        mStream->AsSourceStream()->AddTrack(track_id_, 0, segment_.forget());
      }
    }
   private:
    TrackID track_id_;
    nsAutoPtr<MediaSegment> segment_;
  };

  aSource->GraphImpl()->AppendMessage(
      MakeUnique<Message>(aSource, aTrackId, Move(aSegment)));
  CSFLogInfo(logTag, "Dispatched track-add for track id %u on stream %p",
             aTrackId, aSource);
#endif
}


nsresult
PeerConnectionImpl::CreateNewRemoteTracks(RefPtr<PeerConnectionObserver>& aPco)
{
  JSErrorResult jrv;

  std::vector<RefPtr<JsepTrack>> newTracks =
    mJsepSession->GetRemoteTracksAdded();

  // Group new tracks by stream id
  std::map<std::string, std::vector<RefPtr<JsepTrack>>> tracksByStreamId;
  for (auto i = newTracks.begin(); i != newTracks.end(); ++i) {
    RefPtr<JsepTrack> track = *i;

    if (track->GetMediaType() == mozilla::SdpMediaSection::kApplication) {
      // Ignore datachannel
      continue;
    }

    tracksByStreamId[track->GetStreamId()].push_back(track);
  }

  for (auto i = tracksByStreamId.begin(); i != tracksByStreamId.end(); ++i) {
    std::string streamId = i->first;
    std::vector<RefPtr<JsepTrack>>& tracks = i->second;

    bool newStream = false;
    RefPtr<RemoteSourceStreamInfo> info =
      mMedia->GetRemoteStreamById(streamId);
    if (!info) {
      newStream = true;
      nsresult nrv = CreateRemoteSourceStreamInfo(&info, streamId);
      if (NS_FAILED(nrv)) {
        aPco->OnSetRemoteDescriptionError(
            kInternalError,
            ObString("CreateRemoteSourceStreamInfo failed"),
            jrv);
        return nrv;
      }

      nrv = mMedia->AddRemoteStream(info);
      if (NS_FAILED(nrv)) {
        aPco->OnSetRemoteDescriptionError(
            kInternalError,
            ObString("AddRemoteStream failed"),
            jrv);
        return nrv;
      }

      CSFLogDebug(logTag, "Added remote stream %s", info->GetId().c_str());

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
      info->GetMediaStream()->AssignId(NS_ConvertUTF8toUTF16(streamId.c_str()));
      info->GetMediaStream()->SetLogicalStreamStartTime(
          info->GetMediaStream()->GetPlaybackStream()->GetCurrentTime());
#else
      info->GetMediaStream()->AssignId((streamId));
#endif
    }

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
    Sequence<OwningNonNull<DOMMediaStream>> streams;
    if (!streams.AppendElement(OwningNonNull<DOMMediaStream>(
            *info->GetMediaStream()),
            fallible)) {
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
    }

    // Set the principal used for creating the tracks. This makes the stream
    // data (audio/video samples) accessible to the receiving page. We're
    // only certain that privacy hasn't been requested if we're connected.
    nsCOMPtr<nsIPrincipal> principal;
    nsIDocument* doc = GetWindow()->GetExtantDoc();
    MOZ_ASSERT(doc);
    if (mDtlsConnected && !PrivacyRequested()) {
      principal = doc->NodePrincipal();
    } else {
      // we're either certain that we need isolation for the streams, OR
      // we're not sure and we can fix the stream in SetDtlsConnected
      principal =  nsNullPrincipal::CreateWithInheritedAttributes(doc->NodePrincipal());
    }
#endif

    // We need to select unique ids, just use max + 1
    TrackID maxTrackId = 0;
    {
      nsTArray<RefPtr<dom::MediaStreamTrack>> domTracks;
      info->GetMediaStream()->GetTracks(domTracks);
      for (auto& track : domTracks) {
        maxTrackId = std::max(maxTrackId, track->mTrackID);
      }
    }

    for (RefPtr<JsepTrack>& track : tracks) {
      std::string webrtcTrackId(track->GetTrackId());
      if (!info->HasTrack(webrtcTrackId)) {
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
        RefPtr<RemoteTrackSource> source =
          new RemoteTrackSource(principal, nsString());
#else
        RefPtr<MediaStreamTrackSource> source = new MediaStreamTrackSource();
#endif
        TrackID trackID = ++maxTrackId;
        RefPtr<MediaStreamTrack> domTrack;
        nsAutoPtr<MediaSegment> segment;
        if (track->GetMediaType() == SdpMediaSection::kAudio) {
          domTrack =
            info->GetMediaStream()->CreateDOMTrack(trackID,
                                                   MediaSegment::AUDIO,
                                                   source);
          segment = new AudioSegment;
        } else {
          domTrack =
            info->GetMediaStream()->CreateDOMTrack(trackID,
                                                   MediaSegment::VIDEO,
                                                   source);
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
          segment = new VideoSegment;
#endif
        }

        StartTrack(info->GetMediaStream()->GetInputStream()->AsSourceStream(),
                   trackID, Move(segment));
        info->AddTrack(webrtcTrackId, domTrack);
        CSFLogDebug(logTag, "Added remote track %s/%s",
                    info->GetId().c_str(), webrtcTrackId.c_str());

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
        domTrack->AssignId(NS_ConvertUTF8toUTF16(webrtcTrackId.c_str()));
        aPco->OnAddTrack(*domTrack, streams, jrv);
        if (jrv.Failed()) {
          CSFLogError(logTag, ": OnAddTrack(%s) failed! Error: %u",
                      webrtcTrackId.c_str(),
                      jrv.ErrorCodeAsInt());
        }
#endif
      }
    }

    if (newStream) {
      aPco->OnAddStream(*info->GetMediaStream(), jrv);
      if (jrv.Failed()) {
        CSFLogError(logTag, ": OnAddStream() failed! Error: %u",
                    jrv.ErrorCodeAsInt());
      }
    }
  }
  return NS_OK;
}

void
PeerConnectionImpl::RemoveOldRemoteTracks(RefPtr<PeerConnectionObserver>& aPco)
{
  JSErrorResult jrv;

  std::vector<RefPtr<JsepTrack>> removedTracks =
    mJsepSession->GetRemoteTracksRemoved();

  for (auto i = removedTracks.begin(); i != removedTracks.end(); ++i) {
    const std::string& streamId = (*i)->GetStreamId();
    const std::string& trackId = (*i)->GetTrackId();

    RefPtr<RemoteSourceStreamInfo> info = mMedia->GetRemoteStreamById(streamId);
    if (!info) {
      MOZ_ASSERT(false, "A stream/track was removed that wasn't in PCMedia. "
                        "This is a bug.");
      continue;
    }

    mMedia->RemoveRemoteTrack(streamId, trackId);

    DOMMediaStream* stream = info->GetMediaStream();
    nsTArray<RefPtr<MediaStreamTrack>> tracks;
    stream->GetTracks(tracks);
    for (auto& track : tracks) {
      if (PeerConnectionImpl::GetTrackId(*track) == trackId) {
        aPco->OnRemoveTrack(*track, jrv);
        break;
      }
    }

    // We might be holding the last ref, but that's ok.
    if (!info->GetTrackCount()) {
      aPco->OnRemoveStream(*stream, jrv);
    }
  }
}

NS_IMETHODIMP
PeerConnectionImpl::SetRemoteDescription(int32_t action, const char* aSDP)
{
  PC_AUTO_ENTER_API_CALL(true);

  if (!aSDP) {
    CSFLogError(logTag, "%s - aSDP is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSErrorResult jrv;
  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }

  if (action == IPeerConnection::kActionOffer) {
    if (!PeerConnectionCtx::GetInstance()->isReady()) {
      // Uh oh. We're not ready yet. Enqueue this operation. (This must be a
      // remote offer, or else we would not have gotten this far)
      PeerConnectionCtx::GetInstance()->queueJSEPOperation(
          WrapRunnableNM(DeferredSetRemote,
            mHandle,
            action,
            std::string(aSDP)));
      STAMP_TIMECARD(mTimeCard, "Deferring SetRemote (not ready)");
      return NS_OK;
    }

    nsresult nrv = ConfigureJsepSessionCodecs();
    if (NS_FAILED(nrv)) {
      CSFLogError(logTag, "Failed to configure codecs");
      return nrv;
    }
  }

  STAMP_TIMECARD(mTimeCard, "Set Remote Description");

  mRemoteRequestedSDP = aSDP;
  JsepSdpType sdpType;
  switch (action) {
    case IPeerConnection::kActionOffer:
      sdpType = mozilla::kJsepSdpOffer;
      break;
    case IPeerConnection::kActionAnswer:
      sdpType = mozilla::kJsepSdpAnswer;
      break;
    case IPeerConnection::kActionPRAnswer:
      sdpType = mozilla::kJsepSdpPranswer;
      break;
    case IPeerConnection::kActionRollback:
      sdpType = mozilla::kJsepSdpRollback;
      break;
    default:
      MOZ_ASSERT(false);
      return NS_ERROR_FAILURE;
  }

  nsresult nrv = mJsepSession->SetRemoteDescription(sdpType,
                                                    mRemoteRequestedSDP);
  if (NS_FAILED(nrv)) {
    Error error;
    switch (nrv) {
      case NS_ERROR_INVALID_ARG:
        error = kInvalidSessionDescription;
        break;
      case NS_ERROR_UNEXPECTED:
        error = kInvalidState;
        break;
      default:
        error = kInternalError;
    }

    std::string errorString = mJsepSession->GetLastError();
    CSFLogError(logTag, "%s: pc = %s, error = %s",
                __FUNCTION__, mHandle.c_str(), errorString.c_str());
    pco->OnSetRemoteDescriptionError(error, ObString(errorString.c_str()), jrv);
  } else {
    nrv = CreateNewRemoteTracks(pco);
    if (NS_FAILED(nrv)) {
      // aPco was already notified, just return early.
      return NS_OK;
    }

    RemoveOldRemoteTracks(pco);

    pco->OnSetRemoteDescriptionSuccess(jrv);
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
    startCallTelem();
#endif
  }

  UpdateSignalingState(sdpType == mozilla::kJsepSdpRollback);
  return NS_OK;
}

// WebRTC uses highres time relative to the UNIX epoch (Jan 1, 1970, UTC).

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
nsresult
PeerConnectionImpl::GetTimeSinceEpoch(DOMHighResTimeStamp *result) {
  MOZ_ASSERT(NS_IsMainThread());
  Performance *perf = mWindow->GetPerformance();
  NS_ENSURE_TRUE(perf && perf->Timing(), NS_ERROR_UNEXPECTED);
  *result = perf->Now() + perf->Timing()->NavigationStart();
  return NS_OK;
}

class RTCStatsReportInternalConstruct : public RTCStatsReportInternal {
public:
  RTCStatsReportInternalConstruct(const nsString &pcid, DOMHighResTimeStamp now) {
    mPcid = pcid;
    mInboundRTPStreamStats.Construct();
    mOutboundRTPStreamStats.Construct();
    mMediaStreamTrackStats.Construct();
    mMediaStreamStats.Construct();
    mTransportStats.Construct();
    mIceComponentStats.Construct();
    mIceCandidatePairStats.Construct();
    mIceCandidateStats.Construct();
    mCodecStats.Construct();
    mTimestamp.Construct(now);
  }
};
#endif

NS_IMETHODIMP
PeerConnectionImpl::GetStats(MediaStreamTrack *aSelector) {
  PC_AUTO_ENTER_API_CALL(true);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (!mMedia) {
    // Since we zero this out before the d'tor, we should check.
    return NS_ERROR_UNEXPECTED;
  }

  nsAutoPtr<RTCStatsQuery> query(new RTCStatsQuery(false));

  nsresult rv = BuildStatsQuery_m(aSelector, query.get());

  NS_ENSURE_SUCCESS(rv, rv);

  RUN_ON_THREAD(mSTSThread,
                WrapRunnableNM(&PeerConnectionImpl::GetStatsForPCObserver_s,
                               mHandle,
                               query),
                NS_DISPATCH_NORMAL);
#endif
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::AddIceCandidate(const char* aCandidate, const char* aMid, unsigned short aLevel) {
  PC_AUTO_ENTER_API_CALL(true);

  JSErrorResult rv;
  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }

  STAMP_TIMECARD(mTimeCard, "Add Ice Candidate");

  CSFLogDebug(logTag, "AddIceCandidate: %s", aCandidate);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // When remote candidates are added before our ICE ctx is up and running
  // (the transition to New is async through STS, so this is not impossible),
  // we won't record them as trickle candidates. Is this what we want?
  if(!mIceStartTime.IsNull()) {
    TimeDuration timeDelta = TimeStamp::Now() - mIceStartTime;
    if (mIceConnectionState == PCImplIceConnectionState::Failed) {
      Telemetry::Accumulate((mIsLoop ?
                             Telemetry::LOOP_ICE_LATE_TRICKLE_ARRIVAL_TIME :
                             Telemetry::WEBRTC_ICE_LATE_TRICKLE_ARRIVAL_TIME),
                            timeDelta.ToMilliseconds());
    } else {
      Telemetry::Accumulate((mIsLoop ?
                             Telemetry::LOOP_ICE_ON_TIME_TRICKLE_ARRIVAL_TIME :
                             Telemetry::WEBRTC_ICE_ON_TIME_TRICKLE_ARRIVAL_TIME),
                            timeDelta.ToMilliseconds());
    }
  }
#endif

  nsresult res = mJsepSession->AddRemoteIceCandidate(aCandidate, aMid, aLevel);

  if (NS_SUCCEEDED(res)) {
    // We do not bother PCMedia about this before offer/answer concludes.
    // Once offer/answer concludes, PCMedia will extract these candidates from
    // the remote SDP.
    if (mSignalingState == PCImplSignalingState::SignalingStable) {
      mMedia->AddIceCandidate(aCandidate, aMid, aLevel);
    }
    pco->OnAddIceCandidateSuccess(rv);
  } else {
    ++mAddCandidateErrorCount;
    Error error;
    switch (res) {
      case NS_ERROR_UNEXPECTED:
        error = kInvalidState;
        break;
      case NS_ERROR_INVALID_ARG:
        error = kInvalidCandidate;
        break;
      default:
        error = kInternalError;
    }

    std::string errorString = mJsepSession->GetLastError();

    CSFLogError(logTag, "Failed to incorporate remote candidate into SDP:"
                        " res = %u, candidate = %s, level = %u, error = %s",
                        static_cast<unsigned>(res),
                        aCandidate,
                        static_cast<unsigned>(aLevel),
                        errorString.c_str());

    pco->OnAddIceCandidateError(error, ObString(errorString.c_str()), rv);
  }

  UpdateSignalingState();
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CloseStreams() {
  PC_AUTO_ENTER_API_CALL(false);

  return NS_OK;
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
nsresult
PeerConnectionImpl::SetPeerIdentity(const nsAString& aPeerIdentity)
{
  PC_AUTO_ENTER_API_CALL(true);
  MOZ_ASSERT(!aPeerIdentity.IsEmpty());

  // once set, this can't be changed
  if (mPeerIdentity) {
    if (!mPeerIdentity->Equals(aPeerIdentity)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    mPeerIdentity = new PeerIdentity(aPeerIdentity);
    nsIDocument* doc = GetWindow()->GetExtantDoc();
    if (!doc) {
      CSFLogInfo(logTag, "Can't update principal on streams; document gone");
      return NS_ERROR_FAILURE;
    }
    MediaStreamTrack* allTracks = nullptr;
    mMedia->UpdateSinkIdentity_m(allTracks, doc->NodePrincipal(), mPeerIdentity);
  }
  return NS_OK;
}
#endif

nsresult
PeerConnectionImpl::SetDtlsConnected(bool aPrivacyRequested)
{
  PC_AUTO_ENTER_API_CALL(false);

  // For this, as with mPrivacyRequested, once we've connected to a peer, we
  // fixate on that peer.  Dealing with multiple peers or connections is more
  // than this run-down wreck of an object can handle.
  // Besides, this is only used to say if we have been connected ever.
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (!mPrivacyRequested && !aPrivacyRequested && !mDtlsConnected) {
    // now we know that privacy isn't needed for sure
    nsIDocument* doc = GetWindow()->GetExtantDoc();
    if (!doc) {
      CSFLogInfo(logTag, "Can't update principal on streams; document gone");
      return NS_ERROR_FAILURE;
    }
    mMedia->UpdateRemoteStreamPrincipals_m(doc->NodePrincipal());
  }
#endif
  mDtlsConnected = true;
  mPrivacyRequested = mPrivacyRequested || aPrivacyRequested;
  return NS_OK;
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
void
PeerConnectionImpl::PrincipalChanged(MediaStreamTrack* aTrack) {
  nsIDocument* doc = GetWindow()->GetExtantDoc();
  if (doc) {
    mMedia->UpdateSinkIdentity_m(aTrack, doc->NodePrincipal(), mPeerIdentity);
  } else {
    CSFLogInfo(logTag, "Can't update sink principal; document gone");
  }
}
#endif

std::string
PeerConnectionImpl::GetTrackId(const MediaStreamTrack& aTrack)
{
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  nsString wideTrackId;
  aTrack.GetId(wideTrackId);
  return NS_ConvertUTF16toUTF8(wideTrackId).get();
#else
  return aTrack.GetId();
#endif
}

std::string
PeerConnectionImpl::GetStreamId(const DOMMediaStream& aStream)
{
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  nsString wideStreamId;
  aStream.GetId(wideStreamId);
  return NS_ConvertUTF16toUTF8(wideStreamId).get();
#else
  return aStream.GetId();
#endif
}

void
PeerConnectionImpl::OnMediaError(const std::string& aError)
{
  CSFLogError(logTag, "Encountered media error! %s", aError.c_str());
  // TODO: Let content know about this somehow.
}

nsresult
PeerConnectionImpl::AddTrack(MediaStreamTrack& aTrack,
                             const Sequence<OwningNonNull<DOMMediaStream>>& aStreams)
{
  PC_AUTO_ENTER_API_CALL(true);

  if (!aStreams.Length()) {
    CSFLogError(logTag, "%s: At least one stream arg required", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  return AddTrack(aTrack, aStreams[0]);
}

nsresult
PeerConnectionImpl::AddTrack(MediaStreamTrack& aTrack,
                             DOMMediaStream& aMediaStream)
{
  std::string streamId = PeerConnectionImpl::GetStreamId(aMediaStream);
  std::string trackId = PeerConnectionImpl::GetTrackId(aTrack);
  nsresult res = mMedia->AddTrack(aMediaStream, streamId, aTrack, trackId);
  if (NS_FAILED(res)) {
    return res;
  }

  CSFLogDebug(logTag, "Added track (%s) to stream %s",
                      trackId.c_str(), streamId.c_str());

  aTrack.AddPrincipalChangeObserver(this);
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  PrincipalChanged(&aTrack);
#endif

  if (aTrack.AsAudioStreamTrack()) {
    res = AddTrackToJsepSession(SdpMediaSection::kAudio, streamId, trackId);
    if (NS_FAILED(res)) {
      return res;
    }
    mNumAudioStreams++;
  }

  if (aTrack.AsVideoStreamTrack()) {
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
    if (!Preferences::GetBool("media.peerconnection.video.enabled", true)) {
      // Before this code was moved, this would silently ignore just like it
      // does now. Is this actually what we want to do?
      return NS_OK;
    }
#endif

    res = AddTrackToJsepSession(SdpMediaSection::kVideo, streamId, trackId);
    if (NS_FAILED(res)) {
      return res;
    }
    mNumVideoStreams++;
  }
  OnNegotiationNeeded();
  return NS_OK;
}

nsresult
PeerConnectionImpl::SelectSsrc(MediaStreamTrack& aRecvTrack,
                               unsigned short aSsrcIndex)
{
  for (size_t i = 0; i < mMedia->RemoteStreamsLength(); ++i) {
    if (mMedia->GetRemoteStreamByIndex(i)->GetMediaStream()->
        HasTrack(aRecvTrack)) {
      auto& pipelines = mMedia->GetRemoteStreamByIndex(i)->GetPipelines();
      std::string trackId = PeerConnectionImpl::GetTrackId(aRecvTrack);
      auto it = pipelines.find(trackId);
      if (it != pipelines.end()) {
        it->second->SelectSsrc_m(aSsrcIndex);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::RemoveTrack(MediaStreamTrack& aTrack) {
  PC_AUTO_ENTER_API_CALL(true);

  std::string trackId = PeerConnectionImpl::GetTrackId(aTrack);
  RefPtr<LocalSourceStreamInfo> info = media()->GetLocalStreamByTrackId(trackId);

  if (!info) {
    CSFLogError(logTag, "%s: Unknown stream", __FUNCTION__);
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv =
    mJsepSession->RemoveTrack(info->GetId(), trackId);

  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "%s: Unknown stream/track ids %s %s",
                __FUNCTION__,
                info->GetId().c_str(),
                trackId.c_str());
    return rv;
  }

  media()->RemoveLocalTrack(info->GetId(), trackId);

  aTrack.RemovePrincipalChangeObserver(this);

  OnNegotiationNeeded();

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::ReplaceTrack(MediaStreamTrack& aThisTrack,
                                 MediaStreamTrack& aWithTrack) {
  PC_AUTO_ENTER_API_CALL(true);

  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_ERROR_UNEXPECTED;
  }
  JSErrorResult jrv;

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (&aThisTrack == &aWithTrack) {
    pco->OnReplaceTrackSuccess(jrv);
    if (jrv.Failed()) {
      CSFLogError(logTag, "Error firing replaceTrack success callback");
      return NS_ERROR_UNEXPECTED;
    }
    return NS_OK;
  }

  nsString thisKind;
  aThisTrack.GetKind(thisKind);
  nsString withKind;
  aWithTrack.GetKind(withKind);

  if (thisKind != withKind) {
    pco->OnReplaceTrackError(kIncompatibleMediaStreamTrack,
                             ObString(mJsepSession->GetLastError().c_str()),
                             jrv);
    if (jrv.Failed()) {
      CSFLogError(logTag, "Error firing replaceTrack success callback");
      return NS_ERROR_UNEXPECTED;
    }
    return NS_OK;
  }
#endif
  std::string origTrackId = PeerConnectionImpl::GetTrackId(aThisTrack);
  std::string newTrackId = PeerConnectionImpl::GetTrackId(aWithTrack);

  RefPtr<LocalSourceStreamInfo> info =
    media()->GetLocalStreamByTrackId(origTrackId);
  if (!info) {
    CSFLogError(logTag, "Could not find stream from trackId");
    return NS_ERROR_UNEXPECTED;
  }

  std::string origStreamId = info->GetId();
  std::string newStreamId =
    PeerConnectionImpl::GetStreamId(*aWithTrack.mOwningStream);

  nsresult rv = mJsepSession->ReplaceTrack(origStreamId,
                                           origTrackId,
                                           newStreamId,
                                           newTrackId);
  if (NS_FAILED(rv)) {
    pco->OnReplaceTrackError(kInvalidMediastreamTrack,
                             ObString(mJsepSession->GetLastError().c_str()),
                             jrv);
    if (jrv.Failed()) {
      CSFLogError(logTag, "Error firing replaceTrack error callback");
      return NS_ERROR_UNEXPECTED;
    }
    return NS_OK;
  }

  rv = media()->ReplaceTrack(origStreamId,
                             origTrackId,
                             aWithTrack,
                             newStreamId,
                             newTrackId);

  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "Unexpected error in ReplaceTrack: %d",
                        static_cast<int>(rv));
    pco->OnReplaceTrackError(kInvalidMediastreamTrack,
                             ObString("Failed to replace track"),
                             jrv);
    if (jrv.Failed()) {
      CSFLogError(logTag, "Error firing replaceTrack error callback");
      return NS_ERROR_UNEXPECTED;
    }
    return NS_OK;
  }
  aThisTrack.RemovePrincipalChangeObserver(this);
  aWithTrack.AddPrincipalChangeObserver(this);
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  PrincipalChanged(&aWithTrack);
#endif

  // We update the media pipelines here so we can apply different codec
  // settings for different sources (e.g. screensharing as opposed to camera.)
  // TODO: We should probably only do this if the source has in fact changed.
  mMedia->UpdateMediaPipelines(*mJsepSession);

  pco->OnReplaceTrackSuccess(jrv);
  if (jrv.Failed()) {
    CSFLogError(logTag, "Error firing replaceTrack success callback");
    return NS_ERROR_UNEXPECTED;
  }

  return NS_OK;
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
NS_IMETHODIMP
PeerConnectionImpl::SetParameters(MediaStreamTrack& aTrack,
                                  const RTCRtpParameters& aParameters) {
  PC_AUTO_ENTER_API_CALL(true);

  std::vector<JsepTrack::JsConstraints> constraints;
  if (aParameters.mEncodings.WasPassed()) {
    for (auto& encoding : aParameters.mEncodings.Value()) {
      JsepTrack::JsConstraints constraint;
      if (encoding.mRid.WasPassed()) {
        constraint.rid = NS_ConvertUTF16toUTF8(encoding.mRid.Value()).get();
      }
      if (encoding.mMaxBitrate.WasPassed()) {
        constraint.constraints.maxBr = encoding.mMaxBitrate.Value();
      }
      constraint.constraints.scaleDownBy = encoding.mScaleResolutionDownBy;
      constraints.push_back(constraint);
    }
  }
  return SetParameters(aTrack, constraints);
}
#endif

nsresult
PeerConnectionImpl::SetParameters(
    MediaStreamTrack& aTrack,
    const std::vector<JsepTrack::JsConstraints>& aConstraints)
{
  std::string trackId = PeerConnectionImpl::GetTrackId(aTrack);
  RefPtr<LocalSourceStreamInfo> info = media()->GetLocalStreamByTrackId(trackId);
  if (!info) {
    CSFLogError(logTag, "%s: Unknown stream", __FUNCTION__);
    return NS_ERROR_INVALID_ARG;
  }
  std::string streamId = info->GetId();

  return mJsepSession->SetParameters(streamId, trackId, aConstraints);
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
NS_IMETHODIMP
PeerConnectionImpl::GetParameters(MediaStreamTrack& aTrack,
                                  RTCRtpParameters& aOutParameters) {
  PC_AUTO_ENTER_API_CALL(true);

  std::vector<JsepTrack::JsConstraints> constraints;
  nsresult rv = GetParameters(aTrack, &constraints);
  if (NS_FAILED(rv)) {
    return rv;
  }
  aOutParameters.mEncodings.Construct();
  for (auto& constraint : constraints) {
    RTCRtpEncodingParameters encoding;
    encoding.mRid.Construct(NS_ConvertASCIItoUTF16(constraint.rid.c_str()));
    encoding.mMaxBitrate.Construct(constraint.constraints.maxBr);
    encoding.mScaleResolutionDownBy = constraint.constraints.scaleDownBy;
    aOutParameters.mEncodings.Value().AppendElement(Move(encoding), fallible);
  }
  return NS_OK;
}
#endif

nsresult
PeerConnectionImpl::GetParameters(
    MediaStreamTrack& aTrack,
    std::vector<JsepTrack::JsConstraints>* aOutConstraints)
{
  std::string trackId = PeerConnectionImpl::GetTrackId(aTrack);
  RefPtr<LocalSourceStreamInfo> info = media()->GetLocalStreamByTrackId(trackId);
  if (!info) {
    CSFLogError(logTag, "%s: Unknown stream", __FUNCTION__);
    return NS_ERROR_INVALID_ARG;
  }
  std::string streamId = info->GetId();

  return mJsepSession->GetParameters(streamId, trackId, aOutConstraints);
}

nsresult
PeerConnectionImpl::CalculateFingerprint(
    const std::string& algorithm,
    std::vector<uint8_t>* fingerprint) const {
  uint8_t buf[DtlsIdentity::HASH_ALGORITHM_MAX_LENGTH];
  size_t len = 0;
  CERTCertificate* cert;

  MOZ_ASSERT(fingerprint);
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  cert = mCertificate->Certificate();
#else
  cert = mIdentity->cert();
#endif
  nsresult rv = DtlsIdentity::ComputeFingerprint(cert, algorithm,
                                                 &buf[0], sizeof(buf),
                                                 &len);
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "Unable to calculate certificate fingerprint, rv=%u",
                        static_cast<unsigned>(rv));
    return rv;
  }
  MOZ_ASSERT(len > 0 && len <= DtlsIdentity::HASH_ALGORITHM_MAX_LENGTH);
  fingerprint->assign(buf, buf + len);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetFingerprint(char** fingerprint)
{
  MOZ_ASSERT(fingerprint);
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  MOZ_ASSERT(mCertificate);
#endif
  std::vector<uint8_t> fp;
  nsresult rv = CalculateFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM, &fp);
  NS_ENSURE_SUCCESS(rv, rv);
  std::ostringstream os;
  os << DtlsIdentity::DEFAULT_HASH_ALGORITHM << ' '
     << SdpFingerprintAttributeList::FormatFingerprint(fp);
  std::string fpStr = os.str();

  char* tmp = new char[fpStr.size() + 1];
  std::copy(fpStr.begin(), fpStr.end(), tmp);
  tmp[fpStr.size()] = '\0';

  *fingerprint = tmp;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetLocalDescription(char** aSDP)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aSDP);
  std::string localSdp = mJsepSession->GetLocalDescription();

  char* tmp = new char[localSdp.size() + 1];
  std::copy(localSdp.begin(), localSdp.end(), tmp);
  tmp[localSdp.size()] = '\0';

  *aSDP = tmp;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetRemoteDescription(char** aSDP)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aSDP);
  std::string remoteSdp = mJsepSession->GetRemoteDescription();

  char* tmp = new char[remoteSdp.size() + 1];
  std::copy(remoteSdp.begin(), remoteSdp.end(), tmp);
  tmp[remoteSdp.size()] = '\0';

  *aSDP = tmp;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::SignalingState(PCImplSignalingState* aState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mSignalingState;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::IceConnectionState(PCImplIceConnectionState* aState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mIceConnectionState;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::IceGatheringState(PCImplIceGatheringState* aState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mIceGatheringState;
  return NS_OK;
}

nsresult
PeerConnectionImpl::CheckApiState(bool assert_ice_ready) const
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(mTrickle || !assert_ice_ready ||
             (mIceGatheringState == PCImplIceGatheringState::Complete));

  if (IsClosed()) {
    CSFLogError(logTag, "%s: called API while closed", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  if (!mMedia) {
    CSFLogError(logTag, "%s: called API with disposed mMedia", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::Close()
{
  CSFLogDebug(logTag, "%s: for %s", __FUNCTION__, mHandle.c_str());
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  SetSignalingState_m(PCImplSignalingState::SignalingClosed);

  return NS_OK;
}

bool
PeerConnectionImpl::PluginCrash(uint32_t aPluginID,
                                const nsAString& aPluginName)
{
  // fire an event to the DOM window if this is "ours"
  bool result = mMedia ? mMedia->AnyCodecHasPluginID(aPluginID) : false;
  if (!result) {
    return false;
  }

  CSFLogError(logTag, "%s: Our plugin %llu crashed", __FUNCTION__, static_cast<unsigned long long>(aPluginID));

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  nsCOMPtr<nsIDocument> doc = mWindow->GetExtantDoc();
  if (!doc) {
    NS_WARNING("Couldn't get document for PluginCrashed event!");
    return true;
  }

  PluginCrashedEventInit init;
  init.mPluginID = aPluginID;
  init.mPluginName = aPluginName;
  init.mSubmittedCrashReport = false;
  init.mGmpPlugin = true;
  init.mBubbles = true;
  init.mCancelable = true;

  RefPtr<PluginCrashedEvent> event =
    PluginCrashedEvent::Constructor(doc, NS_LITERAL_STRING("PluginCrashed"), init);

  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;

  EventDispatcher::DispatchDOMEvent(mWindow, nullptr, event, nullptr, nullptr);
#endif

  return true;
}

void
PeerConnectionImpl::RecordEndOfCallTelemetry() const
{
  if (!mJsepSession) {
    return;
  }

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // Bitmask used for WEBRTC/LOOP_CALL_TYPE telemetry reporting
  static const uint32_t kAudioTypeMask = 1;
  static const uint32_t kVideoTypeMask = 2;
  static const uint32_t kDataChannelTypeMask = 4;

  // Report end-of-call Telemetry
  if (mJsepSession->GetNegotiations() > 0) {
    Telemetry::Accumulate(mIsLoop ? Telemetry::LOOP_RENEGOTIATIONS :
                          Telemetry::WEBRTC_RENEGOTIATIONS,
                          mJsepSession->GetNegotiations()-1);
  }
  Telemetry::Accumulate(mIsLoop ? Telemetry::LOOP_MAX_VIDEO_SEND_TRACK :
                        Telemetry::WEBRTC_MAX_VIDEO_SEND_TRACK,
                        mMaxSending[SdpMediaSection::MediaType::kVideo]);
  Telemetry::Accumulate(mIsLoop ? Telemetry::LOOP_MAX_VIDEO_RECEIVE_TRACK :
                        Telemetry::WEBRTC_MAX_VIDEO_RECEIVE_TRACK,
                        mMaxReceiving[SdpMediaSection::MediaType::kVideo]);
  Telemetry::Accumulate(mIsLoop ? Telemetry::LOOP_MAX_AUDIO_SEND_TRACK :
                        Telemetry::WEBRTC_MAX_AUDIO_SEND_TRACK,
                        mMaxSending[SdpMediaSection::MediaType::kAudio]);
  Telemetry::Accumulate(mIsLoop ? Telemetry::LOOP_MAX_AUDIO_RECEIVE_TRACK :
                        Telemetry::WEBRTC_MAX_AUDIO_RECEIVE_TRACK,
                        mMaxReceiving[SdpMediaSection::MediaType::kAudio]);
  // DataChannels appear in both Sending and Receiving
  Telemetry::Accumulate(mIsLoop ? Telemetry::LOOP_DATACHANNEL_NEGOTIATED :
                        Telemetry::WEBRTC_DATACHANNEL_NEGOTIATED,
                        mMaxSending[SdpMediaSection::MediaType::kApplication]);
  // Enumerated/bitmask: 1 = Audio, 2 = Video, 4 = DataChannel
  // A/V = 3, A/V/D = 7, etc
  uint32_t type = 0;
  if (mMaxSending[SdpMediaSection::MediaType::kAudio] ||
      mMaxReceiving[SdpMediaSection::MediaType::kAudio]) {
    type = kAudioTypeMask;
  }
  if (mMaxSending[SdpMediaSection::MediaType::kVideo] ||
      mMaxReceiving[SdpMediaSection::MediaType::kVideo]) {
    type |= kVideoTypeMask;
  }
  if (mMaxSending[SdpMediaSection::MediaType::kApplication]) {
    type |= kDataChannelTypeMask;
  }
  Telemetry::Accumulate(mIsLoop ? Telemetry::LOOP_CALL_TYPE :
                        Telemetry::WEBRTC_CALL_TYPE,
                        type);
#endif
}

nsresult
PeerConnectionImpl::CloseInt()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  // We do this at the end of the call because we want to make sure we've waited
  // for all trickle ICE candidates to come in; this can happen well after we've
  // transitioned to connected. As a bonus, this allows us to detect race
  // conditions where a stats dispatch happens right as the PC closes.
  if (!mPrivateWindow) {
    RecordLongtermICEStatistics();
  }
  RecordEndOfCallTelemetry();
  CSFLogInfo(logTag, "%s: Closing PeerConnectionImpl %s; "
             "ending call", __FUNCTION__, mHandle.c_str());
  if (mJsepSession) {
    mJsepSession->Close();
  }
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (mDataConnection) {
    CSFLogInfo(logTag, "%s: Destroying DataChannelConnection %p for %s",
               __FUNCTION__, (void *) mDataConnection.get(), mHandle.c_str());
    mDataConnection->Destroy();
    mDataConnection = nullptr; // it may not go away until the runnables are dead
  }
#endif
  ShutdownMedia();

  // DataConnection will need to stay alive until all threads/runnables exit

  return NS_OK;
}

void
PeerConnectionImpl::ShutdownMedia()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  if (!mMedia)
    return;

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // before we destroy references to local tracks, detach from them
  for(uint32_t i = 0; i < media()->LocalStreamsLength(); ++i) {
    LocalSourceStreamInfo *info = media()->GetLocalStreamByIndex(i);
    for (const auto& pair : info->GetMediaStreamTracks()) {
      pair.second->RemovePrincipalChangeObserver(this);
    }
  }

  // End of call to be recorded in Telemetry
  if (!mStartTime.IsNull()){
    TimeDuration timeDelta = TimeStamp::Now() - mStartTime;
    Telemetry::Accumulate(mIsLoop ? Telemetry::LOOP_CALL_DURATION :
                                    Telemetry::WEBRTC_CALL_DURATION,
                          timeDelta.ToSeconds());
  }
#endif

  // Forget the reference so that we can transfer it to
  // SelfDestruct().
  mMedia.forget().take()->SelfDestruct();
}

void
PeerConnectionImpl::SetSignalingState_m(PCImplSignalingState aSignalingState,
                                        bool rollback)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  if (mSignalingState == aSignalingState ||
      mSignalingState == PCImplSignalingState::SignalingClosed) {
    return;
  }

  if (aSignalingState == PCImplSignalingState::SignalingHaveLocalOffer ||
      (aSignalingState == PCImplSignalingState::SignalingStable &&
       mSignalingState == PCImplSignalingState::SignalingHaveRemoteOffer &&
       !rollback)) {
    mMedia->EnsureTransports(*mJsepSession);
  }

  mSignalingState = aSignalingState;

  bool fireNegotiationNeeded = false;
  if (mSignalingState == PCImplSignalingState::SignalingStable) {
    if (mMedia->GetIceRestartState() ==
            PeerConnectionMedia::ICE_RESTART_PROVISIONAL) {
      if (rollback) {
        RollbackIceRestart();
      } else {
        mMedia->CommitIceRestart();
      }
    }

    // Either negotiation is done, or we've rolled back. In either case, we
    // need to re-evaluate whether further negotiation is required.
    mNegotiationNeeded = false;
    // If we're rolling back a local offer, we might need to remove some
    // transports, but nothing further needs to be done.
    mMedia->ActivateOrRemoveTransports(*mJsepSession);
    if (!rollback) {
      mMedia->UpdateMediaPipelines(*mJsepSession);
      InitializeDataChannel();
      mMedia->StartIceChecks(*mJsepSession);
    }

    if (!mJsepSession->AllLocalTracksAreAssigned()) {
      CSFLogInfo(logTag, "Not all local tracks were assigned to an "
                 "m-section, either because the offerer did not offer"
                 " to receive enough tracks, or because tracks were "
                 "added after CreateOffer/Answer, but before "
                 "offer/answer completed. This requires "
                 "renegotiation.");
      fireNegotiationNeeded = true;
    }

    // Telemetry: record info on the current state of streams/renegotiations/etc
    // Note: this code gets run on rollbacks as well!

    // Update the max channels used with each direction for each type
    uint16_t receiving[SdpMediaSection::kMediaTypes];
    uint16_t sending[SdpMediaSection::kMediaTypes];
    mJsepSession->CountTracks(receiving, sending);
    for (size_t i = 0; i < SdpMediaSection::kMediaTypes; i++) {
      if (mMaxReceiving[i] < receiving[i]) {
        mMaxReceiving[i] = receiving[i];
      }
      if (mMaxSending[i] < sending[i]) {
        mMaxSending[i] = sending[i];
      }
    }
  }

  if (mSignalingState == PCImplSignalingState::SignalingClosed) {
    CloseInt();
  }

  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }
  JSErrorResult rv;
  pco->OnStateChange(PCObserverStateType::SignalingState, rv);

  if (fireNegotiationNeeded) {
    // We don't use MaybeFireNegotiationNeeded here, since content might have
    // already cased a transition from stable.
    OnNegotiationNeeded();
  }
}

void
PeerConnectionImpl::UpdateSignalingState(bool rollback) {
  mozilla::JsepSignalingState state =
      mJsepSession->GetState();

  PCImplSignalingState newState;

  switch(state) {
    case kJsepStateStable:
      newState = PCImplSignalingState::SignalingStable;
      break;
    case kJsepStateHaveLocalOffer:
      newState = PCImplSignalingState::SignalingHaveLocalOffer;
      break;
    case kJsepStateHaveRemoteOffer:
      newState = PCImplSignalingState::SignalingHaveRemoteOffer;
      break;
    case kJsepStateHaveLocalPranswer:
      newState = PCImplSignalingState::SignalingHaveLocalPranswer;
      break;
    case kJsepStateHaveRemotePranswer:
      newState = PCImplSignalingState::SignalingHaveRemotePranswer;
      break;
    case kJsepStateClosed:
      newState = PCImplSignalingState::SignalingClosed;
      break;
    default:
      MOZ_CRASH();
  }

  SetSignalingState_m(newState, rollback);
}

bool
PeerConnectionImpl::IsClosed() const
{
  return mSignalingState == PCImplSignalingState::SignalingClosed;
}

bool
PeerConnectionImpl::HasMedia() const
{
  return mMedia;
}

PeerConnectionWrapper::PeerConnectionWrapper(const std::string& handle)
    : impl_(nullptr) {
  if (PeerConnectionCtx::GetInstance()->mPeerConnections.find(handle) ==
    PeerConnectionCtx::GetInstance()->mPeerConnections.end()) {
    return;
  }

  PeerConnectionImpl *impl = PeerConnectionCtx::GetInstance()->mPeerConnections[handle];

  if (!impl->media())
    return;

  impl_ = impl;
}

const std::string&
PeerConnectionImpl::GetHandle()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mHandle;
}

const std::string&
PeerConnectionImpl::GetName()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mName;
}

static mozilla::dom::PCImplIceConnectionState
toDomIceConnectionState(NrIceCtx::ConnectionState state) {
  switch (state) {
    case NrIceCtx::ICE_CTX_INIT:
      return PCImplIceConnectionState::New;
    case NrIceCtx::ICE_CTX_CHECKING:
      return PCImplIceConnectionState::Checking;
    case NrIceCtx::ICE_CTX_OPEN:
      return PCImplIceConnectionState::Connected;
    case NrIceCtx::ICE_CTX_FAILED:
      return PCImplIceConnectionState::Failed;
  }
  MOZ_CRASH();
}

static mozilla::dom::PCImplIceGatheringState
toDomIceGatheringState(NrIceCtx::GatheringState state) {
  switch (state) {
    case NrIceCtx::ICE_CTX_GATHER_INIT:
      return PCImplIceGatheringState::New;
    case NrIceCtx::ICE_CTX_GATHER_STARTED:
      return PCImplIceGatheringState::Gathering;
    case NrIceCtx::ICE_CTX_GATHER_COMPLETE:
      return PCImplIceGatheringState::Complete;
  }
  MOZ_CRASH();
}

void
PeerConnectionImpl::CandidateReady(const std::string& candidate,
                                   uint16_t level) {
  PC_AUTO_ENTER_API_CALL_VOID_RETURN(false);

  std::string mid;
  bool skipped = false;
  nsresult res = mJsepSession->AddLocalIceCandidate(candidate,
                                                    level,
                                                    &mid,
                                                    &skipped);

  if (NS_FAILED(res)) {
    std::string errorString = mJsepSession->GetLastError();

    CSFLogError(logTag, "Failed to incorporate local candidate into SDP:"
                        " res = %u, candidate = %s, level = %u, error = %s",
                        static_cast<unsigned>(res),
                        candidate.c_str(),
                        static_cast<unsigned>(level),
                        errorString.c_str());
    return;
  }

  if (skipped) {
    CSFLogDebug(logTag, "Skipped adding local candidate %s (level %u) to SDP, "
                        "this typically happens because the m-section is "
                        "bundled, which means it doesn't make sense for it to "
                        "have its own transport-related attributes.",
                        candidate.c_str(),
                        static_cast<unsigned>(level));
    return;
  }

  CSFLogDebug(logTag, "Passing local candidate to content: %s",
              candidate.c_str());
  SendLocalIceCandidateToContent(level, mid, candidate);

  UpdateSignalingState();
}

static void
SendLocalIceCandidateToContentImpl(nsWeakPtr weakPCObserver,
                                   uint16_t level,
                                   const std::string& mid,
                                   const std::string& candidate) {
  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(weakPCObserver);
  if (!pco) {
    return;
  }

  JSErrorResult rv;
  pco->OnIceCandidate(level,
                      ObString(mid.c_str()),
                      ObString(candidate.c_str()),
                      rv);
}

void
PeerConnectionImpl::SendLocalIceCandidateToContent(
    uint16_t level,
    const std::string& mid,
    const std::string& candidate) {
  // We dispatch this because OnSetLocalDescriptionSuccess does a setTimeout(0)
  // to unwind the stack, but the event handlers don't. We need to ensure that
  // the candidates do not skip ahead of the callback.
  NS_DispatchToMainThread(
      WrapRunnableNM(&SendLocalIceCandidateToContentImpl,
                     mPCObserver,
                     level,
                     mid,
                     candidate),
      NS_DISPATCH_NORMAL);
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
static bool isDone(PCImplIceConnectionState state) {
  return state != PCImplIceConnectionState::Checking &&
         state != PCImplIceConnectionState::New;
}

static bool isSucceeded(PCImplIceConnectionState state) {
  return state == PCImplIceConnectionState::Connected ||
         state == PCImplIceConnectionState::Completed;
}

static bool isFailed(PCImplIceConnectionState state) {
  return state == PCImplIceConnectionState::Failed ||
         state == PCImplIceConnectionState::Disconnected;
}
#endif

void PeerConnectionImpl::IceConnectionStateChange(
    NrIceCtx* ctx,
    NrIceCtx::ConnectionState state) {
  PC_AUTO_ENTER_API_CALL_VOID_RETURN(false);

  CSFLogDebug(logTag, "%s", __FUNCTION__);

  auto domState = toDomIceConnectionState(state);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  if (!isDone(mIceConnectionState) && isDone(domState)) {
    // mIceStartTime can be null if going directly from New to Closed, in which
    // case we don't count it as a success or a failure.
    if (!mIceStartTime.IsNull()){
      TimeDuration timeDelta = TimeStamp::Now() - mIceStartTime;
      if (isSucceeded(domState)) {
        Telemetry::Accumulate(mIsLoop ? Telemetry::LOOP_ICE_SUCCESS_TIME :
                                        Telemetry::WEBRTC_ICE_SUCCESS_TIME,
                              timeDelta.ToMilliseconds());
      } else if (isFailed(domState)) {
        Telemetry::Accumulate(mIsLoop ? Telemetry::LOOP_ICE_FAILURE_TIME :
                                        Telemetry::WEBRTC_ICE_FAILURE_TIME,
                              timeDelta.ToMilliseconds());
      }
    }

    if (isSucceeded(domState)) {
      Telemetry::Accumulate(
          Telemetry::WEBRTC_ICE_ADD_CANDIDATE_ERRORS_GIVEN_SUCCESS,
          mAddCandidateErrorCount);
    } else if (isFailed(domState)) {
      Telemetry::Accumulate(
          Telemetry::WEBRTC_ICE_ADD_CANDIDATE_ERRORS_GIVEN_FAILURE,
          mAddCandidateErrorCount);
    }
  }
#endif

  mIceConnectionState = domState;

  if (mIceConnectionState == PCImplIceConnectionState::Connected ||
      mIceConnectionState == PCImplIceConnectionState::Completed ||
      mIceConnectionState == PCImplIceConnectionState::Failed) {
    if (mMedia->IsIceRestarting()) {
      FinalizeIceRestart();
    }
  }

  // Would be nice if we had a means of converting one of these dom enums
  // to a string that wasn't almost as much text as this switch statement...
  switch (mIceConnectionState) {
    case PCImplIceConnectionState::New:
      STAMP_TIMECARD(mTimeCard, "Ice state: new");
      break;
    case PCImplIceConnectionState::Checking:
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
      // For telemetry
      mIceStartTime = TimeStamp::Now();
#endif
      STAMP_TIMECARD(mTimeCard, "Ice state: checking");
      break;
    case PCImplIceConnectionState::Connected:
      STAMP_TIMECARD(mTimeCard, "Ice state: connected");
      break;
    case PCImplIceConnectionState::Completed:
      STAMP_TIMECARD(mTimeCard, "Ice state: completed");
      break;
    case PCImplIceConnectionState::Failed:
      STAMP_TIMECARD(mTimeCard, "Ice state: failed");
      break;
    case PCImplIceConnectionState::Disconnected:
      STAMP_TIMECARD(mTimeCard, "Ice state: disconnected");
      break;
    case PCImplIceConnectionState::Closed:
      STAMP_TIMECARD(mTimeCard, "Ice state: closed");
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected mIceConnectionState!");
  }

  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }
  WrappableJSErrorResult rv;
  RUN_ON_THREAD(mThread,
                WrapRunnable(pco,
                             &PeerConnectionObserver::OnStateChange,
                             PCObserverStateType::IceConnectionState,
                             rv, static_cast<JSCompartment*>(nullptr)),
                NS_DISPATCH_NORMAL);
}

void
PeerConnectionImpl::IceGatheringStateChange(
    NrIceCtx* ctx,
    NrIceCtx::GatheringState state)
{
  PC_AUTO_ENTER_API_CALL_VOID_RETURN(false);

  CSFLogDebug(logTag, "%s", __FUNCTION__);

  mIceGatheringState = toDomIceGatheringState(state);

  // Would be nice if we had a means of converting one of these dom enums
  // to a string that wasn't almost as much text as this switch statement...
  switch (mIceGatheringState) {
    case PCImplIceGatheringState::New:
      STAMP_TIMECARD(mTimeCard, "Ice gathering state: new");
      break;
    case PCImplIceGatheringState::Gathering:
      STAMP_TIMECARD(mTimeCard, "Ice gathering state: gathering");
      break;
    case PCImplIceGatheringState::Complete:
      STAMP_TIMECARD(mTimeCard, "Ice gathering state: complete");
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("Unexpected mIceGatheringState!");
  }

  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }
  WrappableJSErrorResult rv;
  RUN_ON_THREAD(mThread,
                WrapRunnable(pco,
                             &PeerConnectionObserver::OnStateChange,
                             PCObserverStateType::IceGatheringState,
                             rv, static_cast<JSCompartment*>(nullptr)),
                NS_DISPATCH_NORMAL);

  if (mIceGatheringState == PCImplIceGatheringState::Complete) {
    SendLocalIceCandidateToContent(0, "", "");
  }
}

void
PeerConnectionImpl::UpdateDefaultCandidate(const std::string& defaultAddr,
                                           uint16_t defaultPort,
                                           const std::string& defaultRtcpAddr,
                                           uint16_t defaultRtcpPort,
                                           uint16_t level) {
  CSFLogDebug(logTag, "%s", __FUNCTION__);
  mJsepSession->UpdateDefaultCandidate(defaultAddr,
                                       defaultPort,
                                       defaultRtcpAddr,
                                       defaultRtcpPort,
                                       level);
}

void
PeerConnectionImpl::EndOfLocalCandidates(uint16_t level) {
  CSFLogDebug(logTag, "%s", __FUNCTION__);
  mJsepSession->EndOfLocalCandidates(level);
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
nsresult
PeerConnectionImpl::BuildStatsQuery_m(
    mozilla::dom::MediaStreamTrack *aSelector,
    RTCStatsQuery *query) {

  if (!HasMedia()) {
    return NS_ERROR_UNEXPECTED;
  }

  if (!mThread) {
    CSFLogError(logTag, "Could not build stats query, no MainThread");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = GetTimeSinceEpoch(&(query->now));
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "Could not build stats query, could not get timestamp");
    return rv;
  }

  // Note: mMedia->ice_ctx() is deleted on STS thread; so make sure we grab and hold
  // a ref instead of making multiple calls.  NrIceCtx uses threadsafe refcounting.
  // NOTE: Do this after all other failure tests, to ensure we don't
  // accidentally release the Ctx on Mainthread.
  query->iceCtx = mMedia->ice_ctx();
  if (!query->iceCtx) {
    CSFLogError(logTag, "Could not build stats query, no ice_ctx");
    return NS_ERROR_UNEXPECTED;
  }

  // We do not use the pcHandle here, since that's risky to expose to content.
  query->report = new RTCStatsReportInternalConstruct(
      NS_ConvertASCIItoUTF16(mName.c_str()),
      query->now);

  query->iceStartTime = mIceStartTime;
  query->failed = isFailed(mIceConnectionState);
  query->isHello = mIsLoop;

  // Populate SDP on main
  if (query->internalStats) {
    if (mJsepSession) {
      std::string localDescription = mJsepSession->GetLocalDescription();
      std::string remoteDescription = mJsepSession->GetRemoteDescription();
      query->report->mLocalSdp.Construct(
          NS_ConvertASCIItoUTF16(localDescription.c_str()));
      query->report->mRemoteSdp.Construct(
          NS_ConvertASCIItoUTF16(remoteDescription.c_str()));
    }
  }

  // Gather up pipelines from mMedia so they may be inspected on STS

  std::string trackId;
  if (aSelector) {
    trackId = PeerConnectionImpl::GetTrackId(*aSelector);
  }

  for (int i = 0, len = mMedia->LocalStreamsLength(); i < len; i++) {
    for (auto pipeline : mMedia->GetLocalStreamByIndex(i)->GetPipelines()) {
      if (!aSelector || pipeline.second->trackid() == trackId) {
        query->pipelines.AppendElement(pipeline.second);
      }
    }
  }
  for (int i = 0, len = mMedia->RemoteStreamsLength(); i < len; i++) {
    for (auto pipeline : mMedia->GetRemoteStreamByIndex(i)->GetPipelines()) {
      if (!aSelector || pipeline.second->trackid() == trackId) {
        query->pipelines.AppendElement(pipeline.second);
      }
    }
  }

  if (!aSelector) {
    query->grabAllLevels = true;
  }

  return rv;
}

static void ToRTCIceCandidateStats(
    const std::vector<NrIceCandidate>& candidates,
    RTCStatsType candidateType,
    const nsString& componentId,
    DOMHighResTimeStamp now,
    RTCStatsReportInternal* report) {

  MOZ_ASSERT(report);
  for (auto c = candidates.begin(); c != candidates.end(); ++c) {
    RTCIceCandidateStats cand;
    cand.mType.Construct(candidateType);
    NS_ConvertASCIItoUTF16 codeword(c->codeword.c_str());
    cand.mComponentId.Construct(componentId);
    cand.mId.Construct(codeword);
    cand.mTimestamp.Construct(now);
    cand.mCandidateType.Construct(
        RTCStatsIceCandidateType(c->type));
    cand.mIpAddress.Construct(
        NS_ConvertASCIItoUTF16(c->cand_addr.host.c_str()));
    cand.mPortNumber.Construct(c->cand_addr.port);
    cand.mTransport.Construct(
        NS_ConvertASCIItoUTF16(c->cand_addr.transport.c_str()));
    if (candidateType == RTCStatsType::Localcandidate) {
      cand.mMozLocalTransport.Construct(
          NS_ConvertASCIItoUTF16(c->local_addr.transport.c_str()));
    }
    report->mIceCandidateStats.Value().AppendElement(cand, fallible);
  }
}

static void RecordIceStats_s(
    NrIceMediaStream& mediaStream,
    bool internalStats,
    DOMHighResTimeStamp now,
    RTCStatsReportInternal* report) {

  NS_ConvertASCIItoUTF16 componentId(mediaStream.name().c_str());

  std::vector<NrIceCandidatePair> candPairs;
  nsresult res = mediaStream.GetCandidatePairs(&candPairs);
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Error getting candidate pairs", __FUNCTION__);
    return;
  }

  for (auto p = candPairs.begin(); p != candPairs.end(); ++p) {
    NS_ConvertASCIItoUTF16 codeword(p->codeword.c_str());
    NS_ConvertASCIItoUTF16 localCodeword(p->local.codeword.c_str());
    NS_ConvertASCIItoUTF16 remoteCodeword(p->remote.codeword.c_str());
    // Only expose candidate-pair statistics to chrome, until we've thought
    // through the implications of exposing it to content.

    RTCIceCandidatePairStats s;
    s.mId.Construct(codeword);
    s.mComponentId.Construct(componentId);
    s.mTimestamp.Construct(now);
    s.mType.Construct(RTCStatsType::Candidatepair);
    s.mLocalCandidateId.Construct(localCodeword);
    s.mRemoteCandidateId.Construct(remoteCodeword);
    s.mNominated.Construct(p->nominated);
    s.mPriority.Construct(p->priority);
    s.mSelected.Construct(p->selected);
    s.mState.Construct(RTCStatsIceCandidatePairState(p->state));
    report->mIceCandidatePairStats.Value().AppendElement(s, fallible);
  }

  std::vector<NrIceCandidate> candidates;
  if (NS_SUCCEEDED(mediaStream.GetLocalCandidates(&candidates))) {
    ToRTCIceCandidateStats(candidates,
                           RTCStatsType::Localcandidate,
                           componentId,
                           now,
                           report);
  }
  candidates.clear();

  if (NS_SUCCEEDED(mediaStream.GetRemoteCandidates(&candidates))) {
    ToRTCIceCandidateStats(candidates,
                           RTCStatsType::Remotecandidate,
                           componentId,
                           now,
                           report);
  }
}

nsresult
PeerConnectionImpl::ExecuteStatsQuery_s(RTCStatsQuery *query) {

  ASSERT_ON_THREAD(query->iceCtx->thread());

  // Gather stats from pipelines provided (can't touch mMedia + stream on STS)

  for (size_t p = 0; p < query->pipelines.Length(); ++p) {
    const MediaPipeline& mp = *query->pipelines[p];
    bool isAudio = (mp.Conduit()->type() == MediaSessionConduit::AUDIO);
    nsString mediaType = isAudio ?
        NS_LITERAL_STRING("audio") : NS_LITERAL_STRING("video");
    nsString idstr = mediaType;
    idstr.AppendLiteral("_");
    idstr.AppendInt(mp.level());

    // Gather pipeline stats.
    switch (mp.direction()) {
      case MediaPipeline::TRANSMIT: {
        nsString localId = NS_LITERAL_STRING("outbound_rtp_") + idstr;
        nsString remoteId;
        nsString ssrc;
        unsigned int ssrcval;
        if (mp.Conduit()->GetLocalSSRC(&ssrcval)) {
          ssrc.AppendInt(ssrcval);
        }
        {
          // First, fill in remote stat with rtcp receiver data, if present.
          // ReceiverReports have less information than SenderReports,
          // so fill in what we can.
          DOMHighResTimeStamp timestamp;
          uint32_t jitterMs;
          uint32_t packetsReceived;
          uint64_t bytesReceived;
          uint32_t packetsLost;
          int32_t rtt;
          if (mp.Conduit()->GetRTCPReceiverReport(&timestamp, &jitterMs,
                                                  &packetsReceived,
                                                  &bytesReceived,
                                                  &packetsLost,
                                                  &rtt)) {
            remoteId = NS_LITERAL_STRING("outbound_rtcp_") + idstr;
            RTCInboundRTPStreamStats s;
            s.mTimestamp.Construct(timestamp);
            s.mId.Construct(remoteId);
            s.mType.Construct(RTCStatsType::Inboundrtp);
            if (ssrc.Length()) {
              s.mSsrc.Construct(ssrc);
            }
            s.mMediaType.Construct(mediaType);
            s.mJitter.Construct(double(jitterMs)/1000);
            s.mRemoteId.Construct(localId);
            s.mIsRemote = true;
            s.mPacketsReceived.Construct(packetsReceived);
            s.mBytesReceived.Construct(bytesReceived);
            s.mPacketsLost.Construct(packetsLost);
            s.mMozRtt.Construct(rtt);
            query->report->mInboundRTPStreamStats.Value().AppendElement(s,
                                                                        fallible);
          }
        }
        // Then, fill in local side (with cross-link to remote only if present)
        {
          RTCOutboundRTPStreamStats s;
          s.mTimestamp.Construct(query->now);
          s.mId.Construct(localId);
          s.mType.Construct(RTCStatsType::Outboundrtp);
          if (ssrc.Length()) {
            s.mSsrc.Construct(ssrc);
          }
          s.mMediaType.Construct(mediaType);
          s.mRemoteId.Construct(remoteId);
          s.mIsRemote = false;
          s.mPacketsSent.Construct(mp.rtp_packets_sent());
          s.mBytesSent.Construct(mp.rtp_bytes_sent());

          // Lastly, fill in video encoder stats if this is video
          if (!isAudio) {
            double framerateMean;
            double framerateStdDev;
            double bitrateMean;
            double bitrateStdDev;
            uint32_t droppedFrames;
            if (mp.Conduit()->GetVideoEncoderStats(&framerateMean,
                                                   &framerateStdDev,
                                                   &bitrateMean,
                                                   &bitrateStdDev,
                                                   &droppedFrames)) {
              s.mFramerateMean.Construct(framerateMean);
              s.mFramerateStdDev.Construct(framerateStdDev);
              s.mBitrateMean.Construct(bitrateMean);
              s.mBitrateStdDev.Construct(bitrateStdDev);
              s.mDroppedFrames.Construct(droppedFrames);
            }
          }
          query->report->mOutboundRTPStreamStats.Value().AppendElement(s,
                                                                       fallible);
        }
        break;
      }
      case MediaPipeline::RECEIVE: {
        nsString localId = NS_LITERAL_STRING("inbound_rtp_") + idstr;
        nsString remoteId;
        nsString ssrc;
        unsigned int ssrcval;
        if (mp.Conduit()->GetRemoteSSRC(&ssrcval)) {
          ssrc.AppendInt(ssrcval);
        }
        {
          // First, fill in remote stat with rtcp sender data, if present.
          DOMHighResTimeStamp timestamp;
          uint32_t packetsSent;
          uint64_t bytesSent;
          if (mp.Conduit()->GetRTCPSenderReport(&timestamp,
                                                &packetsSent, &bytesSent)) {
            remoteId = NS_LITERAL_STRING("inbound_rtcp_") + idstr;
            RTCOutboundRTPStreamStats s;
            s.mTimestamp.Construct(timestamp);
            s.mId.Construct(remoteId);
            s.mType.Construct(RTCStatsType::Outboundrtp);
            if (ssrc.Length()) {
              s.mSsrc.Construct(ssrc);
            }
            s.mMediaType.Construct(mediaType);
            s.mRemoteId.Construct(localId);
            s.mIsRemote = true;
            s.mPacketsSent.Construct(packetsSent);
            s.mBytesSent.Construct(bytesSent);
            query->report->mOutboundRTPStreamStats.Value().AppendElement(s,
                                                                         fallible);
          }
        }
        // Then, fill in local side (with cross-link to remote only if present)
        RTCInboundRTPStreamStats s;
        s.mTimestamp.Construct(query->now);
        s.mId.Construct(localId);
        s.mType.Construct(RTCStatsType::Inboundrtp);
        if (ssrc.Length()) {
          s.mSsrc.Construct(ssrc);
        }
        s.mMediaType.Construct(mediaType);
        unsigned int jitterMs, packetsLost;
        if (mp.Conduit()->GetRTPStats(&jitterMs, &packetsLost)) {
          s.mJitter.Construct(double(jitterMs)/1000);
          s.mPacketsLost.Construct(packetsLost);
        }
        if (remoteId.Length()) {
          s.mRemoteId.Construct(remoteId);
        }
        s.mIsRemote = false;
        s.mPacketsReceived.Construct(mp.rtp_packets_received());
        s.mBytesReceived.Construct(mp.rtp_bytes_received());

        if (query->internalStats && isAudio) {
          int32_t jitterBufferDelay;
          int32_t playoutBufferDelay;
          int32_t avSyncDelta;
          if (mp.Conduit()->GetAVStats(&jitterBufferDelay,
                                       &playoutBufferDelay,
                                       &avSyncDelta)) {
            s.mMozJitterBufferDelay.Construct(jitterBufferDelay);
            s.mMozAvSyncDelay.Construct(avSyncDelta);
          }
        }
        // Lastly, fill in video decoder stats if this is video
        if (!isAudio) {
          double framerateMean;
          double framerateStdDev;
          double bitrateMean;
          double bitrateStdDev;
          uint32_t discardedPackets;
          if (mp.Conduit()->GetVideoDecoderStats(&framerateMean,
                                                 &framerateStdDev,
                                                 &bitrateMean,
                                                 &bitrateStdDev,
                                                 &discardedPackets)) {
            s.mFramerateMean.Construct(framerateMean);
            s.mFramerateStdDev.Construct(framerateStdDev);
            s.mBitrateMean.Construct(bitrateMean);
            s.mBitrateStdDev.Construct(bitrateStdDev);
            s.mDiscardedPackets.Construct(discardedPackets);
          }
        }
        query->report->mInboundRTPStreamStats.Value().AppendElement(s,
                                                                    fallible);
        break;
      }
    }

    if (!query->grabAllLevels) {
      // If we're grabbing all levels, that means we want datachannels too,
      // which don't have pipelines.
      if (query->iceCtx->GetStream(p)) {
        RecordIceStats_s(*query->iceCtx->GetStream(p),
                         query->internalStats,
                         query->now,
                         query->report);
      }
    }
  }

  if (query->grabAllLevels) {
    for (size_t i = 0; i < query->iceCtx->GetStreamCount(); ++i) {
      if (query->iceCtx->GetStream(i)) {
        RecordIceStats_s(*query->iceCtx->GetStream(i),
                         query->internalStats,
                         query->now,
                         query->report);
      }
    }
  }

  // NrIceCtx must be destroyed on STS, so it is not safe
  // to dispatch it back to main.
  query->iceCtx = nullptr;
  return NS_OK;
}

void PeerConnectionImpl::GetStatsForPCObserver_s(
    const std::string& pcHandle, // The Runnable holds the memory
    nsAutoPtr<RTCStatsQuery> query) {

  MOZ_ASSERT(query);
  MOZ_ASSERT(query->iceCtx);
  ASSERT_ON_THREAD(query->iceCtx->thread());

  nsresult rv = PeerConnectionImpl::ExecuteStatsQuery_s(query.get());

  NS_DispatchToMainThread(
      WrapRunnableNM(
          &PeerConnectionImpl::DeliverStatsReportToPCObserver_m,
          pcHandle,
          rv,
          query),
      NS_DISPATCH_NORMAL);
}

void PeerConnectionImpl::DeliverStatsReportToPCObserver_m(
    const std::string& pcHandle,
    nsresult result,
    nsAutoPtr<RTCStatsQuery> query) {

  // Is the PeerConnectionImpl still around?
  PeerConnectionWrapper pcw(pcHandle);
  if (pcw.impl()) {
    RefPtr<PeerConnectionObserver> pco =
        do_QueryObjectReferent(pcw.impl()->mPCObserver);
    if (pco) {
      JSErrorResult rv;
      if (NS_SUCCEEDED(result)) {
        pco->OnGetStatsSuccess(*query->report, rv);
      } else {
        pco->OnGetStatsError(kInternalError,
            ObString("Failed to fetch statistics"),
            rv);
      }

      if (rv.Failed()) {
        CSFLogError(logTag, "Error firing stats observer callback");
      }
    }
  }
}

#endif

void
PeerConnectionImpl::RecordLongtermICEStatistics() {
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  WebrtcGlobalInformation::StoreLongTermICEStatistics(*this);
#endif
}

void
PeerConnectionImpl::OnNegotiationNeeded()
{
  if (mSignalingState != PCImplSignalingState::SignalingStable) {
    // We will check whether we need to renegotiate when we reach stable again
    return;
  }

  if (mNegotiationNeeded) {
    return;
  }

  mNegotiationNeeded = true;

  RUN_ON_THREAD(mThread,
                WrapRunnableNM(&MaybeFireNegotiationNeeded_static, mHandle),
                NS_DISPATCH_NORMAL);
}

/* static */
void
PeerConnectionImpl::MaybeFireNegotiationNeeded_static(
    const std::string& pcHandle)
{
  PeerConnectionWrapper wrapper(pcHandle);
  if (!wrapper.impl()) {
    return;
  }

  wrapper.impl()->MaybeFireNegotiationNeeded();
}

void
PeerConnectionImpl::MaybeFireNegotiationNeeded()
{
  if (!mNegotiationNeeded) {
    return;
  }

  RefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }

  JSErrorResult rv;
  pco->OnNegotiationNeeded(rv);
}

void
PeerConnectionImpl::IceStreamReady(NrIceMediaStream *aStream)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aStream);

  CSFLogDebug(logTag, "%s: %s", __FUNCTION__, aStream->name().c_str());
}

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
//Telemetry for when calls start
void
PeerConnectionImpl::startCallTelem() {
  if (!mStartTime.IsNull()) {
    return;
  }

  // Start time for calls
  mStartTime = TimeStamp::Now();

  // Increment session call counter
  // If we want to track Loop calls independently here, we need two histograms.
  Telemetry::Accumulate(Telemetry::WEBRTC_CALL_COUNT_2, 1);
}
#endif

NS_IMETHODIMP
PeerConnectionImpl::GetLocalStreams(nsTArray<RefPtr<DOMMediaStream > >& result)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  for(uint32_t i=0; i < media()->LocalStreamsLength(); i++) {
    LocalSourceStreamInfo *info = media()->GetLocalStreamByIndex(i);
    NS_ENSURE_TRUE(info, NS_ERROR_UNEXPECTED);
    result.AppendElement(info->GetMediaStream());
  }
  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP
PeerConnectionImpl::GetRemoteStreams(nsTArray<RefPtr<DOMMediaStream > >& result)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  for(uint32_t i=0; i < media()->RemoteStreamsLength(); i++) {
    RemoteSourceStreamInfo *info = media()->GetRemoteStreamByIndex(i);
    NS_ENSURE_TRUE(info, NS_ERROR_UNEXPECTED);
    result.AppendElement(info->GetMediaStream());
  }
  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif
}

}  // end mozilla namespace
