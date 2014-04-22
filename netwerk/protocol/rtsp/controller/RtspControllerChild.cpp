/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspControllerChild.h"
#include "RtspMetaData.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/net/NeckoChild.h"
#include "nsITabChild.h"
#include "nsILoadContext.h"
#include "nsNetUtil.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsStringStream.h"
#include "prlog.h"

PRLogModuleInfo* gRtspChildLog = nullptr;
#undef LOG
#define LOG(args) PR_LOG(gRtspChildLog, PR_LOG_DEBUG, args)

const uint32_t kRtspTotalTracks = 2;
using namespace mozilla::ipc;

namespace mozilla {
namespace net {

NS_IMPL_ADDREF(RtspControllerChild)

NS_IMETHODIMP_(nsrefcnt) RtspControllerChild::Release()
{
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  // Enable this to find non-threadsafe destructors:
  // NS_ASSERT_OWNINGTHREAD(RtspControllerChild);
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "RtspControllerChild");

  if (mRefCnt == 1 && mIPCOpen) {
    Send__delete__(this);
    return mRefCnt;
  }

  if (mRefCnt == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
    return 0;
  }
  return mRefCnt;
}

NS_INTERFACE_MAP_BEGIN(RtspControllerChild)
  NS_INTERFACE_MAP_ENTRY(nsIStreamingProtocolController)
  NS_INTERFACE_MAP_ENTRY(nsIStreamingProtocolListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIStreamingProtocolController)
NS_INTERFACE_MAP_END

//-----------------------------------------------------------------------------
// RtspControllerChild methods
//-----------------------------------------------------------------------------
RtspControllerChild::RtspControllerChild(nsIChannel *channel)
  : mIPCOpen(false)
  , mIPCAllowed(false)
  , mChannel(channel)
  , mTotalTracks(0)
  , mSuspendCount(0)
{
#if defined(PR_LOGGING)
  if (!gRtspChildLog)
    gRtspChildLog = PR_NewLogModule("nsRtspChild");
#endif
  AddIPDLReference();
  gNeckoChild->SendPRtspControllerConstructor(this);
}

RtspControllerChild::~RtspControllerChild()
{
  LOG(("RtspControllerChild::~RtspControllerChild()"));
}

void
RtspControllerChild::ReleaseChannel()
{
  static_cast<RtspChannelChild*>(mChannel.get())->ReleaseController();
}

bool
RtspControllerChild::OKToSendIPC()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (mIPCOpen == false) {
    return false;
  }
  return mIPCAllowed;
}

void
RtspControllerChild::AllowIPC()
{
  MOZ_ASSERT(NS_IsMainThread());
  mIPCAllowed = true;
}

void
RtspControllerChild::DisallowIPC()
{
  MOZ_ASSERT(NS_IsMainThread());
  mIPCAllowed = false;
}

//-----------------------------------------------------------------------------
// RtspControllerChild::PRtspControllerChild
//-----------------------------------------------------------------------------
bool
RtspControllerChild::RecvOnMediaDataAvailable(
                       const uint8_t& index,
                       const nsCString& data,
                       const uint32_t& length,
                       const uint32_t& offset,
                       const InfallibleTArray<RtspMetadataParam>& metaArray)
{
  nsRefPtr<RtspMetaData> meta = new RtspMetaData();
  nsresult rv = meta->DeserializeRtspMetaData(metaArray);
  NS_ENSURE_SUCCESS(rv, true);

  if (mListener) {
    mListener->OnMediaDataAvailable(index, data, length, offset, meta.get());
  }
  return true;
}

void
RtspControllerChild::AddMetaData(
                       already_AddRefed<nsIStreamingProtocolMetaData>&& meta)
{
  nsCOMPtr<nsIStreamingProtocolMetaData> data = meta;
  mMetaArray.AppendElement(data);
}

int
RtspControllerChild::GetMetaDataLength()
{
  return mMetaArray.Length();
}

bool
RtspControllerChild::RecvOnConnected(
                       const uint8_t& index,
                       const InfallibleTArray<RtspMetadataParam>& metaArray)
{
  // Deserialize meta data.
  nsRefPtr<RtspMetaData> meta = new RtspMetaData();
  nsresult rv = meta->DeserializeRtspMetaData(metaArray);
  NS_ENSURE_SUCCESS(rv, true);
  meta->GetTotalTracks(&mTotalTracks);
  if (mTotalTracks <= 0) {
    LOG(("RtspControllerChild::RecvOnConnected invalid tracks %d", mTotalTracks));
    // Set the default value.
    mTotalTracks = kRtspTotalTracks;
  }
  AddMetaData(meta.forget().downcast<nsIStreamingProtocolMetaData>());

  // Notify the listener when meta data of tracks are available.
  if ((static_cast<uint32_t>(index) + 1) == mTotalTracks) {
    // The controller provide |GetTrackMetaData| method for his client.
    if (mListener) {
      mListener->OnConnected(index, nullptr);
    }
  }
  return true;
}

bool
RtspControllerChild::RecvOnDisconnected(
                       const uint8_t& index,
                       const nsresult& reason)
{
  DisallowIPC();
  LOG(("RtspControllerChild::RecvOnDisconnected for track %d reason = 0x%x", index, reason));
  if (mListener) {
    mListener->OnDisconnected(index, reason);
  }
  ReleaseChannel();
  return true;
}

bool
RtspControllerChild::RecvAsyncOpenFailed(const nsresult& reason)
{
  DisallowIPC();
  LOG(("RtspControllerChild::RecvAsyncOpenFailed reason = 0x%x", reason));
  if (mListener) {
    mListener->OnDisconnected(0, NS_ERROR_CONNECTION_REFUSED);
  }
  ReleaseChannel();
  return true;
}

void
RtspControllerChild::AddIPDLReference()
{
  NS_ABORT_IF_FALSE(!mIPCOpen,
                    "Attempt to retain more than one IPDL reference");
  mIPCOpen = true;
  AllowIPC();
  AddRef();
}

void
RtspControllerChild::ReleaseIPDLReference()
{
  NS_ABORT_IF_FALSE(mIPCOpen, "Attempt to release nonexistent IPDL reference");
  mIPCOpen = false;
  DisallowIPC();
  Release();
}

NS_IMETHODIMP
RtspControllerChild::GetTrackMetaData(
                       uint8_t index,
                       nsIStreamingProtocolMetaData **result)
{
  if (GetMetaDataLength() <= 0 || index >= GetMetaDataLength()) {
    LOG(("RtspControllerChild:: meta data is not available"));
    return NS_ERROR_NOT_INITIALIZED;
  }
  LOG(("RtspControllerChild::GetTrackMetaData() %d", index));
  NS_IF_ADDREF(*result = mMetaArray[index]);
  return NS_OK;
}

enum IPCEvent
{
  SendNoneEvent = 0,
  SendPlayEvent,
  SendPauseEvent,
  SendSeekEvent,
  SendResumeEvent,
  SendSuspendEvent,
  SendStopEvent
};

class SendIPCEvent : public nsRunnable
{
public:
  SendIPCEvent(RtspControllerChild *aController, IPCEvent aEvent)
    : mController(aController)
    , mEvent(aEvent)
    , mSeekTime(0)
  {
  }

  SendIPCEvent(RtspControllerChild *aController,
               IPCEvent aEvent,
               uint64_t aSeekTime)
    : mController(aController)
    , mEvent(aEvent)
    , mSeekTime(aSeekTime)
  {
  }

  NS_IMETHOD Run()
  {
    MOZ_ASSERT(NS_IsMainThread());
    if (mController->OKToSendIPC() == false) {
      // Don't send any more IPC events; no guarantee that parent objects are
      // still alive.
      return NS_ERROR_FAILURE;
    }
    bool rv = true;

    if (mEvent == SendPlayEvent) {
      rv = mController->SendPlay();
    } else if (mEvent == SendPauseEvent) {
      rv = mController->SendPause();
    } else if (mEvent == SendSeekEvent) {
      rv = mController->SendSeek(mSeekTime);
    } else if (mEvent == SendResumeEvent) {
      rv = mController->SendResume();
    } else if (mEvent == SendSuspendEvent) {
      rv = mController->SendSuspend();
    } else if (mEvent == SendStopEvent) {
      rv = mController->SendStop();
    } else {
      LOG(("RtspControllerChild::SendIPCEvent"));
    }
    if (!rv) {
      return NS_ERROR_FAILURE;
    }
    return NS_OK;
  }
private:
  nsRefPtr<RtspControllerChild> mController;
  IPCEvent mEvent;
  uint64_t mSeekTime;
};

//-----------------------------------------------------------------------------
// RtspControllerChild::nsIStreamingProtocolController
//-----------------------------------------------------------------------------
NS_IMETHODIMP
RtspControllerChild::Play(void)
{
  LOG(("RtspControllerChild::Play()"));

  if (NS_IsMainThread()) {
    if (!OKToSendIPC() || !SendPlay()) {
      return NS_ERROR_FAILURE;
    }
  } else {
    nsresult rv = NS_DispatchToMainThread(
                    new SendIPCEvent(this, SendPlayEvent));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
RtspControllerChild::Pause(void)
{
  LOG(("RtspControllerChild::Pause()"));

  if (NS_IsMainThread()) {
    if (!OKToSendIPC() || !SendPause()) {
      return NS_ERROR_FAILURE;
    }
  } else {
    nsresult rv = NS_DispatchToMainThread(
                    new SendIPCEvent(this, SendPauseEvent));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
RtspControllerChild::Resume(void)
{
  LOG(("RtspControllerChild::Resume()"));
  NS_ENSURE_TRUE(mSuspendCount > 0, NS_ERROR_UNEXPECTED);

  if (!--mSuspendCount) {
    if (NS_IsMainThread()) {
      if (!OKToSendIPC() || !SendResume()) {
        return NS_ERROR_FAILURE;
      }
    } else {
      nsresult rv = NS_DispatchToMainThread(
                      new SendIPCEvent(this, SendResumeEvent));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
RtspControllerChild::Suspend(void)
{
  LOG(("RtspControllerChild::Suspend()"));

  if (!mSuspendCount++) {
    if (NS_IsMainThread()) {
      if (!OKToSendIPC() || !SendSuspend()) {
        return NS_ERROR_FAILURE;
      }
    } else {
      nsresult rv = NS_DispatchToMainThread(
                      new SendIPCEvent(this, SendSuspendEvent));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
RtspControllerChild::Seek(uint64_t seekTimeUs)
{
  LOG(("RtspControllerChild::Seek() %llu", seekTimeUs));

  if (NS_IsMainThread()) {
    if (!OKToSendIPC() || !SendSeek(seekTimeUs)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    nsresult rv = NS_DispatchToMainThread(
                    new SendIPCEvent(this, SendSeekEvent, seekTimeUs));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
RtspControllerChild::Stop()
{
  LOG(("RtspControllerChild::Stop()"));

  if (NS_IsMainThread()) {
    if (!OKToSendIPC() || !SendStop()) {
      return NS_ERROR_FAILURE;
    }
    DisallowIPC();
  } else {
    nsresult rv = NS_DispatchToMainThread(
                    new SendIPCEvent(this, SendStopEvent));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

NS_IMETHODIMP
RtspControllerChild::GetTotalTracks(uint8_t *aTracks)
{
  NS_ENSURE_ARG_POINTER(aTracks);
  *aTracks = kRtspTotalTracks;
  if (mTotalTracks) {
    *aTracks = mTotalTracks;
  }
  LOG(("RtspControllerChild::GetTracks() %d", *aTracks));
  return NS_OK;
}

//-----------------------------------------------------------------------------
// RtspControllerChild::nsIStreamingProtocolListener
//-----------------------------------------------------------------------------
NS_IMETHODIMP
RtspControllerChild::OnMediaDataAvailable(uint8_t index,
                                          const nsACString & data,
                                          uint32_t length,
                                          uint32_t offset,
                                          nsIStreamingProtocolMetaData *meta)
{
  LOG(("RtspControllerChild::OnMediaDataAvailable()"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RtspControllerChild::OnConnected(uint8_t index,
                                 nsIStreamingProtocolMetaData *meta)

{
  LOG(("RtspControllerChild::OnConnected()"));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
RtspControllerChild::OnDisconnected(uint8_t index,
                                    nsresult reason)
{
  LOG(("RtspControllerChild::OnDisconnected() reason = 0x%x", reason));
  return NS_ERROR_NOT_IMPLEMENTED;
}

//-----------------------------------------------------------------------------
// RtspControllerChild::nsIStreamingProtocoController
//-----------------------------------------------------------------------------
NS_IMETHODIMP
RtspControllerChild::Init(nsIURI *aURI)
{
  nsresult rv;

  if (!aURI) {
    LOG(("RtspControllerChild::Init() - invalid URI"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsAutoCString host;
  int32_t port = -1;

  rv = aURI->GetAsciiHost(host);
  if (NS_FAILED(rv)) return rv;

  // Reject the URL if it doesn't specify a host
  if (host.IsEmpty())
    return NS_ERROR_MALFORMED_URI;

  rv = aURI->GetPort(&port);
  if (NS_FAILED(rv)) return rv;

  rv = aURI->GetAsciiSpec(mSpec);
  if (NS_FAILED(rv)) return rv;

  if (!strncmp(mSpec.get(), "rtsp:", 5) == 0)
    return NS_ERROR_UNEXPECTED;

  mURI = aURI;

  return NS_OK;
}

NS_IMETHODIMP
RtspControllerChild::AsyncOpen(nsIStreamingProtocolListener *aListener)
{
  LOG(("RtspControllerChild::AsyncOpen()"));
  if (!aListener) {
    LOG(("RtspControllerChild::AsyncOpen() - invalid listener"));
    return NS_ERROR_NOT_INITIALIZED;
  }
  mListener = aListener;

  if (!mChannel) {
    LOG(("RtspControllerChild::AsyncOpen() - invalid URI"));
    return NS_ERROR_NOT_INITIALIZED;
  }

  nsCOMPtr<nsIURI> uri;
  URIParams uriParams;
  mChannel->GetURI(getter_AddRefs(uri));
  if (!uri) {
    LOG(("RtspControllerChild::AsyncOpen() - invalid URI"));
    return NS_ERROR_NOT_INITIALIZED;
  }
  SerializeURI(uri, uriParams);

  if (!OKToSendIPC() || !SendAsyncOpen(uriParams)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

} // namespace net
} // namespace mozilla
