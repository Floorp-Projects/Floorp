/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClosingService.h"
#include "nsIOService.h"

class ClosingLayerSecret
{
public:
  explicit ClosingLayerSecret(mozilla::net::ClosingService *aClosingService)
    : mClosingService(aClosingService)
  {
  }

  ~ClosingLayerSecret()
  {
    mClosingService = nullptr;
  }

  nsRefPtr<mozilla::net::ClosingService> mClosingService;
};

namespace mozilla {
namespace net {

static PRIOMethods sTcpUdpPRCloseLayerMethods;
static PRIOMethods *sTcpUdpPRCloseLayerMethodsPtr = nullptr;
static PRDescIdentity sTcpUdpPRCloseLayerId;

static PRStatus
TcpUdpPRCloseLayerClose(PRFileDesc *aFd)
{
  if (!aFd) {
    return PR_FAILURE;
  }

  PRFileDesc* layer = PR_PopIOLayer(aFd, PR_TOP_IO_LAYER);
  MOZ_RELEASE_ASSERT(layer &&
                     layer->identity == sTcpUdpPRCloseLayerId,
                     "Closing Layer not on top of stack");

  ClosingLayerSecret *closingLayerSecret =
    reinterpret_cast<ClosingLayerSecret *>(layer->secret);

  PRStatus status = PR_SUCCESS;

  if (aFd) {
    // If this is called during shutdown do not call ..method->close(fd) and
    // let it leak.
    if (gIOService->IsShutdown()) {
      // If the ClosingService layer is the first layer above PR_NSPR_IO_LAYER
      // we are not going to leak anything, but the PR_Close will not be called.
      PR_Free(aFd);
    } else if (closingLayerSecret->mClosingService) {
      closingLayerSecret->mClosingService->PostRequest(aFd);
    } else {
      // Socket is created before closing service has been started or there was
      // a problem with starting it.
      PR_Close(aFd);
    }
  }

  layer->secret = nullptr;
  layer->dtor(layer);
  delete closingLayerSecret;
  return status;
}

ClosingService* ClosingService::sInstance = nullptr;

ClosingService::ClosingService()
  : mShutdown(false)
  , mMonitor("ClosingService.mMonitor")
{
  MOZ_ASSERT(!sInstance,
             "multiple ClosingService instances!");
}

// static
void
ClosingService::Start()
{
  if (!sTcpUdpPRCloseLayerMethodsPtr) {
    sTcpUdpPRCloseLayerId =  PR_GetUniqueIdentity("TCP and UDP PRClose layer");
    PR_ASSERT(PR_INVALID_IO_LAYER != sTcpUdpPRCloseLayerId);

    sTcpUdpPRCloseLayerMethods = *PR_GetDefaultIOMethods();
    sTcpUdpPRCloseLayerMethods.close = TcpUdpPRCloseLayerClose;
    sTcpUdpPRCloseLayerMethodsPtr = &sTcpUdpPRCloseLayerMethods;
  }

  if (!sInstance) {
    ClosingService* service = new ClosingService();
    if (NS_SUCCEEDED(service->StartInternal())) {
      NS_ADDREF(service);
      sInstance = service;
    } else {
      delete service;
    }
  }
}

nsresult
ClosingService::StartInternal()
{
  mThread = PR_CreateThread(PR_USER_THREAD, ThreadFunc, this,
                            PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD,
                            PR_JOINABLE_THREAD, 4 * 4096);
  if (!mThread) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

// static
nsresult
ClosingService::AttachIOLayer(PRFileDesc *aFd)
{
  if (!sTcpUdpPRCloseLayerMethodsPtr) {
    return NS_OK;
  }

  PRFileDesc * layer;
  PRStatus     status;

  layer = PR_CreateIOLayerStub(sTcpUdpPRCloseLayerId,
                               sTcpUdpPRCloseLayerMethodsPtr);

  if (!layer) {
    return NS_OK;
  }

  ClosingLayerSecret *secret = new ClosingLayerSecret(sInstance);
  layer->secret = reinterpret_cast<PRFilePrivate *>(secret);

  status = PR_PushIOLayer(aFd, PR_NSPR_IO_LAYER, layer);

  if (status == PR_FAILURE) {
    delete secret;
    PR_DELETE(layer);
  }
  return NS_OK;
}

void
ClosingService::PostRequest(PRFileDesc *aFd)
{
  mozilla::MonitorAutoLock mon(mMonitor);

  // Check if shutdown is called.
  if (mShutdown) {
    // Let the socket leak. We are in shutdown and some PRClose can take a long
    // time. To prevent shutdown crash (bug 1152046) do not accept sockets any
    // more.
    // If the ClosingService layer is the first layer above PR_NSPR_IO_LAYER
    // we are not going to leak anything, but PR_Close will not be called.
    PR_Free(aFd);
    return;
  }

  mQueue.AppendElement(aFd);
  if (mQueue.Length() == 1) {
    mon.Notify();
  }
}

// static
void
ClosingService::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sInstance) {
    sInstance->ShutdownInternal();
    NS_RELEASE(sInstance);
  }
}

void
ClosingService::ShutdownInternal()
{
  {
    mozilla::MonitorAutoLock mon(mMonitor);
    mShutdown = true;
    // If it is waiting on the empty queue, wake it up.
    if (mQueue.Length() == 0) {
      mon.Notify();
    }
  }

  if (mThread) {
    PR_JoinThread(mThread);
    mThread = nullptr;
  }
}

void
ClosingService::ThreadFunc()
{
  for (;;) {
    PRFileDesc *fd;
    {
      mozilla::MonitorAutoLock mon(mMonitor);
      while (!mShutdown && (mQueue.Length() == 0)) {
        mon.Wait();
      }

      if (mShutdown) {
        // If we are in shutdown leak the rest of the sockets.
        for (uint32_t i = 0; i < mQueue.Length(); i++) {
          fd = mQueue[i];
          // If the ClosingService layer is the first layer above
          // PR_NSPR_IO_LAYER we are not going to leak anything, but PR_Close
          // will not be called.
          PR_Free(fd);
        }
        mQueue.Clear();
        return;
      }

      fd = mQueue[0];
      mQueue.RemoveElementAt(0);
    }
    // Leave lock before closing socket. It can block for a long time and in
    // case we accidentally attach this layer twice this would cause deadlock.

    bool tcp = (PR_GetDescType(PR_GetIdentitiesLayer(fd, PR_NSPR_IO_LAYER)) ==
                PR_DESC_SOCKET_TCP);

    PRIntervalTime closeStarted = PR_IntervalNow();
    fd->methods->close(fd);

    // Post telemetry.
    if (tcp) {
      SendPRCloseTelemetry(closeStarted,
        Telemetry::PRCLOSE_TCP_BLOCKING_TIME_NORMAL,
        Telemetry::PRCLOSE_TCP_BLOCKING_TIME_SHUTDOWN,
        Telemetry::PRCLOSE_TCP_BLOCKING_TIME_CONNECTIVITY_CHANGE,
        Telemetry::PRCLOSE_TCP_BLOCKING_TIME_LINK_CHANGE,
        Telemetry::PRCLOSE_TCP_BLOCKING_TIME_OFFLINE);
    } else {
      SendPRCloseTelemetry(closeStarted,
        Telemetry::PRCLOSE_UDP_BLOCKING_TIME_NORMAL,
        Telemetry::PRCLOSE_UDP_BLOCKING_TIME_SHUTDOWN,
        Telemetry::PRCLOSE_UDP_BLOCKING_TIME_CONNECTIVITY_CHANGE,
        Telemetry::PRCLOSE_UDP_BLOCKING_TIME_LINK_CHANGE,
        Telemetry::PRCLOSE_UDP_BLOCKING_TIME_OFFLINE);
    }
  }
}

void
ClosingService::SendPRCloseTelemetry(PRIntervalTime aStart,
                                     mozilla::Telemetry::ID aIDNormal,
                                     mozilla::Telemetry::ID aIDShutdown,
                                     mozilla::Telemetry::ID aIDConnectivityChange,
                                     mozilla::Telemetry::ID aIDLinkChange,
                                     mozilla::Telemetry::ID aIDOffline)
{
    PRIntervalTime now = PR_IntervalNow();
    if (gIOService->IsShutdown()) {
        Telemetry::Accumulate(aIDShutdown,
                              PR_IntervalToMilliseconds(now - aStart));

    } else if (PR_IntervalToSeconds(now - gIOService->LastConnectivityChange())
               < 60) {
        Telemetry::Accumulate(aIDConnectivityChange,
                              PR_IntervalToMilliseconds(now - aStart));
    } else if (PR_IntervalToSeconds(now - gIOService->LastNetworkLinkChange())
               < 60) {
        Telemetry::Accumulate(aIDLinkChange,
                              PR_IntervalToMilliseconds(now - aStart));

    } else if (PR_IntervalToSeconds(now - gIOService->LastOfflineStateChange())
               < 60) {
        Telemetry::Accumulate(aIDOffline,
                              PR_IntervalToMilliseconds(now - aStart));
    } else {
        Telemetry::Accumulate(aIDNormal,
                              PR_IntervalToMilliseconds(now - aStart));
    }
}

} //namwspacw mozilla
} //namespace net
