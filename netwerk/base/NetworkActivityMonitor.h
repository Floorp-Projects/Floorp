/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NetworkActivityMonitor_h___
#define NetworkActivityMonitor_h___

#include "nsCOMPtr.h"
#include "nscore.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsINetworkActivityData.h"
#include "nsISupports.h"
#include "nsITimer.h"
#include "prinrval.h"
#include "prio.h"
#include "private/pprio.h"
#include <stdint.h>

namespace mozilla { namespace net {

typedef nsDataHashtable<nsUint32HashKey, uint32_t> SocketBytes;
typedef nsDataHashtable<nsUint32HashKey, uint32_t> SocketPort;
typedef nsDataHashtable<nsUint32HashKey, nsString> SocketHost;
typedef nsDataHashtable<nsUint32HashKey, bool> SocketActive;


struct NetworkActivity {
    SocketPort port;
    SocketHost host;
    SocketBytes rx;
    SocketBytes tx;
    SocketActive active;
};


class NetworkData final : public nsINetworkActivityData
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKACTIVITYDATA
  NetworkData(const nsString& aHost, int32_t aPort, int32_t aFd,
              int32_t aRx, int32_t aTx)
      : mHost(aHost), mPort(aPort), mFd(aFd), mRx(aRx), mTx(aTx) {}
private:
  ~NetworkData() = default;

  nsString mHost;
  int32_t mPort;
  int32_t mFd;
  int32_t mRx;
  int32_t mTx;
};


class NetworkActivityMonitor: public nsITimerCallback
                            , public nsINamed
{
public:
  enum Direction {
    kUpload   = 0,
    kDownload = 1
  };

  NetworkActivityMonitor();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  static nsresult Init(int32_t aInterval);
  static nsresult Shutdown();
  static nsresult AttachIOLayer(PRFileDesc *fd);
  static nsresult DataInOut(Direction aDirection, PRFileDesc *aFd, uint32_t aAmount);
  static nsresult RegisterFd(PRFileDesc *aFd, const PRNetAddr *aAddr);
  static nsresult UnregisterFd(PRFileDesc *aFd);
private:
  virtual ~NetworkActivityMonitor() = default;
  nsresult Init_Internal(int32_t aInterval);
  nsresult Shutdown_Internal();
  nsresult RegisterFd_Internal(PROsfd aOsfd, const nsString& host, uint16_t port);
  nsresult UnregisterFd_Internal(PROsfd aOsfd);
  nsresult DataInOut_Internal(PROsfd aOsfd, Direction aDirection, uint32_t aAmount);
  uint32_t                        mInterval;
  NetworkActivity                 mActivity;
  nsCOMPtr<nsITimer>              mTimer;
  Mutex                           mLock;
};

} // namespace net
} // namespace mozilla

#endif /* NetworkActivityMonitor_h___ */
