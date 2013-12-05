/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdlib>
#include <cerrno>
#include <deque>

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

#include "runnable_utils.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionImpl.h"
#include "PeerConnectionMedia.h"
#include "nsDOMDataChannelDeclarations.h"
#include "dtlsidentity.h"

#ifdef MOZILLA_INTERNAL_API
#include "nsPerformance.h"
#include "nsGlobalWindow.h"
#include "nsDOMDataChannel.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Telemetry.h"
#include "mozilla/PublicSSL.h"
#include "nsXULAppAPI.h"
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsIDocument.h"
#include "nsIScriptError.h"
#include "nsPrintfCString.h"
#include "nsURLHelper.h"
#include "nsNetUtil.h"
#include "nsIDOMDataChannel.h"
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "mozilla/dom/RTCStatsReportBinding.h"
#include "mozilla/dom/RTCPeerConnectionBinding.h"
#include "mozilla/dom/PeerConnectionImplBinding.h"
#include "mozilla/dom/DataChannelBinding.h"
#include "MediaStreamList.h"
#include "MediaStreamTrack.h"
#include "nsIScriptGlobalObject.h"
#include "DOMMediaStream.h"
#include "rlogringbuffer.h"
#endif

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
    MOZ_ASSUME_UNREACHABLE("Must be called in content process");
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

static const int DTLS_FINGERPRINT_LENGTH = 64;
static const int MEDIA_STREAM_MUTE = 0x80;

PRLogModuleInfo *signalingLogInfo() {
  static PRLogModuleInfo *logModuleInfo = nullptr;
  if (!logModuleInfo) {
    logModuleInfo = PR_NewLogModule("signaling");
  }
  return logModuleInfo;
}


namespace sipcc {

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
#ifdef MOZILLA_INTERNAL_API
      MOZ_ASSERT(NS_IsMainThread());
#endif
    }
  }
  operator JSErrorResult &() { return mRv; }
private:
  JSErrorResult mRv;
  bool isCopy;
};
}

class PeerConnectionObserverDispatch : public nsRunnable {

public:
  PeerConnectionObserverDispatch(CSF::CC_CallInfoPtr aInfo,
                                 nsRefPtr<PeerConnectionImpl> aPC,
                                 PeerConnectionObserver* aObserver)
      : mPC(aPC),
        mObserver(aObserver),
        mCode(static_cast<PeerConnectionImpl::Error>(aInfo->getStatusCode())),
        mReason(aInfo->getStatus()),
        mSdpStr(),
	mCandidateStr(),
        mCallState(aInfo->getCallState()),
        mFsmState(aInfo->getFsmState()),
        mStateStr(aInfo->callStateToString(mCallState)),
        mFsmStateStr(aInfo->fsmStateToString(mFsmState)) {
    if (mCallState == REMOTESTREAMADD) {
      MediaStreamTable *streams = nullptr;
      streams = aInfo->getMediaStreams();
      mRemoteStream = mPC->media()->GetRemoteStream(streams->media_stream_id);
      MOZ_ASSERT(mRemoteStream);
    } else if (mCallState == FOUNDICECANDIDATE) {
	mCandidateStr = aInfo->getCandidate();
    } else if ((mCallState == CREATEOFFERSUCCESS) ||
	       (mCallState == CREATEANSWERSUCCESS)) {
        mSdpStr = aInfo->getSDP();
    }
  }

  ~PeerConnectionObserverDispatch(){}

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

  NS_IMETHOD Run() {

    CSFLogInfo(logTag, "PeerConnectionObserverDispatch processing "
               "mCallState = %d (%s), mFsmState = %d (%s)",
               mCallState, mStateStr.c_str(), mFsmState, mFsmStateStr.c_str());

    if (mCallState == SETLOCALDESCERROR || mCallState == SETREMOTEDESCERROR) {
      const std::vector<std::string> &errors = mPC->GetSdpParseErrors();
      std::vector<std::string>::const_iterator i;
      for (i = errors.begin(); i != errors.end(); ++i) {
        mReason += " | SDP Parsing Error: " + *i;
      }
      if (errors.size()) {
        mCode = PeerConnectionImpl::kInvalidSessionDescription;
      }
      mPC->ClearSdpParseErrorMessages();
    }

    if (mReason.length()) {
      CSFLogInfo(logTag, "Message contains error: %d: %s",
                 mCode, mReason.c_str());
    }

    /*
     * While the fsm_states_t (FSM_DEF_*) constants are a proper superset
     * of SignalingState, and the order in which the SignalingState values
     * appear matches the order they appear in fsm_states_t, their underlying
     * numeric representation is different. Hence, we need to perform an
     * offset calculation to map from one to the other.
     */

    if (mFsmState >= FSMDEF_S_STABLE && mFsmState <= FSMDEF_S_CLOSED) {
      int offset = FSMDEF_S_STABLE - int(PCImplSignalingState::SignalingStable);
      mPC->SetSignalingState_m(static_cast<PCImplSignalingState>(mFsmState - offset));
    } else {
      CSFLogError(logTag, ": **** UNHANDLED SIGNALING STATE : %d (%s)",
                  mFsmState, mFsmStateStr.c_str());
    }

    JSErrorResult rv;

    switch (mCallState) {
      case CREATEOFFERSUCCESS:
        mObserver->OnCreateOfferSuccess(ObString(mSdpStr.c_str()), rv);
        break;

      case CREATEANSWERSUCCESS:
        mObserver->OnCreateAnswerSuccess(ObString(mSdpStr.c_str()), rv);
        break;

      case CREATEOFFERERROR:
        mObserver->OnCreateOfferError(mCode, ObString(mReason.c_str()), rv);
        break;

      case CREATEANSWERERROR:
        mObserver->OnCreateAnswerError(mCode, ObString(mReason.c_str()), rv);
        break;

      case SETLOCALDESCSUCCESS:
        // TODO: The SDP Parse error list should be copied out and sent up
        // to the Javascript layer before being cleared here. Even though
        // there was not a failure, it is possible that the SDP parse generated
        // warnings. The WebRTC spec does not currently have a mechanism for
        // providing non-fatal warnings.
        mPC->ClearSdpParseErrorMessages();
        mObserver->OnSetLocalDescriptionSuccess(rv);
        break;

      case SETREMOTEDESCSUCCESS:
        // TODO: The SDP Parse error list should be copied out and sent up
        // to the Javascript layer before being cleared here. Even though
        // there was not a failure, it is possible that the SDP parse generated
        // warnings. The WebRTC spec does not currently have a mechanism for
        // providing non-fatal warnings.
        mPC->ClearSdpParseErrorMessages();
        mObserver->OnSetRemoteDescriptionSuccess(rv);
#ifdef MOZILLA_INTERNAL_API
        mPC->startCallTelem();
#endif
        break;

      case SETLOCALDESCERROR:
        mObserver->OnSetLocalDescriptionError(mCode,
                                              ObString(mReason.c_str()), rv);
        break;

      case SETREMOTEDESCERROR:
        mObserver->OnSetRemoteDescriptionError(mCode,
                                               ObString(mReason.c_str()), rv);
        break;

      case ADDICECANDIDATE:
        mObserver->OnAddIceCandidateSuccess(rv);
        break;

      case ADDICECANDIDATEERROR:
        mObserver->OnAddIceCandidateError(mCode, ObString(mReason.c_str()), rv);
        break;

      case FOUNDICECANDIDATE:
        {
            size_t end_of_level = mCandidateStr.find('\t');
            if (end_of_level == std::string::npos) {
                MOZ_ASSERT(false);
                return NS_OK;
            }
            std::string level = mCandidateStr.substr(0, end_of_level);
            if (!level.size()) {
                MOZ_ASSERT(false);
                return NS_OK;
            }
            char *endptr;
            errno = 0;
            unsigned long level_long =
                strtoul(level.c_str(), &endptr, 10);
            if (errno || *endptr != 0 || level_long > 65535) {
                /* Conversion failure */
                MOZ_ASSERT(false);
                return NS_OK;
            }
            size_t end_of_mid = mCandidateStr.find('\t', end_of_level + 1);
            if (end_of_mid == std::string::npos) {
                MOZ_ASSERT(false);
                return NS_OK;
            }

            std::string mid = mCandidateStr.substr(end_of_level + 1,
                                                   end_of_mid - (end_of_level + 1));

            std::string candidate = mCandidateStr.substr(end_of_mid + 1);

            mObserver->OnIceCandidate(level_long & 0xffff,
                                      ObString(mid.c_str()),
                                      ObString(candidate.c_str()), rv);
        }
        break;
      case REMOTESTREAMADD:
        {
          DOMMediaStream* stream = nullptr;

          if (!mRemoteStream) {
            CSFLogError(logTag, "%s: GetRemoteStream returned NULL", __FUNCTION__);
          } else {
            stream = mRemoteStream->GetMediaStream();
          }

          if (!stream) {
            CSFLogError(logTag, "%s: GetMediaStream returned NULL", __FUNCTION__);
          } else {
#ifdef MOZILLA_INTERNAL_API
            TracksAvailableCallback* tracksAvailableCallback =
              new TracksAvailableCallback(mRemoteStream->mTrackTypeHints, mObserver);

            stream->OnTracksAvailable(tracksAvailableCallback);
#else
            mObserver->OnAddStream(stream, rv);
#endif
          }
          break;
        }

      case UPDATELOCALDESC:
        /* No action necessary */
        break;

      default:
        CSFLogError(logTag, ": **** UNHANDLED CALL STATE : %d (%s)",
                    mCallState, mStateStr.c_str());
        break;
    }
    return NS_OK;
  }

private:
  nsRefPtr<PeerConnectionImpl> mPC;
  nsRefPtr<PeerConnectionObserver> mObserver;
  PeerConnectionImpl::Error mCode;
  std::string mReason;
  std::string mSdpStr;
  std::string mCandidateStr;
  cc_call_state_t mCallState;
  fsmdef_states_t mFsmState;
  std::string mStateStr;
  std::string mFsmStateStr;
  nsRefPtr<RemoteSourceStreamInfo> mRemoteStream;
};

NS_IMPL_ISUPPORTS0(PeerConnectionImpl)

#ifdef MOZILLA_INTERNAL_API
JSObject*
PeerConnectionImpl::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return PeerConnectionImplBinding::Wrap(aCx, aScope, this);
}
#endif

struct PeerConnectionImpl::Internal {
  CSF::CC_CallPtr mCall;
};

PeerConnectionImpl::PeerConnectionImpl(const GlobalObject* aGlobal)
: mTimeCard(PR_LOG_TEST(signalingLogInfo(),PR_LOG_ERROR) ?
            create_timecard() : nullptr)
  , mInternal(new Internal())
  , mReadyState(PCImplReadyState::New)
  , mSignalingState(PCImplSignalingState::SignalingStable)
  , mIceConnectionState(PCImplIceConnectionState::New)
  , mIceGatheringState(PCImplIceGatheringState::New)
  , mWindow(nullptr)
  , mIdentity(nullptr)
  , mSTSThread(nullptr)
  , mMedia(nullptr)
  , mNumAudioStreams(0)
  , mNumVideoStreams(0)
  , mHaveDataStream(false)
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
  // Deregister as an NSS Shutdown Object
  shutdown(calledFromObject);
#endif

  // Since this and Initialize() occur on MainThread, they can't both be
  // running at once

  // Right now, we delete PeerConnectionCtx at XPCOM shutdown only, but we
  // probably want to shut it down more aggressively to save memory.  We
  // could shut down here when there are no uses.  It might be more optimal
  // to release off a timer (and XPCOM Shutdown) to avoid churn
}

already_AddRefed<DOMMediaStream>
PeerConnectionImpl::MakeMediaStream(nsPIDOMWindow* aWindow,
                                    uint32_t aHint)
{
  nsRefPtr<DOMMediaStream> stream =
    DOMMediaStream::CreateSourceStream(aWindow, aHint);
#ifdef MOZILLA_INTERNAL_API
  nsIDocument* doc = aWindow->GetExtantDoc();
  if (!doc) {
    return nullptr;
  }
  // Make the stream data (audio/video samples) accessible to the receiving page.
  stream->CombineWithPrincipal(doc->NodePrincipal());
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
  nsRefPtr<DOMMediaStream> stream = MakeMediaStream(mWindow, 0);
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
#ifdef MOZILLA_INTERNAL_API
  MOZ_ASSERT(NS_IsMainThread());
#endif
  MOZ_ASSERT(aThread);
  mThread = do_QueryInterface(aThread);

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
#endif // MOZILLA_INTERNAL_API

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

  // Set the fingerprint. Right now assume we only have one
  // DTLS identity
  unsigned char fingerprint[DTLS_FINGERPRINT_LENGTH];
  size_t fingerprint_length;
  res = mIdentity->ComputeFingerprint("sha-256",
                                      fingerprint,
                                      sizeof(fingerprint),
                                      &fingerprint_length);

  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: ComputeFingerprint failed: %u",
      __FUNCTION__, static_cast<uint32_t>(res));
    return res;
  }

  mFingerprint = "sha-256 " + mIdentity->FormatFingerprint(fingerprint,
                                                         fingerprint_length);
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: do_GetService failed: %u",
      __FUNCTION__, static_cast<uint32_t>(res));
    return res;
  }

  return NS_OK;
}

RefPtr<DtlsIdentity> const
PeerConnectionImpl::GetIdentity() {
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  return mIdentity;
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

  nsRefPtr<DOMMediaStream> stream = MakeMediaStream(mWindow, aHint);
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

  *aRetval = stream.forget().get();
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
    mInternal->mCall->addStream(0, 2, DATA);
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

void
PeerConnectionImpl::NotifyConnection()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  CSFLogDebug(logTag, "%s", __FUNCTION__);

#ifdef MOZILLA_INTERNAL_API
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }
  WrappableJSErrorResult rv;
  RUN_ON_THREAD(mThread,
                WrapRunnable(pco,
                             &PeerConnectionObserver::NotifyConnection,
                             rv, static_cast<JSCompartment*>(nullptr)),
                NS_DISPATCH_NORMAL);
#endif
}

void
PeerConnectionImpl::NotifyClosedConnection()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  CSFLogDebug(logTag, "%s", __FUNCTION__);

#ifdef MOZILLA_INTERNAL_API
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }
  WrappableJSErrorResult rv;
  RUN_ON_THREAD(mThread,
    WrapRunnable(pco, &PeerConnectionObserver::NotifyClosedConnection,
                 rv, static_cast<JSCompartment*>(nullptr)),
    NS_DISPATCH_NORMAL);
#endif
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
  MOZ_ASSERT(aChannel.get());

  CSFLogDebug(logTag, "%s: channel: %p", __FUNCTION__, aChannel.get());

#ifdef MOZILLA_INTERNAL_API
  nsCOMPtr<nsIDOMDataChannel> domchannel;
  nsresult rv = NS_NewDOMDataChannel(aChannel, mWindow,
                                     getter_AddRefs(domchannel));
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
PeerConnectionImpl::CreateOffer(const MediaConstraintsInternal& aConstraints)
{
  return CreateOffer(MediaConstraintsExternal (aConstraints));
}

// Used by unit tests and the IDL CreateOffer.
NS_IMETHODIMP
PeerConnectionImpl::CreateOffer(const MediaConstraintsExternal& aConstraints)
{
  PC_AUTO_ENTER_API_CALL(true);

  Timecard *tc = mTimeCard;
  mTimeCard = nullptr;
  STAMP_TIMECARD(tc, "Create Offer");

  cc_media_constraints_t* cc_constraints = aConstraints.build();
  NS_ENSURE_TRUE(cc_constraints, NS_ERROR_UNEXPECTED);
  mInternal->mCall->createOffer(cc_constraints, tc);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CreateAnswer(const MediaConstraintsInternal& aConstraints)
{
  return CreateAnswer(MediaConstraintsExternal (aConstraints));
}

NS_IMETHODIMP
PeerConnectionImpl::CreateAnswer(const MediaConstraintsExternal& aConstraints)
{
  PC_AUTO_ENTER_API_CALL(true);

  Timecard *tc = mTimeCard;
  mTimeCard = nullptr;
  STAMP_TIMECARD(tc, "Create Answer");

  cc_media_constraints_t* cc_constraints = aConstraints.build();
  NS_ENSURE_TRUE(cc_constraints, NS_ERROR_UNEXPECTED);
  mInternal->mCall->createAnswer(cc_constraints, tc);
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

  Timecard *tc = mTimeCard;
  mTimeCard = nullptr;
  STAMP_TIMECARD(tc, "Set Local Description");

  mLocalRequestedSDP = aSDP;
  mInternal->mCall->setLocalDescription((cc_jsep_action_t)aAction,
                                        mLocalRequestedSDP, tc);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::SetRemoteDescription(int32_t action, const char* aSDP)
{
  PC_AUTO_ENTER_API_CALL(true);

  if (!aSDP) {
    CSFLogError(logTag, "%s - aSDP is NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  Timecard *tc = mTimeCard;
  mTimeCard = nullptr;
  STAMP_TIMECARD(tc, "Set Remote Description");

  mRemoteRequestedSDP = aSDP;
  mInternal->mCall->setRemoteDescription((cc_jsep_action_t)action,
                                         mRemoteRequestedSDP, tc);
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
#endif

NS_IMETHODIMP
PeerConnectionImpl::GetStats(MediaStreamTrack *aSelector,
                             bool internalStats) {
  PC_AUTO_ENTER_API_CALL(true);

#ifdef MOZILLA_INTERNAL_API
  uint32_t track = aSelector ? aSelector->GetTrackID() : 0;
  DOMHighResTimeStamp now;
  nsresult rv = GetTimeSinceEpoch(&now);
  NS_ENSURE_SUCCESS(rv, rv);
  nsRefPtr<PeerConnectionImpl> pc(this);
  RUN_ON_THREAD(mSTSThread,
                WrapRunnable(pc,
                             &PeerConnectionImpl::GetStats_s,
                             track,
                             internalStats,
                             now),
                NS_DISPATCH_NORMAL);
#endif
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetLogging(const nsAString& aPattern) {
  PC_AUTO_ENTER_API_CALL(true);

#ifdef MOZILLA_INTERNAL_API
  std::string pattern(NS_ConvertUTF16toUTF8(aPattern).get());
  nsRefPtr<PeerConnectionImpl> pc(this);
  RUN_ON_THREAD(mSTSThread,
                WrapRunnable(pc,
                             &PeerConnectionImpl::GetLogging_s,
                             pattern),
                NS_DISPATCH_NORMAL);

#endif
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::AddIceCandidate(const char* aCandidate, const char* aMid, unsigned short aLevel) {
  PC_AUTO_ENTER_API_CALL(true);

  Timecard *tc = mTimeCard;
  mTimeCard = nullptr;
  STAMP_TIMECARD(tc, "Add Ice Candidate");

  mInternal->mCall->addICECandidate(aCandidate, aMid, aLevel, tc);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CloseStreams() {
  PC_AUTO_ENTER_API_CALL(false);

  if (mReadyState != PCImplReadyState::Closed)  {
    ChangeReadyState(PCImplReadyState::Closing);
  }

  CSFLogInfo(logTag, "%s: Ending associated call", __FUNCTION__);

  mInternal->mCall->endCall();
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::AddStream(DOMMediaStream& aMediaStream) {
  PC_AUTO_ENTER_API_CALL(true);

  uint32_t hints = aMediaStream.GetHintContents();

  // XXX Remove this check once addStream has an error callback
  // available and/or we have plumbing to handle multiple
  // local audio streams.
  if ((hints & DOMMediaStream::HINT_CONTENTS_AUDIO) &&
      mNumAudioStreams > 0) {
    CSFLogError(logTag, "%s: Only one local audio stream is supported for now",
                __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  // XXX Remove this check once addStream has an error callback
  // available and/or we have plumbing to handle multiple
  // local video streams.
  if ((hints & DOMMediaStream::HINT_CONTENTS_VIDEO) &&
      mNumVideoStreams > 0) {
    CSFLogError(logTag, "%s: Only one local video stream is supported for now",
                __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  uint32_t stream_id;
  nsresult res = mMedia->AddStream(&aMediaStream, &stream_id);
  if (NS_FAILED(res))
    return res;

  // TODO(ekr@rtfm.com): these integers should be the track IDs
  if (hints & DOMMediaStream::HINT_CONTENTS_AUDIO) {
    mInternal->mCall->addStream(stream_id, 0, AUDIO);
    mNumAudioStreams++;
  }

  if (hints & DOMMediaStream::HINT_CONTENTS_VIDEO) {
    mInternal->mCall->addStream(stream_id, 1, VIDEO);
    mNumVideoStreams++;
  }

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::RemoveStream(DOMMediaStream& aMediaStream) {
  PC_AUTO_ENTER_API_CALL(true);

  uint32_t stream_id;
  nsresult res = mMedia->RemoveStream(&aMediaStream, &stream_id);

  if (NS_FAILED(res))
    return res;

  uint32_t hints = aMediaStream.GetHintContents();

  if (hints & DOMMediaStream::HINT_CONTENTS_AUDIO) {
    mInternal->mCall->removeStream(stream_id, 0, AUDIO);
    MOZ_ASSERT(mNumAudioStreams > 0);
    mNumAudioStreams--;
  }

  if (hints & DOMMediaStream::HINT_CONTENTS_VIDEO) {
    mInternal->mCall->removeStream(stream_id, 1, VIDEO);
    MOZ_ASSERT(mNumVideoStreams > 0);
    mNumVideoStreams--;
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
PeerConnectionImpl::ReadyState(PCImplReadyState* aState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mReadyState;
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

  if (mReadyState == PCImplReadyState::Closed)
    return NS_ERROR_FAILURE;
  if (!mMedia)
    return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::Close()
{
  CSFLogDebug(logTag, "%s: for %s", __FUNCTION__, mHandle.c_str());
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  return CloseInt();
}


nsresult
PeerConnectionImpl::CloseInt()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  if (mInternal->mCall) {
    CSFLogInfo(logTag, "%s: Closing PeerConnectionImpl %s; "
               "ending call", __FUNCTION__, mHandle.c_str());
    mInternal->mCall->endCall();
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
  // End of call to be recorded in Telemetry
  if (!mStartTime.IsNull()){
    TimeDuration timeDelta = TimeStamp::Now() - mStartTime;
    Telemetry::Accumulate(Telemetry::WEBRTC_CALL_DURATION, timeDelta.ToSeconds());
  }
#endif

  // Forget the reference so that we can transfer it to
  // SelfDestruct().
  mMedia.forget().get()->SelfDestruct();
}

#ifdef MOZILLA_INTERNAL_API
// If NSS is shutting down, then we need to get rid of the DTLS
// identity right now; otherwise, we'll cause wreckage when we do
// finally deallocate it in our destructor.
void
PeerConnectionImpl::virtualDestroyNSSReference()
{
  MOZ_ASSERT(NS_IsMainThread());
  CSFLogDebug(logTag, "%s: NSS shutting down; freeing our DtlsIdentity.", __FUNCTION__);
  mIdentity = nullptr;
}
#endif

void
PeerConnectionImpl::onCallEvent(const OnCallEventArgs& args)
{
  const ccapi_call_event_e &aCallEvent = args.mCallEvent;
  const CSF::CC_CallInfoPtr &aInfo = args.mInfo;

  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aInfo.get());

  cc_call_state_t event = aInfo->getCallState();
  std::string statestr = aInfo->callStateToString(event);
  Timecard *timecard = aInfo->takeTimecard();

  if (timecard) {
    mTimeCard = timecard;
    STAMP_TIMECARD(mTimeCard, "Operation Completed");
  }

  if (CCAPI_CALL_EV_CREATED != aCallEvent && CCAPI_CALL_EV_STATE != aCallEvent) {
    CSFLogDebug(logTag, "%s: **** CALL HANDLE IS: %s, **** CALL STATE IS: %s",
      __FUNCTION__, mHandle.c_str(), statestr.c_str());
    return;
  }

  switch (event) {
    case SETLOCALDESCSUCCESS:
    case UPDATELOCALDESC:
      mLocalSDP = aInfo->getSDP();
      break;

    case SETREMOTEDESCSUCCESS:
    case ADDICECANDIDATE:
      mRemoteSDP = aInfo->getSDP();
      break;

    case CONNECTED:
      CSFLogDebug(logTag, "Setting PeerConnnection state to kActive");
      ChangeReadyState(PCImplReadyState::Active);
      break;
    default:
      break;
  }

  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }

  PeerConnectionObserverDispatch* runnable =
      new PeerConnectionObserverDispatch(aInfo, this, pco);

  if (mThread) {
    mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
    return;
  }
  runnable->Run();
  delete runnable;
}

void
PeerConnectionImpl::ChangeReadyState(PCImplReadyState aReadyState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  mReadyState = aReadyState;

  // Note that we are passing an nsRefPtr which keeps the observer live.
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }
  WrappableJSErrorResult rv;
  RUN_ON_THREAD(mThread,
                WrapRunnable(pco,
                             &PeerConnectionObserver::OnStateChange,
                             PCObserverStateType::ReadyState,
                             rv, static_cast<JSCompartment*>(nullptr)),
                NS_DISPATCH_NORMAL);
}

void
PeerConnectionImpl::SetSignalingState_m(PCImplSignalingState aSignalingState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  if (mSignalingState == aSignalingState) {
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

nsresult
PeerConnectionImpl::IceConnectionStateChange_m(PCImplIceConnectionState aState)
{
  PC_AUTO_ENTER_API_CALL(false);

  CSFLogDebug(logTag, "%s", __FUNCTION__);

  mIceConnectionState = aState;

  // Would be nice if we had a means of converting one of these dom enums
  // to a string that wasn't almost as much text as this switch statement...
  switch (mIceConnectionState) {
    case PCImplIceConnectionState::New:
      STAMP_TIMECARD(mTimeCard, "Ice state: new");
      break;
    case PCImplIceConnectionState::Checking:
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
      STAMP_TIMECARD(mTimeCard, "Ice state: complete");
      break;
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
  return NS_OK;
}

#ifdef MOZILLA_INTERNAL_API
void PeerConnectionImpl::GetStats_s(
    uint32_t trackId,
    bool internalStats,
    DOMHighResTimeStamp now) {

  nsresult result = NS_OK;
  nsAutoPtr<RTCStatsReportInternal> report(new RTCStatsReportInternal);
  if (!report) {
    result = NS_ERROR_FAILURE;
  }

  report->mPcid.Construct(NS_ConvertASCIItoUTF16(mHandle.c_str()));
  if (mMedia) {
    RefPtr<NrIceMediaStream> mediaStream(
        mMedia->ice_media_stream(trackId));
    if (mediaStream) {
      std::vector<NrIceCandidatePair> candPairs;
      mediaStream->GetCandidatePairs(&candPairs);
      report->mIceCandidatePairStats.Construct();
      report->mIceCandidateStats.Construct();
      NS_ConvertASCIItoUTF16 componentId(mediaStream->name().c_str());
      for (auto p = candPairs.begin(); p != candPairs.end(); ++p) {
        NS_ConvertASCIItoUTF16 codeword(p->codeword.c_str());
        NS_ConvertASCIItoUTF16 localCodeword(p->local.codeword.c_str());
        NS_ConvertASCIItoUTF16 remoteCodeword(p->remote.codeword.c_str());
        // Only expose candidate-pair statistics to chrome, until we've thought
        // through the implications of exposing it to content.

        if (internalStats) {
          RTCIceCandidatePairStats s;
          s.mId.Construct(codeword);
          s.mComponentId.Construct(componentId);
          s.mTimestamp.Construct(now);
          s.mType.Construct(RTCStatsType::Candidatepair);

          // Not quite right; we end up with duplicate candidates. Will fix.
          s.mLocalCandidateId.Construct(localCodeword);
          s.mRemoteCandidateId.Construct(remoteCodeword);
          s.mNominated.Construct(p->nominated);
          s.mMozPriority.Construct(p->priority);
          s.mSelected.Construct(p->selected);
          s.mState.Construct(RTCStatsIceCandidatePairState(p->state));
          report->mIceCandidatePairStats.Value().AppendElement(s);
        }

        {
          RTCIceCandidateStats local;
          local.mId.Construct(localCodeword);
          local.mTimestamp.Construct(now);
          local.mType.Construct(RTCStatsType::Localcandidate);
          local.mCandidateType.Construct(
              RTCStatsIceCandidateType(p->local.type));
          local.mIpAddress.Construct(
              NS_ConvertASCIItoUTF16(p->local.host.c_str()));
          local.mPortNumber.Construct(p->local.port);
          report->mIceCandidateStats.Value().AppendElement(local);
        }

        {
          RTCIceCandidateStats remote;
          remote.mId.Construct(remoteCodeword);
          remote.mTimestamp.Construct(now);
          remote.mType.Construct(RTCStatsType::Remotecandidate);
          remote.mCandidateType.Construct(
              RTCStatsIceCandidateType(p->remote.type));
          remote.mIpAddress.Construct(
              NS_ConvertASCIItoUTF16(p->remote.host.c_str()));
          remote.mPortNumber.Construct(p->remote.port);
          report->mIceCandidateStats.Value().AppendElement(remote);
        }
      }
    }
  }

  nsRefPtr<PeerConnectionImpl> pc(this);
  RUN_ON_THREAD(mThread,
                WrapRunnable(pc,
                             &PeerConnectionImpl::OnStatsReport_m,
                             trackId,
                             result,
                             report),
                NS_DISPATCH_NORMAL);
}

void PeerConnectionImpl::OnStatsReport_m(
    uint32_t trackId,
    nsresult result,
    nsAutoPtr<RTCStatsReportInternal> report) {
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (pco) {
    JSErrorResult rv;
    if (NS_SUCCEEDED(result)) {
      pco->OnGetStatsSuccess(*report, rv);
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

void PeerConnectionImpl::GetLogging_s(const std::string& pattern) {
  RLogRingBuffer* logs = RLogRingBuffer::GetInstance();
  std::deque<std::string> result;
  logs->Filter(pattern, 0, &result);
  nsRefPtr<PeerConnectionImpl> pc(this);
  RUN_ON_THREAD(mThread,
                WrapRunnable(pc,
                             &PeerConnectionImpl::OnGetLogging_m,
                             pattern,
                             result),
                NS_DISPATCH_NORMAL);
}

void PeerConnectionImpl::OnGetLogging_m(const std::string& pattern,
                                        const std::deque<std::string>& logging) {
  nsRefPtr<PeerConnectionObserver> pco = do_QueryObjectReferent(mPCObserver);
  if (!pco) {
    return;
  }

  JSErrorResult rv;
  if (!logging.empty()) {
    Sequence<nsString> nsLogs;
    for (auto l = logging.begin(); l != logging.end(); ++l) {
      nsLogs.AppendElement(ObString(l->c_str()));
    }
    pco->OnGetLoggingSuccess(nsLogs, rv);
  } else {
    pco->OnGetLoggingError(kInternalError,
        ObString(("No logging matching pattern " + pattern).c_str()), rv);
  }

  if (rv.Failed()) {
    CSFLogError(logTag, "Error firing stats observer callback");
  }
}


#endif

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
