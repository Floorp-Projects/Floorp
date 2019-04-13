/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSIOLayer_h
#define nsNSSIOLayer_h

#include "TransportSecurityInfo.h"
#include "mozilla/Assertions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsDataHashtable.h"
#include "nsIClientAuthDialogs.h"
#include "nsIProxyInfo.h"
#include "nsISSLSocketControl.h"
#include "nsNSSCertificate.h"
#include "nsTHashtable.h"
#include "sslt.h"

namespace mozilla {
class OriginAttributes;
namespace psm {
class SharedSSLState;
}  // namespace psm
}  // namespace mozilla

using mozilla::OriginAttributes;

class nsIObserver;

class nsNSSSocketInfo final : public mozilla::psm::TransportSecurityInfo,
                              public nsISSLSocketControl,
                              public nsIClientAuthUserDecision {
 public:
  nsNSSSocketInfo(mozilla::psm::SharedSSLState& aState, uint32_t providerFlags,
                  uint32_t providerTlsFlags);

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSISSLSOCKETCONTROL
  NS_DECL_NSICLIENTAUTHUSERDECISION

  void SetForSTARTTLS(bool aForSTARTTLS);
  bool GetForSTARTTLS();

  nsresult GetFileDescPtr(PRFileDesc** aFilePtr);
  nsresult SetFileDescPtr(PRFileDesc* aFilePtr);

  bool IsHandshakePending() const { return mHandshakePending; }
  void SetHandshakeNotPending() { mHandshakePending = false; }

  void SetTLSVersionRange(SSLVersionRange range) { mTLSVersionRange = range; }
  SSLVersionRange GetTLSVersionRange() const { return mTLSVersionRange; };

  PRStatus CloseSocketAndDestroy();

  void SetNegotiatedNPN(const char* value, uint32_t length);
  void SetEarlyDataAccepted(bool aAccepted);

  void SetResumed(bool aResumed);

  void SetHandshakeCompleted();
  bool IsHandshakeCompleted() const { return mHandshakeCompleted; }
  void NoteTimeUntilReady();

  void SetFalseStartCallbackCalled() { mFalseStartCallbackCalled = true; }
  void SetFalseStarted() { mFalseStarted = true; }

  // Note that this is only valid *during* a handshake; at the end of the
  // handshake, it gets reset back to false.
  void SetFullHandshake() { mIsFullHandshake = true; }
  bool IsFullHandshake() const { return mIsFullHandshake; }

  bool GetJoined() { return mJoined; }
  void SetSentClientCert() { mSentClientCert = true; }

  uint32_t GetProviderFlags() const { return mProviderFlags; }
  uint32_t GetProviderTlsFlags() const { return mProviderTlsFlags; }

  mozilla::psm::SharedSSLState& SharedState();

  // XXX: These are only used on for diagnostic purposes
  enum CertVerificationState {
    before_cert_verification,
    waiting_for_cert_verification,
    after_cert_verification
  };
  void SetCertVerificationWaiting();
  // Use errorCode == 0 to indicate success;
  void SetCertVerificationResult(PRErrorCode errorCode);

  // for logging only
  PRBool IsWaitingForCertVerification() const {
    return mCertVerificationState == waiting_for_cert_verification;
  }
  void AddPlaintextBytesRead(uint64_t val) { mPlaintextBytesRead += val; }

  bool IsPreliminaryHandshakeDone() const { return mPreliminaryHandshakeDone; }
  void SetPreliminaryHandshakeDone() { mPreliminaryHandshakeDone = true; }

  void SetKEAUsed(uint16_t kea) { mKEAUsed = kea; }

  void SetKEAKeyBits(uint32_t keaBits) { mKEAKeyBits = keaBits; }

  void SetBypassAuthentication(bool val) {
    if (!mHandshakeCompleted) {
      mBypassAuthentication = val;
    }
  }

  void SetSSLVersionUsed(int16_t version) { mSSLVersionUsed = version; }

  void SetMACAlgorithmUsed(int16_t mac) { mMACAlgorithmUsed = mac; }

  void SetShortWritePending(int32_t amount, unsigned char data) {
    mIsShortWritePending = true;
    mShortWriteOriginalAmount = amount;
    mShortWritePendingByte = data;
  }

  bool IsShortWritePending() { return mIsShortWritePending; }

  unsigned char const* GetShortWritePendingByteRef() {
    return &mShortWritePendingByte;
  }

  int32_t ResetShortWritePending() {
    mIsShortWritePending = false;
    return mShortWriteOriginalAmount;
  }

#ifdef DEBUG
  // These helpers assert that the caller does try to send the same data
  // as it was previously when we hit the short-write.  This is a measure
  // to make sure we communicate correctly to the consumer.
  void RememberShortWrittenBuffer(const unsigned char* data) {
    mShortWriteBufferCheck =
        mozilla::MakeUnique<char[]>(mShortWriteOriginalAmount);
    memcpy(mShortWriteBufferCheck.get(), data, mShortWriteOriginalAmount);
  }
  void CheckShortWrittenBuffer(const unsigned char* data, int32_t amount) {
    if (!mShortWriteBufferCheck) return;
    MOZ_ASSERT(amount >= mShortWriteOriginalAmount,
               "unexpected amount length after short write");
    MOZ_ASSERT(
        !memcmp(mShortWriteBufferCheck.get(), data, mShortWriteOriginalAmount),
        "unexpected buffer content after short write");
    mShortWriteBufferCheck = nullptr;
  }
#endif

  void SetSharedOwningReference(mozilla::psm::SharedSSLState* ref);

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
  bool mPreliminaryHandshakeDone;  // after false start items are complete

  nsresult ActivateSSL();

  nsCString mNegotiatedNPN;
  nsCString mEsniTxt;
  bool mNPNCompleted;
  bool mEarlyDataAccepted;
  bool mDenyClientCert;
  bool mFalseStartCallbackCalled;
  bool mFalseStarted;
  bool mIsFullHandshake;
  bool mHandshakeCompleted;
  bool mJoined;
  bool mSentClientCert;
  bool mNotedTimeUntilReady;
  bool mFailedVerification;
  mozilla::Atomic<bool, mozilla::Relaxed> mResumed;

  // True when SSL layer has indicated an "SSL short write", i.e. need
  // to call on send one or more times to push all pending data to write.
  bool mIsShortWritePending;

  // These are only valid if mIsShortWritePending is true.
  //
  // Value of the last byte pending from the SSL short write that needs
  // to be passed to subsequent calls to send to perform the flush.
  unsigned char mShortWritePendingByte;

  // Original amount of data the upper layer has requested to write to
  // return after the successful flush.
  int32_t mShortWriteOriginalAmount;

#ifdef DEBUG
  mozilla::UniquePtr<char[]> mShortWriteBufferCheck;
#endif

  // mKEA* are used in false start and http/2 detetermination
  // Values are from nsISSLSocketControl
  int16_t mKEAUsed;
  uint32_t mKEAKeyBits;
  int16_t mSSLVersionUsed;
  int16_t mMACAlgorithmUsed;
  bool mBypassAuthentication;

  uint32_t mProviderFlags;
  uint32_t mProviderTlsFlags;
  mozilla::TimeStamp mSocketCreationTimestamp;
  uint64_t mPlaintextBytesRead;

  nsCOMPtr<nsIX509Cert> mClientCert;

  // if non-null this is a reference to the mSharedState (which is
  // not an owning reference). If this is used, the info has a private
  // state that does not share things like intolerance lists with the
  // rest of the session. This is normally used when you have per
  // socket tls flags overriding session wide defaults.
  RefPtr<mozilla::psm::SharedSSLState> mOwningSharedRef;
};

class nsSSLIOLayerHelpers {
 public:
  explicit nsSSLIOLayerHelpers(uint32_t aTlsFlags = 0);
  ~nsSSLIOLayerHelpers();

  nsresult Init();
  void Cleanup();

  static bool nsSSLIOLayerInitialized;
  static PRDescIdentity nsSSLIOLayerIdentity;
  static PRDescIdentity nsSSLPlaintextLayerIdentity;
  static PRIOMethods nsSSLIOLayerMethods;
  static PRIOMethods nsSSLPlaintextLayerMethods;

  bool mTreatUnsafeNegotiationAsBroken;

  void setTreatUnsafeNegotiationAsBroken(bool broken);
  bool treatUnsafeNegotiationAsBroken();

 private:
  struct IntoleranceEntry {
    uint16_t tolerant;
    uint16_t intolerant;
    PRErrorCode intoleranceReason;

    void AssertInvariant() const {
      MOZ_ASSERT(intolerant == 0 || tolerant < intolerant);
    }
  };
  nsDataHashtable<nsCStringHashKey, IntoleranceEntry> mTLSIntoleranceInfo;
  // Sites that require insecure fallback to TLS 1.0, set by the pref
  // security.tls.insecure_fallback_hosts, which is a comma-delimited
  // list of domain names.
  nsTHashtable<nsCStringHashKey> mInsecureFallbackSites;

 public:
  void rememberTolerantAtVersion(const nsACString& hostname, int16_t port,
                                 uint16_t tolerant);
  bool fallbackLimitReached(const nsACString& hostname, uint16_t intolerant);
  bool rememberIntolerantAtVersion(const nsACString& hostname, int16_t port,
                                   uint16_t intolerant, uint16_t minVersion,
                                   PRErrorCode intoleranceReason);
  void forgetIntolerance(const nsACString& hostname, int16_t port);
  void adjustForTLSIntolerance(const nsACString& hostname, int16_t port,
                               /*in/out*/ SSLVersionRange& range);
  PRErrorCode getIntoleranceReason(const nsACString& hostname, int16_t port);

  void clearStoredData();
  void loadVersionFallbackLimit();
  void setInsecureFallbackSites(const nsCString& str);
  void initInsecureFallbackSites();
  bool isPublic() const;
  void removeInsecureFallbackSite(const nsACString& hostname, uint16_t port);
  bool isInsecureFallbackSite(const nsACString& hostname);

  uint16_t mVersionFallbackLimit;

 private:
  mozilla::Mutex mutex;
  nsCOMPtr<nsIObserver> mPrefObserver;
  uint32_t mTlsFlags;
};

nsresult nsSSLIOLayerNewSocket(int32_t family, const char* host, int32_t port,
                               nsIProxyInfo* proxy,
                               const OriginAttributes& originAttributes,
                               PRFileDesc** fd, nsISupports** securityInfo,
                               bool forSTARTTLS, uint32_t flags,
                               uint32_t tlsFlags);

nsresult nsSSLIOLayerAddToSocket(int32_t family, const char* host, int32_t port,
                                 nsIProxyInfo* proxy,
                                 const OriginAttributes& originAttributes,
                                 PRFileDesc* fd, nsISupports** securityInfo,
                                 bool forSTARTTLS, uint32_t flags,
                                 uint32_t tlsFlags);

nsresult nsSSLIOLayerFreeTLSIntolerantSites();

#endif  // nsNSSIOLayer_h
