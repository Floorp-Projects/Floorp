/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <cerrno>
#include <deque>
#include <sstream>

#include "base/histogram.h"
#include "vcm.h"
#include "CSFLog.h"
#include "timecard.h"
#include "ccapi_call_info.h"
#include "CC_SIPCCCallInfo.h"
#include "ccapi_device_info.h"
#include "CC_SIPCCDeviceInfo.h"
#include "cpr_string.h"
#include "cpr_stdlib.h"

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

#define ICE_PARSING "In RTCConfiguration passed to RTCPeerConnection constructor"

using namespace mozilla;
using namespace mozilla::dom;

typedef PCObserverString ObString;

static const char* logTag = "PeerConnectionImpl";

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

namespace sipcc {

#ifdef MOZILLA_INTERNAL_API
RTCStatsQuery::RTCStatsQuery(bool internal) :
  failed(false),
  internalStats(internal) {
}

RTCStatsQuery::~RTCStatsQuery() {
  MOZ_ASSERT(NS_IsMainThread());
}

#endif

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

NS_IMPL_ISUPPORTS0(PeerConnectionImpl)

#ifdef MOZILLA_INTERNAL_API
JSObject*
PeerConnectionImpl::WrapObject(JSContext* aCx)
{
  return PeerConnectionImplBinding::Wrap(aCx, this);
}
#endif

struct PeerConnectionImpl::Internal {
  CSF::CC_CallPtr mCall;
};

PeerConnectionImpl::PeerConnectionImpl(const GlobalObject* aGlobal)
: mTimeCard(PR_LOG_TEST(signalingLogInfo(),PR_LOG_ERROR) ?
            create_timecard() : nullptr)
  , mInternal(new Internal())
  , mSignalingState(PCImplSignalingState::SignalingStable)
  , mIceConnectionState(PCImplIceConnectionState::New)
  , mIceGatheringState(PCImplIceGatheringState::New)
  , mDtlsConnected(false)
  , mWindow(nullptr)
  , mIdentity(nullptr)
  , mPrivacyRequested(false)
  , mSTSThread(nullptr)
  , mMedia(nullptr)
  , mNumAudioStreams(0)
  , mNumVideoStreams(0)
  , mHaveDataStream(false)
  , mNumMlines(0)
  , mAddCandidateErrorCount(0)
  , mTrickle(true) // TODO(ekr@rtfm.com): Use pref
{
#ifdef MOZILLA_INTERNAL_API
  MOZ_ASSERT(NS_IsMainThread());
  if (aGlobal) {
    mWindow = do_QueryInterface(aGlobal->GetAsSupports());
  }
#endif
  MOZ_ASSERT(mInternal);
  CSFLogInfo(logTag, "%s: PeerConnectionImpl constructor for %s",
             __FUNCTION__, mHandle.c_str());
  STAMP_TIMECARD(mTimeCard, "Constructor Completed");
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
  CloseInt();

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
                                                 aInfo)
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
  remote = new RemoteSourceStreamInfo(stream.forget(), mMedia);
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

  PeerConnectionCtx *pcctx = PeerConnectionCtx::GetInstance();
  MOZ_ASSERT(pcctx);
  STAMP_TIMECARD(mTimeCard, "Done Initializing PC Ctx");

  mInternal->mCall = pcctx->createCall();
  if (!mInternal->mCall.get()) {
    CSFLogError(logTag, "%s: Couldn't Create Call Object", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

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

  // Connect ICE slots.
  mMedia->SignalIceGatheringStateChange.connect(
      this,
      &PeerConnectionImpl::IceGatheringStateChange);
  mMedia->SignalIceConnectionStateChange.connect(
      this,
      &PeerConnectionImpl::IceConnectionStateChange);

  mMedia->SignalCandidate.connect(this, &PeerConnectionImpl::CandidateReady_s);

  // Initialize the media object.
  res = mMedia->Init(aConfiguration->getStunServers(),
                     aConfiguration->getTurnServers());
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Couldn't initialize media object", __FUNCTION__);
    return res;
  }

  // Store under mHandle
  mInternal->mCall->setPeerConnection(mHandle);
  PeerConnectionCtx::GetInstance()->mPeerConnections[mHandle] = this;

  STAMP_TIMECARD(mTimeCard, "Generating DTLS Identity");
  // Create the DTLS Identity
  mIdentity = DtlsIdentity::Generate();
  STAMP_TIMECARD(mTimeCard, "Done Generating DTLS Identity");

  if (!mIdentity) {
    CSFLogError(logTag, "%s: Generate returned NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  mFingerprint = mIdentity->GetFormattedFingerprint();
  if (mFingerprint.empty()) {
    CSFLogError(logTag, "%s: unable to get fingerprint", __FUNCTION__);
    return res;
  }

  return NS_OK;
}

RefPtr<DtlsIdentity> const
PeerConnectionImpl::GetIdentity() const
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mIdentity;
}

std::string
PeerConnectionImpl::GetFingerprint() const
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mFingerprint;
}

NS_IMETHODIMP
PeerConnectionImpl::FingerprintSplitHelper(std::string& fingerprint,
    size_t& spaceIdx) const
{
  fingerprint = GetFingerprint();
  spaceIdx = fingerprint.find_first_of(' ');
  if (spaceIdx == std::string::npos) {
    CSFLogError(logTag, "%s: fingerprint is messed up: %s",
        __FUNCTION__, fingerprint.c_str());
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

std::string
PeerConnectionImpl::GetFingerprintAlgorithm() const
{
  std::string fp;
  size_t spc;
  if (NS_SUCCEEDED(FingerprintSplitHelper(fp, spc))) {
    return fp.substr(0, spc);
  }
  return "";
}

std::string
PeerConnectionImpl::GetFingerprintHexValue() const
{
  std::string fp;
  size_t spc;
  if (NS_SUCCEEDED(FingerprintSplitHelper(fp, spc))) {
    return fp.substr(spc + 1);
  }
  return "";
}


nsresult
PeerConnectionImpl::CreateFakeMediaStream(uint32_t aHint, nsIDOMMediaStream** aRetval)
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

// Stubbing this call out for now.
// We can remove it when we are confident of datachannels being started
// correctly on SDP negotiation (bug 852908)
NS_IMETHODIMP
PeerConnectionImpl::ConnectDataConnection(uint16_t aLocalport,
                                          uint16_t aRemoteport,
                                          uint16_t aNumstreams)
{
  return NS_OK; // InitializeDataChannel(aLocalport, aRemoteport, aNumstreams);
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
PeerConnectionImpl::InitializeDataChannel(int track_id,
                                          uint16_t aLocalport,
                                          uint16_t aRemoteport,
                                          uint16_t aNumstreams)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

#ifdef MOZILLA_INTERNAL_API
  nsresult rv = EnsureDataConnection(aNumstreams);
  if (NS_SUCCEEDED(rv)) {
    // use the specified TransportFlow
    nsRefPtr<TransportFlow> flow = mMedia->GetTransportFlow(track_id, false).get();
    CSFLogDebug(logTag, "Transportflow[%d] = %p", track_id, flow.get());
    if (flow) {
      if (mDataConnection->ConnectViaTransportFlow(flow, aLocalport, aRemoteport)) {
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
    // XXX stream_id of 0 might confuse things...
    if (mInternal->mCall->addStream(0, 2, DATA)) {
      return NS_ERROR_FAILURE;
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
  return CreateOffer(SipccOfferOptions(aOptions));
}

static void DeferredCreateOffer(const std::string& aPcHandle,
                                const SipccOfferOptions& aOptions) {
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
PeerConnectionImpl::CreateOffer(const SipccOfferOptions& aOptions)
{
  PC_AUTO_ENTER_API_CALL(true);

  JSErrorResult rv;
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

  STAMP_TIMECARD(mTimeCard, "Create Offer");

  cc_media_options_t* cc_options = aOptions.build();
  NS_ENSURE_TRUE(cc_options, NS_ERROR_UNEXPECTED);

  cc_int32_t error = mInternal->mCall->createOffer(cc_options, mTimeCard);

  if (error) {
    std::string error_string;
    mInternal->mCall->getErrorString(&error_string);
    CSFLogError(logTag, "%s: pc = %s, error = %s",
                __FUNCTION__, mHandle.c_str(), error_string.c_str());
    pco->OnCreateOfferError(error, ObString(error_string.c_str()), rv);
  } else {
    std::string sdp;
    mInternal->mCall->getLocalSdp(&sdp);
    pco->OnCreateOfferSuccess(ObString(sdp.c_str()), rv);
  }

  UpdateSignalingState();
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CreateAnswer()
{
  PC_AUTO_ENTER_API_CALL(true);

  JSErrorResult rv;
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }

  STAMP_TIMECARD(mTimeCard, "Create Answer");

  cc_int32_t error = mInternal->mCall->createAnswer(mTimeCard);

  if (error) {
    std::string error_string;
    mInternal->mCall->getErrorString(&error_string);
    CSFLogError(logTag, "%s: pc = %s, error = %s",
                __FUNCTION__, mHandle.c_str(), error_string.c_str());
    pco->OnCreateAnswerError(error, ObString(error_string.c_str()), rv);
  } else {
    std::string sdp;
    mInternal->mCall->getLocalSdp(&sdp);
    pco->OnCreateAnswerSuccess(ObString(sdp.c_str()), rv);
  }

  UpdateSignalingState();
  return NS_OK;
}

static void appendSdpParseErrors(const std::vector<std::string>& aErrors,
                                 std::string* aErrorString,
                                 cc_int32_t* aErrorCode) {
   for (auto i = aErrors.begin(); i != aErrors.end(); ++i) {
     *aErrorString += " | SDP Parsing Error: " + *i;
   }
   if (aErrors.size()) {
     *aErrorCode = PeerConnectionImpl::kInvalidSessionDescription;
   }
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
  cc_int32_t error  = mInternal->mCall->setLocalDescription(
      (cc_jsep_action_t)aAction,
      mLocalRequestedSDP, mTimeCard);

  if (error) {
    std::string error_string;
    mInternal->mCall->getErrorString(&error_string);
    appendSdpParseErrors(mSDPParseErrorMessages, &error_string, &error);
    CSFLogError(logTag, "%s: pc = %s, error = %s",
                __FUNCTION__, mHandle.c_str(), error_string.c_str());
    pco->OnSetLocalDescriptionError(error, ObString(error_string.c_str()), rv);
  } else {
    mInternal->mCall->getLocalSdp(&mLocalSDP);
    pco->OnSetLocalDescriptionSuccess(rv);
    StartTrickle();
  }

  ClearSdpParseErrorMessages();

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

  JSErrorResult rv;
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }

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

  STAMP_TIMECARD(mTimeCard, "Set Remote Description");

  mRemoteRequestedSDP = aSDP;

  cc_int32_t error = mInternal->mCall->setRemoteDescription(
                                         (cc_jsep_action_t)action,
                                         mRemoteRequestedSDP, mTimeCard);

  if (error) {
    std::string error_string;
    mInternal->mCall->getErrorString(&error_string);
    appendSdpParseErrors(mSDPParseErrorMessages, &error_string, &error);
    CSFLogError(logTag, "%s: pc = %s, error = %s",
                __FUNCTION__, mHandle.c_str(), error_string.c_str());
    pco->OnSetRemoteDescriptionError(error, ObString(error_string.c_str()), rv);
  } else {
    mInternal->mCall->getRemoteSdp(&mRemoteSDP);
    pco->OnSetRemoteDescriptionSuccess(rv);
#ifdef MOZILLA_INTERNAL_API
    startCallTelem();
#endif
  }

  ClearSdpParseErrorMessages();

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

  cc_int32_t error = mInternal->mCall->addICECandidate(aCandidate, aMid, aLevel, mTimeCard);

  if (error) {
    OnAddIceCandidateError();
    std::string error_string;
    mInternal->mCall->getErrorString(&error_string);
    CSFLogError(logTag, "%s: pc = %s, error = %s (note, this should not be "
                        "a show-stopper, since whether we incorporate "
                        "candidates into the SDP doesn't really matter since "
                        "we're full trickle)",
                __FUNCTION__, mHandle.c_str(), error_string.c_str());
    pco->OnAddIceCandidateError(error, ObString(error_string.c_str()), rv);
  } else {
    pco->OnAddIceCandidateSuccess(rv);
    mInternal->mCall->getRemoteSdp(&mRemoteSDP);
  }

  UpdateSignalingState();
  return NS_OK;
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

void PeerConnectionImpl::OnRemoteStreamAdded(const MediaStreamTable& aStream) {
  DOMMediaStream* stream = nullptr;

  nsRefPtr<RemoteSourceStreamInfo> mRemoteStreamInfo =
    media()->GetRemoteStream(aStream.media_stream_id);
  MOZ_ASSERT(mRemoteStreamInfo);

  if (!mRemoteStreamInfo) {
    CSFLogError(logTag, "%s: GetRemoteStream returned NULL", __FUNCTION__);
  } else {
    stream = mRemoteStreamInfo->GetMediaStream();
  }

  if (!stream) {
    CSFLogError(logTag, "%s: GetMediaStream returned NULL", __FUNCTION__);
  } else {
    nsRefPtr<PeerConnectionObserver> pco =
      do_QueryObjectReferent(mPCObserver);
    if (!pco) {
      return;
    }

#ifdef MOZILLA_INTERNAL_API
    TracksAvailableCallback* tracksAvailableCallback =
      new TracksAvailableCallback(mRemoteStreamInfo->mTrackTypeHints, pco);

    stream->OnTracksAvailable(tracksAvailableCallback);
#else
    JSErrorResult rv;
    pco->OnAddStream(stream, rv);
#endif
  }
}

NS_IMETHODIMP
PeerConnectionImpl::CloseStreams() {
  PC_AUTO_ENTER_API_CALL(false);

  CSFLogInfo(logTag, "%s: Ending associated call", __FUNCTION__);

  mInternal->mCall->endCall();
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

  uint32_t stream_id;
  nsresult res = mMedia->AddStream(&aMediaStream, hints, &stream_id);
  if (NS_FAILED(res)) {
    return res;
  }

  if (num != mMedia->LocalStreamsLength()) {
    aMediaStream.AddPrincipalChangeObserver(this);
  }

  // TODO(ekr@rtfm.com): these integers should be the track IDs
  if (hints & DOMMediaStream::HINT_CONTENTS_AUDIO) {
    if (mInternal->mCall->addStream(stream_id, 0, AUDIO)) {
      std::string error_string;
      mInternal->mCall->getErrorString(&error_string);
      CSFLogError(logTag, "%s (audio) : pc = %s, error = %s",
                  __FUNCTION__, mHandle.c_str(), error_string.c_str());
      return NS_ERROR_FAILURE;
    }
    mNumAudioStreams++;
  }

  if (hints & DOMMediaStream::HINT_CONTENTS_VIDEO) {
    if (mInternal->mCall->addStream(stream_id, 1, VIDEO)) {
      std::string error_string;
      mInternal->mCall->getErrorString(&error_string);
      CSFLogError(logTag, "%s: (video) pc = %s, error = %s",
                  __FUNCTION__, mHandle.c_str(), error_string.c_str());
      return NS_ERROR_FAILURE;
    }
    mNumVideoStreams++;
  }

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::RemoveTrack(MediaStreamTrack& aTrack) {
  PC_AUTO_ENTER_API_CALL(true);

  DOMMediaStream *stream = nullptr;
  for (uint32_t i = 0; i < mMedia->LocalStreamsLength(); ++i) {
    auto* candidate = mMedia->GetLocalStream(i)->GetMediaStream();
    if (candidate->HasTrack(aTrack)) {
      stream = candidate;
      break;
    }
  }
  if (!stream) {
    CSFLogError(logTag, "%s: Track not found", __FUNCTION__);
    return NS_OK;
  }
  auto& aMediaStream = *stream;

  uint32_t hints = aMediaStream.GetHintContents() &
      ((aTrack.AsAudioStreamTrack()? DOMMediaStream::HINT_CONTENTS_AUDIO : 0) |
       (aTrack.AsVideoStreamTrack()? DOMMediaStream::HINT_CONTENTS_VIDEO : 0));
  uint32_t num = mMedia->LocalStreamsLength();

  uint32_t stream_id;
  nsresult res = mMedia->RemoveStream(&aMediaStream, hints, &stream_id);

  if (NS_FAILED(res)) {
    return res;
  }

  if (num != mMedia->LocalStreamsLength()) {
    aMediaStream.RemovePrincipalChangeObserver(this);
  }

  if (hints & DOMMediaStream::HINT_CONTENTS_AUDIO) {
    if (mInternal->mCall->removeStream(stream_id, 0, AUDIO)) {
      std::string error_string;
      mInternal->mCall->getErrorString(&error_string);
      CSFLogError(logTag, "%s (audio) : pc = %s, error = %s",
                  __FUNCTION__, mHandle.c_str(), error_string.c_str());
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(mNumAudioStreams > 0);
    mNumAudioStreams--;
  }

  if (hints & DOMMediaStream::HINT_CONTENTS_VIDEO) {
    if (mInternal->mCall->removeStream(stream_id, 1, VIDEO)) {
      std::string error_string;
      mInternal->mCall->getErrorString(&error_string);
      CSFLogError(logTag, "%s (video) : pc = %s, error = %s",
                  __FUNCTION__, mHandle.c_str(), error_string.c_str());
      return NS_ERROR_FAILURE;
    }
    MOZ_ASSERT(mNumVideoStreams > 0);
    mNumVideoStreams--;
  }

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
  // have.  Solution is to have SIPCC/VcmSIPCCBinding read track ids and use those.

  // Because DirectListeners see the SourceMediaStream's TrackID's, and not the
  // TrackUnionStream's TrackID's, this value won't currently match what is used in
  // MediaPipelineTransmit.  Bug 1056652
  //  TrackID thisID = aThisTrack.GetTrackID();
  TrackID withID = aWithTrack.GetTrackID();

  bool success = false;
  for(uint32_t i = 0; i < media()->LocalStreamsLength(); ++i) {
    LocalSourceStreamInfo *info = media()->GetLocalStream(i);
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

/*
NS_IMETHODIMP
PeerConnectionImpl::SetRemoteFingerprint(const char* hash, const char* fingerprint)
{
  MOZ_ASSERT(hash);
  MOZ_ASSERT(fingerprint);

  if (fingerprint != nullptr && (strcmp(hash, "sha-1") == 0)) {
    mRemoteFingerprint = std::string(fingerprint);
    CSFLogDebug(logTag, "Setting remote fingerprint to %s", mRemoteFingerprint.c_str());
    return NS_OK;
  } else {
    CSFLogError(logTag, "%s: Invalid Remote Finger Print", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }
}
*/

NS_IMETHODIMP
PeerConnectionImpl::GetFingerprint(char** fingerprint)
{
  MOZ_ASSERT(fingerprint);

  if (!mIdentity) {
    return NS_ERROR_FAILURE;
  }

  char* tmp = new char[mFingerprint.size() + 1];
  std::copy(mFingerprint.begin(), mFingerprint.end(), tmp);
  tmp[mFingerprint.size()] = '\0';

  *fingerprint = tmp;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetLocalDescription(char** aSDP)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aSDP);

  char* tmp = new char[mLocalSDP.size() + 1];
  std::copy(mLocalSDP.begin(), mLocalSDP.end(), tmp);
  tmp[mLocalSDP.size()] = '\0';

  *aSDP = tmp;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetRemoteDescription(char** aSDP)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aSDP);

  char* tmp = new char[mRemoteSDP.size() + 1];
  std::copy(mRemoteSDP.begin(), mRemoteSDP.end(), tmp);
  tmp[mRemoteSDP.size()] = '\0';

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
PeerConnectionImpl::SipccState(PCImplSipccState* aState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  PeerConnectionCtx* pcctx = PeerConnectionCtx::GetInstance();
  // Avoid B2G error: operands to ?: have different types
  // 'mozilla::dom::PCImplSipccState' and 'mozilla::dom::PCImplSipccState::Enum'
  if (pcctx) {
    *aState = pcctx->sipcc_state();
  } else {
    *aState = PCImplSipccState::Idle;
  }
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

  nsresult res = CloseInt();

  return res;
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

  ErrorResult rv;
  nsRefPtr<Event> event =
    doc->CreateEvent(NS_LITERAL_STRING("customevent"), rv);
  nsCOMPtr<nsIDOMCustomEvent> customEvent(do_QueryObject(event));
  if (!customEvent) {
    NS_WARNING("Couldn't QI event for PluginCrashed event!");
    return true;
  }

  nsCOMPtr<nsIWritableVariant> variant;
  variant = do_CreateInstance("@mozilla.org/variant;1");
  if (!variant) {
    NS_WARNING("Couldn't create detail variant for PluginCrashed event!");
    return true;
  }

  customEvent->InitCustomEvent(NS_LITERAL_STRING("PluginCrashed"),
                               true, true, variant);
  event->SetTrusted(true);
  event->GetInternalNSEvent()->mFlags.mOnlyChromeDispatch = true;

  nsCOMPtr<nsIWritablePropertyBag2> propBag;
  propBag = do_CreateInstance("@mozilla.org/hash-property-bag;1");
  if (!propBag) {
    NS_WARNING("Couldn't create a property bag for PluginCrashed event!");
    return true;
  }

  // add a "pluginDumpID" property to this event
  propBag->SetPropertyAsAString(NS_LITERAL_STRING("pluginDumpID"),
                                aPluginDumpID);


  // add a "pluginName" property to this event
  propBag->SetPropertyAsAString(NS_LITERAL_STRING("pluginName"),
                                aPluginName);

  // add a "pluginName" property to this event
  propBag->SetPropertyAsBool(NS_LITERAL_STRING("gmpPlugin"),
                             true);

  // add a "submittedCrashReport" property to this event
  propBag->SetPropertyAsBool(NS_LITERAL_STRING("submittedCrashReport"),
                             false);

  variant->SetAsISupports(propBag);

  EventDispatcher::DispatchDOMEvent(mWindow, nullptr, event, nullptr, nullptr);
#endif

  return true;
}

nsresult
PeerConnectionImpl::CloseInt()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  if (IsClosed()) {
    return NS_OK;
  }

  SetSignalingState_m(PCImplSignalingState::SignalingClosed);

  // We do this at the end of the call because we want to make sure we've waited
  // for all trickle ICE candidates to come in; this can happen well after we've
  // transitioned to connected. As a bonus, this allows us to detect race
  // conditions where a stats dispatch happens right as the PC closes.
  RecordLongtermICEStatistics();

  if (mInternal->mCall) {
    CSFLogInfo(logTag, "%s: Closing PeerConnectionImpl %s; "
               "ending call", __FUNCTION__, mHandle.c_str());
    mInternal->mCall->endCall();
    mInternal->mCall = nullptr;
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
    LocalSourceStreamInfo *info = media()->GetLocalStream(i);
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
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }
  JSErrorResult rv;
  pco->OnStateChange(PCObserverStateType::SignalingState, rv);
  MOZ_ASSERT(!rv.Failed());
}

void
PeerConnectionImpl::UpdateSignalingState() {
  fsmdef_states_t state = mInternal->mCall->getFsmState();
  /*
   * While the fsm_states_t (FSM_DEF_*) constants are a proper superset
   * of SignalingState, and the order in which the SignalingState values
   * appear matches the order they appear in fsm_states_t, their underlying
   * numeric representation is different. Hence, we need to perform an
   * offset calculation to map from one to the other.
   */

  if (state >= FSMDEF_S_STABLE && state <= FSMDEF_S_CLOSED) {
    int offset = FSMDEF_S_STABLE - int(PCImplSignalingState::SignalingStable);
    PCImplSignalingState newState =
      static_cast<PCImplSignalingState>(state - offset);
    if (newState == PCImplSignalingState::SignalingClosed) {
      Close();
    } else {
      SetSignalingState_m(newState);
    }
  } else {
    CSFLogError(logTag, ": **** UNHANDLED SIGNALING STATE : %d (%s)",
                state,
                mInternal->mCall->fsmStateToString(state).c_str());
  }
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

// This is called from the STS thread and so we need to thunk
// to the main thread.
void PeerConnectionImpl::IceConnectionStateChange(
    NrIceCtx* ctx,
    NrIceCtx::ConnectionState state) {
  (void)ctx;
  // Do an async call here to unwind the stack. refptr keeps the PC alive.
  nsRefPtr<PeerConnectionImpl> pc(this);
  RUN_ON_THREAD(mThread,
                WrapRunnable(pc,
                             &PeerConnectionImpl::IceConnectionStateChange_m,
                             toDomIceConnectionState(state)),
                NS_DISPATCH_NORMAL);
}

void
PeerConnectionImpl::IceGatheringStateChange(
    NrIceCtx* ctx,
    NrIceCtx::GatheringState state)
{
  (void)ctx;
  // Do an async call here to unwind the stack. refptr keeps the PC alive.
  nsRefPtr<PeerConnectionImpl> pc(this);
  RUN_ON_THREAD(mThread,
                WrapRunnable(pc,
                             &PeerConnectionImpl::IceGatheringStateChange_m,
                             toDomIceGatheringState(state)),
                NS_DISPATCH_NORMAL);
}

void
PeerConnectionImpl::CandidateReady_s(const std::string& candidate,
                                     uint16_t level)
{
  ASSERT_ON_THREAD(mSTSThread);
  nsRefPtr<PeerConnectionImpl> pc(this);
  RUN_ON_THREAD(mThread,
                WrapRunnable(pc,
                             &PeerConnectionImpl::CandidateReady_m,
                             candidate,
                             level),
                NS_DISPATCH_NORMAL);
}

nsresult
PeerConnectionImpl::CandidateReady_m(const std::string& candidate,
                                     uint16_t level) {
  PC_AUTO_ENTER_API_CALL(false);

  if (mLocalSDP.empty()) {
    // It is not appropriate to trickle yet; buffer.
    mCandidateBuffer.push_back(std::make_pair(candidate, level));
  } else {
    if (level <= mNumMlines) {
      FoundIceCandidate(candidate, level);
    }
  }

  return NS_OK;
}

void
PeerConnectionImpl::StartTrickle() {
  for (auto it = mCandidateBuffer.begin(); it != mCandidateBuffer.end(); ++it) {
    if (it->second <= mNumMlines) {
      FoundIceCandidate(it->first, it->second);
    }
  }

  // If the buffer was empty to begin with, we have already sent the
  // end-of-candidates event in IceGatheringStateChange_m.
  if (mIceGatheringState == PCImplIceGatheringState::Complete &&
      !mCandidateBuffer.empty()) {
    SendLocalIceCandidateToContent(0, "", "");
  }

  mCandidateBuffer.clear();
}

void PeerConnectionImpl::FoundIceCandidate(const std::string& candidate,
                                           uint16_t level) {
  // TODO: What about mid? Is this something that we will choose, or will
  // SIPCC choose for us? If the latter, we'll need to make it an outparam or
  // something.
  std::string mid;

  cc_int32_t error = mInternal->mCall->foundICECandidate(candidate,
                                                         mid,
                                                         level,
                                                         nullptr);

  if (error) {
    std::string error_string;
    mInternal->mCall->getErrorString(&error_string);
    CSFLogError(logTag, "%s: pc = %s, error = %s (note, this should not be "
                        "a show-stopper, since whether we incorporate "
                        "candidates into the SDP doesn't really matter since "
                        "we're full trickle)",
                __FUNCTION__, mHandle.c_str(), error_string.c_str());
  } else {
    CSFLogDebug(logTag, "Passing local candidate to content: %s",
                candidate.c_str());
    mInternal->mCall->getLocalSdp(&mLocalSDP);
    SendLocalIceCandidateToContent(level, mid, candidate);
  }
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

nsresult
PeerConnectionImpl::IceConnectionStateChange_m(PCImplIceConnectionState aState)
{
  PC_AUTO_ENTER_API_CALL(false);

  CSFLogDebug(logTag, "%s", __FUNCTION__);

#ifdef MOZILLA_INTERNAL_API
  if (!isDone(mIceConnectionState) && isDone(aState)) {
    // mIceStartTime can be null if going directly from New to Closed, in which
    // case we don't count it as a success or a failure.
    if (!mIceStartTime.IsNull()){
      TimeDuration timeDelta = TimeStamp::Now() - mIceStartTime;
      if (isSucceeded(aState)) {
        Telemetry::Accumulate(Telemetry::WEBRTC_ICE_SUCCESS_TIME,
                              timeDelta.ToMilliseconds());
      } else if (isFailed(aState)) {
        Telemetry::Accumulate(Telemetry::WEBRTC_ICE_FAILURE_TIME,
                              timeDelta.ToMilliseconds());
      }
    }

    if (isSucceeded(aState)) {
      Telemetry::Accumulate(
          Telemetry::WEBRTC_ICE_ADD_CANDIDATE_ERRORS_GIVEN_SUCCESS,
          mAddCandidateErrorCount);
    } else if (isFailed(aState)) {
      Telemetry::Accumulate(
          Telemetry::WEBRTC_ICE_ADD_CANDIDATE_ERRORS_GIVEN_FAILURE,
          mAddCandidateErrorCount);
    }
  }
#endif

  mIceConnectionState = aState;

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
    return NS_OK;
  }
  WrappableJSErrorResult rv;
  RUN_ON_THREAD(mThread,
                WrapRunnable(pco,
                             &PeerConnectionObserver::OnStateChange,
                             PCObserverStateType::IceConnectionState,
                             rv, static_cast<JSCompartment*>(nullptr)),
                NS_DISPATCH_NORMAL);
  return NS_OK;
}

nsresult
PeerConnectionImpl::IceGatheringStateChange_m(PCImplIceGatheringState aState)
{
  PC_AUTO_ENTER_API_CALL(false);

  CSFLogDebug(logTag, "%s", __FUNCTION__);

  mIceGatheringState = aState;

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
    return NS_OK;
  }
  WrappableJSErrorResult rv;
  RUN_ON_THREAD(mThread,
                WrapRunnable(pco,
                             &PeerConnectionObserver::OnStateChange,
                             PCObserverStateType::IceGatheringState,
                             rv, static_cast<JSCompartment*>(nullptr)),
                NS_DISPATCH_NORMAL);

  if (mIceGatheringState == PCImplIceGatheringState::Complete &&
      mCandidateBuffer.empty()) {
    SendLocalIceCandidateToContent(0, "", "");
  }

  return NS_OK;
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
    query->report->mLocalSdp.Construct(
        NS_ConvertASCIItoUTF16(mLocalSDP.c_str()));
    query->report->mRemoteSdp.Construct(
        NS_ConvertASCIItoUTF16(mRemoteSDP.c_str()));
  }

  // Gather up pipelines from mMedia so they may be inspected on STS

  for (int i = 0, len = mMedia->LocalStreamsLength(); i < len; i++) {
    auto& pipelines = mMedia->GetLocalStream(i)->GetPipelines();
    if (aSelector) {
      if (mMedia->GetLocalStream(i)->GetMediaStream()->HasTrack(*aSelector)) {
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

  for (int i = 0, len = mMedia->RemoteStreamsLength(); i < len; i++) {
    auto& pipelines = mMedia->GetRemoteStream(i)->GetPipelines();
    if (aSelector) {
      if (mMedia->GetRemoteStream(i)->GetMediaStream()->HasTrack(*aSelector)) {
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
      MOZ_ASSERT(level);
      levelsToGrab.insert(level);
    }
  } else {
    // We want to grab all streams, so ignore the pipelines (this also ends up
    // grabbing DataChannel streams, which is what we want)
    for (size_t s = 0; s < mMedia->num_ice_media_streams(); ++s) {
      levelsToGrab.insert(s + 1); // mIceStreams is 0-indexed
    }
  }

  for (auto s = levelsToGrab.begin(); s != levelsToGrab.end(); ++s) {
    // TODO(bcampen@mozilla.com): I may need to revisit this for bundle.
    // (Bug 786234)
    RefPtr<NrIceMediaStream> temp(mMedia->ice_media_stream(*s - 1));
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

void
PeerConnectionImpl::OnSdpParseError(const char *message) {
  CSFLogError(logTag, "%s SDP Parse Error: %s", __FUNCTION__, message);
  // Save the parsing errors in the PC to be delivered with OnSuccess or OnError
  mSDPParseErrorMessages.push_back(message);
}

void
PeerConnectionImpl::ClearSdpParseErrorMessages() {
  mSDPParseErrorMessages.clear();
}

const std::vector<std::string> &
PeerConnectionImpl::GetSdpParseErrors() {
  return mSDPParseErrorMessages;
}

#ifdef MOZILLA_INTERNAL_API
//Telemetry for when calls start
void
PeerConnectionImpl::startCallTelem() {
  // Start time for calls
  mStartTime = TimeStamp::Now();

  // Increment session call counter
#ifdef MOZILLA_INTERNAL_API
  int &cnt = PeerConnectionCtx::GetInstance()->mConnectionCounter;
  Telemetry::GetHistogramById(Telemetry::WEBRTC_CALL_COUNT)->Subtract(cnt);
  cnt++;
  Telemetry::GetHistogramById(Telemetry::WEBRTC_CALL_COUNT)->Add(cnt);
#endif
}
#endif

NS_IMETHODIMP
PeerConnectionImpl::GetLocalStreams(nsTArray<nsRefPtr<DOMMediaStream > >& result)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
#ifdef MOZILLA_INTERNAL_API
  for(uint32_t i=0; i < media()->LocalStreamsLength(); i++) {
    LocalSourceStreamInfo *info = media()->GetLocalStream(i);
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
    RemoteSourceStreamInfo *info = media()->GetRemoteStream(i);
    NS_ENSURE_TRUE(info, NS_ERROR_UNEXPECTED);
    result.AppendElement(info->GetMediaStream());
  }
  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif
}

}  // end sipcc namespace
