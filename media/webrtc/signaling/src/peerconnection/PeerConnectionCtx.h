/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef peerconnectionctx_h___h__
#define peerconnectionctx_h___h__

#include <string>

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
#include "WebrtcGlobalChild.h"
#endif

#include "mozilla/Attributes.h"
#include "StaticPtr.h"
#include "PeerConnectionImpl.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsIRunnable.h"

namespace mozilla {
class PeerConnectionCtxShutdown;

namespace dom {
class WebrtcGlobalInformation;
}

// A class to hold some of the singleton objects we need:
// * The global PeerConnectionImpl table and its associated lock.
// * Stats report objects for PCs that are gone
// * GMP related state
class PeerConnectionCtx {
 public:
  static nsresult InitializeGlobal(nsIThread *mainThread, nsIEventTarget *stsThread);
  static PeerConnectionCtx* GetInstance();
  static bool isActive();
  static void Destroy();

  bool isReady() {
    // If mGMPService is not set, we aren't using GMP.
    if (mGMPService) {
      return mGMPReady;
    }
    return true;
  }

  void queueJSEPOperation(nsIRunnable* aJSEPOperation);
  void onGMPReady();

  bool gmpHasH264();

  // Make these classes friend so that they can access mPeerconnections.
  friend class PeerConnectionImpl;
  friend class PeerConnectionWrapper;
  friend class mozilla::dom::WebrtcGlobalInformation;

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // WebrtcGlobalInformation uses this; we put it here so we don't need to
  // create another shutdown observer class.
  mozilla::dom::Sequence<mozilla::dom::RTCStatsReportInternal>
    mStatsForClosedPeerConnections;
#endif

  const std::map<const std::string, PeerConnectionImpl *>& mGetPeerConnections();
 private:
  // We could make these available only via accessors but it's too much trouble.
  std::map<const std::string, PeerConnectionImpl *> mPeerConnections;

  PeerConnectionCtx() :  mGMPReady(false) {}
  // This is a singleton, so don't copy construct it, etc.
  PeerConnectionCtx(const PeerConnectionCtx& other) = delete;
  void operator=(const PeerConnectionCtx& other) = delete;
  virtual ~PeerConnectionCtx();

  nsresult Initialize();
  nsresult Cleanup();

  void initGMP();

  static void
  EverySecondTelemetryCallback_m(nsITimer* timer, void *);

#if !defined(MOZILLA_EXTERNAL_LINKAGE)
  // Telemetry Peer conection counter
  int mConnectionCounter;

  nsCOMPtr<nsITimer> mTelemetryTimer;

public:
  // TODO(jib): If we ever enable move semantics on std::map...
  //std::map<nsString,nsAutoPtr<mozilla::dom::RTCStatsReportInternal>> mLastReports;
  nsTArray<nsAutoPtr<mozilla::dom::RTCStatsReportInternal>> mLastReports;
private:
#endif

  // We cannot form offers/answers properly until the Gecko Media Plugin stuff
  // has been initted, which is a complicated mess of thread dispatches,
  // including sync dispatches to main. So, we need to be able to queue up
  // offer creation (or SetRemote, when we're the answerer) until all of this is
  // ready to go, since blocking on this init is just begging for deadlock.
  nsCOMPtr<mozIGeckoMediaPluginService> mGMPService;
  bool mGMPReady;
  nsTArray<nsCOMPtr<nsIRunnable>> mQueuedJSEPOperations;

  static PeerConnectionCtx *gInstance;
public:
  static nsIThread *gMainThread;
  static mozilla::StaticRefPtr<mozilla::PeerConnectionCtxShutdown> gPeerConnectionCtxShutdown;
};

}  // namespace mozilla

#endif
