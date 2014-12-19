/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <cerrno>
#include <deque>
#include <set>
#include <sstream>
#include <vector>

#include "base/histogram.h"
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

#ifdef MOZILLA_INTERNAL_API
#ifdef XP_WIN
// We need to undef the MS macro for nsIDocument::CreateEvent
#ifdef CreateEvent
#undef CreateEvent
#endif
#endif // XP_WIN

#ifdef MOZILLA_INTERNAL_API
#include "nsIDocument.h"
#endif
#include "nsPerformance.h"
#include "nsGlobalWindow.h"
#include "nsDOMDataChannel.h"
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
#include "nsIDOMDataChannel.h"
#include "nsIDOMLocation.h"
#include "nsNullPrincipal.h"
#include "mozilla/PeerIdentity.h"
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/dom/PeerConnectionImplBinding.h"
#include "mozilla/dom/DataChannelBinding.h"
#include "mozilla/dom/PluginCrashedEvent.h"
#include "MediaStreamList.h"
#include "MediaStreamTrack.h"
#include "AudioStreamTrack.h"
#include "VideoStreamTrack.h"
#include "nsIScriptGlobalObject.h"
#include "DOMMediaStream.h"
#include "rlogringbuffer.h"
#include "WebrtcGlobalInformation.h"
#include "mozilla/dom/Event.h"
#include "nsIDOMCustomEvent.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/net/DataChannelProtocol.h"
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
#ifdef MOZILLA_INTERNAL_API
    WouldReportJSException();
    if (IsJSException()) {
      MOZ_ASSERT(NS_IsMainThread());
      AutoJSContext cx;
      Optional<JS::Handle<JS::Value> > value(cx);
      StealJSException(cx, &value.Value());
    }
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
  WrappableJSErrorResult(WrappableJSErrorResult &other) : mRv(), isCopy(true) {}
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

#ifdef MOZILLA_INTERNAL_API
class TracksAvailableCallback : public DOMMediaStream::OnTracksAvailableCallback
{
public:
  TracksAvailableCallback(DOMMediaStream::TrackTypeHints aTrackTypeHints,
                          nsRefPtr<PeerConnectionObserver> aObserver)
  : DOMMediaStream::OnTracksAvailableCallback(aTrackTypeHints)
  , mObserver(aObserver) {}

  virtual void NotifyTracksAvailable(DOMMediaStream* aStream) MOZ_OVERRIDE
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Start currentTime from the point where this stream was successfully
    // returned.
    aStream->SetLogicalStreamStartTime(aStream->GetStream()->GetCurrentTime());

    CSFLogInfo(logTag, "Returning success for OnAddStream()");
    // We are running on main thread here so we shouldn't have a race
    // on this callback

    nsTArray<nsRefPtr<MediaStreamTrack>> tracks;
    aStream->GetTracks(tracks);
    for (uint32_t i = 0; i < tracks.Length(); i++) {
      JSErrorResult rv;
      mObserver->OnAddTrack(*tracks[i], rv);
      if (rv.Failed()) {
        CSFLogError(logTag, ": OnAddTrack(%d) failed! Error: %d", i,
                    rv.ErrorCode());
      }
    }
    JSErrorResult rv;
    mObserver->OnAddStream(*aStream, rv);
    if (rv.Failed()) {
      CSFLogError(logTag, ": OnAddStream() failed! Error: %d", rv.ErrorCode());
    }
  }
private:
  nsRefPtr<PeerConnectionObserver> mObserver;
};
#endif

#ifdef MOZILLA_INTERNAL_API
static nsresult InitNSSInContent()
{
  NS_ENSURE_TRUE(NS_IsMainThread(), NS_ERROR_NOT_SAME_THREAD);

  if (XRE_GetProcessType() != GeckoProcessType_Content) {
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

static const int MEDIA_STREAM_MUTE = 0x80;

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

#ifdef MOZILLA_INTERNAL_API
RTCStatsQuery::RTCStatsQuery(bool internal) :
  failed(false),
  internalStats(internal) {
}

RTCStatsQuery::~RTCStatsQuery() {
  MOZ_ASSERT(NS_IsMainThread());
}

#endif

NS_IMPL_ISUPPORTS0(PeerConnectionImpl)

#ifdef MOZILLA_INTERNAL_API
JSObject*
PeerConnectionImpl::WrapObject(JSContext* aCx)
{
  return PeerConnectionImplBinding::Wrap(aCx, this);
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


PeerConnectionImpl::PeerConnectionImpl(const GlobalObject* aGlobal)
: mTimeCard(PR_LOG_TEST(signalingLogInfo(),PR_LOG_ERROR) ?
            create_timecard() : nullptr)
  , mSignalingState(PCImplSignalingState::SignalingStable)
  , mIceConnectionState(PCImplIceConnectionState::New)
  , mIceGatheringState(PCImplIceGatheringState::New)
  , mDtlsConnected(false)
  , mWindow(nullptr)
  , mIdentity(nullptr)
  , mPrivacyRequested(false)
  , mSTSThread(nullptr)
  , mAllowIceLoopback(false)
  , mMedia(nullptr)
  , mUuidGen(MakeUnique<PCUuidGenerator>())
  , mNumAudioStreams(0)
  , mNumVideoStreams(0)
  , mHaveDataStream(false)
  , mAddCandidateErrorCount(0)
  , mTrickle(true) // TODO(ekr@rtfm.com): Use pref
{
#ifdef MOZILLA_INTERNAL_API
  MOZ_ASSERT(NS_IsMainThread());
  if (aGlobal) {
    mWindow = do_QueryInterface(aGlobal->GetAsSupports());
  }
#endif
  CSFLogInfo(logTag, "%s: PeerConnectionImpl constructor for %s",
             __FUNCTION__, mHandle.c_str());
  STAMP_TIMECARD(mTimeCard, "Constructor Completed");
#ifdef MOZILLA_INTERNAL_API
  mAllowIceLoopback = Preferences::GetBool(
    "media.peerconnection.ice.loopback", false);
#endif
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
  if (PeerConnectionCtx::isActive()) {
    PeerConnectionCtx::GetInstance()->mPeerConnections.erase(mHandle);
  } else {
    CSFLogError(logTag, "PeerConnectionCtx is already gone. Ignoring...");
  }

  CSFLogInfo(logTag, "%s: PeerConnectionImpl destructor invoked for %s",
             __FUNCTION__, mHandle.c_str());

  Close();

#ifdef MOZILLA_INTERNAL_API
  {
    // Deregister as an NSS Shutdown Object
    nsNSSShutDownPreventionLock locker;
    if (!isAlreadyShutDown()) {
      destructorSafeDestroyNSSReference();
      shutdown(calledFromObject);
    }
  }
#endif

  // Since this and Initialize() occur on MainThread, they can't both be
  // running at once

  // Right now, we delete PeerConnectionCtx at XPCOM shutdown only, but we
  // probably want to shut it down more aggressively to save memory.  We
  // could shut down here when there are no uses.  It might be more optimal
  // to release off a timer (and XPCOM Shutdown) to avoid churn
}

already_AddRefed<DOMMediaStream>
PeerConnectionImpl::MakeMediaStream(uint32_t aHint)
{
  nsRefPtr<DOMMediaStream> stream =
    DOMMediaStream::CreateSourceStream(GetWindow(), aHint);

#ifdef MOZILLA_INTERNAL_API
  // Make the stream data (audio/video samples) accessible to the receiving page.
  // We're only certain that privacy hasn't been requested if we're connected.
  if (mDtlsConnected && !PrivacyRequested()) {
    nsIDocument* doc = GetWindow()->GetExtantDoc();
    if (!doc) {
      return nullptr;
    }
    stream->CombineWithPrincipal(doc->NodePrincipal());
  } else {
    // we're either certain that we need isolation for the streams, OR
    // we're not sure and we can fix the stream in SetDtlsConnected
    nsCOMPtr<nsIPrincipal> principal =
      do_CreateInstance(NS_NULLPRINCIPAL_CONTRACTID);
    stream->CombineWithPrincipal(principal);
  }
#endif

  CSFLogDebug(logTag, "Created media stream %p, inner: %p", stream.get(), stream->GetStream());

  return stream.forget();
}

nsresult
PeerConnectionImpl::CreateRemoteSourceStreamInfo(nsRefPtr<RemoteSourceStreamInfo>*
                                                 aInfo,
                                                 const std::string& aStreamID)
{
  MOZ_ASSERT(aInfo);
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  // We need to pass a dummy hint here because FakeMediaStream currently
  // needs to actually propagate a hint for local streams.
  // TODO(ekr@rtfm.com): Clean up when we have explicit track lists.
  // See bug 834835.
  nsRefPtr<DOMMediaStream> stream = MakeMediaStream(0);
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  static_cast<SourceMediaStream*>(stream->GetStream())->SetPullEnabled(true);

  nsRefPtr<RemoteSourceStreamInfo> remote;
  remote = new RemoteSourceStreamInfo(stream.forget(), mMedia, aStreamID);
  *aInfo = remote;

  return NS_OK;
}

/**
 * In JS, an RTCConfiguration looks like this:
 *
 * { "iceServers": [ { url:"stun:stun.example.org" },
 *                   { url:"turn:turn.example.org?transport=udp",
 *                     username: "jib", credential:"mypass"} ] }
 *
 * This function converts that into an internal IceConfiguration object.
 */
nsresult
PeerConnectionImpl::ConvertRTCConfiguration(const RTCConfiguration& aSrc,
                                            IceConfiguration *aDst)
{
#ifdef MOZILLA_INTERNAL_API
  if (!aSrc.mIceServers.WasPassed()) {
    return NS_OK;
  }
  for (uint32_t i = 0; i < aSrc.mIceServers.Value().Length(); i++) {
    const RTCIceServer& server = aSrc.mIceServers.Value()[i];
    NS_ENSURE_TRUE(server.mUrl.WasPassed(), NS_ERROR_UNEXPECTED);

    // Without STUN/TURN handlers, NS_NewURI returns nsSimpleURI rather than
    // nsStandardURL. To parse STUN/TURN URI's to spec
    // http://tools.ietf.org/html/draft-nandakumar-rtcweb-stun-uri-02#section-3
    // http://tools.ietf.org/html/draft-petithuguenin-behave-turn-uri-03#section-3
    // we parse out the query-string, and use ParseAuthority() on the rest
    nsRefPtr<nsIURI> url;
    nsresult rv = NS_NewURI(getter_AddRefs(url), server.mUrl.Value());
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
      NS_ConvertUTF16toUTF8 credential(server.mCredential);
      NS_ConvertUTF16toUTF8 username(server.mUsername);

      // Bug 1039655 - TURN TCP is not e10s ready
      if ((transport == kNrIceTransportTcp) &&
          (XRE_GetProcessType() != GeckoProcessType_Default)) {
        continue;
      }

      if (!aDst->addTurnServer(host.get(), port,
                               username.get(),
                               credential.get(),
                               (transport.IsEmpty() ?
                                kNrIceTransportUdp : transport.get()))) {
        return NS_ERROR_FAILURE;
      }
    } else {
      if (!aDst->addStunServer(host.get(), port)) {
        return NS_ERROR_FAILURE;
      }
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::Initialize(PeerConnectionObserver& aObserver,
                               nsGlobalWindow* aWindow,
                               const IceConfiguration* aConfiguration,
                               const RTCConfiguration* aRTCConfiguration,
                               nsISupports* aThread)
{
  nsresult res;

  // Invariant: we receive configuration one way or the other but not both (XOR)
  MOZ_ASSERT(!aConfiguration != !aRTCConfiguration);
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aThread);
  mThread = do_QueryInterface(aThread);
  CheckThread();

  mPCObserver = do_GetWeakReference(&aObserver);

  // Find the STS thread

  mSTSThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
  MOZ_ASSERT(mSTSThread);
#ifdef MOZILLA_INTERNAL_API

  // Initialize NSS if we are in content process. For chrome process, NSS should already
  // been initialized.
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    // This code interferes with the C++ unit test startup code.
    nsCOMPtr<nsISupports> nssDummy = do_GetService("@mozilla.org/psm;1", &res);
    NS_ENSURE_SUCCESS(res, res);
  } else {
    NS_ENSURE_SUCCESS(res = InitNSSInContent(), res);
  }

  // Currently no standalone unit tests for DataChannel,
  // which is the user of mWindow
  MOZ_ASSERT(aWindow);
  mWindow = aWindow;
  NS_ENSURE_STATE(mWindow);

  if (!aRTCConfiguration->mPeerIdentity.IsEmpty()) {
    mPeerIdentity = new PeerIdentity(aRTCConfiguration->mPeerIdentity);
    mPrivacyRequested = true;
  }
#endif // MOZILLA_INTERNAL_API

  PRTime timestamp = PR_Now();
  // Ok if we truncate this.
  char temp[128];

#ifdef MOZILLA_INTERNAL_API
  nsAutoCString locationCStr;
  nsIDOMLocation* location;
  res = mWindow->GetLocation(&location);

  if (location && NS_SUCCEEDED(res)) {
    nsAutoString locationAStr;
    location->ToString(locationAStr);
    location->Release();

    CopyUTF16toUTF8(locationAStr, locationCStr);
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


  IceConfiguration converted;
  if (aRTCConfiguration) {
    res = ConvertRTCConfiguration(*aRTCConfiguration, &converted);
    if (NS_FAILED(res)) {
      CSFLogError(logTag, "%s: Invalid RTCConfiguration", __FUNCTION__);
      return res;
    }
    aConfiguration = &converted;
  }

  mMedia = new PeerConnectionMedia(this);
  mMedia->SetAllowIceLoopback(mAllowIceLoopback);

  // Connect ICE slots.
  mMedia->SignalIceGatheringStateChange.connect(
      this,
      &PeerConnectionImpl::IceGatheringStateChange);
  mMedia->SignalEndOfLocalCandidates.connect(
      this,
      &PeerConnectionImpl::EndOfLocalCandidates);
  mMedia->SignalIceConnectionStateChange.connect(
      this,
      &PeerConnectionImpl::IceConnectionStateChange);

  mMedia->SignalCandidate.connect(this, &PeerConnectionImpl::CandidateReady);

  // Initialize the media object.
  res = mMedia->Init(aConfiguration->getStunServers(),
                     aConfiguration->getTurnServers());
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Couldn't initialize media object", __FUNCTION__);
    return res;
  }

  PeerConnectionCtx::GetInstance()->mPeerConnections[mHandle] = this;

  STAMP_TIMECARD(mTimeCard, "Generating DTLS Identity");
  // Create the DTLS Identity
  mIdentity = DtlsIdentity::Generate();
  STAMP_TIMECARD(mTimeCard, "Done Generating DTLS Identity");

  if (!mIdentity) {
    CSFLogError(logTag, "%s: Generate returned NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

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

  const std::string& fpAlg = DtlsIdentity::DEFAULT_HASH_ALGORITHM;
  std::vector<uint8_t> fingerprint;
  res = CalculateFingerprint(fpAlg, fingerprint);
  NS_ENSURE_SUCCESS(res, res);
  res = mJsepSession->AddDtlsFingerprint(fpAlg, fingerprint);
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Couldn't set DTLS credentials, res=%u",
                        __FUNCTION__,
                        static_cast<unsigned>(res));
    return res;
  }

  return NS_OK;
}

class CompareCodecPriority {
  public:
    explicit CompareCodecPriority(int32_t preferredCodec) {
      // This pref really ought to be a string, preferably something like
      // "H264" or "VP8" instead of a payload type.
      // Bug 1101259.
      std::ostringstream os;
      os << preferredCodec;
      mPreferredCodec = os.str();
    }

    bool operator()(JsepCodecDescription* lhs,
                    JsepCodecDescription* rhs) const {
      return !mPreferredCodec.empty() &&
             lhs->mDefaultPt == mPreferredCodec &&
             rhs->mDefaultPt != mPreferredCodec;
    }

  private:
    std::string mPreferredCodec;
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


  bool hardwareH264Supported = false;

#ifdef MOZ_WEBRTC_OMX
  bool hardwareH264Enabled = false;

  // Check to see if what HW codecs are available (not in use) at this moment.
  // Note that streaming video decode can reserve a decoder

  // XXX See bug 1018791 Implement W3 codec reservation policy
  // Note that currently, OMXCodecReservation needs to be held by an sp<> because it puts
  // 'this' into an sp<EventListener> to talk to the resource reservation code

  // This pref is a misnomer; it is solely for h264 _hardware_ support.
  branch->GetBoolPref("media.peerconnection.video.h264_enabled",
                      &hardwareH264Enabled);

  if (hardwareH264Enabled) {
    // Ok, it is preffed on. Can we actually do it?
    android::sp<android::OMXCodecReservation> encode = new android::OMXCodecReservation(true);
    android::sp<android::OMXCodecReservation> decode = new android::OMXCodecReservation(false);

    // Currently we just check if they're available right now, which will fail if we're
    // trying to call ourself, for example.  It will work for most real-world cases, like
    // if we try to add a person to a 2-way call to make a 3-way mesh call
    if (encode->ReserveOMXCodec() && decode->ReserveOMXCodec()) {
      CSFLogDebug( logTag, "%s: H264 hardware codec available", __FUNCTION__);
      hardwareH264Supported = true;
    }
  }

#endif // MOZ_WEBRTC_OMX

#ifdef MOZILLA_INTERNAL_API
  bool softwareH264Enabled = PeerConnectionCtx::GetInstance()->gmpHasH264();
#else
  // For unit-tests
  bool softwareH264Enabled = true;
#endif

  bool h264Enabled = hardwareH264Supported || softwareH264Enabled;

  auto codecs = mJsepSession->Codecs();

  // Set parameters
  for (auto i = codecs.begin(); i != codecs.end(); ++i) {
    auto &codec = **i;
    switch (codec.mType) {
      case SdpMediaSection::kAudio:
        // Nothing to configure here, for now.
        break;
      case SdpMediaSection::kVideo:
        {
          JsepVideoCodecDescription& videoCodec =
            static_cast<JsepVideoCodecDescription&>(codec);

          if (videoCodec.mName == "H264") {
            int32_t level = 13; // minimum suggested for WebRTC spec
            branch->GetIntPref("media.navigator.video.h264.level", &level);
            level &= 0xFF;
            // Override level
            videoCodec.mProfileLevelId &= 0xFFFF00;
            videoCodec.mProfileLevelId |= level;

            int32_t maxBr = 0; // Unlimited
            branch->GetIntPref("media.navigator.video.h264.max_br", &maxBr);
            videoCodec.mMaxBr = maxBr;

            int32_t maxMbps = 0; // Unlimited
#ifdef MOZ_WEBRTC_OMX
            maxMbps = 11880;
#endif
            branch->GetIntPref("media.navigator.video.h264.max_mbps",
                               &maxMbps);
            videoCodec.mMaxBr = maxMbps;

            // Might disable it, but we set up other params anyway
            videoCodec.mEnabled = h264Enabled;

            if (videoCodec.mPacketizationMode == 0 && !softwareH264Enabled) {
              // We're assuming packetization mode 0 is unsupported by
              // hardware.
              videoCodec.mEnabled = false;
            }
          } else if (codec.mName == "VP8") {
            int32_t maxFs = 0;
            branch->GetIntPref("media.navigator.video.max_fs", &maxFs);
            if (maxFs <= 0) {
              maxFs = 12288; // We must specify something other than 0
            }
            videoCodec.mMaxFs = maxFs;

            int32_t maxFr = 0;
            branch->GetIntPref("media.navigator.video.max_fr", &maxFr);
            if (maxFr <= 0) {
              maxFr = 60; // We must specify something other than 0
            }
            videoCodec.mMaxFr = maxFr;
          }
        }
        break;
      case SdpMediaSection::kText:
      case SdpMediaSection::kApplication:
      case SdpMediaSection::kMessage:
        {} // Nothing to configure for these.
    }
  }

  // Sort by priority
  int32_t preferredCodec = 0;
  branch->GetIntPref("media.navigator.video.preferred_codec",
                     &preferredCodec);

  if (preferredCodec) {
    CompareCodecPriority comparator(preferredCodec);
    std::stable_sort(codecs.begin(), codecs.end(), comparator);
  }

  return NS_OK;
}

RefPtr<DtlsIdentity> const
PeerConnectionImpl::GetIdentity() const
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mIdentity;
}

nsresult
PeerConnectionImpl::CreateFakeMediaStream(uint32_t aHint, DOMMediaStream** aRetval)
{
  MOZ_ASSERT(aRetval);
  PC_AUTO_ENTER_API_CALL(false);

  bool mute = false;

  // Hack to allow you to mute the stream
  if (aHint & MEDIA_STREAM_MUTE) {
    mute = true;
    aHint &= ~MEDIA_STREAM_MUTE;
  }

  nsRefPtr<DOMMediaStream> stream = MakeMediaStream(aHint);
  if (!stream) {
    return NS_ERROR_FAILURE;
  }

  if (!mute) {
    if (aHint & DOMMediaStream::HINT_CONTENTS_AUDIO) {
      new Fake_AudioGenerator(stream);
    } else {
#ifdef MOZILLA_INTERNAL_API
    new Fake_VideoGenerator(stream);
#endif
    }
  }

  stream.forget(aRetval);
  return NS_OK;
}

// Data channels won't work without a window, so in order for the C++ unit
// tests to work (it doesn't have a window available) we ifdef the following
// two implementations.
NS_IMETHODIMP
PeerConnectionImpl::EnsureDataConnection(uint16_t aNumstreams)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

#ifdef MOZILLA_INTERNAL_API
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

  for (size_t j = 0; j < mJsepSession->GetNegotiatedTrackPairCount(); ++j) {
    const JsepTrackPair* trackPair;
    nsresult res = mJsepSession->GetNegotiatedTrackPair(j, &trackPair);

    if (NS_SUCCEEDED(res) &&
        trackPair->mSending && // Assumes we don't do recvonly datachannel
        trackPair->mSending->GetMediaType() == SdpMediaSection::kApplication) {

      if (!trackPair->mSending->GetNegotiatedDetails()->GetCodecCount()) {
        CSFLogError(logTag, "%s: Negotiated m=application with no codec. "
                            "This is likely to be broken.",
                            __FUNCTION__);
        return NS_ERROR_FAILURE;
      }

      for (size_t i = 0;
           i < trackPair->mSending->GetNegotiatedDetails()->GetCodecCount();
           ++i) {
        const JsepCodecDescription* codec;
        res = trackPair->mSending->GetNegotiatedDetails()->GetCodec(i, &codec);

        if (NS_FAILED(res)) {
          CSFLogError(logTag, "%s: Failed getting codec for m=application.",
                              __FUNCTION__);
          continue;
        }

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
        *level = static_cast<uint16_t>(trackPair->mLevel);
        return NS_OK;
      }
    }
  }

  *datachannelCodec = nullptr;
  *level = 0;
  return NS_OK;
}

nsresult
PeerConnectionImpl::InitializeDataChannel()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  CSFLogDebug(logTag, "%s", __FUNCTION__);

  const JsepApplicationCodecDescription* codec;
  uint16_t level;
  nsresult rv = GetDatachannelParameters(&codec, &level);

  NS_ENSURE_SUCCESS(rv, rv);

  if (!codec) {
    CSFLogDebug(logTag, "%s: We did not negotiate datachannel", __FUNCTION__);
    return NS_OK;
  }

#ifdef MOZILLA_INTERNAL_API
  uint32_t channels = codec->mChannels;
  if (channels > MAX_NUM_STREAMS) {
    channels = MAX_NUM_STREAMS;
  }

  rv = EnsureDataConnection(codec->mChannels);
  if (NS_SUCCEEDED(rv)) {
    uint16_t localport = 5000;
    uint16_t remoteport = 0;
    // The logic that reflects the remote payload type is what sets the remote
    // port here.
    if (!codec->GetPtAsInt(&remoteport)) {
      return NS_ERROR_FAILURE;
    }

    // use the specified TransportFlow
    nsRefPtr<TransportFlow> flow = mMedia->GetTransportFlow(level, false).get();
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
#ifdef MOZILLA_INTERNAL_API
  nsRefPtr<nsDOMDataChannel> result;
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
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aRetval);

#ifdef MOZILLA_INTERNAL_API
  nsRefPtr<DataChannel> dataChannel;
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
          JsepTrack::kJsepTrackSending));

    rv = mJsepSession->AddTrack(track);
    if (NS_FAILED(rv)) {
      CSFLogError(logTag, "%s: Failed to add application track.",
                          __FUNCTION__);
      return rv;
    }
    mHaveDataStream = true;
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
//  nsRefPtr<nsSupportsWeakReference> tmp2 = do_QueryObject(tmp);
//  nsRefPtr<PeerConnectionObserver> pco = static_cast<PeerConnectionObserver*>(&*tmp2);

static already_AddRefed<PeerConnectionObserver>
do_QueryObjectReferent(nsIWeakReference* aRawPtr) {
  nsCOMPtr<nsISupportsWeakReference> tmp = do_QueryReferent(aRawPtr);
  if (!tmp) {
    return nullptr;
  }
  nsRefPtr<nsSupportsWeakReference> tmp2 = do_QueryObject(tmp);
  nsRefPtr<PeerConnectionObserver> tmp3 = static_cast<PeerConnectionObserver*>(&*tmp2);
  return tmp3.forget();
}


#ifdef MOZILLA_INTERNAL_API
// Not a member function so that we don't need to keep the PC live.
static void NotifyDataChannel_m(nsRefPtr<nsIDOMDataChannel> aChannel,
                                nsRefPtr<PeerConnectionObserver> aObserver)
{
  MOZ_ASSERT(NS_IsMainThread());
  JSErrorResult rv;
  nsRefPtr<nsDOMDataChannel> channel = static_cast<nsDOMDataChannel*>(&*aChannel);
  aObserver->NotifyDataChannel(*channel, rv);
  NS_DataChannelAppReady(aChannel);
}
#endif

void
PeerConnectionImpl::NotifyDataChannel(already_AddRefed<DataChannel> aChannel)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  // XXXkhuey this is completely fucked up.  We can't use nsRefPtr<DataChannel>
  // here because DataChannel's AddRef/Release are non-virtual and not visible
  // if !MOZILLA_INTERNAL_API, but this function leaks the DataChannel if
  // !MOZILLA_INTERNAL_API because it never transfers the ref to
  // NS_NewDOMDataChannel.
  DataChannel* channel = aChannel.take();
  MOZ_ASSERT(channel);

  CSFLogDebug(logTag, "%s: channel: %p", __FUNCTION__, channel);

#ifdef MOZILLA_INTERNAL_API
  nsCOMPtr<nsIDOMDataChannel> domchannel;
  nsresult rv = NS_NewDOMDataChannel(already_AddRefed<DataChannel>(channel),
                                     mWindow, getter_AddRefs(domchannel));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
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
#ifdef MOZILLA_INTERNAL_API
  if (aOptions.mOfferToReceiveAudio.WasPassed()) {
    options.mOfferToReceiveAudio =
      mozilla::Some(size_t(aOptions.mOfferToReceiveAudio.Value()));
  }

  if (aOptions.mOfferToReceiveVideo.WasPassed()) {
    options.mOfferToReceiveVideo =
        mozilla::Some(size_t(aOptions.mOfferToReceiveVideo.Value()));
  }

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

  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
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

  nsresult nrv = ConfigureJsepSessionCodecs();
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

  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }

  CSFLogDebug(logTag, "CreateAnswer()");
  STAMP_TIMECARD(mTimeCard, "Create Answer");
  // TODO(bug 1098015): Once RTCAnswerOptions is standardized, we'll need to
  // add it as a param to CreateAnswer, and convert it here.
  JsepAnswerOptions options;
  std::string answer;

  nsresult nrv = mJsepSession->CreateAnswer(options, &answer);
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

NS_IMETHODIMP
PeerConnectionImpl::SetLocalDescription(int32_t aAction, const char* aSDP)
{
  PC_AUTO_ENTER_API_CALL(true);

  if (!aSDP) {
    CSFLogError(logTag, "%s - aSDP is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSErrorResult rv;
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }

  STAMP_TIMECARD(mTimeCard, "Set Local Description");

#ifdef MOZILLA_INTERNAL_API
  bool isolated = mMedia->AnyLocalStreamHasPeerIdentity();
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

  UpdateSignalingState();
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

NS_IMETHODIMP
PeerConnectionImpl::SetRemoteDescription(int32_t action, const char* aSDP)
{
  PC_AUTO_ENTER_API_CALL(true);

  if (!aSDP) {
    CSFLogError(logTag, "%s - aSDP is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  JSErrorResult jrv;
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
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
    // Add the tracks. This code is pretty complicated because the tracks
    // come in arbitrary orders and we want to group them by streamId.
    // We go through all the tracks and then for each track that represents
    // a new stream id, go through the rest of the tracks and deal with
    // them at once.
    size_t numTracks = mJsepSession->GetRemoteTrackCount();
    MOZ_ASSERT(numTracks <= 3);
    bool hasAudio = false;
    bool hasVideo = false;

    std::set<std::string> streamsToNotify;
    for (size_t i = 0; i < numTracks; ++i) {
      RefPtr<JsepTrack> track;
      nrv = mJsepSession->GetRemoteTrack(i, &track);
      if (NS_FAILED(nrv)) {
        pco->OnSetRemoteDescriptionError(kInternalError,
                                         ObString("GetRemoteTrack failed"),
                                         jrv);
        return NS_OK;
      }

      if (track->GetMediaType() == mozilla::SdpMediaSection::kApplication) {
        // Ignore datachannel
        continue;
      }

      nsRefPtr<RemoteSourceStreamInfo> info =
        mMedia->GetRemoteStreamById(track->GetStreamId());
      if (!info) {
        nsresult nrv = CreateRemoteSourceStreamInfo(&info, track->GetStreamId());
        if (NS_FAILED(nrv)) {
          pco->OnSetRemoteDescriptionError(
              kInternalError,
              ObString("CreateRemoteSourceStreamInfo failed"),
              jrv);
          return NS_OK;
        }

        nrv = mMedia->AddRemoteStream(info);
        if (NS_FAILED(nrv)) {
          pco->OnSetRemoteDescriptionError(
              kInternalError,
              ObString("AddRemoteStream failed"),
              jrv);
          return NS_OK;
        }
      }

      streamsToNotify.insert(track->GetStreamId());

      if (track->GetMediaType() == mozilla::SdpMediaSection::kAudio) {
        MOZ_ASSERT(!hasAudio);
        (void)hasAudio;
        hasAudio = true;
        info->mTrackTypeHints |= DOMMediaStream::HINT_CONTENTS_AUDIO;
      } else if (track->GetMediaType() == mozilla::SdpMediaSection::kVideo) {
        MOZ_ASSERT(!hasVideo);
        (void)hasVideo;
        hasVideo = true;
        info->mTrackTypeHints |= DOMMediaStream::HINT_CONTENTS_VIDEO;
      }
    }

    for (auto i = streamsToNotify.begin(); i != streamsToNotify.end(); ++i) {
      // Now that the streams are all set up, notify about track availability.
      // TODO(bug 1017888): Suppress on renegotiation when no change.
      nsRefPtr<RemoteSourceStreamInfo> info =
        mMedia->GetRemoteStreamById(*i);
      MOZ_ASSERT(info);

#ifdef MOZILLA_INTERNAL_API
      TracksAvailableCallback* tracksAvailableCallback =
        new TracksAvailableCallback(info->mTrackTypeHints, pco);
      info->GetMediaStream()->OnTracksAvailable(tracksAvailableCallback);
#else
      pco->OnAddStream(info->GetMediaStream(), jrv);
#endif
    }

    pco->OnSetRemoteDescriptionSuccess(jrv);
#ifdef MOZILLA_INTERNAL_API
    startCallTelem();
#endif
  }

  UpdateSignalingState();
  return NS_OK;
}

// WebRTC uses highres time relative to the UNIX epoch (Jan 1, 1970, UTC).

#ifdef MOZILLA_INTERNAL_API
nsresult
PeerConnectionImpl::GetTimeSinceEpoch(DOMHighResTimeStamp *result) {
  MOZ_ASSERT(NS_IsMainThread());
  nsPerformance *perf = mWindow->GetPerformance();
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

#ifdef MOZILLA_INTERNAL_API
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
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }

  STAMP_TIMECARD(mTimeCard, "Add Ice Candidate");

  CSFLogDebug(logTag, "AddIceCandidate: %s", aCandidate);

#ifdef MOZILLA_INTERNAL_API
  // When remote candidates are added before our ICE ctx is up and running
  // (the transition to New is async through STS, so this is not impossible),
  // we won't record them as trickle candidates. Is this what we want?
  if(!mIceStartTime.IsNull()) {
    TimeDuration timeDelta = TimeStamp::Now() - mIceStartTime;
    if (mIceConnectionState == PCImplIceConnectionState::Failed) {
      Telemetry::Accumulate(Telemetry::WEBRTC_ICE_LATE_TRICKLE_ARRIVAL_TIME,
                            timeDelta.ToMilliseconds());
    } else {
      Telemetry::Accumulate(Telemetry::WEBRTC_ICE_ON_TIME_TRICKLE_ARRIVAL_TIME,
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

#ifdef MOZILLA_INTERNAL_API
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
    mMedia->UpdateSinkIdentity_m(doc->NodePrincipal(), mPeerIdentity);
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
#ifdef MOZILLA_INTERNAL_API
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

#ifdef MOZILLA_INTERNAL_API
void
PeerConnectionImpl::PrincipalChanged(DOMMediaStream* aMediaStream) {
  nsIDocument* doc = GetWindow()->GetExtantDoc();
  if (doc) {
    mMedia->UpdateSinkIdentity_m(doc->NodePrincipal(), mPeerIdentity);
  } else {
    CSFLogInfo(logTag, "Can't update sink principal; document gone");
  }
}
#endif

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
  if (!aMediaStream.HasTrack(aTrack)) {
    CSFLogError(logTag, "%s: Track is not in stream", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  uint32_t hints = aMediaStream.GetHintContents() &
      ((aTrack.AsAudioStreamTrack()? DOMMediaStream::HINT_CONTENTS_AUDIO : 0) |
       (aTrack.AsVideoStreamTrack()? DOMMediaStream::HINT_CONTENTS_VIDEO : 0));

  // XXX Remove this check once addStream has an error callback
  // available and/or we have plumbing to handle multiple
  // local audio streams.  bug 1056650
  if ((hints & DOMMediaStream::HINT_CONTENTS_AUDIO) &&
      mNumAudioStreams > 0) {
    CSFLogError(logTag, "%s: Only one local audio stream is supported for now",
                __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  // XXX Remove this check once addStream has an error callback
  // available and/or we have plumbing to handle multiple
  // local video streams. bug 1056650
  if ((hints & DOMMediaStream::HINT_CONTENTS_VIDEO) &&
      mNumVideoStreams > 0) {
    CSFLogError(logTag, "%s: Only one local video stream is supported for now",
                __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  uint32_t num = mMedia->LocalStreamsLength();

  std::string streamId;
  // TODO(bug 1089798): These ids should really come from the MS.
  nsresult res = mMedia->AddStream(&aMediaStream, hints, &streamId);
  if (NS_FAILED(res)) {
    return res;
  }

  if (num != mMedia->LocalStreamsLength()) {
    aMediaStream.AddPrincipalChangeObserver(this);
  }

  if (hints & DOMMediaStream::HINT_CONTENTS_AUDIO) {
    res = mJsepSession->AddTrack(new JsepTrack(
        mozilla::SdpMediaSection::kAudio,
        streamId,
        "audio_track_id",
        JsepTrack::kJsepTrackSending));
    if (NS_FAILED(res)) {
      std::string errorString = mJsepSession->GetLastError();
      CSFLogError(logTag, "%s (audio) : pc = %s, error = %s",
                  __FUNCTION__, mHandle.c_str(), errorString.c_str());
      return NS_ERROR_FAILURE;
    }
    mNumAudioStreams++;
  }

  if (hints & DOMMediaStream::HINT_CONTENTS_VIDEO) {
    res = mJsepSession->AddTrack(new JsepTrack(
        mozilla::SdpMediaSection::kVideo,
        streamId,
        "video_track_id",
        JsepTrack::kJsepTrackSending));
    if (NS_FAILED(res)) {
      std::string errorString = mJsepSession->GetLastError();
      CSFLogError(logTag, "%s (video) : pc = %s, error = %s",
                  __FUNCTION__, mHandle.c_str(), errorString.c_str());
      return NS_ERROR_FAILURE;
    }
    mNumVideoStreams++;
  }
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::RemoveTrack(MediaStreamTrack& aTrack) {
  MOZ_CRASH(); // TODO(bug 1021647): Implement and expose RemoveTrack
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::ReplaceTrack(MediaStreamTrack& aThisTrack,
                                 MediaStreamTrack& aWithTrack,
                                 DOMMediaStream& aStream) {
  PC_AUTO_ENTER_API_CALL(true);

  // TODO: Do an aStream.HasTrack() check on both track args someday.
  //
  // The proposed API will be that both tracks must already be in the same
  // stream. However, since our MediaStreams currently are limited to one
  // track per type, we allow replacement with an outside track not already
  // in the same stream. This works because sync happens receiver-side and
  // timestamps are tied to capture.
  //
  // Since a track may be replaced more than once, the track being replaced
  // may not be in the stream either, so we check neither arg right now.

  // XXX This MUST be addressed when we add multiple tracks of a type!!
  // This is needed because the track IDs used by MSG are from TrackUnion
  // (for getUserMedia streams) and aren't the same as the values the source tracks
  // have.  Solution is to have JsepSession read track ids and use those.

  // Because DirectListeners see the SourceMediaStream's TrackID's, and not the
  // TrackUnionStream's TrackID's, this value won't currently match what is used in
  // MediaPipelineTransmit.  Bug 1056652
  //  TrackID thisID = aThisTrack.GetTrackID();
  TrackID withID = aWithTrack.GetTrackID();

  bool success = false;
  for(uint32_t i = 0; i < media()->LocalStreamsLength(); ++i) {
    LocalSourceStreamInfo *info = media()->GetLocalStreamByIndex(i);
    // XXX use type instead of TrackID - bug 1056650
    int pipeline = info->HasTrackType(&aStream, !!(aThisTrack.AsVideoStreamTrack()));
    if (pipeline >= 0) {
      // XXX GetStream() will likely be invalid once a track can be in more than one
      info->ReplaceTrack(pipeline, aWithTrack.GetStream(), withID);
      success = true;
      break;
    }
  }
  if (!success) {
    return NS_ERROR_FAILURE;
  }

  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_ERROR_UNEXPECTED;
  }
  JSErrorResult rv;

  if (success) {
    pco->OnReplaceTrackSuccess(rv);
  } else {
    pco->OnReplaceTrackError(kInternalError,
                             ObString("Failed to replace track"),
                             rv);
  }
  if (rv.Failed()) {
    CSFLogError(logTag, "Error firing replaceTrack callback");
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult
PeerConnectionImpl::CalculateFingerprint(
    const std::string& algorithm,
    std::vector<uint8_t>& fingerprint) const {
  uint8_t buf[DtlsIdentity::HASH_ALGORITHM_MAX_LENGTH];
  size_t len = 0;
  nsresult rv = mIdentity->ComputeFingerprint(algorithm, &buf[0], sizeof(buf),
                                              &len);
  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "Unable to calculate certificate fingerprint, rv=%u",
                        static_cast<unsigned>(rv));
    return rv;
  }
  MOZ_ASSERT(len > 0 && len <= DtlsIdentity::HASH_ALGORITHM_MAX_LENGTH);
  fingerprint.assign(buf, buf + len);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetFingerprint(char** fingerprint)
{
  MOZ_ASSERT(fingerprint);
  MOZ_ASSERT(mIdentity);
  std::vector<uint8_t> fp;
  nsresult rv = CalculateFingerprint(DtlsIdentity::DEFAULT_HASH_ALGORITHM, fp);
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
PeerConnectionImpl::PluginCrash(uint64_t aPluginID,
                                const nsAString& aPluginName,
                                const nsAString& aPluginDumpID)
{
  // fire an event to the DOM window if this is "ours"
  bool result = mMedia ? mMedia->AnyCodecHasPluginID(aPluginID) : false;
  if (!result) {
    return false;
  }

  CSFLogError(logTag, "%s: Our plugin %llu crashed", __FUNCTION__, static_cast<unsigned long long>(aPluginID));

#ifdef MOZILLA_INTERNAL_API
  nsCOMPtr<nsIDocument> doc = mWindow->GetExtantDoc();
  if (!doc) {
    NS_WARNING("Couldn't get document for PluginCrashed event!");
    return true;
  }

  PluginCrashedEventInit init;
  init.mPluginDumpID = aPluginDumpID;
  init.mPluginName = aPluginName;
  init.mSubmittedCrashReport = false;
  init.mGmpPlugin = true;
  init.mBubbles = true;
  init.mCancelable = true;

  nsRefPtr<PluginCrashedEvent> event =
    PluginCrashedEvent::Constructor(doc, NS_LITERAL_STRING("PluginCrashed"), init);

  event->SetTrusted(true);
  event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;

  EventDispatcher::DispatchDOMEvent(mWindow, nullptr, event, nullptr, nullptr);
#endif

  return true;
}

nsresult
PeerConnectionImpl::CloseInt()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  // We do this at the end of the call because we want to make sure we've waited
  // for all trickle ICE candidates to come in; this can happen well after we've
  // transitioned to connected. As a bonus, this allows us to detect race
  // conditions where a stats dispatch happens right as the PC closes.
  RecordLongtermICEStatistics();
  CSFLogInfo(logTag, "%s: Closing PeerConnectionImpl %s; "
             "ending call", __FUNCTION__, mHandle.c_str());
  if (mJsepSession) {
    mJsepSession->Close();
  }
#ifdef MOZILLA_INTERNAL_API
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

#ifdef MOZILLA_INTERNAL_API
  // before we destroy references to local streams, detach from them
  for(uint32_t i = 0; i < media()->LocalStreamsLength(); ++i) {
    LocalSourceStreamInfo *info = media()->GetLocalStreamByIndex(i);
    info->GetMediaStream()->RemovePrincipalChangeObserver(this);
  }

  // End of call to be recorded in Telemetry
  if (!mStartTime.IsNull()){
    TimeDuration timeDelta = TimeStamp::Now() - mStartTime;
    Telemetry::Accumulate(Telemetry::WEBRTC_CALL_DURATION, timeDelta.ToSeconds());
  }
#endif

  // Forget the reference so that we can transfer it to
  // SelfDestruct().
  mMedia.forget().take()->SelfDestruct();
}

#ifdef MOZILLA_INTERNAL_API
// If NSS is shutting down, then we need to get rid of the DTLS
// identity right now; otherwise, we'll cause wreckage when we do
// finally deallocate it in our destructor.
void
PeerConnectionImpl::virtualDestroyNSSReference()
{
  destructorSafeDestroyNSSReference();
}

void
PeerConnectionImpl::destructorSafeDestroyNSSReference()
{
  MOZ_ASSERT(NS_IsMainThread());
  CSFLogDebug(logTag, "%s: NSS shutting down; freeing our DtlsIdentity.", __FUNCTION__);
  mIdentity = nullptr;
}
#endif

void
PeerConnectionImpl::SetSignalingState_m(PCImplSignalingState aSignalingState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  if (mSignalingState == aSignalingState ||
      mSignalingState == PCImplSignalingState::SignalingClosed) {
    return;
  }

  mSignalingState = aSignalingState;

  if (mSignalingState == PCImplSignalingState::SignalingHaveLocalOffer ||
      mSignalingState == PCImplSignalingState::SignalingStable) {
    mMedia->UpdateTransports(*mJsepSession);
  }

  if (mSignalingState == PCImplSignalingState::SignalingStable) {
    mMedia->UpdateMediaPipelines(*mJsepSession);
    InitializeDataChannel();
    mMedia->StartIceChecks(*mJsepSession);
  }

  if (mSignalingState == PCImplSignalingState::SignalingClosed) {
    CloseInt();
  }

  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }
  JSErrorResult rv;
  pco->OnStateChange(PCObserverStateType::SignalingState, rv);
}

void
PeerConnectionImpl::UpdateSignalingState() {
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

  SetSignalingState_m(newState);
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

  // TODO: What about mid? Is this something that we will choose, or will
  // JsepSession choose for us? If the latter, we'll need to make it an
  // outparam or something. Bug 1051052.
  std::string mid;

  nsresult res = mJsepSession->AddLocalIceCandidate(candidate, mid, level);

  if (NS_FAILED(res)) {
    std::string errorString = mJsepSession->GetLastError();

    CSFLogError(logTag, "Failed to incorporate local candidate into SDP:"
                        " res = %u, candidate = %s, level = %u, error = %s",
                        static_cast<unsigned>(res),
                        candidate.c_str(),
                        static_cast<unsigned>(level),
                        errorString.c_str());
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
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(weakPCObserver);
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

#ifdef MOZILLA_INTERNAL_API
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

#ifdef MOZILLA_INTERNAL_API
  if (!isDone(mIceConnectionState) && isDone(domState)) {
    // mIceStartTime can be null if going directly from New to Closed, in which
    // case we don't count it as a success or a failure.
    if (!mIceStartTime.IsNull()){
      TimeDuration timeDelta = TimeStamp::Now() - mIceStartTime;
      if (isSucceeded(domState)) {
        Telemetry::Accumulate(Telemetry::WEBRTC_ICE_SUCCESS_TIME,
                              timeDelta.ToMilliseconds());
      } else if (isFailed(domState)) {
        Telemetry::Accumulate(Telemetry::WEBRTC_ICE_FAILURE_TIME,
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

  // Would be nice if we had a means of converting one of these dom enums
  // to a string that wasn't almost as much text as this switch statement...
  switch (mIceConnectionState) {
    case PCImplIceConnectionState::New:
      STAMP_TIMECARD(mTimeCard, "Ice state: new");
      break;
    case PCImplIceConnectionState::Checking:
#ifdef MOZILLA_INTERNAL_API
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

  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
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

  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
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
PeerConnectionImpl::EndOfLocalCandidates(const std::string& defaultAddr,
                                         uint16_t defaultPort,
                                         uint16_t level) {
  CSFLogDebug(logTag, "%s", __FUNCTION__);
  mJsepSession->EndOfLocalCandidates(defaultAddr, defaultPort, level);
}

#ifdef MOZILLA_INTERNAL_API
nsresult
PeerConnectionImpl::BuildStatsQuery_m(
    mozilla::dom::MediaStreamTrack *aSelector,
    RTCStatsQuery *query) {

  if (!HasMedia()) {
    return NS_OK;
  }

  if (!mMedia->ice_ctx() || !mThread) {
    CSFLogError(logTag, "Could not build stats query, critical components of "
                        "PeerConnectionImpl not set.");
    return NS_ERROR_UNEXPECTED;
  }

  nsresult rv = GetTimeSinceEpoch(&(query->now));

  if (NS_FAILED(rv)) {
    CSFLogError(logTag, "Could not build stats query, could not get timestamp");
    return rv;
  }

  // We do not use the pcHandle here, since that's risky to expose to content.
  query->report = new RTCStatsReportInternalConstruct(
      NS_ConvertASCIItoUTF16(mName.c_str()),
      query->now);

  query->iceStartTime = mIceStartTime;
  query->failed = isFailed(mIceConnectionState);

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

  for (int i = 0, len = mMedia->LocalStreamsLength(); i < len; i++) {
    auto& pipelines = mMedia->GetLocalStreamByIndex(i)->GetPipelines();
    if (aSelector) {
      if (mMedia->GetLocalStreamByIndex(i)->GetMediaStream()->
          HasTrack(*aSelector)) {
        // XXX use type instead of TrackID - bug 1056650
        for (auto it = pipelines.begin(); it != pipelines.end(); ++it) {
          if (it->second->IsVideo() == !!aSelector->AsVideoStreamTrack()) {
            query->pipelines.AppendElement(it->second);
          }
        }
      }
    } else {
      for (auto it = pipelines.begin(); it != pipelines.end(); ++it) {
        query->pipelines.AppendElement(it->second);
      }
    }
  }

  for (size_t i = 0, len = mMedia->RemoteStreamsLength(); i < len; i++) {
    auto& pipelines = mMedia->GetRemoteStreamByIndex(i)->GetPipelines();
    if (aSelector) {
      if (mMedia->GetRemoteStreamByIndex(i)->
          GetMediaStream()->HasTrack(*aSelector)) {
        for (auto it = pipelines.begin(); it != pipelines.end(); ++it) {
          if (it->second->trackid() == aSelector->GetTrackID()) {
            query->pipelines.AppendElement(it->second);
          }
        }
      }
    } else {
      for (auto it = pipelines.begin(); it != pipelines.end(); ++it) {
        query->pipelines.AppendElement(it->second);
      }
    }
  }

  query->iceCtx = mMedia->ice_ctx();

  // From the list of MediaPipelines, determine the set of NrIceMediaStreams
  // we are interested in.
  std::set<size_t> levelsToGrab;
  if (aSelector) {
    for (size_t p = 0; p < query->pipelines.Length(); ++p) {
      size_t level = query->pipelines[p]->level();
      levelsToGrab.insert(level);
    }
  } else {
    // We want to grab all streams, so ignore the pipelines (this also ends up
    // grabbing DataChannel streams, which is what we want)
    for (size_t s = 0; s < mMedia->num_ice_media_streams(); ++s) {
      levelsToGrab.insert(s);
    }
  }

  for (auto s = levelsToGrab.begin(); s != levelsToGrab.end(); ++s) {
    // TODO(bcampen@mozilla.com): I may need to revisit this for bundle.
    // (Bug 786234)
    RefPtr<NrIceMediaStream> temp(mMedia->ice_media_stream(*s));
    RefPtr<TransportFlow> flow(mMedia->GetTransportFlow(*s, false));
    // flow can be null for unused levels, such as unused DataChannels
    if (temp && flow) {
      query->streams.AppendElement(temp);
    }
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
    report->mIceCandidateStats.Value().AppendElement(cand);
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
    s.mMozPriority.Construct(p->priority);
    s.mSelected.Construct(p->selected);
    s.mState.Construct(RTCStatsIceCandidatePairState(p->state));
    report->mIceCandidatePairStats.Value().AppendElement(s);
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
    idstr.AppendInt(mp.trackid());

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
            query->report->mInboundRTPStreamStats.Value().AppendElement(s);
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
          query->report->mOutboundRTPStreamStats.Value().AppendElement(s);
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
            query->report->mOutboundRTPStreamStats.Value().AppendElement(s);
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
        query->report->mInboundRTPStreamStats.Value().AppendElement(s);
        break;
      }
    }
  }

  // Gather stats from ICE
  for (size_t s = 0; s != query->streams.Length(); ++s) {
    RecordIceStats_s(*query->streams[s],
                     query->internalStats,
                     query->now,
                     query->report);
  }

  // NrIceCtx and NrIceMediaStream must be destroyed on STS, so it is not safe
  // to dispatch them back to main.
  // We clear streams first to maintain destruction order
  query->streams.Clear();
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
    nsRefPtr<PeerConnectionObserver> pco =
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
#ifdef MOZILLA_INTERNAL_API
  WebrtcGlobalInformation::StoreLongTermICEStatistics(*this);
#endif
}

void
PeerConnectionImpl::IceStreamReady(NrIceMediaStream *aStream)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aStream);

  CSFLogDebug(logTag, "%s: %s", __FUNCTION__, aStream->name().c_str());
}

#ifdef MOZILLA_INTERNAL_API
//Telemetry for when calls start
void
PeerConnectionImpl::startCallTelem() {
  // Start time for calls
  mStartTime = TimeStamp::Now();

  // Increment session call counter
  int &cnt = PeerConnectionCtx::GetInstance()->mConnectionCounter;
  Telemetry::GetHistogramById(Telemetry::WEBRTC_CALL_COUNT)->Subtract(cnt);
  cnt++;
  Telemetry::GetHistogramById(Telemetry::WEBRTC_CALL_COUNT)->Add(cnt);
}
#endif

NS_IMETHODIMP
PeerConnectionImpl::GetLocalStreams(nsTArray<nsRefPtr<DOMMediaStream > >& result)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
#ifdef MOZILLA_INTERNAL_API
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
PeerConnectionImpl::GetRemoteStreams(nsTArray<nsRefPtr<DOMMediaStream > >& result)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
#ifdef MOZILLA_INTERNAL_API
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
