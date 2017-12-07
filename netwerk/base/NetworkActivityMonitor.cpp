/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "NetworkActivityMonitor.h"
#include "nsIObserverService.h"
#include "nsPISocketTransportService.h"
#include "nsPrintfCString.h"
#include "nsSocketTransport2.h"
#include "nsSocketTransportService2.h"
#include "nsThreadUtils.h"
#include "mozilla/Services.h"
#include "prerror.h"
#include "prio.h"
#include "prmem.h"
#include <vector>


using namespace mozilla::net;


struct SocketActivity {
    PROsfd fd;
    uint32_t port;
    nsString host;
    uint32_t rx;
    uint32_t tx;
};

mozilla::StaticRefPtr<NetworkActivityMonitor> gInstance;
static PRDescIdentity sNetActivityMonitorLayerIdentity;
static PRIOMethods sNetActivityMonitorLayerMethods;
static PRIOMethods *sNetActivityMonitorLayerMethodsPtr = nullptr;

static PRStatus
nsNetMon_Connect(PRFileDesc *fd, const PRNetAddr *addr, PRIntervalTime timeout)
{
  PRStatus ret;
  PRErrorCode code;
  ret = fd->lower->methods->connect(fd->lower, addr, timeout);
  if (ret == PR_SUCCESS || (code = PR_GetError()) == PR_WOULD_BLOCK_ERROR ||
      code == PR_IN_PROGRESS_ERROR) {
      NetworkActivityMonitor::RegisterFd(fd, addr);
      NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kUpload, fd, 0);
  }
  return ret;
}


static PRStatus
nsNetMon_Close(PRFileDesc *fd)
{
  if (!fd) {
    return PR_FAILURE;
  }
  NetworkActivityMonitor::UnregisterFd(fd);
  PRFileDesc* layer = PR_PopIOLayer(fd, PR_TOP_IO_LAYER);
  MOZ_RELEASE_ASSERT(layer &&
                     layer->identity == sNetActivityMonitorLayerIdentity,
                     "NetActivityMonitor Layer not on top of stack");
  layer->dtor(layer);
  return fd->methods->close(fd);
}


static int32_t
nsNetMon_Read(PRFileDesc *fd, void *buf, int32_t len)
{
  int32_t ret;
  ret = fd->lower->methods->read(fd->lower, buf, len);
  if (ret >= 0) {
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kDownload, fd, len);
  }
  return ret;
}

static int32_t
nsNetMon_Write(PRFileDesc *fd, const void *buf, int32_t len)
{
  int32_t ret;
  ret = fd->lower->methods->write(fd->lower, buf, len);
  if (ret > 0) {
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kUpload, fd, len);
  }
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
  if (ret > 0) {
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kUpload, fd, size);
  }
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
  if (ret >= 0) {
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kDownload, fd, amount);
  }
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
  if (ret > 0) {
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kUpload, fd, amount);
  }
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
  if (ret >= 0) {
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kDownload, fd, amount);
  }
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
  if (ret > 0) {
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kUpload, fd, amount);
  }
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
  if (ret > 0) {
    NetworkActivityMonitor::DataInOut(NetworkActivityMonitor::kDownload, listenSock, amount);
  }
  return ret;
}


NS_IMPL_ISUPPORTS(NetworkData, nsINetworkActivityData);

NS_IMETHODIMP
NetworkData::GetHost(nsAString& aHost) {
  aHost = mHost;
  return NS_OK;
};

NS_IMETHODIMP
NetworkData::GetPort(int32_t* aPort) {
  *aPort = mPort;
  return NS_OK;
};

NS_IMETHODIMP
NetworkData::GetRx(int32_t* aRx) {
  *aRx = mRx;
  return NS_OK;
};

NS_IMETHODIMP
NetworkData::GetTx(int32_t* aTx) {
  *aTx = mTx;
  return NS_OK;
};

NS_IMETHODIMP
NetworkData::GetFd(int32_t* aFd) {
  *aFd = mFd;
  return NS_OK;
};

class NotifyNetworkActivity : public mozilla::Runnable {
public:
  explicit NotifyNetworkActivity(NetworkActivity* aActivity)
    : mozilla::Runnable("NotifyNetworkActivity")

  {
    uint32_t rx;
    uint32_t tx;
    PROsfd fd;
    for (auto iter = aActivity->rx.Iter(); !iter.Done(); iter.Next()) {
      rx = iter.Data();
      fd = iter.Key();
      tx = aActivity->tx.Get(fd);
      if (rx == 0 && tx == 0) {
        // nothing to do
      } else {
        SocketActivity activity;
        activity.fd = fd;
        activity.rx = rx;
        activity.tx = tx;
        activity.host = aActivity->host.Get(fd);
        activity.port = aActivity->port.Get(fd);
        mActivities.AppendElement(activity);
      }
    }
  }

  NS_IMETHODIMP
  Run() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    if (mActivities.Length() == 0) {
      return NS_OK;
    }

    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (!obs) {
      return NS_ERROR_FAILURE;
    }

    nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);
    if (NS_WARN_IF(!array)) {
      return NS_ERROR_FAILURE;
    }

    for (unsigned long i = 0; i < mActivities.Length(); i++) {
      nsCOMPtr<nsINetworkActivityData> data =
        new NetworkData(mActivities[i].host,
                        mActivities[i].port,
                        mActivities[i].fd,
                        mActivities[i].rx,
                        mActivities[i].tx);

      nsresult rv = array->AppendElement(data);
      if (NS_WARN_IF(NS_FAILED(rv))) {
        return rv;
      }
    }
    obs->NotifyObservers(array, NS_NETWORK_ACTIVITY, nullptr);
    return NS_OK;
  }
private:
  nsTArray<SocketActivity> mActivities;
};


NS_IMPL_ISUPPORTS(NetworkActivityMonitor, nsITimerCallback, nsINamed)

NetworkActivityMonitor::NetworkActivityMonitor()
  : mInterval(PR_INTERVAL_NO_TIMEOUT)
  , mLock("NetworkActivityMonitor::mLock")
{
  RefPtr<NetworkActivityMonitor> mon(gInstance);
  MOZ_ASSERT(!mon, "multiple NetworkActivityMonitor instances!");
}

NS_IMETHODIMP
NetworkActivityMonitor::Notify(nsITimer* aTimer)
{
  mozilla::MutexAutoLock lock(mLock);
  nsCOMPtr<nsIRunnable> ev = new NotifyNetworkActivity(&mActivity);
  NS_DispatchToMainThread(ev);
  // reset the counters
  for (auto iter = mActivity.host.Iter(); !iter.Done(); iter.Next()) {
      uint32_t fd = iter.Key();
      mActivity.tx.Put(fd, 0);
      mActivity.rx.Put(fd, 0);
  }
  return NS_OK;
}

NS_IMETHODIMP
NetworkActivityMonitor::GetName(nsACString& aName)
{
  aName.AssignLiteral("NetworkActivityMonitor");
  return NS_OK;
}


nsresult
NetworkActivityMonitor::Init(int32_t aInterval)
{
  nsresult rv = NS_OK;
  RefPtr<NetworkActivityMonitor> mon(gInstance);
  if (mon) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mon = new NetworkActivityMonitor();
  rv = mon->Init_Internal(aInterval);
  if (NS_SUCCEEDED(rv)) {
    gInstance = mon;
  } else {
    rv = NS_ERROR_FAILURE;
  }
  return rv;
}

nsresult
NetworkActivityMonitor::Shutdown()
{
  RefPtr<NetworkActivityMonitor> mon(gInstance);
  if (!mon) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  mon->Shutdown_Internal();
  gInstance = nullptr;
  return NS_OK;
}

nsresult
NetworkActivityMonitor::Init_Internal(int32_t aInterval)
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
    sNetActivityMonitorLayerMethods.close      = nsNetMon_Close;
    sNetActivityMonitorLayerMethodsPtr = &sNetActivityMonitorLayerMethods;
  }

  // create and fire the timer
  mInterval = aInterval;
  mTimer = NS_NewTimer();
  if (!mTimer) {
    return NS_ERROR_FAILURE;
  }
  return mTimer->InitWithCallback(this, mInterval, nsITimer::TYPE_REPEATING_SLACK);
}

nsresult
NetworkActivityMonitor::Shutdown_Internal()
{
  mTimer->Cancel();
  return NS_OK;
}

nsresult
NetworkActivityMonitor::AttachIOLayer(PRFileDesc *fd)
{
  RefPtr<NetworkActivityMonitor> mon(gInstance);
  if (!mon) {
    return NS_OK;
  }
  PRFileDesc* layer;
  PRStatus status;
  layer = PR_CreateIOLayerStub(sNetActivityMonitorLayerIdentity,
                               sNetActivityMonitorLayerMethodsPtr);
  if (!layer) {
    return NS_ERROR_FAILURE;
  }

  status = PR_PushIOLayer(fd, PR_NSPR_IO_LAYER, layer);

  if (status == PR_FAILURE) {
    PR_Free(layer); // PR_CreateIOLayerStub() uses PR_Malloc().
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult
NetworkActivityMonitor::RegisterFd(PRFileDesc *aFd, const PRNetAddr *aAddr) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  RefPtr<NetworkActivityMonitor> mon(gInstance);
  if (!mon) {
    return NS_ERROR_FAILURE;
  }

  PROsfd osfd = PR_FileDesc2NativeHandle(aFd);
  if (NS_WARN_IF(osfd == -1)) {
    return ErrorAccordingToNSPR(PR_GetError());
  }
  uint16_t port;
  if (aAddr->raw.family == PR_AF_INET) {
    port = aAddr->inet.port;
  } else {
    port = aAddr->ipv6.port;
  }
  char _host[net::kNetAddrMaxCStrBufSize] = {0};
  nsAutoCString host;
  if (PR_NetAddrToString(aAddr, _host, sizeof(_host) - 1) == PR_SUCCESS) {
    host.Assign(_host);
  } else {
    host.AppendPrintf("N/A");
  }
  return mon->RegisterFd_Internal(osfd, NS_ConvertUTF8toUTF16(host), port);
}

nsresult
NetworkActivityMonitor::UnregisterFd(PRFileDesc *aFd)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  RefPtr<NetworkActivityMonitor> mon(gInstance);
  if (!mon) {
    return NS_ERROR_FAILURE;
  }
  PROsfd osfd = PR_FileDesc2NativeHandle(aFd);
  if (NS_WARN_IF(osfd == -1)) {
    return ErrorAccordingToNSPR(PR_GetError());
  }
  return mon->UnregisterFd_Internal(osfd);
}

nsresult
NetworkActivityMonitor::UnregisterFd_Internal(PROsfd aOsfd)
{
  mozilla::MutexAutoLock lock(mLock);
  // XXX indefinitely growing list
  mActivity.active.Put(aOsfd, false);
  return NS_OK;
}


nsresult
NetworkActivityMonitor::RegisterFd_Internal(PROsfd aOsfd, const nsString& host, uint16_t port)
{
  mozilla::MutexAutoLock lock(mLock);
  mActivity.port.Put(aOsfd, port);
  mActivity.host.Put(aOsfd, host);
  mActivity.rx.Put(aOsfd, 0);
  mActivity.tx.Put(aOsfd, 0);
  mActivity.active.Put(aOsfd, true);
  return NS_OK;
}

nsresult
NetworkActivityMonitor::DataInOut(Direction aDirection, PRFileDesc *aFd, uint32_t aAmount)
{
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  RefPtr<NetworkActivityMonitor> mon(gInstance);
  if (!mon) {
    return NS_ERROR_FAILURE;
  }
  PROsfd osfd = PR_FileDesc2NativeHandle(aFd);
  if (NS_WARN_IF(osfd == -1)) {
    return ErrorAccordingToNSPR(PR_GetError());
  }
  return mon->DataInOut_Internal(osfd, aDirection, aAmount);
}

nsresult
NetworkActivityMonitor::DataInOut_Internal(PROsfd aOsfd, Direction aDirection, uint32_t aAmount)
{
  mozilla::MutexAutoLock lock(mLock);
  uint32_t current;
  if (aDirection == NetworkActivityMonitor::kUpload) {
    current = mActivity.tx.Get(aOsfd);
    mActivity.tx.Put(aOsfd, aAmount + current);
  }
  else {
    current = mActivity.rx.Get(aOsfd);
    mActivity.rx.Put(aOsfd, aAmount + current);
  }
  return NS_OK;
}
