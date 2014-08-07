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
#include "sslt.h"

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

  void SetForSTARTTLS(bool aForSTARTTLS);
  bool GetForSTARTTLS();

  nsresult GetFileDescPtr(PRFileDesc** aFilePtr);
  nsresult SetFileDescPtr(PRFileDesc* aFilePtr);

  bool IsHandshakePending() const { return mHandshakePending; }
  void SetHandshakeNotPending() { mHandshakePending = false; }

  void GetPreviousCert(nsIX509Cert** _result);

  void SetTLSVersionRange(SSLVersionRange range) { mTLSVersionRange = range; }
  SSLVersionRange GetTLSVersionRange() const { return mTLSVersionRange; };

  PRStatus CloseSocketAndDestroy(
                const nsNSSShutDownPreventionLock& proofOfLock);

  void SetNegotiatedNPN(const char* value, uint32_t length);

  void SetHandshakeCompleted();
  void NoteTimeUntilReady();


  void SetFalseStartCallbackCalled() { mFalseStartCallbackCalled = true; }
  void SetFalseStarted() { mFalseStarted = true; }

  // Note that this is only valid *during* a handshake; at the end of the handshake,
  // it gets reset back to false.
  void SetFullHandshake() { mIsFullHandshake = true; }
  bool IsFullHandshake() const { return mIsFullHandshake; }

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
  void AddPlaintextBytesRead(uint64_t val) { mPlaintextBytesRead += val; }

  bool IsPreliminaryHandshakeDone() const { return mPreliminaryHandshakeDone; }
  void SetPreliminaryHandshakeDone() { mPreliminaryHandshakeDone = true; }

  void SetKEAUsed(uint16_t kea) { mKEAUsed = kea; }
  inline int16_t GetKEAExpected() // infallible in nsISSLSocketControl
  {
    int16_t result;
    mozilla::DebugOnly<nsresult> rv = GetKEAExpected(&result);
    MOZ_ASSERT(NS_SUCCEEDED(rv));
    return result;
  }

  void SetKEAKeyBits(uint32_t keaBits) { mKEAKeyBits = keaBits; }

  void SetSSLVersionUsed(int16_t version)
  {
    mSSLVersionUsed = version;
  }

  void SetMACAlgorithmUsed(int16_t mac) { mMACAlgorithmUsed = mac; }

protected:
  virtual ~nsNSSSocketInfo();

private:
  PRFileDesc* mFd;

  CertVerificationState mCertVerificationState;

  mozilla::psm::SharedSSLState& mSharedState;
  bool mForSTARTTLS;
  SSLVersionRange mTLSVersionRange;
  bool mHandshakePending;
  bool mRememberClientAuthCertificate;
  bool mPreliminaryHandshakeDone; // after false start items are complete

  nsresult ActivateSSL();

  nsCString mNegotiatedNPN;
  bool      mNPNCompleted;
  bool      mFalseStartCallbackCalled;
  bool      mFalseStarted;
  bool      mIsFullHandshake;
  bool      mHandshakeCompleted;
  bool      mJoined;
  bool      mSentClientCert;
  bool      mNotedTimeUntilReady;

  // mKEA* are used in false start and http/2 detetermination
  // Values are from nsISSLSocketControl
  int16_t mKEAUsed;
  int16_t mKEAExpected;
  uint32_t mKEAKeyBits;
  int16_t mSSLVersionUsed;
  int16_t mMACAlgorithmUsed;

  uint32_t mProviderFlags;
  mozilla::TimeStamp mSocketCreationTimestamp;
  uint64_t mPlaintextBytesRead;

  nsCOMPtr<nsIX509Cert> mClientCert;
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
  static PRDescIdentity nsSSLPlaintextLayerIdentity;
  static PRIOMethods nsSSLIOLayerMethods;
  static PRIOMethods nsSSLPlaintextLayerMethods;

  nsTHashtable<nsCStringHashKey>* mRenegoUnrestrictedSites;
  bool mTreatUnsafeNegotiationAsBroken;
  int32_t mWarnLevelMissingRFC5746;

  void setTreatUnsafeNegotiationAsBroken(bool broken);
  bool treatUnsafeNegotiationAsBroken();
  void setWarnLevelMissingRFC5746(int32_t level);
  int32_t getWarnLevelMissingRFC5746();

private:
  struct IntoleranceEntry
  {
    uint16_t tolerant;
    uint16_t intolerant;

    void AssertInvariant() const
    {
      MOZ_ASSERT(intolerant == 0 || tolerant < intolerant);
    }
  };
  nsDataHashtable<nsCStringHashKey, IntoleranceEntry> mTLSIntoleranceInfo;
public:
  void rememberTolerantAtVersion(const nsACString& hostname, int16_t port,
                                 uint16_t tolerant);
  bool rememberIntolerantAtVersion(const nsACString& hostname, int16_t port,
                                   uint16_t intolerant, uint16_t minVersion);
  void adjustForTLSIntolerance(const nsACString& hostname, int16_t port,
                               /*in/out*/ SSLVersionRange& range);

  void setRenegoUnrestrictedSites(const nsCString& str);
  bool isRenegoUnrestrictedSite(const nsCString& str);
  void clearStoredData();

  bool mFalseStartRequireNPN;
  bool mFalseStartRequireForwardSecrecy;
private:
  mozilla::Mutex mutex;
  nsCOMPtr<nsIObserver> mPrefObserver;
};

nsresult nsSSLIOLayerNewSocket(int32_t family,
                               const char* host,
                               int32_t port,
                               const char* proxyHost,
                               int32_t proxyPort,
                               PRFileDesc** fd,
                               nsISupports** securityInfo,
                               bool forSTARTTLS,
                               uint32_t flags);

nsresult nsSSLIOLayerAddToSocket(int32_t family,
                                 const char* host,
                                 int32_t port,
                                 const char* proxyHost,
                                 int32_t proxyPort,
                                 PRFileDesc* fd,
                                 nsISupports** securityInfo,
                                 bool forSTARTTLS,
                                 uint32_t flags);

nsresult nsSSLIOLayerFreeTLSIntolerantSites();
nsresult displayUnknownCertErrorAlert(nsNSSSocketInfo* infoObject, int error);

#endif /* _NSNSSIOLAYER_H */
