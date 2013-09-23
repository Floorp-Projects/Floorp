/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspControllerParent.h"
#include "RtspController.h"
#include "nsIAuthPromptProvider.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsNetUtil.h"
#include "prlog.h"

#include <sys/types.h>
#include <sys/socket.h>

PRLogModuleInfo* gRtspLog;
#undef LOG
#define LOG(args) PR_LOG(gRtspLog, PR_LOG_DEBUG, args)

using namespace mozilla::ipc;

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS2(RtspControllerParent,
                   nsIInterfaceRequestor,
                   nsIStreamingProtocolListener)

RtspControllerParent::RtspControllerParent()
  : mIPCOpen(true)
{
#if defined(PR_LOGGING)
  if (!gRtspLog)
    gRtspLog = PR_NewLogModule("nsRtsp");
#endif
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
  mController->Stop();
  mController = nullptr;
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
  NS_ENSURE_TRUE(mController, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mController->Play();
  NS_ENSURE_SUCCESS(rv, false);
  return true;
}

bool
RtspControllerParent::RecvPause()
{
  LOG(("RtspControllerParent::RecvPause()"));
  NS_ENSURE_TRUE(mController, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mController->Pause();
  NS_ENSURE_SUCCESS(rv, false);
  return true;
}

bool
RtspControllerParent::RecvResume()
{
  LOG(("RtspControllerParent::RecvResume()"));
  NS_ENSURE_TRUE(mController, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mController->Resume();
  NS_ENSURE_SUCCESS(rv, false);
  return true;
}

bool
RtspControllerParent::RecvSuspend()
{
  LOG(("RtspControllerParent::RecvSuspend()"));
  NS_ENSURE_TRUE(mController, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mController->Suspend();
  NS_ENSURE_SUCCESS(rv, false);
  return true;
}

bool
RtspControllerParent::RecvSeek(const uint64_t& offset)
{
  LOG(("RtspControllerParent::RecvSeek()"));
  NS_ENSURE_TRUE(mController, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mController->Seek(offset);
  NS_ENSURE_SUCCESS(rv, false);
  return true;
}

bool
RtspControllerParent::RecvStop()
{
  LOG(("RtspControllerParent::RecvStop()"));
  NS_ENSURE_TRUE(mController, NS_ERROR_NOT_INITIALIZED);

  nsresult rv = mController->Stop();
  NS_ENSURE_SUCCESS(rv, false);
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

  LOG(("RtspControllerParent:: OnMediaDataAvailable %d:%d time %lld",
       index, length, int64Value));

  // Serialize meta data.
  nsCString name;
  name.AssignLiteral("TIMESTAMP");
  nsresult rv = meta->GetTimeStamp(&int64Value);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
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
  nsresult rv = meta->GetTotalTracks(&int32Value);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
  metaData.AppendElement(RtspMetadataParam(name, int32Value));

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
                                     uint32_t reason)
{
  LOG(("RtspControllerParent::OnDisconnected()"));
  if (!mIPCOpen || !SendOnDisconnected(index, reason)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
RtspControllerParent::GetInterface(const nsIID & iid, void **result)
{
  LOG(("RtspControllerParent::GetInterface()"));
  return QueryInterface(iid, result);
}

} // namespace net
} // namespace mozilla
