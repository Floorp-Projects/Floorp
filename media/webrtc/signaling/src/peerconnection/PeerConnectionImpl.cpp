/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>
#include <iostream>


#include "vcm.h"
#include "CSFLog.h"
#include "CSFLogStream.h"
#include "ccapi_call_info.h"
#include "CC_SIPCCCallInfo.h"
#include "ccapi_device_info.h"
#include "CC_SIPCCDeviceInfo.h"

#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"

#include "nsNetCID.h"
#include "nsIServiceManager.h"
#include "nsServiceManagerUtils.h"
#include "nsISocketTransportService.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"

#include "runnable_utils.h"
#include "PeerConnectionCtx.h"
#include "PeerConnectionImpl.h"

#include "nsPIDOMWindow.h"
#include "nsDOMDataChannel.h"

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
static const mozilla::TrackID TRACK_AUDIO = 0;
static const mozilla::TrackID TRACK_VIDEO = 1;
static const int DTLS_FINGERPRINT_LENGTH = 64;
static const int MEDIA_STREAM_MUTE = 0x80;

namespace sipcc {

typedef enum {
  PC_OBSERVER_CALLBACK,
  PC_OBSERVER_CONNECTION,
  PC_OBSERVER_CLOSEDCONNECTION,
  PC_OBSERVER_DATACHANNEL,
  PC_OBSERVER_ICE,
  PC_OBSERVER_READYSTATE
} PeerConnectionObserverType;

// TODO: Refactor this.
class PeerConnectionObserverDispatch : public nsRunnable {

public:
  PeerConnectionObserverDispatch(CSF::CC_CallInfoPtr aInfo,
                                 nsRefPtr<PeerConnectionImpl> aPC,
                                 IPeerConnectionObserver* aObserver) :
    mType(PC_OBSERVER_CALLBACK), mInfo(aInfo), mChannel(nullptr), mPC(aPC), mObserver(aObserver) {}

  PeerConnectionObserverDispatch(PeerConnectionObserverType aType,
                                 nsRefPtr<nsIDOMDataChannel> aChannel,
                                 nsRefPtr<PeerConnectionImpl> aPC,
                                 IPeerConnectionObserver* aObserver) :
    mType(aType), mInfo(nullptr), mChannel(aChannel), mPC(aPC), mObserver(aObserver) {}

  PeerConnectionObserverDispatch(PeerConnectionObserverType aType,
                                 nsRefPtr<PeerConnectionImpl> aPC,
                                 IPeerConnectionObserver* aObserver) :
    mType(aType), mInfo(nullptr), mPC(aPC), mObserver(aObserver) {}

  ~PeerConnectionObserverDispatch(){}

  NS_IMETHOD Run()
  {
    switch (mType) {
      case PC_OBSERVER_CALLBACK:
        {
          StatusCode code;
          std::string s_sdpstr;
          MediaStreamTable *streams = NULL;

          cc_call_state_t state = mInfo->getCallState();
          std::string statestr = mInfo->callStateToString(state);

          nsDOMMediaStream* stream;
          uint32_t hint;

          switch (state) {
            case CREATEOFFER:
              s_sdpstr = mInfo->getSDP();
              mObserver->OnCreateOfferSuccess(s_sdpstr.c_str());
              break;

            case CREATEANSWER:
              s_sdpstr = mInfo->getSDP();
              mObserver->OnCreateAnswerSuccess(s_sdpstr.c_str());
              break;

            case CREATEOFFERERROR:
              code = (StatusCode)mInfo->getStatusCode();
              mObserver->OnCreateOfferError(code);
              break;

            case CREATEANSWERERROR:
              code = (StatusCode)mInfo->getStatusCode();
              mObserver->OnCreateAnswerError(code);
              break;

            case SETLOCALDESC:
              code = (StatusCode)mInfo->getStatusCode();
              mObserver->OnSetLocalDescriptionSuccess(code);
              break;

            case SETREMOTEDESC:
              code = (StatusCode)mInfo->getStatusCode();
              mObserver->OnSetRemoteDescriptionSuccess(code);
              break;

            case SETLOCALDESCERROR:
              code = (StatusCode)mInfo->getStatusCode();
              mObserver->OnSetLocalDescriptionError(code);
              break;

            case SETREMOTEDESCERROR:
              code = (StatusCode)mInfo->getStatusCode();
              mObserver->OnSetRemoteDescriptionError(code);
              break;

            case REMOTESTREAMADD:
            {
              streams = mInfo->getMediaStreams();
              nsRefPtr<RemoteSourceStreamInfo> remoteStream = mPC->GetRemoteStream(streams->media_stream_id);

              MOZ_ASSERT(remoteStream);
              if (!remoteStream)
              {
                CSFLogErrorS(logTag, __FUNCTION__ << " GetRemoteStream returned NULL");
              }
              else
              {
                stream = remoteStream->GetMediaStream();
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
              CSFLogDebugS(logTag, ": **** UNHANDLED CALL STATE : " << statestr);
              break;
          }
          break;
        }
      case PC_OBSERVER_CONNECTION:
        CSFLogDebugS(logTag, __FUNCTION__ << ": Delivering PeerConnection onconnection");
        mObserver->NotifyConnection();
        break;
      case PC_OBSERVER_CLOSEDCONNECTION:
        CSFLogDebugS(logTag, __FUNCTION__ << ": Delivering PeerConnection onclosedconnection");
        mObserver->NotifyClosedConnection();
        break;
      case PC_OBSERVER_DATACHANNEL:
        CSFLogDebugS(logTag, __FUNCTION__ << ": Delivering PeerConnection ondatachannel");
        mObserver->NotifyDataChannel(mChannel);
#ifdef MOZILLA_INTERNAL_API
        NS_DataChannelAppReady(mChannel);
#endif
        break;
      case PC_OBSERVER_ICE:
        CSFLogDebugS(logTag, __FUNCTION__ << ": Delivering PeerConnection ICE callback ");
        mObserver->OnStateChange(IPeerConnectionObserver::kIceState);
        break;
      case PC_OBSERVER_READYSTATE:
        CSFLogDebugS(logTag, __FUNCTION__ << ": Delivering PeerConnection Ready State callback ");
        mObserver->OnStateChange(IPeerConnectionObserver::kReadyState);
    }
    return NS_OK;
  }

private:
  PeerConnectionObserverType mType;
  CSF::CC_CallInfoPtr mInfo;
  nsRefPtr<nsIDOMDataChannel> mChannel;
  nsRefPtr<PeerConnectionImpl> mPC;
  nsCOMPtr<IPeerConnectionObserver> mObserver;
};

/* We get this callback in order to find out which tracks are audio and which
 * are video. We should get this callback right away for existing streams after
 * we add this class as a listener.
 */
void
LocalSourceStreamInfo::NotifyQueuedTrackChanges(
  mozilla::MediaStreamGraph* aGraph,
  mozilla::TrackID aID,
  mozilla::TrackRate aTrackRate,
  mozilla::TrackTicks aTrackOffset,
  uint32_t aTrackEvents,
  const mozilla::MediaSegment& aQueuedMedia)
{
  /* TODO: use this callback to keep track of changes to the MediaStream */
}

/* If the ExpectAudio hint is on we will add a track at the default first
 * audio track ID (0)
 * FIX - Do we need to iterate over the tracks instead of taking these hints?
 */
void
LocalSourceStreamInfo::ExpectAudio(const mozilla::TrackID aID)
{
  mAudioTracks.AppendElement(aID);
}

// If the ExpectVideo hint is on we will add a track at the default first
// video track ID (1).
void
LocalSourceStreamInfo::ExpectVideo(const mozilla::TrackID aID)
{
  mVideoTracks.AppendElement(aID);
}

unsigned
LocalSourceStreamInfo::AudioTrackCount()
{
  return mAudioTracks.Length();
}

unsigned
LocalSourceStreamInfo::VideoTrackCount()
{
  return mVideoTracks.Length();
}

PeerConnectionImpl* PeerConnectionImpl::CreatePeerConnection()
{
  PeerConnectionImpl *pc = new PeerConnectionImpl();

  CSFLogDebugS(logTag, "Created PeerConnection: " << static_cast<void*>(pc));

  return pc;
}

std::map<const std::string, PeerConnectionImpl *>
  PeerConnectionImpl::peerconnections;

NS_IMPL_THREADSAFE_ISUPPORTS1(PeerConnectionImpl, IPeerConnection)

PeerConnectionImpl::PeerConnectionImpl()
: mRole(kRoleUnknown)
  , mCall(NULL)
  , mReadyState(kNew)
  , mPCObserver(NULL)
  , mWindow(NULL)
  , mFingerprint("TempFingerprint")
  , mLocalSourceStreamsLock(PR_NewLock())
  , mIceCtx(NULL)
  , mIceState(kIceGathering)
  , mIdentity(NULL)
  , mSTSThread(NULL)
 {}

PeerConnectionImpl::~PeerConnectionImpl()
{
  peerconnections.erase(mHandle);
  Close();
  PR_DestroyLock(mLocalSourceStreamsLock);

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

  nsRefPtr<nsDOMMediaStream> stream = nsDOMMediaStream::CreateInputStream(aHint);
  NS_ADDREF(*aRetval = stream);

  CSFLogDebugS(logTag, "PeerConnection " << static_cast<void*>(this)
    << ": Created media stream " << static_cast<void*>(stream)
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

  nsIDOMMediaStream* stream;

  nsresult res;
  if (!mThread || NS_IsMainThread()) {
    res = MakeMediaStream(aHint, &stream);
  } else {
    mThread->Dispatch(WrapRunnableRet(
      this, &PeerConnectionImpl::MakeMediaStream, aHint, &stream, &res
    ), NS_DISPATCH_SYNC);
  }

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

  mThread->Dispatch(WrapRunnableRet(
    this, &PeerConnectionImpl::MakeRemoteSource, comstream, aInfo, &res
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
  MOZ_ASSERT(aObserver);
  mPCObserver = aObserver;

#ifdef MOZILLA_INTERNAL_API
  // Currently no standalone unit tests for DataChannel,
  // which is the user of mWindow
  MOZ_ASSERT(aWindow);
  mWindow = do_QueryInterface(aWindow);
  NS_ENSURE_STATE(mWindow);
#endif

  // The thread parameter can be passed in as NULL
  mThread = aThread;

  PeerConnectionCtx *pcctx = PeerConnectionCtx::GetInstance();
  MOZ_ASSERT(pcctx);

  mCall = pcctx->createCall();
  if(!mCall.get()) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": Couldn't Create Call Object");
    return NS_ERROR_FAILURE;
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


  // TODO(ekr@rtfm.com): need some way to set not offerer later
  // Looks like a bug in the NrIceCtx API.
  mIceCtx = NrIceCtx::Create("PC:" + mHandle, true);
  if(!mIceCtx) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": Failed to create Ice Context");
    return NS_ERROR_FAILURE;
  }

  mIceCtx->SignalGatheringCompleted.connect(this, &PeerConnectionImpl::IceGatheringCompleted);
  mIceCtx->SignalCompleted.connect(this, &PeerConnectionImpl::IceCompleted);

  // Create three streams to start with.
  // One each for audio, video and DataChannel
  // TODO: this will be re-visited
  RefPtr<NrIceMediaStream> audioStream = mIceCtx->CreateStream("stream1", 2);
  RefPtr<NrIceMediaStream> videoStream = mIceCtx->CreateStream("stream2", 2);
  RefPtr<NrIceMediaStream> dcStream = mIceCtx->CreateStream("stream3", 2);

  if (!audioStream) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": audio stream is NULL");
    return NS_ERROR_FAILURE;
  } else {
    mIceStreams.push_back(audioStream);
  }

  if (!videoStream) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": video stream is NULL");
    return NS_ERROR_FAILURE;
  } else {
    mIceStreams.push_back(videoStream);
  }

  if (!dcStream) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": datachannel stream is NULL");
    return NS_ERROR_FAILURE;
  } else {
    mIceStreams.push_back(dcStream);
  }

  for (std::size_t i=0; i<mIceStreams.size(); i++) {
    mIceStreams[i]->SignalReady.connect(this, &PeerConnectionImpl::IceStreamReady);
  }

  // Start gathering
  nsresult res;
  mIceCtx->thread()->Dispatch(WrapRunnableRet(
    mIceCtx, &NrIceCtx::StartGathering, &res), NS_DISPATCH_SYNC
  );

  if (NS_FAILED(res)) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": StartGathering failed: " << res);
    return res;
  }

  // Store under mHandle
  mCall->setPeerConnection(mHandle);
  peerconnections[mHandle] = this;

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
    CSFLogErrorS(logTag, __FUNCTION__ << ": ComputeFingerprint failed: " << res);
    return res;
  }

  mFingerprint = "sha-1 " + mIdentity->FormatFingerprint(fingerprint,
                                                         fingerprint_length);

  // Find the STS thread
  mSTSThread = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &res);

  if (NS_FAILED(res)) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": do_GetService failed: " << res);
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
    mThread->Dispatch(WrapRunnableRet(
      this, &PeerConnectionImpl::MakeMediaStream, aHint, aRetval, &res
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
#ifdef MOZILLA_INTERNAL_API
  mDataConnection = new mozilla::DataChannelConnection(this);
  NS_ENSURE_TRUE(mDataConnection,NS_ERROR_FAILURE);
  if (!mDataConnection->Init(aLocalport, aNumstreams, true)) {
    CSFLogError(logTag,"%s DataConnection Init Failed",__FUNCTION__);
    return NS_ERROR_FAILURE;
  }
  // XXX Fix! Get the correct flow for DataChannel. Also error handling.
  nsRefPtr<TransportFlow> flow = GetTransportFlow(1,false).get();
  CSFLogDebugS(logTag, "Transportflow[1] = " << flow.get());
  if (!mDataConnection->ConnectDTLS(flow, aLocalport, aRemoteport)) {
    return NS_ERROR_FAILURE;
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
  MOZ_ASSERT(aRetval);

#ifdef MOZILLA_INTERNAL_API
  mozilla::DataChannel* dataChannel;
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

  return NS_NewDOMDataChannel(dataChannel, mWindow, aRetval);
#else
  return NS_OK;
#endif
}

void
PeerConnectionImpl::NotifyConnection()
{
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebugS(logTag, __FUNCTION__);

#ifdef MOZILLA_INTERNAL_API
  if (mPCObserver) {
    PeerConnectionObserverDispatch* runnable =
      new PeerConnectionObserverDispatch(PC_OBSERVER_CONNECTION, nullptr,
                                         this, mPCObserver);
    if (mThread) {
      mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
      return;
    }
    runnable->Run();
  }
#endif
}

void
PeerConnectionImpl::NotifyClosedConnection()
{
  MOZ_ASSERT(NS_IsMainThread());

  CSFLogDebugS(logTag, __FUNCTION__);

#ifdef MOZILLA_INTERNAL_API
  if (mPCObserver) {
    PeerConnectionObserverDispatch* runnable =
      new PeerConnectionObserverDispatch(PC_OBSERVER_CLOSEDCONNECTION, nullptr,
                                         this, mPCObserver);
    if (mThread) {
      mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
      return;
    }
    runnable->Run();
  }
#endif
}

void
PeerConnectionImpl::NotifyDataChannel(mozilla::DataChannel *aChannel)
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aChannel);

  CSFLogDebugS(logTag, __FUNCTION__ << ": channel: " << static_cast<void*>(aChannel));

#ifdef MOZILLA_INTERNAL_API
  nsCOMPtr<nsIDOMDataChannel> domchannel;
  nsresult rv = NS_NewDOMDataChannel(aChannel, mWindow,
                                     getter_AddRefs(domchannel));
  NS_ENSURE_SUCCESS(rv,);
  if (mPCObserver) {
    PeerConnectionObserverDispatch* runnable =
      new PeerConnectionObserverDispatch(PC_OBSERVER_DATACHANNEL, domchannel.get(),
                                         this, mPCObserver);
    if (mThread) {
      mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
      return;
    }
    runnable->Run();
  }
#endif
}

/*
 * CC_SDP_DIRECTION_SENDRECV will not be used when Constraints are implemented
 */
NS_IMETHODIMP
PeerConnectionImpl::CreateOffer(const char* aHints) {
  MOZ_ASSERT(aHints);

  CheckIceState();
  mRole = kRoleOfferer;  // TODO(ekr@rtfm.com): Interrogate SIPCC here?
  mCall->createOffer(aHints);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CreateAnswer(const char* aHints, const char* aOffer) {
  MOZ_ASSERT(aHints);
  MOZ_ASSERT(aOffer);

  CheckIceState();
  mRole = kRoleAnswerer;  // TODO(ekr@rtfm.com): Interrogate SIPCC here?
  mCall->createAnswer(aHints, aOffer);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::SetLocalDescription(int32_t aAction, const char* aSDP) {
  MOZ_ASSERT(aSDP);

  CheckIceState();
  mLocalRequestedSDP = aSDP;
  mCall->setLocalDescription((cc_jsep_action_t)aAction, mLocalRequestedSDP);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::SetRemoteDescription(int32_t action, const char* aSDP) {
  MOZ_ASSERT(aSDP);

  CheckIceState();
  mRemoteRequestedSDP = aSDP;
  mCall->setRemoteDescription((cc_jsep_action_t)action, mRemoteRequestedSDP);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::AddStream(nsIDOMMediaStream* aMediaStream)
{
  MOZ_ASSERT(aMediaStream);

  nsDOMMediaStream* stream = static_cast<nsDOMMediaStream*>(aMediaStream);

  CSFLogDebugS(logTag, __FUNCTION__ << ": MediaStream: " << static_cast<void*>(aMediaStream));

  // TODO(ekr@rtfm.com): Remove these asserts?
  // Adding tracks here based on nsDOMMediaStream expectation settings
  uint32_t hints = stream->GetHintContents();

  if (!(hints & (nsDOMMediaStream::HINT_CONTENTS_AUDIO |
        nsDOMMediaStream::HINT_CONTENTS_VIDEO))) {
    CSFLogDebug(logTag, "Empty Stream !!");
    return NS_OK;
  }

  // Now see if we already have a stream of this type, since we only
  // allow one of each.
  // TODO(ekr@rtfm.com): remove this when multiple of each stream
  // is allowed
  PR_Lock(mLocalSourceStreamsLock);
  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    nsRefPtr<LocalSourceStreamInfo> localSourceStream = mLocalSourceStreams[u];

    if (localSourceStream->GetMediaStream()->GetHintContents() & hints) {
      CSFLogError(logTag, "Only one stream of any given type allowed");
      PR_Unlock(mLocalSourceStreamsLock);
      return NS_ERROR_FAILURE;
    }
  }

  // OK, we're good to add
  nsRefPtr<LocalSourceStreamInfo> localSourceStream =
    new LocalSourceStreamInfo(stream);
  cc_media_track_id_t media_stream_id = mLocalSourceStreams.Length();

  // TODO(ekr@rtfm.com): these integers should be the track IDs
  if (hints & nsDOMMediaStream::HINT_CONTENTS_AUDIO) {
    localSourceStream->ExpectAudio(TRACK_AUDIO);
    mCall->addStream(media_stream_id, 0, AUDIO);
  }

  if (hints & nsDOMMediaStream::HINT_CONTENTS_VIDEO) {
    localSourceStream->ExpectVideo(TRACK_VIDEO);
    mCall->addStream(media_stream_id, 1, VIDEO);
  }

  // Make it the listener for info from the MediaStream and add it to the list
  mozilla::MediaStream *plainMediaStream = stream->GetStream();

  if (plainMediaStream) {
    plainMediaStream->AddListener(localSourceStream);
  }

  mLocalSourceStreams.AppendElement(localSourceStream);

  PR_Unlock(mLocalSourceStreamsLock);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::RemoveStream(nsIDOMMediaStream* aMediaStream)
{
  MOZ_ASSERT(aMediaStream);

  nsDOMMediaStream* stream = static_cast<nsDOMMediaStream*>(aMediaStream);

  CSFLogDebugS(logTag, __FUNCTION__ << ": MediaStream: " << static_cast<void*>(aMediaStream));

  PR_Lock(mLocalSourceStreamsLock);
  for (uint32_t u = 0; u < mLocalSourceStreams.Length(); u++) {
    nsRefPtr<LocalSourceStreamInfo> localSourceStream = mLocalSourceStreams[u];
    if (localSourceStream->GetMediaStream() == stream) {
      uint32_t hints = stream->GetHintContents();
      if (hints & nsDOMMediaStream::HINT_CONTENTS_AUDIO) {
        // <emannion>  This API will change when we implement multiple streams
        //             It will only need the ID
        mCall->removeStream(u, 0, AUDIO);
      }
      if (hints & nsDOMMediaStream::HINT_CONTENTS_VIDEO) {
        mCall->removeStream(u, 1, VIDEO);
      }
      break;
    }
  }

  PR_Unlock(mLocalSourceStreamsLock);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::AddIceCandidate(const char* aCandidate, const char* aMid, unsigned short aLevel) {
  CheckIceState();
  mCall->addICECandidate(aCandidate, aMid, aLevel);
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::CloseStreams() {
  if (mReadyState != PeerConnectionImpl::kClosed)  {
    ChangeReadyState(PeerConnectionImpl::kClosing);
  }

  mCall->endCall();
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
  MOZ_ASSERT(aState);

  *aState = mReadyState;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetSipccState(uint32_t* aState)
{
  MOZ_ASSERT(aState);

  PeerConnectionCtx* pcctx = PeerConnectionCtx::GetInstance();
  *aState = pcctx ? pcctx->sipcc_state() : kIdle;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::GetIceState(uint32_t* aState)
{
  MOZ_ASSERT(aState);

  *aState = mIceState;
  return NS_OK;
}

NS_IMETHODIMP
PeerConnectionImpl::Close()
{
  if (mCall != NULL)
    mCall->endCall();
#ifdef MOZILLA_INTERNAL_API
  if (mDataConnection != NULL)
    mDataConnection->CloseAll();
#endif

  ShutdownMedia();

  // DataConnection will need to stay alive until all threads/runnables exit

  return NS_OK;
}

 void
 PeerConnectionImpl::ShutdownMedia()
 {
   CSFLogDebugS(logTag, __FUNCTION__ << " Disconnecting media streams from PC");
   // Disconnect ourselves from the DOM Media Streams
   RUN_ON_THREAD(mThread, WrapRunnable(this,
       &PeerConnectionImpl::DisconnectMediaStreams), NS_DISPATCH_SYNC);

   CSFLogDebugS(logTag, __FUNCTION__ << " Disconnecting transport");
   // Shutdown the transport.
   RUN_ON_THREAD(mSTSThread, WrapRunnable(
       this, &PeerConnectionImpl::ShutdownMediaTransport), NS_DISPATCH_SYNC);

  CSFLogDebugS(logTag, __FUNCTION__ << " Media shut down");
}

void
PeerConnectionImpl::DisconnectMediaStreams()
{
  // TODO(ekr@rtfm.com): Lock
  for (uint32_t i=0; i < mLocalSourceStreams.Length(); ++i) {
    mLocalSourceStreams[i]->Detach();
  }

  for (uint32_t i=0; i < mRemoteSourceStreams.Length(); ++i) {
    mRemoteSourceStreams[i]->Detach();
  }

  mLocalSourceStreams.Clear();
  mRemoteSourceStreams.Clear();
}

void
PeerConnectionImpl::ShutdownMediaTransport()
{
  mTransportFlows.clear();
  mIceStreams.clear();
  mIceCtx = NULL;
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
  mReadyState = aReadyState;
  // FIXME: Dispatch on main thread.
  if (mPCObserver) {
    PeerConnectionObserverDispatch* runnable =
        new PeerConnectionObserverDispatch(PC_OBSERVER_READYSTATE, this, mPCObserver);

    if (mThread) {
      mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
      return;
    }
    runnable->Run();
  }
}

PeerConnectionWrapper *PeerConnectionImpl::AcquireInstance(const std::string& aHandle)
{
  if (peerconnections.find(aHandle) == peerconnections.end()) {
    return NULL;
  }

  PeerConnectionImpl *impl = peerconnections[aHandle];
  impl->AddRef();

  return new PeerConnectionWrapper(impl);
}

void
PeerConnectionImpl::ReleaseInstance()
{
  Release();
}

const std::string&
PeerConnectionImpl::GetHandle()
{
  return mHandle;
}

void
PeerConnectionImpl::IceGatheringCompleted(NrIceCtx *aCtx)
{
  MOZ_ASSERT(aCtx);

  CSFLogDebugS(logTag, __FUNCTION__ << ": ctx: " << static_cast<void*>(aCtx));

  mIceState = kIceWaiting;

#ifdef MOZILLA_INTERNAL_API
  if (mPCObserver) {
    PeerConnectionObserverDispatch* runnable =
      new PeerConnectionObserverDispatch(PC_OBSERVER_ICE, this, mPCObserver);
    if (mThread) {
      mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
      return;
    }
    runnable->Run();
  }
#endif
}

void
PeerConnectionImpl::IceCompleted(NrIceCtx *aCtx)
{
  MOZ_ASSERT(aCtx);

  CSFLogDebugS(logTag, __FUNCTION__ << ": ctx: " << static_cast<void*>(aCtx));

  mIceState = kIceConnected;

#ifdef MOZILLA_INTERNAL_API
  if (mPCObserver) {
    PeerConnectionObserverDispatch* runnable =
      new PeerConnectionObserverDispatch(PC_OBSERVER_ICE, this, mPCObserver);
    if (mThread) {
      mThread->Dispatch(runnable, NS_DISPATCH_NORMAL);
      return;
    }
    runnable->Run();
  }
#endif
}

void
PeerConnectionImpl::IceStreamReady(NrIceMediaStream *aStream)
{
  MOZ_ASSERT(aStream);

  CSFLogDebugS(logTag, __FUNCTION__ << ": "  << aStream->name().c_str());
}

nsRefPtr<LocalSourceStreamInfo>
PeerConnectionImpl::GetLocalStream(int aIndex)
{
  if(aIndex < 0 || aIndex >= (int) mLocalSourceStreams.Length()) {
    return NULL;
  }

  MOZ_ASSERT(mLocalSourceStreams[aIndex]);
  return mLocalSourceStreams[aIndex];
}

nsRefPtr<RemoteSourceStreamInfo>
PeerConnectionImpl::GetRemoteStream(int aIndex)
{
  if(aIndex < 0 || aIndex >= (int) mRemoteSourceStreams.Length()) {
    return NULL;
  }

  MOZ_ASSERT(mRemoteSourceStreams[aIndex]);
  return mRemoteSourceStreams[aIndex];
}

nsresult
PeerConnectionImpl::AddRemoteStream(nsRefPtr<RemoteSourceStreamInfo> aInfo,
  int *aIndex)
{
  MOZ_ASSERT(aIndex);

  *aIndex = mRemoteSourceStreams.Length();

  mRemoteSourceStreams.AppendElement(aInfo);

  return NS_OK;
}

void
LocalSourceStreamInfo::StorePipeline(int aTrack,
  mozilla::RefPtr<mozilla::MediaPipeline> aPipeline)
{
  MOZ_ASSERT(mPipelines.find(aTrack) == mPipelines.end());
  if (mPipelines.find(aTrack) != mPipelines.end()) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": Storing duplicate track");
    return;
  }
  //TODO: Revisit once we start supporting multiple streams or multiple tracks
  // of same type
  mPipelines[aTrack] = aPipeline;
}

void
RemoteSourceStreamInfo::StorePipeline(int aTrack,
  mozilla::RefPtr<mozilla::MediaPipeline> aPipeline)
{
  MOZ_ASSERT(mPipelines.find(aTrack) == mPipelines.end());
  if (mPipelines.find(aTrack) != mPipelines.end()) {
    CSFLogErrorS(logTag, __FUNCTION__ << ": Storing duplicate track");
    return;
  }
  //TODO: Revisit once we start supporting multiple streams or multiple tracks
  // of same type
  mPipelines[aTrack] = aPipeline;
}

}  // end sipcc namespace
