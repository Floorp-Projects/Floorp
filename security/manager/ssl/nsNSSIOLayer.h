/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSIOLayer_h
#define nsNSSIOLayer_h

#include "CommonSocketControl.h"
#include "mozilla/Assertions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsTHashMap.h"
#include "nsIProxyInfo.h"
#include "nsISSLSocketControl.h"
#include "nsITlsHandshakeListener.h"
#include "nsNSSCertificate.h"
#include "nsTHashtable.h"
#include "sslt.h"

namespace mozilla {
class OriginAttributes;
namespace psm {
class SharedSSLState;
}  // namespace psm
}  // namespace mozilla

const uint32_t kIPCClientCertsSlotTypeModern = 1;
const uint32_t kIPCClientCertsSlotTypeLegacy = 2;

using mozilla::OriginAttributes;

class nsIObserver;

class nsNSSSocketInfo final : public CommonSocketControl {
 public:
  nsNSSSocketInfo(mozilla::psm::SharedSSLState& aState, uint32_t providerFlags,
                  uint32_t providerTlsFlags);

  NS_DECL_ISUPPORTS_INHERITED

  void SetForSTARTTLS(bool aForSTARTTLS);
  bool GetForSTARTTLS();

  nsresult GetFileDescPtr(PRFileDesc** aFilePtr);
  nsresult SetFileDescPtr(PRFileDesc* aFilePtr);

  bool IsHandshakePending() const { return mHandshakePending; }
  void SetHandshakeNotPending() { mHandshakePending = false; }

  void SetTLSVersionRange(SSLVersionRange range) { mTLSVersionRange = range; }
  SSLVersionRange GetTLSVersionRange() const { return mTLSVersionRange; };

  // From nsISSLSocketControl.
  NS_IMETHOD ProxyStartSSL(void) override;
  NS_IMETHOD StartTLS(void) override;
  NS_IMETHOD SetNPNList(nsTArray<nsCString>& aNPNList) override;
  NS_IMETHOD GetAlpnEarlySelection(nsACString& _retval) override;
  NS_IMETHOD GetEarlyDataAccepted(bool* aEarlyDataAccepted) override;
  NS_IMETHOD DriveHandshake(void) override;
  using nsISSLSocketControl::GetKEAUsed;
  NS_IMETHOD GetKEAUsed(int16_t* aKEAUsed) override;
  NS_IMETHOD GetKEAKeyBits(uint32_t* aKEAKeyBits) override;
  NS_IMETHOD GetProviderTlsFlags(uint32_t* aProviderTlsFlags) override;
  NS_IMETHOD GetSSLVersionOffered(int16_t* aSSLVersionOffered) override;
  NS_IMETHOD GetMACAlgorithmUsed(int16_t* aMACAlgorithmUsed) override;
  bool GetDenyClientCert() override;
  void SetDenyClientCert(bool aDenyClientCert) override;
  NS_IMETHOD GetEsniTxt(nsACString& aEsniTxt) override;
  NS_IMETHOD SetEsniTxt(const nsACString& aEsniTxt) override;
  NS_IMETHOD GetEchConfig(nsACString& aEchConfig) override;
  NS_IMETHOD SetEchConfig(const nsACString& aEchConfig) override;
  NS_IMETHOD GetPeerId(nsACString& aResult) override;
  NS_IMETHOD GetRetryEchConfig(nsACString& aEchConfig) override;
  NS_IMETHOD DisableEarlyData(void) override;
  NS_IMETHOD SetHandshakeCallbackListener(
      nsITlsHandshakeCallbackListener* callback) override;

  PRStatus CloseSocketAndDestroy();

  void SetNegotiatedNPN(const char* value, uint32_t length);
  void SetEarlyDataAccepted(bool aAccepted);

  void SetHandshakeCompleted();
  bool IsHandshakeCompleted() const { return mHandshakeCompleted; }
  void NoteTimeUntilReady();

  void SetFalseStartCallbackCalled() { mFalseStartCallbackCalled = true; }
  void SetFalseStarted() { mFalseStarted = true; }

  // Note that this is only valid *during* a handshake; at the end of the
  // handshake, it gets reset back to false.
  void SetFullHandshake() { mIsFullHandshake = true; }
  bool IsFullHandshake() const { return mIsFullHandshake; }

  void SetEchGreaseUsed() { mEchGreaseUsed = true; }

  bool WasEchUsed() const { return mEchConfig.Length() > 0; }
  bool WasEchGreaseUsed() const { return mEchGreaseUsed; }

  bool GetJoined() { return mJoined; }
  void SetSentClientCert() { mSentClientCert = true; }

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
  void SetCertVerificationResult(PRErrorCode errorCode) override;

  // for logging only
  PRBool IsWaitingForCertVerification() const {
    return mCertVerificationState == waiting_for_cert_verification;
  }
  void AddPlaintextBytesRead(uint64_t val) { mPlaintextBytesRead += val; }

  bool IsPreliminaryHandshakeDone() const { return mPreliminaryHandshakeDone; }
  void SetPreliminaryHandshakeDone() { mPreliminaryHandshakeDone = true; }

  void SetKEAUsed(uint16_t kea) { mKEAUsed = kea; }

  void SetKEAKeyBits(uint32_t keaBits) { mKEAKeyBits = keaBits; }

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

  nsresult SetResumptionTokenFromExternalCache();

  void SetClientCertChain(mozilla::UniqueCERTCertList&& clientCertChain) {
    mClientCertChain = std::move(clientCertChain);
  }

 protected:
  virtual ~nsNSSSocketInfo();

 private:
  PRFileDesc* mFd;

  CertVerificationState mCertVerificationState;

  mozilla::psm::SharedSSLState& mSharedState;
  bool mForSTARTTLS;
  SSLVersionRange mTLSVersionRange;
  bool mHandshakePending;
  bool mPreliminaryHandshakeDone;  // after false start items are complete

  nsresult ActivateSSL();

  nsCString mEsniTxt;
  nsCString mEchConfig;
  bool mEarlyDataAccepted;
  bool mDenyClientCert;
  bool mFalseStartCallbackCalled;
  bool mFalseStarted;
  bool mIsFullHandshake;
  bool mNotedTimeUntilReady;
  bool mEchGreaseUsed;

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
  int16_t mMACAlgorithmUsed;

  uint32_t mProviderTlsFlags;
  mozilla::TimeStamp mSocketCreationTimestamp;
  uint64_t mPlaintextBytesRead;

  // Regarding the client certificate message in the TLS handshake, RFC 5246
  // (TLS 1.2) says:
  //   If the certificate_authorities list in the certificate request
  //   message was non-empty, one of the certificates in the certificate
  //   chain SHOULD be issued by one of the listed CAs.
  // (RFC 8446 (TLS 1.3) has a similar provision)
  // These certificates may be known to gecko but not NSS (e.g. enterprise
  // intermediates). In order to make these certificates discoverable to NSS
  // so it can include them in the message, we cache them here as temporary
  // certificates.
  mozilla::UniqueCERTCertList mClientCertChain;

  // if non-null this is a reference to the mSharedState (which is
  // not an owning reference). If this is used, the info has a private
  // state that does not share things like intolerance lists with the
  // rest of the session. This is normally used when you have per
  // socket tls flags overriding session wide defaults.
  RefPtr<mozilla::psm::SharedSSLState> mOwningSharedRef;

  nsCOMPtr<nsITlsHandshakeCallbackListener> mTlsHandshakeCallback;
};

// This class is used to store the needed information for invoking the client
// cert selection UI.
class ClientAuthInfo final {
 public:
  explicit ClientAuthInfo(const nsACString& hostName,
                          const OriginAttributes& originAttributes,
                          int32_t port, uint32_t providerFlags,
                          uint32_t providerTlsFlags);
  ~ClientAuthInfo() = default;
  ClientAuthInfo(ClientAuthInfo&& aOther) noexcept;

  const nsACString& HostName() const;
  const OriginAttributes& OriginAttributesRef() const;
  int32_t Port() const;
  uint32_t ProviderFlags() const;
  uint32_t ProviderTlsFlags() const;

 private:
  ClientAuthInfo(const ClientAuthInfo&) = delete;
  void operator=(const ClientAuthInfo&) = delete;

  nsCString mHostName;
  OriginAttributes mOriginAttributes;
  int32_t mPort;
  uint32_t mProviderFlags;
  uint32_t mProviderTlsFlags;
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
  nsTHashMap<nsCStringHashKey, IntoleranceEntry> mTLSIntoleranceInfo;
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
  mozilla::Mutex mutex MOZ_UNANNOTATED;
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

extern "C" {
using FindObjectsCallback = void (*)(uint8_t type, size_t id_len,
                                     const uint8_t* id, size_t data_len,
                                     const uint8_t* data, uint32_t slotType,
                                     void* ctx);
void DoFindObjects(FindObjectsCallback cb, void* ctx);
using SignCallback = void (*)(size_t data_len, const uint8_t* data, void* ctx);
void DoSign(size_t cert_len, const uint8_t* cert, size_t data_len,
            const uint8_t* data, size_t params_len, const uint8_t* params,
            SignCallback cb, void* ctx);
}

SECStatus DoGetClientAuthData(ClientAuthInfo&& info,
                              const mozilla::UniqueCERTCertificate& serverCert,
                              nsTArray<nsTArray<uint8_t>>&& collectedCANames,
                              mozilla::UniqueCERTCertificate& outCert,
                              mozilla::UniqueCERTCertList& outBuiltChain);

#endif  // nsNSSIOLayer_h
