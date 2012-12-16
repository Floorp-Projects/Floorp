/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include "vcm.h"
#include "CSFLog.h"
#include "CSFLogStream.h"
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

#include "nsThreadUtils.h"
#include "nsProxyRelease.h"

#include "runnable_utils.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionImpl.h"

#include "nsPIDOMWindow.h"
#include "nsDOMDataChannel.h"
#ifdef MOZILLA_INTERNAL_API
#include "MediaStreamList.h"
#include "nsIScriptGlobalObject.h"
#include "jsapi.h"
#endif

#ifndef USE_FAKE_MEDIA_STREAMS
#include "MediaSegment.h"
#endif

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
        mObserver->OnSetLocalDescriptionSuccess(mCode);
        break;

      case SETREMOTEDESC:
        mObserver->OnSetRemoteDescriptionSuccess(mCode);
        break;

      case SETLOCALDESCERROR:
        mObserver->OnSetLocalDescriptionError(mCode);
        break;

      case SETREMOTEDESCERROR:
        mObserver->OnSetRemoteDescriptionError(mCode);
        break;

      case REMOTESTREAMADD:
        {
          nsDOMMediaStream* stream;
          uint32_t hint;

          if (!mRemoteStream) {
            CSFLogErrorS(logTag, __FUNCTION__ << " GetRemoteStream returned NULL");
          } else {
            stream = mRemoteStream->GetMediaStream();
            hint = stream->GetHintContents();
            if (hint == nsDOMMediaStream::HINT_CONTENTS_AUDIO) {
              mObserver->OnAddStream(stream, "audio");
            } else if (hint == nsDOMMediaStream::HINT_CONTENTS_VIDEO) {
              mObserver->OnAddStream(stream, "video");
            } else {
              CSFLogErrorS(logTag, __FUNCTION__ << "Audio & Video not supported");
              MOZ_ASSERT(PR_FALSE);
            }
          }
          break;
        }
      default:
        CSFLogDebugS(logTag, ": **** UNHANDLED CALL STATE : " << mStateStr);
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
  , mMedia(new PeerConnectionMedia(this)) {
  MOZ_ASSERT(NS_IsMainThread());
}

PeerConnectionImpl::~PeerConnectionImpl()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  PeerConnectionCtx::GetInstance()->mPeerConnections.erase(mHandle);
  CloseInt(false);

#if 0
  // TODO(ekr@rtfm.com): figure out how to shut down PCCtx.
  // bug 820011.

  // Since this and Initialize() occur on MainThread, they can't both be
  // running at once
  // Might be more optimal to release off a timer (and XPCOM Shutdown)
  // to avoid churn
  if (PeerConnectionCtx::GetInstance()->mPeerConnections.empty())
    Shutdown();
#endif

  /* We should release mPCObserver on the main thread, but also prevent a double free.
  nsCOMPtr<nsIThread> mainThread;
  NS_GetMainThread(getter_AddRefs(mainThread));
  NS_ProxyRelease(mainThread, mPCObserver);
  */
}

// One level of indirection so we can use WrapRunnable in CreateMediaStream.
nsresult
PeerConnectionImpl::MakeMediaStream(uint32_t aHint, nsIDOMMediaStream** aRetval)
{
  MOZ_ASSERT(aRetval);

  nsRefPtr<nsDOMMediaStream> stream = nsDOMMediaStream::CreateSourceStream(aHint);
  NS_ADDREF(*aRetval = stream);

  CSFLogDebugS(logTag, "Created media stream " << static_cast<void*>(stream)
    << " inner: " << static_cast<void*>(stream->GetStream()));

  return NS_OK;
}

nsresult
PeerConnectionImpl::MakeRemoteSource(nsDOMMediaStream* aStream, RemoteSourceStreamInfo** aInfo)
{
  MOZ_ASSERT(aInfo);
  MOZ_ASSERT(aStream);

  // TODO(ekr@rtfm.com): Add the track info with the first segment
  nsRefPtr<RemoteSourceStreamInfo> remote = new RemoteSourceStreamInfo(aStream);
  NS_ADDREF(*aInfo = remote);
  return NS_OK;
}

nsresult
PeerConnectionImpl::CreateRemoteSourceStreamInfo(uint32_t aHint, RemoteSourceStreamInfo** aInfo)
{
  MOZ_ASSERT(aInfo);
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  nsIDOMMediaStream* stream;

  nsresult res = MakeMediaStream(aHint, &stream);
  if (NS_FAILED(res)) {
    return res;
  }

  nsDOMMediaStream* comstream = static_cast<nsDOMMediaStream*>(stream);
  static_cast<mozilla::SourceMediaStream*>(comstream->GetStream())->SetPullEnabled(true);

  nsRefPtr<RemoteSourceStreamInfo> remote;
  if (!mThread || NS_IsMainThread()) {
    remote = new RemoteSourceStreamInfo(comstream);
    NS_ADDREF(*aInfo = remote);
    return NS_OK;
  }

  mThread->Dispatch(WrapRunnableNMRet(
    &PeerConnectionImpl::MakeRemoteSource, comstream, aInfo, &res
  ), NS_DISPATCH_SYNC);

  if (NS_FAILED(res)) {
    return res;
  }

  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::Initialize(IPeerConnectionObserver* aObserver,
                               nsIDOMWindow* aWindow,
                               nsIThread* aThread) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aObserver);
  MOZ_ASSERT(aThread);
  mPCObserver = aObserver;

  nsresult res;

#ifdef MOZILLA_INTERNAL_API
  // This code interferes with the C++ unit test startup code.
  nsCOMPtr<nsISupports> nssDummy = do_GetService("@mozilla.org/psm;1", &res);
  NS_ENSURE_SUCCESS(res, res);
#endif

  mThread = aThread;

#ifdef MOZILLA_INTERNAL_API
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
    CSFLogErrorS(logTag, __FUNCTION__ << ": Couldn't Create Call Object");
    return NS_ERROR_FAILURE;
  }

  // Connect ICE slots.
  mMedia->SignalIceGatheringCompleted.connect(this, &PeerConnectionImpl::IceGatheringCompleted);
  mMedia->SignalIceCompleted.connect(this, &PeerConnectionImpl::IceCompleted);

  // Initialize the media object.
  res = mMedia->Init();
  if (NS_FAILED(res)) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": Couldn't initialize media object");
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
    CSFLogErrorS(logTag, __FUNCTION__ << ": Generate returned NULL");
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
    CSFLogErrorS(logTag, __FUNCTION__ << ": ComputeFingerprint failed: " <<
        static_cast<uint32_t>(res));
    return res;
  }

  mFingerprint = "sha-1 " + mIdentity->FormatFingerprint(fingerprint,
                                                         fingerprint_length);

  // Find the STS thread
  mSTSThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);

  if (NS_FAILED(res)) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": do_GetService failed: " <<
        static_cast<uint32_t>(res));
    return res;
  }

#ifndef MOZILLA_INTERNAL_API
  // Busy-wait until we are ready, for C++ unit tests. Remove when tests are fixed.
  CSFLogDebugS(logTag, __FUNCTION__ << ": Sleeping until kStarted");
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

  nsresult res;
  if (!mThread || NS_IsMainThread()) {
    res = MakeMediaStream(aHint, aRetval);
  } else {
    mThread->Dispatch(WrapRunnableNMRet(
        &PeerConnectionImpl::MakeMediaStream, aHint, aRetval, &res
    ), NS_DISPATCH_SYNC);
  }

  if (NS_FAILED(res)) {
    return res;
  }

  if (!mute) {
    if (aHint & nsDOMMediaStream::HINT_CONTENTS_AUDIO) {
      new Fake_AudioGenerator(static_cast<nsDOMMediaStream*>(*aRetval));
    } else {
#ifdef MOZILLA_INTERNAL_API
    new Fake_VideoGenerator(static_cast<nsDOMMediaStream*>(*aRetval));
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
  mDataConnection = new mozilla::DataChannelConnection(this);
  NS_ENSURE_TRUE(mDataConnection,NS_ERROR_FAILURE);
  if (!mDataConnection->Init(aLocalport, aNumstreams, true)) {
    CSFLogError(logTag,"%s DataConnection Init Failed",__FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  // XXX Fix! Get the correct flow for DataChannel. Also error handling.
  for (int i = 2; i >= 0; i--) {
    nsRefPtr<TransportFlow> flow = mMedia->GetTransportFlow(i,false).get();
    CSFLogDebugS(logTag, "Transportflow[" << i << "] = " << flow.get());
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

  CSFLogDebugS(logTag, __FUNCTION__ << ": making DOMDataChannel");

  return NS_NewDOMDataChannel(dataChannel.forget(), mWindow, aRetval);
#else
  return NS_OK;
#endif
}

void
PeerConnectionImpl::NotifyConnection()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  CSFLogDebugS(logTag, __FUNCTION__);

#ifdef MOZILLA_INTERNAL_API
  RUN_ON_THREAD(mThread,
                WrapRunnable(mPCObserver,
                             &IPeerConnectionObserver::NotifyConnection),
                NS_DISPATCH_NORMAL);
#endif
}

void
PeerConnectionImpl::NotifyClosedConnection()
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  CSFLogDebugS(logTag, __FUNCTION__);

#ifdef MOZILLA_INTERNAL_API
  RUN_ON_THREAD(mThread,
                WrapRunnable(mPCObserver,
                             &IPeerConnectionObserver::NotifyClosedConnection),
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
PeerConnectionImpl::NotifyDataChannel(mozilla::DataChannel *aChannel)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aChannel);

  CSFLogDebugS(logTag, __FUNCTION__ << ": channel: " << static_cast<void*>(aChannel));

#ifdef MOZILLA_INTERNAL_API
   nsCOMPtr<nsIDOMDataChannel> domchannel;
   nsresult rv = NS_NewDOMDataChannel(aChannel, mWindow,
                                      getter_AddRefs(domchannel));
  NS_ENSURE_SUCCESS(rv,);

  RUN_ON_THREAD(mThread,
                WrapRunnableNM(NotifyDataChannel_m,
                               domchannel.get(),
                               mPCObserver),
                NS_DISPATCH_NORMAL);
#endif
}

/**
 * Constraints look like this:
 *
 * {
 *    "mandatory": {"foo":"hello", "bar": false, "baz": 10},
 *    "optional": [{"hello":"foo"}, {"baz": false}]
 * }
 *
 * Optional constraints are ordered, and hence in an array. This function
 * converts a jsval that looks like the above into a MediaConstraints object.
 */
nsresult
PeerConnectionImpl::ConvertConstraints(
  const JS::Value& aConstraints, MediaConstraints* aObj, JSContext* aCx)
{
  size_t i;
  jsval mandatory, optional;
  JSObject& constraints = aConstraints.toObject();

  // Mandatory constraints.
  if (JS_GetProperty(aCx, &constraints, "mandatory", &mandatory)) {
    if (JSVAL_IS_PRIMITIVE(mandatory) && mandatory.isObject() && !JSVAL_IS_NULL(mandatory)) {
      JSObject* opts = JSVAL_TO_OBJECT(mandatory);
      JS::AutoIdArray mandatoryOpts(aCx, JS_Enumerate(aCx, opts));

      // Iterate over each property.
      for (i = 0; i < mandatoryOpts.length(); i++) {
        jsval option, optionName;
        if (JS_GetPropertyById(aCx, opts, mandatoryOpts[i], &option)) {
          if (JS_IdToValue(aCx, mandatoryOpts[i], &optionName)) {
            // We only support boolean constraints for now.
            if (JSVAL_IS_BOOLEAN(option)) {
              JSString* optionNameString = JS_ValueToString(aCx, optionName);
              NS_ConvertUTF16toUTF8 stringVal(JS_GetStringCharsZ(aCx, optionNameString));
              aObj->setBooleanConstraint(stringVal.get(), JSVAL_TO_BOOLEAN(option), true);
            }
          }
        }
      }
    }
  }

  // Optional constraints.
  if (JS_GetProperty(aCx, &constraints, "optional", &optional)) {
    if (JSVAL_IS_PRIMITIVE(optional) && optional.isObject() && !JSVAL_IS_NULL(optional)) {
      JSObject* opts = JSVAL_TO_OBJECT(optional);
      if (JS_IsArrayObject(aCx, opts)) {
        uint32_t length;
        if (!JS_GetArrayLength(aCx, opts, &length)) {
          return NS_ERROR_FAILURE;
        }
        for (i = 0; i < length; i++) {
          jsval val;
          JS_GetElement(aCx, opts, i, &val);
          if (JSVAL_IS_PRIMITIVE(val)) {
            // Extract name & value and store.
            // FIXME: MediaConstraints does not support optional constraints?
          }
        }
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

  mCall->endCall();
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::AddStream(nsIDOMMediaStream* aMediaStream) {
  PC_AUTO_ENTER_API_CALL(true);

  uint32_t stream_id;
  nsresult res = mMedia->AddStream(aMediaStream, &stream_id);
  if (NS_FAILED(res))
    return res;

  nsDOMMediaStream* stream = static_cast<nsDOMMediaStream*>(aMediaStream);
  uint32_t hints = stream->GetHintContents();

  // TODO(ekr@rtfm.com): these integers should be the track IDs
  if (hints & nsDOMMediaStream::HINT_CONTENTS_AUDIO) {
    mCall->addStream(stream_id, 0, AUDIO);
  }

  if (hints & nsDOMMediaStream::HINT_CONTENTS_VIDEO) {
    mCall->addStream(stream_id, 1, VIDEO);
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

  nsDOMMediaStream* stream = static_cast<nsDOMMediaStream*>(aMediaStream);
  uint32_t hints = stream->GetHintContents();

  if (hints & nsDOMMediaStream::HINT_CONTENTS_AUDIO) {
    mCall->removeStream(stream_id, 0, AUDIO);
  }

  if (hints & nsDOMMediaStream::HINT_CONTENTS_VIDEO) {
    mCall->removeStream(stream_id, 1, VIDEO);
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
    CSFLogDebugS(logTag, "Setting remote fingerprint to " << mRemoteFingerprint);
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
  PC_AUTO_ENTER_API_CALL(true);
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
  PC_AUTO_ENTER_API_CALL(true);
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
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::Close(bool aIsSynchronous)
{
  CSFLogDebugS(logTag, __FUNCTION__);
  PC_AUTO_ENTER_API_CALL(false);

  return CloseInt(aIsSynchronous);
}


nsresult
PeerConnectionImpl::CloseInt(bool aIsSynchronous)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();

  if (mCall != nullptr)
    mCall->endCall();
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

  // Post back to our own thread to shutdown the media objects.
  // This avoids reentrancy issues with the garbage collector.
  // Note that no media calls may be made after this point
  // because we have removed the pointer.
  // For the aIsSynchronous case, we *know* the PeerConnection is
  // still alive, and are shutting it down on network teardown/etc, so
  // recursive GC isn't an issue. (Recursive GC should assert)

  // Forget the reference so that we can transfer it to
  // SelfDestruct().
  RUN_ON_THREAD(mThread, WrapRunnable(mMedia.forget().get(),
                                      &PeerConnectionMedia::SelfDestruct),
                aIsSynchronous ? NS_DISPATCH_SYNC : NS_DISPATCH_NORMAL);
}

void
PeerConnectionImpl::Shutdown()
{
  PeerConnectionCtx::Destroy();
}

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
    CSFLogDebugS(logTag, ": **** CALL HANDLE IS: " << mHandle <<
      ": **** CALL STATE IS: " << statestr);
    return;
  }

  switch (event) {
    case SETLOCALDESC:
      mLocalSDP = mLocalRequestedSDP;
      break;
    case SETREMOTEDESC:
      mRemoteSDP = mRemoteRequestedSDP;
      break;
    case CONNECTED:
      CSFLogDebugS(logTag, "Setting PeerConnnection state to kActive");
      ChangeReadyState(kActive);
      break;
    default:
      break;
  }

  if (mPCObserver) {
    PeerConnectionObserverDispatch* runnable =
        new PeerConnectionObserverDispatch(aInfo, this, mPCObserver);

    if (mThread) {
      mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
      return;
    }
    runnable->Run();
  }
}

void
PeerConnectionImpl::ChangeReadyState(PeerConnectionImpl::ReadyState aReadyState)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  mReadyState = aReadyState;

  // Note that we are passing an nsRefPtr<IPeerConnectionObserver> which
  // keeps the observer live.
  RUN_ON_THREAD(mThread, WrapRunnable(mPCObserver,
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
  RUN_ON_THREAD(mThread,
                WrapRunnable(this,
                             &PeerConnectionImpl::IceGatheringCompleted_m,
                             aCtx),
                NS_DISPATCH_SYNC);
}

void
PeerConnectionImpl::IceGatheringCompleted_m(NrIceCtx *aCtx)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aCtx);

  CSFLogDebugS(logTag, __FUNCTION__ << ": ctx: " << static_cast<void*>(aCtx));

  mIceState = kIceWaiting;

#ifdef MOZILLA_INTERNAL_API
  if (mPCObserver) {
    RUN_ON_THREAD(mThread,
                  WrapRunnable(mPCObserver,
                               &IPeerConnectionObserver::OnStateChange,
                               // static_cast required to work around old C++ compiler on Android NDK r5c
                               static_cast<int>(IPeerConnectionObserver::kIceState)),
                  NS_DISPATCH_NORMAL);
  }
#endif
}

void
PeerConnectionImpl::IceCompleted(NrIceCtx *aCtx)
{
  RUN_ON_THREAD(mThread,
                WrapRunnable(this,
                             &PeerConnectionImpl::IceCompleted_m,
                             aCtx),
                NS_DISPATCH_SYNC);
}

void
PeerConnectionImpl::IceCompleted_m(NrIceCtx *aCtx)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aCtx);

  CSFLogDebugS(logTag, __FUNCTION__ << ": ctx: " << static_cast<void*>(aCtx));

  mIceState = kIceConnected;

#ifdef MOZILLA_INTERNAL_API
  if (mPCObserver) {
    RUN_ON_THREAD(mThread,
                  WrapRunnable(mPCObserver,
                               &IPeerConnectionObserver::OnStateChange,
                               // static_cast required to work around old C++ compiler on Android NDK r5c
			       static_cast<int>(IPeerConnectionObserver::kIceState)),
                  NS_DISPATCH_NORMAL);
  }
#endif
}

void
PeerConnectionImpl::IceStreamReady(NrIceMediaStream *aStream)
{
  PC_AUTO_ENTER_API_CALL_NO_CHECK();
  MOZ_ASSERT(aStream);

  CSFLogDebugS(logTag, __FUNCTION__ << ": "  << aStream->name().c_str());
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
