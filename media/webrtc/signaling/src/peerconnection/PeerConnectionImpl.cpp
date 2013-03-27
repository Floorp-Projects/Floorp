/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include "vcm.h"
#include "CSFLog.h"
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
#include "nsPIDOMWindow.h"
#include "nsDOMDataChannel.h"

#ifdef MOZILLA_INTERNAL_API
#include "nsContentUtils.h"
#include "nsDOMJSUtils.h"
#include "nsIDocument.h"
#include "nsIScriptError.h"
#include "nsPrintfCString.h"
#include "nsURLHelper.h"
#include "nsNetUtil.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/RTCConfigurationBinding.h"
#include "MediaStreamList.h"
#include "nsIScriptGlobalObject.h"
#include "jsapi.h"
#endif

#ifndef USE_FAKE_MEDIA_STREAMS
#include "MediaSegment.h"
#endif

#define ICE_PARSING "In RTCConfiguration passed to RTCPeerConnection constructor"

using namespace mozilla;
using namespace mozilla::dom;

namespace mozilla {
  class DataChannel;
}

class nsIDOMDataChannel;

static const char* logTag = "PeerConnectionImpl";
static const int DTLS_FINGERPRINT_LENGTH = 64;
static const int MEDIA_STREAM_MUTE = 0x80;

namespace sipcc {

void MediaConstraints::setBooleanConstraint(const std::string& constraint, bool enabled, bool mandatory) {

  ConstraintInfo booleanconstraint;
  booleanconstraint.mandatory = mandatory;

  if (enabled)
    booleanconstraint.value = "TRUE";
  else
    booleanconstraint.value = "FALSE";

  mConstraints[constraint] = booleanconstraint;
}

void MediaConstraints::buildArray(cc_media_constraints_t** constraintarray) {

  if (0 == mConstraints.size())
    return;

  short i = 0;
  std::string tmpStr;
  *constraintarray = (cc_media_constraints_t*) cpr_malloc(sizeof(cc_media_constraints_t));
  int tmpStrAllocLength;

  (*constraintarray)->constraints = (cc_media_constraint_t**) cpr_malloc(mConstraints.size() * sizeof(cc_media_constraint_t));

  for (constraints_map::iterator it = mConstraints.begin();
          it != mConstraints.end(); ++it) {
    (*constraintarray)->constraints[i] = (cc_media_constraint_t*) cpr_malloc(sizeof(cc_media_constraint_t));

    tmpStr = it->first;
    tmpStrAllocLength = tmpStr.size() + 1;
    (*constraintarray)->constraints[i]->name = (char*) cpr_malloc(tmpStrAllocLength);
    sstrncpy((*constraintarray)->constraints[i]->name, tmpStr.c_str(), tmpStrAllocLength);

    tmpStr = it->second.value;
    tmpStrAllocLength = tmpStr.size() + 1;
    (*constraintarray)->constraints[i]->value = (char*) cpr_malloc(tmpStrAllocLength);
    sstrncpy((*constraintarray)->constraints[i]->value, tmpStr.c_str(), tmpStrAllocLength);

    (*constraintarray)->constraints[i]->mandatory = it->second.mandatory;
    i++;
  }
  (*constraintarray)->constraint_count = i;
}

class PeerConnectionObserverDispatch : public nsRunnable {

public:
  PeerConnectionObserverDispatch(CSF::CC_CallInfoPtr aInfo,
                                 nsRefPtr<PeerConnectionImpl> aPC,
                                 IPeerConnectionObserver* aObserver)
      : mPC(aPC),
        mObserver(aObserver),
        mCode(static_cast<StatusCode>(aInfo->getStatusCode())),
        mSdpStr(),
        mCallState(aInfo->getCallState()),
        mStateStr(aInfo->callStateToString(mCallState)) {
    if (mCallState == REMOTESTREAMADD) {
      MediaStreamTable *streams = NULL;
      streams = aInfo->getMediaStreams();
      mRemoteStream = mPC->media()->GetRemoteStream(streams->media_stream_id);
      MOZ_ASSERT(mRemoteStream);
    }
    if ((mCallState == CREATEOFFER) || (mCallState == CREATEANSWER)) {
        mSdpStr = aInfo->getSDP();
    }
  }

  ~PeerConnectionObserverDispatch(){}

  NS_IMETHOD Run() {
    switch (mCallState) {
      case CREATEOFFER:
        mObserver->OnCreateOfferSuccess(mSdpStr.c_str());
        break;

      case CREATEANSWER:
        mObserver->OnCreateAnswerSuccess(mSdpStr.c_str());
        break;

      case CREATEOFFERERROR:
        mObserver->OnCreateOfferError(mCode);
        break;

      case CREATEANSWERERROR:
        mObserver->OnCreateAnswerError(mCode);
        break;

      case SETLOCALDESC:
        // TODO: The SDP Parse error list should be copied out and sent up
        // to the Javascript layer before being cleared here.
        mPC->ClearSdpParseErrorMessages();
        mObserver->OnSetLocalDescriptionSuccess(mCode);
        break;

      case SETREMOTEDESC:
        // TODO: The SDP Parse error list should be copied out and sent up
        // to the Javascript layer before being cleared here.
        mPC->ClearSdpParseErrorMessages();
        mObserver->OnSetRemoteDescriptionSuccess(mCode);
        break;

      case SETLOCALDESCERROR:
        // TODO: The SDP Parse error list should be copied out and sent up
        // to the Javascript layer before being cleared here.
        mPC->ClearSdpParseErrorMessages();
        mObserver->OnSetLocalDescriptionError(mCode);
        break;

      case SETREMOTEDESCERROR:
        // TODO: The SDP Parse error list should be copied out and sent up
        // to the Javascript layer before being cleared here.
        mPC->ClearSdpParseErrorMessages();
        mObserver->OnSetRemoteDescriptionError(mCode);
        break;

      case ADDICECANDIDATE:
        mObserver->OnAddIceCandidateSuccess(mCode);
        break;

      case ADDICECANDIDATEERROR:
        mObserver->OnAddIceCandidateError(mCode);
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
            // We provide a type field because it is in the IDL
            // and we want code that looks at it not to crash.
            // We use "video" so that if an app looks for
            // that string it has some chance of working.
            // TODO(ekr@rtfm.com): Bug 834847
            // The correct way for content JS to know stream type
            // is via get{Audio,Video}Tracks. See Bug 834835.
            mObserver->OnAddStream(stream, "video");
          }
          break;
        }

      case UPDATELOCALDESC:
        /* No action necessary */
        break;

      default:
        CSFLogDebug(logTag, ": **** UNHANDLED CALL STATE : %s", mStateStr.c_str());
        break;
    }

    return NS_OK;
  }

private:
  nsRefPtr<PeerConnectionImpl> mPC;
  nsCOMPtr<IPeerConnectionObserver> mObserver;
  StatusCode mCode;
  std::string mSdpStr;
  cc_call_state_t mCallState;
  std::string mStateStr;
  nsRefPtr<RemoteSourceStreamInfo> mRemoteStream;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(PeerConnectionImpl, IPeerConnection)

PeerConnectionImpl::PeerConnectionImpl()
: mRole(kRoleUnknown)
  , mCall(NULL)
  , mReadyState(kNew)
  , mIceState(kIceGathering)
  , mPCObserver(NULL)
  , mWindow(NULL)
  , mIdentity(NULL)
  , mSTSThread(NULL)
  , mMedia(new PeerConnectionMedia(this))
  , mNumAudioStreams(0)
  , mNumVideoStreams(0) {
#ifdef MOZILLA_INTERNAL_API
  MOZ_ASSERT(NS_IsMainThread());
#endif
}

PeerConnectionImpl::~PeerConnectionImpl()
{
  // This aborts if not on main thread (in Debug builds)
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  if (PeerConnectionCtx::isActive()) {
    PeerConnectionCtx::GetInstance()->mPeerConnections.erase(mHandle);
  } else {
    CSFLogError(logTag, "PeerConnectionCtx is already gone. Ignoring...");
  }

  CSFLogInfo(logTag, "%s: PeerConnectionImpl destructor invoked", __FUNCTION__);
  CloseInt(false);

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

  /* We should release mPCObserver on the main thread, but also prevent a double free.
  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));
  NS_ProxyRelease(mainThread, mPCObserver);
  */
}

// One level of indirection so we can use WrapRunnable in CreateMediaStream.
nsresult
PeerConnectionImpl::MakeMediaStream(nsIDOMWindow* aWindow,
                                    uint32_t aHint, nsIDOMMediaStream** aRetval)
{
  MOZ_ASSERT(aRetval);

  nsRefPtr<DOMMediaStream> stream =
    DOMMediaStream::CreateSourceStream(aWindow, aHint);
  NS_ADDREF(*aRetval = stream);

  CSFLogDebug(logTag, "Created media stream %p, inner: %p", stream.get(), stream->GetStream());

  return NS_OK;
}

nsresult
PeerConnectionImpl::CreateRemoteSourceStreamInfo(nsRefPtr<RemoteSourceStreamInfo>*
                                                 aInfo)
{
  MOZ_ASSERT(aInfo);
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  nsIDOMMediaStream* stream;

  // We need to pass a dummy hint here because FakeMediaStream currently
  // needs to actually propagate a hint for local streams.
  // TODO(ekr@rtfm.com): Clean up when we have explicit track lists.
  // See bug 834835.
  nsresult res = MakeMediaStream(mWindow, 0, &stream);
  if (NS_FAILED(res)) {
    return res;
  }

  DOMMediaStream* comstream = static_cast<DOMMediaStream*>(stream);
  static_cast<mozilla::SourceMediaStream*>(comstream->GetStream())->SetPullEnabled(true);

  nsRefPtr<RemoteSourceStreamInfo> remote;
  remote = new RemoteSourceStreamInfo(comstream, mMedia);
  *aInfo = remote;

  return NS_OK;
}

#ifdef MOZILLA_INTERNAL_API
static void
Warn(JSContext* aCx, const nsCString& aMsg) {
  CSFLogError(logTag, "Warning: %s", aMsg.get());
  nsIScriptContext* sc = GetScriptContextFromJSContext(aCx);
  if (sc) {
    nsCOMPtr<nsIDocument> doc;
    doc = nsContentUtils::GetDocumentFromScriptContext(sc);
    if (doc) {
      // Passing in line-# 1 hides peerconnection.js (shows document instead)
      nsContentUtils::ReportToConsoleNonLocalized(NS_ConvertUTF8toUTF16 (aMsg),
          nsIScriptError::warningFlag, logTag, doc, nullptr, EmptyString(), 1);
    }
  }
}
#endif

/**
 * In JS, an RTCConfiguration looks like this:
 *
 * { "iceServers": [ { url:"stun:23.21.150.121" },
 *                   { url:"turn:user@turn.example.org", credential:"mypass"} ] }
 *
 * This function converts an already-validated jsval that looks like the above
 * into an IceConfiguration object.
 */
nsresult
PeerConnectionImpl::ConvertRTCConfiguration(const JS::Value& aSrc,
                                            IceConfiguration *aDst,
                                            JSContext* aCx)
{
#ifdef MOZILLA_INTERNAL_API
  if (!aSrc.isObject()) {
    return NS_ERROR_FAILURE;
  }
  JSAutoCompartment ac(aCx, &aSrc.toObject());
  RTCConfiguration config;
  if (!(config.Init(aCx, nullptr, aSrc) && config.mIceServers.WasPassed())) {
    return NS_ERROR_FAILURE;
  }
  for (uint32_t i = 0; i < config.mIceServers.Value().Length(); i++) {
    // XXXbz once this moves to WebIDL, remove RTCConfiguration in DummyBinding.webidl.
    RTCIceServer& server = config.mIceServers.Value()[i];
    if (!server.mUrl.WasPassed()) {
      return NS_ERROR_FAILURE;
    }
    nsRefPtr<nsIURI> url;
    nsresult rv;
    rv = NS_NewURI(getter_AddRefs(url), server.mUrl.Value());
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
    if (!server.mCredential.IsEmpty()) {
      // TODO(jib@mozilla.com): Support username, credentials & TURN servers
      Warn(aCx, nsPrintfCString(ICE_PARSING
          ": Credentials not yet implemented. Omitting \"%s\"", spec.get()));
      continue;
    }
    if (isTurn || isTurns) {
      Warn(aCx, nsPrintfCString(ICE_PARSING
          ": TURN servers not yet supported. Treating as STUN: \"%s\"", spec.get()));
    }
    // TODO(jib@mozilla.com): Revisit once nsURI supports host and port on STUN
    int32_t port;
    nsAutoCString host;
    {
      uint32_t hostPos;
      int32_t hostLen;
      nsAutoCString path;
      rv = url->GetPath(path);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = net_GetAuthURLParser()->ParseAuthority(path.get(), path.Length(),
                                                  nullptr,  nullptr,
                                                  nullptr,  nullptr,
                                                  &hostPos,  &hostLen, &port);
      NS_ENSURE_SUCCESS(rv, rv);
      if (!hostLen) {
        return NS_ERROR_FAILURE;
      }
      path.Mid(host, hostPos, hostLen);
    }
    if (port == -1)
      port = (isStuns || isTurns)? 5349 : 3478;
    if (!aDst->addServer(host.get(), port)) {
      Warn(aCx, nsPrintfCString(ICE_PARSING
          ": FQDN not yet implemented (only IP-#s). Omitting \"%s\"", spec.get()));
    }
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::Initialize(IPeerConnectionObserver* aObserver,
                               nsIDOMWindow* aWindow,
                               const JS::Value &aRTCConfiguration,
                               nsIThread* aThread,
                               JSContext* aCx)
{
  return Initialize(aObserver, aWindow, nullptr, &aRTCConfiguration, aThread, aCx);
}

nsresult
PeerConnectionImpl::Initialize(IPeerConnectionObserver* aObserver,
                               nsIDOMWindow* aWindow,
                               const IceConfiguration* aConfiguration,
                               const JS::Value* aRTCConfiguration,
                               nsIThread* aThread,
                               JSContext* aCx)
{
  nsresult res;

#ifdef MOZILLA_INTERNAL_API
  MOZ_ASSERT(NS_IsMainThread());
#endif
  MOZ_ASSERT(aObserver);
  MOZ_ASSERT(aThread);
  mThread = aThread;

  mPCObserver = do_GetWeakReference(aObserver);

  // Find the STS thread

  mSTSThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);
  MOZ_ASSERT(mSTSThread);

#ifdef MOZILLA_INTERNAL_API
  // This code interferes with the C++ unit test startup code.
  nsCOMPtr<nsISupports> nssDummy = do_GetService("@mozilla.org/psm;1", &res);
  NS_ENSURE_SUCCESS(res, res);
  // Currently no standalone unit tests for DataChannel,
  // which is the user of mWindow
  MOZ_ASSERT(aWindow);
  mWindow = do_QueryInterface(aWindow);
  NS_ENSURE_STATE(mWindow);
#endif

  res = PeerConnectionCtx::InitializeGlobal(mThread);
  NS_ENSURE_SUCCESS(res, res);

  PeerConnectionCtx *pcctx = PeerConnectionCtx::GetInstance();
  MOZ_ASSERT(pcctx);

  mCall = pcctx->createCall();
  if(!mCall.get()) {
    CSFLogError(logTag, "%s: Couldn't Create Call Object", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  // Connect ICE slots.
  mMedia->SignalIceGatheringCompleted.connect(this, &PeerConnectionImpl::IceGatheringCompleted);
  mMedia->SignalIceCompleted.connect(this, &PeerConnectionImpl::IceCompleted);
  mMedia->SignalIceFailed.connect(this, &PeerConnectionImpl::IceFailed);

  // Initialize the media object.
  if (aRTCConfiguration) {
    IceConfiguration ic;
    res = ConvertRTCConfiguration(*aRTCConfiguration, &ic, aCx);
    NS_ENSURE_SUCCESS(res, res);
    res = mMedia->Init(ic.getServers());
  } else {
    res = mMedia->Init(aConfiguration->getServers());
  }
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: Couldn't initialize media object", __FUNCTION__);
    return res;
  }

  // Generate a random handle
  unsigned char handle_bin[8];
  PK11_GenerateRandom(handle_bin, sizeof(handle_bin));

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

  mHandle += hex;

  // Store under mHandle
  mCall->setPeerConnection(mHandle);
  PeerConnectionCtx::GetInstance()->mPeerConnections[mHandle] = this;

  // Create the DTLS Identity
  mIdentity = DtlsIdentity::Generate();

  if (!mIdentity) {
    CSFLogError(logTag, "%s: Generate returned NULL", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  // Set the fingerprint. Right now assume we only have one
  // DTLS identity
  unsigned char fingerprint[DTLS_FINGERPRINT_LENGTH];
  size_t fingerprint_length;
  res = mIdentity->ComputeFingerprint("sha-1",
                                      fingerprint,
                                      sizeof(fingerprint),
                                      &fingerprint_length);

  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: ComputeFingerprint failed: %u",
      __FUNCTION__, static_cast<uint32_t>(res));
    return res;
  }

  mFingerprint = "sha-1 " + mIdentity->FormatFingerprint(fingerprint,
                                                         fingerprint_length);
  if (NS_FAILED(res)) {
    CSFLogError(logTag, "%s: do_GetService failed: %u",
      __FUNCTION__, static_cast<uint32_t>(res));
    return res;
  }

#ifndef MOZILLA_INTERNAL_API
  // Busy-wait until we are ready, for C++ unit tests. Remove when tests are fixed.
  CSFLogDebug(logTag, "%s: Sleeping until kStarted", __FUNCTION__);
  while(PeerConnectionCtx::GetInstance()->sipcc_state() != kStarted) {
    PR_Sleep(100);
  }
#endif

  return NS_OK;
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

  nsresult res = MakeMediaStream(mWindow, aHint, aRetval);
  if (NS_FAILED(res)) {
    return res;
  }

  if (!mute) {
    if (aHint & DOMMediaStream::HINT_CONTENTS_AUDIO) {
      new Fake_AudioGenerator(static_cast<DOMMediaStream*>(*aRetval));
    } else {
#ifdef MOZILLA_INTERNAL_API
    new Fake_VideoGenerator(static_cast<DOMMediaStream*>(*aRetval));
#endif
    }
  }

  return NS_OK;
}

// Data channels won't work without a window, so in order for the C++ unit
// tests to work (it doesn't have a window available) we ifdef the following
// two implementations.
NS_IMETHODIMP
PeerConnectionImpl::ConnectDataConnection(uint16_t aLocalport,
                                          uint16_t aRemoteport,
                                          uint16_t aNumstreams)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

#ifdef MOZILLA_INTERNAL_API
  if (mDataConnection) {
    CSFLogError(logTag,"%s DataConnection already connected",__FUNCTION__);
    // Ignore the request to connect when already connected.  This entire
    // implementation is temporary.  Ignore aNumstreams as it's merely advisory
    // and we increase the number of streams dynamically as needed.
    return NS_OK;
  }
  mDataConnection = new mozilla::DataChannelConnection(this);
  if (!mDataConnection->Init(aLocalport, aNumstreams, true)) {
    CSFLogError(logTag,"%s DataConnection Init Failed",__FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  // XXX Fix! Get the correct flow for DataChannel. Also error handling.
  for (int i = 2; i >= 0; i--) {
    nsRefPtr<TransportFlow> flow = mMedia->GetTransportFlow(i,false).get();
    CSFLogDebug(logTag, "Transportflow[%d] = %p", i, flow.get());
    if (flow) {
      if (!mDataConnection->ConnectDTLS(flow, aLocalport, aRemoteport)) {
        return NS_ERROR_FAILURE;
      }
      break;
    }
  }
  return NS_OK;
#else
    return NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP
PeerConnectionImpl::CreateDataChannel(const nsACString& aLabel,
                                      uint16_t aType,
                                      bool outOfOrderAllowed,
                                      uint16_t aMaxTime,
                                      uint16_t aMaxNum,
                                      nsIDOMDataChannel** aRetval)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aRetval);

#ifdef MOZILLA_INTERNAL_API
  nsRefPtr<mozilla::DataChannel> dataChannel;
  mozilla::DataChannelConnection::Type theType =
    static_cast<mozilla::DataChannelConnection::Type>(aType);

  if (!mDataConnection) {
    return NS_ERROR_FAILURE;
  }
  dataChannel = mDataConnection->Open(
    aLabel, theType, !outOfOrderAllowed,
    aType == mozilla::DataChannelConnection::PARTIAL_RELIABLE_REXMIT ? aMaxNum :
    (aType == mozilla::DataChannelConnection::PARTIAL_RELIABLE_TIMED ? aMaxTime : 0),
    nullptr, nullptr
  );
  NS_ENSURE_TRUE(dataChannel,NS_ERROR_FAILURE);

  CSFLogDebug(logTag, "%s: making DOMDataChannel", __FUNCTION__);

  // TODO -- need something like "mCall->addStream(stream_id, 0, DATA);" so
  // the SDP can be generated correctly

  return NS_NewDOMDataChannel(dataChannel.forget(), mWindow, aRetval);
#else
  return NS_OK;
#endif
}

void
PeerConnectionImpl::NotifyConnection()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  CSFLogDebug(logTag, __FUNCTION__);

#ifdef MOZILLA_INTERNAL_API
  nsCOMPtr<IPeerConnectionObserver> pco = do_QueryReferent(mPCObserver);
  if (!pco) {
    return;
  }
  RUN_ON_THREAD(mThread,
                WrapRunnable(pco,
                             &IPeerConnectionObserver::NotifyConnection),
                NS_DISPATCH_NORMAL);
#endif
}

void
PeerConnectionImpl::NotifyClosedConnection()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  CSFLogDebug(logTag, __FUNCTION__);

#ifdef MOZILLA_INTERNAL_API
  nsCOMPtr<IPeerConnectionObserver> pco = do_QueryReferent(mPCObserver);
  if (!pco) {
    return;
  }
  RUN_ON_THREAD(mThread,
    WrapRunnable(pco, &IPeerConnectionObserver::NotifyClosedConnection),
    NS_DISPATCH_NORMAL);
#endif
}


#ifdef MOZILLA_INTERNAL_API
// Not a member function so that we don't need to keep the PC live.
static void NotifyDataChannel_m(nsRefPtr<nsIDOMDataChannel> aChannel,
                                nsCOMPtr<IPeerConnectionObserver> aObserver)
{
  MOZ_ASSERT(NS_IsMainThread());

  aObserver->NotifyDataChannel(aChannel);
  NS_DataChannelAppReady(aChannel);
}
#endif

void
PeerConnectionImpl::NotifyDataChannel(already_AddRefed<mozilla::DataChannel> aChannel)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aChannel.get());

  CSFLogDebug(logTag, "%s: channel: %p", __FUNCTION__, aChannel.get());

#ifdef MOZILLA_INTERNAL_API
  nsCOMPtr<nsIDOMDataChannel> domchannel;
  nsresult rv = NS_NewDOMDataChannel(aChannel, mWindow,
                                     getter_AddRefs(domchannel));
  NS_ENSURE_SUCCESS_VOID(rv);

  nsCOMPtr<IPeerConnectionObserver> pco = do_QueryReferent(mPCObserver);
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

/**
 * Constraints look like this:
 *
 * {
 *   mandatory: {"OfferToReceiveAudio": true, "OfferToReceiveVideo": true },
 *   optional: [{"VoiceActivityDetection": true}, {"FooBar": 10}]
 * }
 *
 * Optional constraints are ordered, and hence in an array. This function
 * converts a jsval that looks like the above into a MediaConstraints object.
 */
nsresult
PeerConnectionImpl::ConvertConstraints(
  const JS::Value& aConstraints, MediaConstraints* aObj, JSContext* aCx)
{
  JS::Value mandatory, optional;
  JSObject& constraints = aConstraints.toObject();

  // Mandatory constraints.  Note that we only care if the constraint array exists
  if (!JS_GetProperty(aCx, &constraints, "mandatory", &mandatory)) {
    return NS_ERROR_FAILURE;
  }
  if (!mandatory.isNullOrUndefined()) {
    if (!mandatory.isObject()) {
      return NS_ERROR_FAILURE;
    }

    JSObject* opts = &mandatory.toObject();
    JS::AutoIdArray mandatoryOpts(aCx, JS_Enumerate(aCx, opts));

    // Iterate over each property.
    for (size_t i = 0; i < mandatoryOpts.length(); i++) {
      JS::Value option, optionName;
      if (!JS_GetPropertyById(aCx, opts, mandatoryOpts[i], &option) ||
          !JS_IdToValue(aCx, mandatoryOpts[i], &optionName) ||
          // We only support boolean constraints for now.
          !option.isBoolean()) {
        return NS_ERROR_FAILURE;
      }
      JSString* optionNameString = JS_ValueToString(aCx, optionName);
      if (!optionNameString) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
      NS_ConvertUTF16toUTF8 stringVal(JS_GetStringCharsZ(aCx, optionNameString));
      aObj->setBooleanConstraint(stringVal.get(), JSVAL_TO_BOOLEAN(option), true);
    }
  }

  // Optional constraints.
  if (!JS_GetProperty(aCx, &constraints, "optional", &optional)) {
    return NS_ERROR_FAILURE;
  }
  if (!optional.isNullOrUndefined()) {
    if (!optional.isObject()) {
      return NS_ERROR_FAILURE;
    }

    JSObject* array = &optional.toObject();
    uint32_t length;
    if (!JS_GetArrayLength(aCx, array, &length)) {
      return NS_ERROR_FAILURE;
    }
    for (uint32_t i = 0; i < length; i++) {
      JS::Value element;
      if (!JS_GetElement(aCx, array, i, &element) ||
          !element.isObject()) {
        return NS_ERROR_FAILURE;
      }
      JSObject* opts = &element.toObject();
      JS::AutoIdArray optionalOpts(aCx, JS_Enumerate(aCx, opts));
      // Expect one property per entry.
      if (optionalOpts.length() != 1) {
        return NS_ERROR_FAILURE;
      }
      JS::Value option, optionName;
      if (!JS_GetPropertyById(aCx, opts, optionalOpts[0], &option) ||
          !JS_IdToValue(aCx, optionalOpts[0], &optionName)) {
        return NS_ERROR_FAILURE;
      }
      // Ignore constraints other than boolean, as that's all we support.
      if (option.isBoolean()) {
        JSString* optionNameString = JS_ValueToString(aCx, optionName);
        if (!optionNameString) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        NS_ConvertUTF16toUTF8 stringVal(JS_GetStringCharsZ(aCx, optionNameString));
        aObj->setBooleanConstraint(stringVal.get(), JSVAL_TO_BOOLEAN(option), false);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CreateOffer(const JS::Value& aConstraints, JSContext* aCx)
{
  PC_AUTO_ENTER_API_CALL(true);

  MediaConstraints cs;
  nsresult rv = ConvertConstraints(aConstraints, &cs, aCx);
  if (rv != NS_OK) {
    return rv;
  }

  return CreateOffer(cs);
}

// Used by unit tests and the IDL CreateOffer.
NS_IMETHODIMP
PeerConnectionImpl::CreateOffer(MediaConstraints& constraints)
{
  PC_AUTO_ENTER_API_CALL(true);

  mRole = kRoleOfferer;  // TODO(ekr@rtfm.com): Interrogate SIPCC here?

  cc_media_constraints_t* cc_constraints = nullptr;
  constraints.buildArray(&cc_constraints);

  mCall->createOffer(cc_constraints);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CreateAnswer(const JS::Value& aConstraints, JSContext* aCx)
{
  PC_AUTO_ENTER_API_CALL(true);

  MediaConstraints cs;
  nsresult rv = ConvertConstraints(aConstraints, &cs, aCx);
  if (rv != NS_OK) {
    return rv;
  }

  return CreateAnswer(cs);
}

NS_IMETHODIMP
PeerConnectionImpl::CreateAnswer(MediaConstraints& constraints)
{
  PC_AUTO_ENTER_API_CALL(true);

  mRole = kRoleAnswerer;  // TODO(ekr@rtfm.com): Interrogate SIPCC here?

  cc_media_constraints_t* cc_constraints = nullptr;
  constraints.buildArray(&cc_constraints);

  mCall->createAnswer(cc_constraints);
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

  mLocalRequestedSDP = aSDP;
  mCall->setLocalDescription((cc_jsep_action_t)aAction, mLocalRequestedSDP);
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

  mRemoteRequestedSDP = aSDP;
  mCall->setRemoteDescription((cc_jsep_action_t)action, mRemoteRequestedSDP);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::AddIceCandidate(const char* aCandidate, const char* aMid, unsigned short aLevel) {
  PC_AUTO_ENTER_API_CALL(true);

  mCall->addICECandidate(aCandidate, aMid, aLevel);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CloseStreams() {
  PC_AUTO_ENTER_API_CALL(false);

  if (mReadyState != PeerConnectionImpl::kClosed)  {
    ChangeReadyState(PeerConnectionImpl::kClosing);
  }

  CSFLogInfo(logTag, "%s: Ending associated call", __FUNCTION__);

  mCall->endCall();
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::AddStream(nsIDOMMediaStream* aMediaStream) {
  PC_AUTO_ENTER_API_CALL(true);

  if (!aMediaStream) {
    CSFLogError(logTag, "%s: Attempt to add null stream",__FUNCTION__);
    return NS_ERROR_FAILURE;
  }

  DOMMediaStream* stream = static_cast<DOMMediaStream*>(aMediaStream);
  uint32_t hints = stream->GetHintContents();

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
  nsresult res = mMedia->AddStream(aMediaStream, &stream_id);
  if (NS_FAILED(res))
    return res;

  // TODO(ekr@rtfm.com): these integers should be the track IDs
  if (hints & DOMMediaStream::HINT_CONTENTS_AUDIO) {
    mCall->addStream(stream_id, 0, AUDIO);
    mNumAudioStreams++;
  }

  if (hints & DOMMediaStream::HINT_CONTENTS_VIDEO) {
    mCall->addStream(stream_id, 1, VIDEO);
    mNumVideoStreams++;
  }

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::RemoveStream(nsIDOMMediaStream* aMediaStream) {
  PC_AUTO_ENTER_API_CALL(true);

  uint32_t stream_id;
  nsresult res = mMedia->RemoveStream(aMediaStream, &stream_id);

  if (NS_FAILED(res))
    return res;

  DOMMediaStream* stream = static_cast<DOMMediaStream*>(aMediaStream);
  uint32_t hints = stream->GetHintContents();

  if (hints & DOMMediaStream::HINT_CONTENTS_AUDIO) {
    mCall->removeStream(stream_id, 0, AUDIO);
    MOZ_ASSERT(mNumAudioStreams > 0);
    mNumAudioStreams--;
  }

  if (hints & DOMMediaStream::HINT_CONTENTS_VIDEO) {
    mCall->removeStream(stream_id, 1, VIDEO);
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

  if (fingerprint != NULL && (strcmp(hash, "sha-1") == 0)) {
    mRemoteFingerprint = std::string(fingerprint);
    CSFLogDebug(logTag, "Setting remote fingerprint to %s", mRemoteFingerprint.c_str());
    return NS_OK;
  } else {
    CSFLogError(logTag, "%s: Invalid Remote Finger Print", __FUNCTION__);
    return NS_ERROR_FAILURE;
  }
}

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
*/

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
PeerConnectionImpl::GetReadyState(uint32_t* aState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mReadyState;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetSipccState(uint32_t* aState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  PeerConnectionCtx* pcctx = PeerConnectionCtx::GetInstance();
  *aState = pcctx ? pcctx->sipcc_state() : kIdle;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetIceState(uint32_t* aState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aState);

  *aState = mIceState;
  return NS_OK;
}

nsresult
PeerConnectionImpl::CheckApiState(bool assert_ice_ready) const
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  PR_ASSERT(!assert_ice_ready || (mIceState != kIceGathering));

  if (mReadyState == kClosed)
    return NS_ERROR_FAILURE;
  if (!mMedia)
    return NS_ERROR_FAILURE;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::Close(bool aIsSynchronous)
{
  CSFLogDebug(logTag, __FUNCTION__);
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  return CloseInt(aIsSynchronous);
}


nsresult
PeerConnectionImpl::CloseInt(bool aIsSynchronous)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  if (mCall) {
    CSFLogInfo(logTag, "%s: Closing PeerConnectionImpl; "
                       "ending call", __FUNCTION__);
    mCall->endCall();
  }
#ifdef MOZILLA_INTERNAL_API
  if (mDataConnection) {
    mDataConnection->Destroy();
    mDataConnection = nullptr; // it may not go away until the runnables are dead
  }
#endif

  ShutdownMedia(aIsSynchronous);

  // DataConnection will need to stay alive until all threads/runnables exit

  return NS_OK;
}

void
PeerConnectionImpl::ShutdownMedia(bool aIsSynchronous)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  if (!mMedia)
    return;

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
PeerConnectionImpl::onCallEvent(ccapi_call_event_e aCallEvent,
                                CSF::CC_CallPtr aCall, CSF::CC_CallInfoPtr aInfo)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aCall.get());
  MOZ_ASSERT(aInfo.get());

  cc_call_state_t event = aInfo->getCallState();
  std::string statestr = aInfo->callStateToString(event);

  if (CCAPI_CALL_EV_CREATED != aCallEvent && CCAPI_CALL_EV_STATE != aCallEvent) {
    CSFLogDebug(logTag, "%s: **** CALL HANDLE IS: %s, **** CALL STATE IS: %s",
      __FUNCTION__, mHandle.c_str(), statestr.c_str());
    return;
  }

  switch (event) {
    case SETLOCALDESC:
    case UPDATELOCALDESC:
      mLocalSDP = aInfo->getSDP();
      break;

    case SETREMOTEDESC:
    case ADDICECANDIDATE:
      mRemoteSDP = aInfo->getSDP();
      break;

    case CONNECTED:
      CSFLogDebug(logTag, "Setting PeerConnnection state to kActive");
      ChangeReadyState(kActive);
      break;
    default:
      break;
  }

  nsCOMPtr<IPeerConnectionObserver> pco = do_QueryReferent(mPCObserver);
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
PeerConnectionImpl::ChangeReadyState(PeerConnectionImpl::ReadyState aReadyState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  mReadyState = aReadyState;

  // Note that we are passing an nsRefPtr<IPeerConnectionObserver> which
  // keeps the observer live.
  nsCOMPtr<IPeerConnectionObserver> pco = do_QueryReferent(mPCObserver);
  if (!pco) {
    return;
  }
  RUN_ON_THREAD(mThread, WrapRunnable(pco,
                                      &IPeerConnectionObserver::OnStateChange,
                                      // static_cast needed to work around old Android NDK r5c compiler
                                      static_cast<int>(IPeerConnectionObserver::kReadyState)),
    NS_DISPATCH_NORMAL);
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

// This is called from the STS thread and so we need to thunk
// to the main thread.
void
PeerConnectionImpl::IceGatheringCompleted(NrIceCtx *aCtx)
{
  (void) aCtx;
  // Do an async call here to unwind the stack. refptr keeps the PC alive.
  nsRefPtr<PeerConnectionImpl> pc(this);
  RUN_ON_THREAD(mThread,
                WrapRunnable(pc,
                             &PeerConnectionImpl::IceStateChange_m,
                             kIceWaiting),
                NS_DISPATCH_NORMAL);
}

void
PeerConnectionImpl::IceCompleted(NrIceCtx *aCtx)
{
  (void) aCtx;
  // Do an async call here to unwind the stack. refptr keeps the PC alive.
  nsRefPtr<PeerConnectionImpl> pc(this);
  RUN_ON_THREAD(mThread,
                WrapRunnable(pc,
                             &PeerConnectionImpl::IceStateChange_m,
                             kIceConnected),
                NS_DISPATCH_NORMAL);
}

void
PeerConnectionImpl::IceFailed(NrIceCtx *aCtx)
{
  (void) aCtx;
  // Do an async call here to unwind the stack. refptr keeps the PC alive.
  nsRefPtr<PeerConnectionImpl> pc(this);
  RUN_ON_THREAD(mThread,
                WrapRunnable(pc,
                             &PeerConnectionImpl::IceStateChange_m,
                             kIceFailed),
                NS_DISPATCH_NORMAL);
}

nsresult
PeerConnectionImpl::IceStateChange_m(IceState aState)
{
  PC_AUTO_ENTER_API_CALL(false);

  CSFLogDebug(logTag, __FUNCTION__);

  mIceState = aState;

#ifdef MOZILLA_INTERNAL_API
  nsCOMPtr<IPeerConnectionObserver> pco = do_QueryReferent(mPCObserver);
  if (!pco) {
    return NS_OK;
  }
  RUN_ON_THREAD(mThread,
                WrapRunnable(pco,
                             &IPeerConnectionObserver::OnStateChange,
                             // static_cast required to work around old C++ compiler on Android NDK r5c
                             static_cast<int>(IPeerConnectionObserver::kIceState)),
                NS_DISPATCH_NORMAL);
#endif
  return NS_OK;
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


#ifdef MOZILLA_INTERNAL_API
static nsresult
GetStreams(JSContext* cx, PeerConnectionImpl* peerConnection,
           MediaStreamList::StreamType type, JS::Value* streams)
{
  nsAutoPtr<MediaStreamList> list(new MediaStreamList(peerConnection, type));

  ErrorResult rv;
  JSObject* obj = list->WrapObject(cx, rv);
  if (rv.Failed()) {
    streams->setNull();
    return rv.ErrorCode();
  }

  // Transfer ownership to the binding.
  streams->setObject(*obj);
  list.forget();
  return NS_OK;
}
#endif

NS_IMETHODIMP
PeerConnectionImpl::GetLocalStreams(JSContext* cx, JS::Value* streams)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
#ifdef MOZILLA_INTERNAL_API
  return GetStreams(cx, this, MediaStreamList::Local, streams);
#else
  return NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP
PeerConnectionImpl::GetRemoteStreams(JSContext* cx, JS::Value* streams)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
#ifdef MOZILLA_INTERNAL_API
  return GetStreams(cx, this, MediaStreamList::Remote, streams);
#else
  return NS_ERROR_FAILURE;
#endif
}


}  // end sipcc namespace
