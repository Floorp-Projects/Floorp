/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NetworkActivityMonitor.h"
#include "prmem.h"
#include "nsIObserverService.h"
#include "nsPISocketTransportService.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"
#include "prerror.h"

using namespace mozilla::net;

static PRStatus
nsNetMon_Connect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
  PRStatus ret;
  PRErrorCode code;
  ret = fd->lower->methods->connect(fd->lower, addr, timeout);
  if (ret == PR_SUCCESS || (code = PR_GetError()) == PR_WOULD_BLOCK_ERROR ||
      code == PR_IN_PROGRESS_ERROR)
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kUpload);
  return ret;
}

static int32_t
nsNetMon_Read(PRFileDesc *fd, void *buf, int32_t len)
{
  int32_t ret;
  ret = fd->lower->methods->read(fd->lower, buf, len);
  if (ret >= 0)
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kDownload);
  return ret;
}

static int32_t
nsNetMon_Write(PRFileDesc *fd, const void *buf, int32_t len)
{
  int32_t ret;
  ret = fd->lower->methods->write(fd->lower, buf, len);
  if (ret > 0)
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kUpload);
  return ret;
}

static int32_t
nsNetMon_Writev(PRFileDesc *fd,
                const PRIOVec *iov,
                int32_t size,
                PRIntervalTime timeout)
{
  int32_t ret;
  ret = fd->lower->methods->writev(fd->lower, iov, size, timeout);
  if (ret > 0)
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kUpload);
  return ret;
}

static int32_t
nsNetMon_Recv(PRFileDesc *fd,
              void *buf,
              int32_t amount,
              int flags,
              PRIntervalTime timeout)
{
  int32_t ret;
  ret = fd->lower->methods->recv(fd->lower, buf, amount, flags, timeout);
  if (ret >= 0)
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kDownload);
  return ret;
}

static int32_t
nsNetMon_Send(PRFileDesc *fd,
              const void *buf,
              int32_t amount,
              int flags,
              PRIntervalTime timeout)
{
  int32_t ret;
  ret = fd->lower->methods->send(fd->lower, buf, amount, flags, timeout);
  if (ret > 0)
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kUpload);
  return ret;
}

static int32_t
nsNetMon_RecvFrom(PRFileDesc *fd,
                  void *buf,
                  int32_t amount,
                  int flags,
                  PRNetAddr *addr,
                  PRIntervalTime timeout)
{
  int32_t ret;
  ret = fd->lower->methods->recvfrom(fd->lower,
                                     buf,
                                     amount,
                                     flags,
                                     addr,
                                     timeout);
  if (ret >= 0)
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kDownload);
  return ret;
}

static int32_t
nsNetMon_SendTo(PRFileDesc *fd,
                const void *buf,
                int32_t amount,
                int flags,
                const PRNetAddr *addr,
                PRIntervalTime timeout)
{
  int32_t ret;
  ret = fd->lower->methods->sendto(fd->lower,
                                   buf,
                                   amount,
                                   flags,
                                   addr,
                                   timeout);
  if (ret > 0)
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kUpload);
  return ret;
}

static int32_t
nsNetMon_AcceptRead(PRFileDesc *listenSock,
                    PRFileDesc **acceptedSock,
                    PRNetAddr **peerAddr,
                    void *buf,
                    int32_t amount,
                    PRIntervalTime timeout)
{
  int32_t ret;
  ret = listenSock->lower->methods->acceptread(listenSock->lower,
                                               acceptedSock,
                                               peerAddr,
                                               buf,
                                               amount,
                                               timeout);
  if (ret > 0)
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kDownload);
  return ret;
}


class NotifyNetworkActivity : public mozilla::Runnable {
public:
  explicit NotifyNetworkActivity(NetworkActivityMonitor::Direction aDirection)
    : mozilla::Runnable("NotifyNetworkActivity")
    , mDirection(aDirection)
  {}
  NS_IMETHOD Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (!obs)
      return NS_ERROR_FAILURE;

    obs->NotifyObservers(nullptr,
                         mDirection == NetworkActivityMonitor::kUpload
                           ? NS_NETWORK_ACTIVITY_BLIP_UPLOAD_TOPIC
                           : NS_NETWORK_ACTIVITY_BLIP_DOWNLOAD_TOPIC,
                         nullptr);
    return NS_OK;
  }
private:
  NetworkActivityMonitor::Direction mDirection;
};

NetworkActivityMonitor * NetworkActivityMonitor::gInstance = nullptr;
static PRDescIdentity sNetActivityMonitorLayerIdentity;
static PRIOMethods sNetActivityMonitorLayerMethods;
static PRIOMethods *sNetActivityMonitorLayerMethodsPtr = nullptr;

NetworkActivityMonitor::NetworkActivityMonitor()
  : mBlipInterval(PR_INTERVAL_NO_TIMEOUT)
{
  MOZ_COUNT_CTOR(NetworkActivityMonitor);

  NS_ASSERTION(gInstance==nullptr,
               "multiple NetworkActivityMonitor instances!");
}

NetworkActivityMonitor::~NetworkActivityMonitor()
{
  MOZ_COUNT_DTOR(NetworkActivityMonitor);
  gInstance = nullptr;
}

nsresult
NetworkActivityMonitor::Init(int32_t blipInterval)
{
  nsresult rv;

  if (gInstance)
    return NS_ERROR_ALREADY_INITIALIZED;

  NetworkActivityMonitor * mon = new NetworkActivityMonitor();
  rv = mon->Init_Internal(blipInterval);
  if (NS_FAILED(rv)) {
    delete mon;
    return rv;
  }

  gInstance = mon;
  return NS_OK;
}

nsresult
NetworkActivityMonitor::Shutdown()
{
  if (!gInstance)
    return NS_ERROR_NOT_INITIALIZED;

  delete gInstance;
  return NS_OK;
}

nsresult
NetworkActivityMonitor::Init_Internal(int32_t blipInterval)
{
  if (!sNetActivityMonitorLayerMethodsPtr) {
    sNetActivityMonitorLayerIdentity =
      PR_GetUniqueIdentity("network activity monitor layer");
    sNetActivityMonitorLayerMethods  = *PR_GetDefaultIOMethods();
    sNetActivityMonitorLayerMethods.connect    = nsNetMon_Connect;
    sNetActivityMonitorLayerMethods.read       = nsNetMon_Read;
    sNetActivityMonitorLayerMethods.write      = nsNetMon_Write;
    sNetActivityMonitorLayerMethods.writev     = nsNetMon_Writev;
    sNetActivityMonitorLayerMethods.recv       = nsNetMon_Recv;
    sNetActivityMonitorLayerMethods.send       = nsNetMon_Send;
    sNetActivityMonitorLayerMethods.recvfrom   = nsNetMon_RecvFrom;
    sNetActivityMonitorLayerMethods.sendto     = nsNetMon_SendTo;
    sNetActivityMonitorLayerMethods.acceptread = nsNetMon_AcceptRead;
    sNetActivityMonitorLayerMethodsPtr = &sNetActivityMonitorLayerMethods;
  }

  mBlipInterval = PR_MillisecondsToInterval(blipInterval);
  // Set the last notification times to time that has just expired, so any
  // activity even right now will trigger notification.
  mLastNotificationTime[kUpload] = PR_IntervalNow() - mBlipInterval;
  mLastNotificationTime[kDownload] = mLastNotificationTime[kUpload];

  return NS_OK;
}

nsresult
NetworkActivityMonitor::AttachIOLayer(PRFileDesc *fd)
{
  if (!gInstance)
    return NS_OK;

  PRFileDesc * layer;
  PRStatus     status;

  layer = PR_CreateIOLayerStub(sNetActivityMonitorLayerIdentity,
                               sNetActivityMonitorLayerMethodsPtr);
  if (!layer) {
    return NS_ERROR_FAILURE;
  }

  status = PR_PushIOLayer(fd, PR_NSPR_IO_LAYER, layer);

  if (status == PR_FAILURE) {
    PR_DELETE(layer);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult
NetworkActivityMonitor::DataInOut(Direction direction)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");

  if (gInstance) {
    PRIntervalTime now = PR_IntervalNow();
    if ((now - gInstance->mLastNotificationTime[direction]) >
        gInstance->mBlipInterval) {
      gInstance->mLastNotificationTime[direction] = now;
      gInstance->PostNotification(direction);
    }
  }

  return NS_OK;
}

void
NetworkActivityMonitor::PostNotification(Direction direction)
{
  nsCOMPtr<nsIRunnable> ev = new NotifyNetworkActivity(direction);
  NS_DispatchToMainThread(ev);
}
