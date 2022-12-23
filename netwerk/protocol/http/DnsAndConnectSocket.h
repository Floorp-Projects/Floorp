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
#include "nsICancelable.h"
#include "nsIDNSListener.h"
#include "nsIDNSRecord.h"
#include "nsIDNSService.h"
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
                                  public nsSupportsWeakReference,
                                  public nsIDNSListener {
  ~DnsAndConnectSocket();

 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_DNSANDCONNECTSOCKET_IID)
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAMCALLBACK
  NS_DECL_NSITRANSPORTEVENTSINK
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED
  NS_DECL_NSIDNSLISTENER

  DnsAndConnectSocket(nsHttpConnectionInfo* ci, nsAHttpTransaction* trans,
                      uint32_t caps, bool speculative, bool isFromPredictor,
                      bool urgentStart);

  [[nodiscard]] nsresult Init(ConnectionEntry* ent);
  void Abandon();
  double Duration(TimeStamp epoch);
  void CloseTransports(nsresult error);

  bool IsSpeculative() { return mSpeculative; }

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
  // This performs checks that the DnsAndConnectSocket has been properly cleand
  // up.
  void CheckIsDone();

  /**
   * State:
   *   INIT: initial state. From this state:
   *         1) change the state to RESOLVING and start the primary DNS lookup
   *            if mSkipDnsResolution is false,
   *         2) or the lookup is skip and the state changes to CONNECTING and
   *            start the backup timer.
   *         3) or changes to DONE in case of an error.
   *   RESOLVING: the primary DNS resolution is in progress. From this state
   *              we transition into CONNECTING or DONE.
   *   CONNECTING: We change to this state when the primary connection has
   *               started. At that point the backup timer is started.
   *   ONE_CONNECTED: We change into this state when one of the connections
   *                  is connected and the second is in progres.
   *   DONE
   *
   * Events:
   *   INIT_EVENT: Start the primary dns resolution (if mSkipDnsResolution is
   *               false), otherwise start the primary connection.
   *   RESOLVED_PRIMARY_EVENT: the primary DNS resolution is done. This event
   *                           may be resent due to DNS retries
   *   CONNECTED_EVENT: A connecion (primary or backup) is done
   */
  enum DnsAndSocketState {
    INIT,
    RESOLVING,
    CONNECTING,
    ONE_CONNECTED,
    DONE
  } mState = INIT;

  enum SetupEvents {
    INIT_EVENT,
    RESOLVED_PRIMARY_EVENT,
    PRIMARY_DONE_EVENT,
    BACKUP_DONE_EVENT,
    BACKUP_TIMER_FIRED_EVENT
  };

  // This structure is responsible for performing DNS lookup, creating socket
  // and connecting the socket.
  struct TransportSetup {
    enum TransportSetupState {
      INIT,
      RESOLVING,
      RESOLVED,
      RETRY_RESOLVING,
      CONNECTING,
      CONNECTING_DONE,
      DONE
    } mState;

    bool FirstResolving() {
      return mState == TransportSetup::TransportSetupState::RESOLVING;
    }
    bool ConnectingOrRetry() {
      return (mState == TransportSetup::TransportSetupState::CONNECTING) ||
             (mState == TransportSetup::TransportSetupState::RETRY_RESOLVING) ||
             (mState == TransportSetup::TransportSetupState::CONNECTING_DONE);
    }
    bool Resolved() {
      return mState == TransportSetup::TransportSetupState::RESOLVED;
    }
    bool DoneConnecting() {
      return (mState == TransportSetup::TransportSetupState::CONNECTING_DONE) ||
             (mState == TransportSetup::TransportSetupState::DONE);
    }

    nsCString mHost;
    nsCOMPtr<nsICancelable> mDNSRequest;
    nsCOMPtr<nsIDNSAddrRecord> mDNSRecord;
    nsIDNSService::DNSFlags mDnsFlags = nsIDNSService::RESOLVE_DEFAULT_FLAGS;
    bool mRetryWithDifferentIPFamily = false;
    bool mResetFamilyPreference = false;
    bool mSkipDnsResolution = false;

    nsCOMPtr<nsISocketTransport> mSocketTransport;
    nsCOMPtr<nsIAsyncOutputStream> mStreamOut;
    nsCOMPtr<nsIAsyncInputStream> mStreamIn;
    TimeStamp mSynStarted;
    bool mConnectedOK = false;
    bool mIsBackup;

    bool mWaitingForConnect = false;
    void SetConnecting();
    void MaybeSetConnectingDone();

    nsresult Init(DnsAndConnectSocket* dnsAndSock);
    void CancelDnsResolution();
    void Abandon();
    void CloseAll();
    nsresult SetupConn(nsAHttpTransaction* transaction, ConnectionEntry* ent,
                       nsresult status, uint32_t cap,
                       HttpConnectionBase** connection);
    [[nodiscard]] nsresult SetupStreams(DnsAndConnectSocket* dnsAndSock);
    nsresult ResolveHost(DnsAndConnectSocket* dnsAndSock);
    bool ShouldRetryDNS();
    nsresult OnLookupComplete(DnsAndConnectSocket* dnsAndSock,
                              nsIDNSRecord* rec, nsresult status);
    nsresult CheckConnectedResult(DnsAndConnectSocket* dnsAndSock);

   protected:
    explicit TransportSetup(bool isBackup);
  };

  struct PrimaryTransportSetup final : TransportSetup {
    PrimaryTransportSetup() : TransportSetup(false) {}
  };

  struct BackupTransportSetup final : TransportSetup {
    BackupTransportSetup() : TransportSetup(true) {}
  };

  nsresult SetupConn(bool isPrimary, nsresult status);
  void SetupBackupTimer();
  void CancelBackupTimer();

  bool IsPrimary(nsITransport* trans);
  bool IsPrimary(nsIAsyncOutputStream* out);
  bool IsPrimary(nsICancelable* dnsRequest);
  bool IsBackup(nsITransport* trans);
  bool IsBackup(nsIAsyncOutputStream* out);
  bool IsBackup(nsICancelable* dnsRequest);

  // To find out whether |mTransaction| is still in the connection entry's
  // pending queue. If the transaction is found and |removeWhenFound| is
  // true, the transaction will be removed from the pending queue.
  already_AddRefed<PendingTransactionInfo> FindTransactionHelper(
      bool removeWhenFound);

  void CheckProxyConfig();
  nsresult SetupDnsFlags(ConnectionEntry* ent);
  nsresult SetupEvent(SetupEvents event);

  RefPtr<nsAHttpTransaction> mTransaction;
  bool mDispatchedMTransaction = false;

  PrimaryTransportSetup mPrimaryTransport;
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

  RefPtr<nsHttpConnectionInfo> mConnInfo;
  nsCOMPtr<nsITimer> mSynTimer;
  BackupTransportSetup mBackupTransport;

  bool mIsHttp3;

  bool mSkipDnsResolution = false;
  bool mProxyNotTransparent = false;
  bool mProxyTransparentResolvesHost = false;
};

NS_DEFINE_STATIC_IID_ACCESSOR(DnsAndConnectSocket, NS_DNSANDCONNECTSOCKET_IID)

}  // namespace net
}  // namespace mozilla

#endif  // DnsAndConnectSocket_h__
