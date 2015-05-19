/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspControllerParent.h"
#include "RtspController.h"
#include "nsIAuthPromptProvider.h"
#include "nsThreadUtils.h"
#include "nsProxyRelease.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "mozilla/unused.h"
#include "nsNetUtil.h"
#include "mozilla/Logging.h"

#include <sys/types.h>

PRLogModuleInfo* gRtspLog;
#undef LOG
#define LOG(args) PR_LOG(gRtspLog, PR_LOG_DEBUG, args)

#define SEND_DISCONNECT_IF_ERROR(rv)                         \
  if (NS_FAILED(rv) && mIPCOpen && mTotalTracks > 0ul) {     \
    for (uint32_t i = 0; i < mTotalTracks; i++) {            \
      unused << SendOnDisconnected(i, rv);                   \
    }                                                        \
  }

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

void
RtspControllerParent::Destroy()
{
  // If we're being destroyed on a non-main thread, we AddRef again and use a
  // proxy to release the RtspControllerParent on the main thread, where the
  // RtspControllerParent is deleted. This ensures we only delete the
  // RtspControllerParent on the main thread.
  if (!NS_IsMainThread()) {
    nsCOMPtr<nsIThread> mainThread = do_GetMainThread();
    NS_ENSURE_TRUE_VOID(mainThread);
    nsRefPtr<RtspControllerParent> doomed(this);
    if (NS_FAILED(NS_ProxyRelease(mainThread,
            static_cast<nsIStreamingProtocolListener*>(doomed), true))) {
      NS_WARNING("Failed to proxy release to main thread!");
    }
  } else {
    delete this;
  }
}

NS_IMPL_ADDREF(RtspControllerParent)
NS_IMPL_RELEASE_WITH_DESTROY(RtspControllerParent, Destroy())
NS_IMPL_QUERY_INTERFACE(RtspControllerParent,
                        nsIStreamingProtocolListener)

RtspControllerParent::RtspControllerParent()
  : mIPCOpen(true)
  , mTotalTracks(0)
{
  if (!gRtspLog)
    gRtspLog = PR_NewLogModule("nsRtsp");
}

RtspControllerParent::~RtspControllerParent()
{
}

void
RtspControllerParent::ActorDestroy(ActorDestroyReason why)
{
  LOG(("RtspControllerParent::ActorDestroy()"));
  mIPCOpen = false;

  NS_ENSURE_TRUE_VOID(mController);
  if (mController) {
    mController->Stop();
    mController = nullptr;
  }
}

bool
RtspControllerParent::RecvAsyncOpen(const URIParams& aURI)
{
  LOG(("RtspControllerParent::RecvAsyncOpen()"));

  mURI = DeserializeURI(aURI);

  mController = new RtspController(nullptr);
  mController->Init(mURI);
  nsresult rv = mController->AsyncOpen(this);
  if (NS_SUCCEEDED(rv)) return true;

  mController = nullptr;
  return SendAsyncOpenFailed(rv);
}

bool
RtspControllerParent::RecvPlay()
{
  LOG(("RtspControllerParent::RecvPlay()"));
  NS_ENSURE_TRUE(mController, true);

  nsresult rv = mController->Play();
  SEND_DISCONNECT_IF_ERROR(rv)
  return true;
}

bool
RtspControllerParent::RecvPause()
{
  LOG(("RtspControllerParent::RecvPause()"));
  NS_ENSURE_TRUE(mController, true);

  nsresult rv = mController->Pause();
  SEND_DISCONNECT_IF_ERROR(rv)
  return true;
}

bool
RtspControllerParent::RecvResume()
{
  LOG(("RtspControllerParent::RecvResume()"));
  NS_ENSURE_TRUE(mController, true);

  nsresult rv = mController->Resume();
  SEND_DISCONNECT_IF_ERROR(rv)
  return true;
}

bool
RtspControllerParent::RecvSuspend()
{
  LOG(("RtspControllerParent::RecvSuspend()"));
  NS_ENSURE_TRUE(mController, true);

  nsresult rv = mController->Suspend();
  SEND_DISCONNECT_IF_ERROR(rv)
  return true;
}

bool
RtspControllerParent::RecvSeek(const uint64_t& offset)
{
  LOG(("RtspControllerParent::RecvSeek()"));
  NS_ENSURE_TRUE(mController, true);

  nsresult rv = mController->Seek(offset);
  SEND_DISCONNECT_IF_ERROR(rv)
  return true;
}

bool
RtspControllerParent::RecvStop()
{
  LOG(("RtspControllerParent::RecvStop()"));
  NS_ENSURE_TRUE(mController, true);

  nsresult rv = mController->Stop();
  NS_ENSURE_SUCCESS(rv, true);
  return true;
}

bool
RtspControllerParent::RecvPlaybackEnded()
{
  LOG(("RtspControllerParent::RecvPlaybackEnded()"));
  NS_ENSURE_TRUE(mController, true);

  nsresult rv = mController->PlaybackEnded();
  SEND_DISCONNECT_IF_ERROR(rv)
  return true;
}

NS_IMETHODIMP
RtspControllerParent::OnMediaDataAvailable(uint8_t index,
                                           const nsACString & data,
                                           uint32_t length,
                                           uint32_t offset,
                                           nsIStreamingProtocolMetaData *meta)
{
  NS_ENSURE_ARG_POINTER(meta);
  uint32_t int32Value;
  uint64_t int64Value;

  nsresult rv = meta->GetTimeStamp(&int64Value);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  LOG(("RtspControllerParent:: OnMediaDataAvailable %d:%d time %lld",
       index, length, int64Value));

  // Serialize meta data.
  nsCString name;
  name.AssignLiteral("TIMESTAMP");
  InfallibleTArray<RtspMetadataParam> metaData;
  metaData.AppendElement(RtspMetadataParam(name, int64Value));

  name.AssignLiteral("FRAMETYPE");
  rv = meta->GetFrameType(&int32Value);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  metaData.AppendElement(RtspMetadataParam(name, int32Value));

  nsCString stream;
  stream.Assign(data);
  if (!mIPCOpen ||
      !SendOnMediaDataAvailable(index, stream, length, offset, metaData)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
RtspControllerParent::OnConnected(uint8_t index,
                                  nsIStreamingProtocolMetaData *meta)
{
  NS_ENSURE_ARG_POINTER(meta);
  uint32_t int32Value;
  uint64_t int64Value;

  LOG(("RtspControllerParent:: OnConnected"));
  // Serialize meta data.
  InfallibleTArray<RtspMetadataParam> metaData;
  nsCString name;
  name.AssignLiteral("TRACKS");
  nsresult rv = meta->GetTotalTracks(&mTotalTracks);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  metaData.AppendElement(RtspMetadataParam(name, mTotalTracks));

  name.AssignLiteral("MIMETYPE");
  nsCString mimeType;
  rv = meta->GetMimeType(mimeType);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  metaData.AppendElement(RtspMetadataParam(name, mimeType));

  name.AssignLiteral("WIDTH");
  rv = meta->GetWidth(&int32Value);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  metaData.AppendElement(RtspMetadataParam(name, int32Value));

  name.AssignLiteral("HEIGHT");
  rv = meta->GetHeight(&int32Value);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  metaData.AppendElement(RtspMetadataParam(name, int32Value));

  name.AssignLiteral("DURATION");
  rv = meta->GetDuration(&int64Value);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  metaData.AppendElement(RtspMetadataParam(name, int64Value));

  name.AssignLiteral("SAMPLERATE");
  rv = meta->GetSampleRate(&int32Value);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  metaData.AppendElement(RtspMetadataParam(name, int32Value));

  name.AssignLiteral("TIMESTAMP");
  rv = meta->GetTimeStamp(&int64Value);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  metaData.AppendElement(RtspMetadataParam(name, int64Value));

  name.AssignLiteral("CHANNELCOUNT");
  rv = meta->GetChannelCount(&int32Value);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  metaData.AppendElement(RtspMetadataParam(name, int32Value));

  nsCString esds;
  rv = meta->GetEsdsData(esds);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  name.AssignLiteral("ESDS");
  metaData.AppendElement(RtspMetadataParam(name, esds));

  nsCString avcc;
  rv = meta->GetAvccData(avcc);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  name.AssignLiteral("AVCC");
  metaData.AppendElement(RtspMetadataParam(name, avcc));

  if (!mIPCOpen || !SendOnConnected(index, metaData)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
RtspControllerParent::OnDisconnected(uint8_t index,
                                     nsresult reason)
{
  LOG(("RtspControllerParent::OnDisconnected() for track %d reason = 0x%x", index, reason));
  if (!mIPCOpen || !SendOnDisconnected(index, reason)) {
    return NS_ERROR_FAILURE;
  }
  if (mController) {
    mController = nullptr;
  }
  return NS_OK;
}

} // namespace net
} // namespace mozilla
