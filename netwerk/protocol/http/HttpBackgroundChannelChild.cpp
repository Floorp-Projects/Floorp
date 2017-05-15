/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HttpBackgroundChannelChild.h"

using mozilla::ipc::IPCResult;

namespace mozilla {
namespace net {

HttpBackgroundChannelChild::HttpBackgroundChannelChild()
{
}

HttpBackgroundChannelChild::~HttpBackgroundChannelChild()
{
}

IPCResult
HttpBackgroundChannelChild::RecvOnStartRequestSent()
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvOnTransportAndData(
                                               const nsresult& aChannelStatus,
                                               const nsresult& aTransportStatus,
                                               const uint64_t& aOffset,
                                               const uint32_t& aCount,
                                               const nsCString& aData)
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvOnStopRequest(
                                            const nsresult& aChannelStatus,
                                            const ResourceTimingStruct& aTiming)
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvOnProgress(const int64_t& aProgress,
                                           const int64_t& aProgressMax)
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvOnStatus(const nsresult& aStatus)
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvFlushedForDiversion()
{
  return IPC_OK();
}

IPCResult
HttpBackgroundChannelChild::RecvDivertMessages()
{
  return IPC_OK();
}

void
HttpBackgroundChannelChild::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace net
} // namespace mozilla
