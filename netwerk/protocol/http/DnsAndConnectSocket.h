/* vim:set ts=4 sw=2 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DnsAndConnectSocket_h__
#define DnsAndConnectSocket_h__

#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsAHttpConnection.h"
#include "nsHttpConnection.h"
#include "nsHttpTransaction.h"
#include "nsIAsyncOutputStream.h"
#include "nsINamed.h"
#include "nsITransport.h"
#include "nsWeakReference.h"

namespace mozilla {
namespace net {

// 8d411b53-54bc-4a99-8b78-ff125eab1564
#define NS_DNSANDCONNECTSOCKET_IID                   \
  {                                                  \
    0x8d411b53, 0x54bc, 0x4a99, {                    \
      0x8b, 0x78, 0xff, 0x12, 0x5e, 0xab, 0x15, 0x64 \
    }                                                \
  }

class PendingTransactionInfo;
class ConnectionEntry;

class DnsAndConnectSocket final : public nsIOutputStreamCallback,
                                  public nsITransportEventSink,
                                  public nsIInterfaceRequestor,
                                  public nsITimerCallback,
                                  public nsINamed,
                                  public nsSupportsWeakReference {
  ~DnsAndConnectSocket();

 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DNSANDCONNECTSOCKET_IID)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSITRANSPORTEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  DnsAndConnectSocket(ConnectionEntry* ent, nsAHttpTransaction* trans,
                      uint32_t caps, bool speculative, bool isFromPredictor,
                      bool urgentStart);

  [[nodiscard]] nsresult SetupPrimaryStreams();
  void Abandon();
  double Duration(TimeStamp epoch);
  void CloseTransports(nsresult error);

  bool IsSpeculative() { return mSpeculative; }

  bool IsFromPredictor() { return mIsFromPredictor; }

  bool Allow1918() { return mAllow1918; }
  void SetAllow1918(bool val) { mAllow1918 = val; }

  bool HasConnected() { return mHasConnected; }

  void PrintDiagnostics(nsCString& log);

  // Checks whether the transaction can be dispatched using this
  // half-open's connection.  If this half-open is marked as urgent-start,
  // it only accepts urgent start transactions.  Call only before Claim().
  bool AcceptsTransaction(nsHttpTransaction* trans);
  bool Claim();
  void Unclaim();

 private:
  struct TransportSetup {
    nsCOMPtr<nsISocketTransport> mSocketTransport;
    nsCOMPtr<nsIAsyncOutputStream> mStreamOut;
    nsCOMPtr<nsIAsyncInputStream> mStreamIn;
    TimeStamp mSynStarted;
    bool mConnectedOK = false;
    void Abandon();
    nsresult SetupConn(nsAHttpTransaction* transaction, ConnectionEntry* ent,
                       HttpConnectionBase** connection);
    [[nodiscard]] nsresult SetupStreams(DnsAndConnectSocket* dnsAndSock,
                                        bool isBackup);
  };

  [[nodiscard]] nsresult SetupBackupStreams();
  [[nodiscard]] nsresult SetupStreams(bool isBackup);
  nsresult SetupConn(nsIAsyncOutputStream* out);
  void SetupBackupTimer();
  void CancelBackupTimer();

  bool IsPrimary(nsITransport* trans);
  bool IsPrimary(nsIAsyncOutputStream* out);
  bool IsBackup(nsITransport* trans);
  bool IsBackup(nsIAsyncOutputStream* out);

  // To find out whether |mTransaction| is still in the connection entry's
  // pending queue. If the transaction is found and |removeWhenFound| is
  // true, the transaction will be removed from the pending queue.
  already_AddRefed<PendingTransactionInfo> FindTransactionHelper(
      bool removeWhenFound);

  RefPtr<nsAHttpTransaction> mTransaction;
  bool mDispatchedMTransaction = false;

  TransportSetup mPrimaryTransport;
  uint32_t mCaps;

  // mSpeculative is set if the socket was created from
  // SpeculativeConnect(). It is cleared when a transaction would normally
  // start a new connection from scratch but instead finds this one in
  // the half open list and claims it for its own use. (which due to
  // the vagaries of scheduling from the pending queue might not actually
  // match up - but it prevents a speculative connection from opening
  // more connections that are needed.)
  bool mSpeculative;

  // If created with a non-null urgent transaction, remember it, so we can
  // mark the connection as urgent rightaway it's created.
  bool mUrgentStart;

  // mIsFromPredictor is set if the socket originated from the network
  // Predictor. It is used to gather telemetry data on used speculative
  // connections from the predictor.
  bool mIsFromPredictor;

  bool mAllow1918 = true;

  // mHasConnected tracks whether one of the sockets has completed the
  // connection process. It may have completed unsuccessfully.
  bool mHasConnected = false;

  bool mBackupConnStatsSet = false;

  // A DnsAndConnectSocket can be made for a concrete non-null transaction,
  // but the transaction can be dispatch to another connection. In that
  // case we can free this transaction to be claimed by other
  // transactions.
  bool mFreeToUse = true;
  nsresult mPrimaryStreamStatus = NS_OK;

  RefPtr<ConnectionEntry> mEnt;
  nsCOMPtr<nsITimer> mSynTimer;
  TransportSetup mBackupTransport;

  bool mIsHttp3;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DnsAndConnectSocket, NS_DNSANDCONNECTSOCKET_IID)

}  // namespace net
}  // namespace mozilla

#endif  // DnsAndConnectSocket_h__
