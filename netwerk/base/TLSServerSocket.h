/* vim:set ts=2 sw=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_TLSServerSocket_h
#define mozilla_net_TLSServerSocket_h

#include "nsITLSServerSocket.h"
#include "nsServerSocket.h"
#include "nsString.h"
#include "mozilla/Mutex.h"
#include "seccomon.h"

namespace mozilla {
namespace net {

class TLSServerSocket final : public nsServerSocket, public nsITLSServerSocket {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_FORWARD_NSISERVERSOCKET(nsServerSocket::)
  NS_DECL_NSITLSSERVERSOCKET

  // Override methods from nsServerSocket
  virtual void CreateClientTransport(PRFileDesc* clientFD,
                                     const NetAddr& clientAddr) override;
  virtual nsresult SetSocketDefaults() override;
  virtual nsresult OnSocketListen() override;

  TLSServerSocket() = default;

 private:
  virtual ~TLSServerSocket() = default;

  static SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd,
                                       PRBool checksig, PRBool isServer);

  nsCOMPtr<nsIX509Cert> mServerCert;
};

class TLSServerConnectionInfo : public nsITLSServerConnectionInfo,
                                public nsITLSClientStatus,
                                public nsIInterfaceRequestor {
  friend class TLSServerSocket;

 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITLSSERVERCONNECTIONINFO
  NS_DECL_NSITLSCLIENTSTATUS
  NS_DECL_NSIINTERFACEREQUESTOR

  TLSServerConnectionInfo() = default;

 private:
  virtual ~TLSServerConnectionInfo();

  static void HandshakeCallback(PRFileDesc* aFD, void* aArg);
  nsresult HandshakeCallback(PRFileDesc* aFD);

  RefPtr<TLSServerSocket> mServerSocket;
  // Weak ref to the transport, to avoid cycles since the transport holds a
  // reference to the TLSServerConnectionInfo object.  This is not handed out to
  // anyone, and is only used in HandshakeCallback to close the transport in
  // case of an error.  After this, it's set to nullptr.
  nsISocketTransport* mTransport{nullptr};
  nsCOMPtr<nsIX509Cert> mPeerCert;
  int16_t mTlsVersionUsed{TLS_VERSION_UNKNOWN};
  nsCString mCipherName;
  uint32_t mKeyLength{0};
  uint32_t mMacLength{0};
  // lock protects access to mSecurityObserver
  mozilla::Mutex mLock{"TLSServerConnectionInfo.mLock"};
  nsCOMPtr<nsITLSServerSecurityObserver> mSecurityObserver
      MOZ_GUARDED_BY(mLock);
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_TLSServerSocket_h
