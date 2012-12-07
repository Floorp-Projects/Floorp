/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSNSSIOLAYER_H
#define _NSNSSIOLAYER_H

#include "TransportSecurityInfo.h"
#include "nsISSLSocketControl.h"
#include "nsIClientAuthDialogs.h"
#include "nsNSSCertificate.h"
#include "nsDataHashtable.h"
#include "nsTHashtable.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace psm {
class SharedSSLState;
}
}

class nsIObserver;

class nsNSSSocketInfo : public mozilla::psm::TransportSecurityInfo,
                        public nsISSLSocketControl,
                        public nsIClientAuthUserDecision
{
public:
  nsNSSSocketInfo(mozilla::psm::SharedSSLState& aState, uint32_t providerFlags);
  
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISSLSOCKETCONTROL
  NS_DECL_NSICLIENTAUTHUSERDECISION
 
  nsresult SetForSTARTTLS(bool aForSTARTTLS);
  nsresult GetForSTARTTLS(bool *aForSTARTTLS);

  nsresult GetFileDescPtr(PRFileDesc** aFilePtr);
  nsresult SetFileDescPtr(PRFileDesc* aFilePtr);

  nsresult GetHandshakePending(bool *aHandshakePending);
  nsresult SetHandshakePending(bool aHandshakePending);

  void GetPreviousCert(nsIX509Cert** _result);
  
  void SetHasCleartextPhase(bool aHasCleartextPhase);
  bool GetHasCleartextPhase();
  
  void SetHandshakeInProgress(bool aIsIn);
  bool GetHandshakeInProgress() { return mHandshakeInProgress; }
  void SetFirstServerHelloReceived() { mFirstServerHelloReceived = true; }
  bool HandshakeTimeout();

  void SetAllowTLSIntoleranceTimeout(bool aAllow);

  PRStatus CloseSocketAndDestroy(
                const nsNSSShutDownPreventionLock & proofOfLock);
  
  void SetNegotiatedNPN(const char *value, uint32_t length);
  void SetHandshakeCompleted();

  bool GetJoined() { return mJoined; }
  void SetSentClientCert() { mSentClientCert = true; }

  uint32_t GetProviderFlags() const { return mProviderFlags; }

  mozilla::psm::SharedSSLState& SharedState();

  // XXX: These are only used on for diagnostic purposes
  enum CertVerificationState {
    before_cert_verification,
    waiting_for_cert_verification,
    after_cert_verification
  };
  void SetCertVerificationWaiting();
  // Use errorCode == 0 to indicate success; in that case, errorMessageType is
  // ignored.
  void SetCertVerificationResult(PRErrorCode errorCode,
              ::mozilla::psm::SSLErrorMessageType errorMessageType);
  
  // for logging only
  PRBool IsWaitingForCertVerification() const
  {
    return mCertVerificationState == waiting_for_cert_verification;
  }

  bool IsSSL3Enabled() const { return mSSL3Enabled; }
  void SetSSL3Enabled(bool enabled) { mSSL3Enabled = enabled; }
  bool IsTLSEnabled() const { return mTLSEnabled; }
  void SetTLSEnabled(bool enabled) { mTLSEnabled = enabled; }
private:
  PRFileDesc* mFd;

  CertVerificationState mCertVerificationState;

  mozilla::psm::SharedSSLState& mSharedState;
  bool mForSTARTTLS;
  bool mSSL3Enabled;
  bool mTLSEnabled;
  bool mHandshakePending;
  bool mHasCleartextPhase;
  bool mHandshakeInProgress;
  bool mAllowTLSIntoleranceTimeout;
  bool mRememberClientAuthCertificate;
  PRIntervalTime mHandshakeStartTime;
  bool mFirstServerHelloReceived;

  nsresult ActivateSSL();

  nsCString mNegotiatedNPN;
  bool      mNPNCompleted;
  bool      mHandshakeCompleted;
  bool      mJoined;
  bool      mSentClientCert;

  uint32_t mProviderFlags;
  mozilla::TimeStamp mSocketCreationTimestamp;
};

class nsSSLIOLayerHelpers
{
public:
  nsSSLIOLayerHelpers();
  ~nsSSLIOLayerHelpers();

  nsresult Init();
  void Cleanup();

  static bool nsSSLIOLayerInitialized;
  static PRDescIdentity nsSSLIOLayerIdentity;
  static PRIOMethods nsSSLIOLayerMethods;

  mozilla::Mutex *mutex;
  nsTHashtable<nsCStringHashKey> *mTLSIntolerantSites;
  nsTHashtable<nsCStringHashKey> *mTLSTolerantSites;

  nsTHashtable<nsCStringHashKey> *mRenegoUnrestrictedSites;
  bool mTreatUnsafeNegotiationAsBroken;
  int32_t mWarnLevelMissingRFC5746;

  void setTreatUnsafeNegotiationAsBroken(bool broken);
  bool treatUnsafeNegotiationAsBroken();

  void setWarnLevelMissingRFC5746(int32_t level);
  int32_t getWarnLevelMissingRFC5746();

  static void getSiteKey(nsNSSSocketInfo *socketInfo, nsCSubstring &key);
  bool rememberPossibleTLSProblemSite(nsNSSSocketInfo *socketInfo);
  void rememberTolerantSite(nsNSSSocketInfo *socketInfo);

  void addIntolerantSite(const nsCString &str);
  void removeIntolerantSite(const nsCString &str);
  bool isKnownAsIntolerantSite(const nsCString &str);

  void setRenegoUnrestrictedSites(const nsCString &str);
  bool isRenegoUnrestrictedSite(const nsCString &str);

  void clearStoredData();
private:
  nsCOMPtr<nsIObserver> mPrefObserver;
};

nsresult nsSSLIOLayerNewSocket(int32_t family,
                               const char *host,
                               int32_t port,
                               const char *proxyHost,
                               int32_t proxyPort,
                               PRFileDesc **fd,
                               nsISupports **securityInfo,
                               bool forSTARTTLS,
                               uint32_t flags);

nsresult nsSSLIOLayerAddToSocket(int32_t family,
                                 const char *host,
                                 int32_t port,
                                 const char *proxyHost,
                                 int32_t proxyPort,
                                 PRFileDesc *fd,
                                 nsISupports **securityInfo,
                                 bool forSTARTTLS,
                                 uint32_t flags);

nsresult nsSSLIOLayerFreeTLSIntolerantSites();
nsresult displayUnknownCertErrorAlert(nsNSSSocketInfo *infoObject, int error);

#endif /* _NSNSSIOLAYER_H */
