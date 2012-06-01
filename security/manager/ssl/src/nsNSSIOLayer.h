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
#include "nsAutoPtr.h"
#include "nsNSSCertificate.h"
#include "nsDataHashtable.h"
#include "nsTHashtable.h"

class nsNSSSocketInfo : public mozilla::psm::TransportSecurityInfo,
                        public nsISSLSocketControl,
                        public nsIClientAuthUserDecision
{
public:
  nsNSSSocketInfo();
  
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
  
  void SetNegotiatedNPN(const char *value, PRUint32 length);
  void SetHandshakeCompleted() { mHandshakeCompleted = true; }

  bool GetJoined() { return mJoined; }
  void SetSentClientCert() { mSentClientCert = true; }
  
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
};

class nsSSLIOLayerHelpers
{
public:
  static nsresult Init();
  static void Cleanup();

  static bool nsSSLIOLayerInitialized;
  static PRDescIdentity nsSSLIOLayerIdentity;
  static PRIOMethods nsSSLIOLayerMethods;

  static mozilla::Mutex *mutex;
  static nsTHashtable<nsCStringHashKey> *mTLSIntolerantSites;
  static nsTHashtable<nsCStringHashKey> *mTLSTolerantSites;

  static nsTHashtable<nsCStringHashKey> *mRenegoUnrestrictedSites;
  static bool mTreatUnsafeNegotiationAsBroken;
  static PRInt32 mWarnLevelMissingRFC5746;

  static void setTreatUnsafeNegotiationAsBroken(bool broken);
  static bool treatUnsafeNegotiationAsBroken();

  static void setWarnLevelMissingRFC5746(PRInt32 level);
  static PRInt32 getWarnLevelMissingRFC5746();

  static void getSiteKey(nsNSSSocketInfo *socketInfo, nsCSubstring &key);
  static bool rememberPossibleTLSProblemSite(nsNSSSocketInfo *socketInfo);
  static void rememberTolerantSite(nsNSSSocketInfo *socketInfo);

  static void addIntolerantSite(const nsCString &str);
  static void removeIntolerantSite(const nsCString &str);
  static bool isKnownAsIntolerantSite(const nsCString &str);

  static void setRenegoUnrestrictedSites(const nsCString &str);
  static bool isRenegoUnrestrictedSite(const nsCString &str);
};

nsresult nsSSLIOLayerNewSocket(PRInt32 family,
                               const char *host,
                               PRInt32 port,
                               const char *proxyHost,
                               PRInt32 proxyPort,
                               PRFileDesc **fd,
                               nsISupports **securityInfo,
                               bool forSTARTTLS,
                               bool anonymousLoad);

nsresult nsSSLIOLayerAddToSocket(PRInt32 family,
                                 const char *host,
                                 PRInt32 port,
                                 const char *proxyHost,
                                 PRInt32 proxyPort,
                                 PRFileDesc *fd,
                                 nsISupports **securityInfo,
                                 bool forSTARTTLS,
                                 bool anonymousLoad);

nsresult nsSSLIOLayerFreeTLSIntolerantSites();
nsresult displayUnknownCertErrorAlert(nsNSSSocketInfo *infoObject, int error);

#endif /* _NSNSSIOLAYER_H */
