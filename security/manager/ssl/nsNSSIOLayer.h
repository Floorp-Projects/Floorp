/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNSSIOLayer_h
#define nsNSSIOLayer_h

#include "mozilla/Assertions.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsIProxyInfo.h"
#include "nsITLSSocketControl.h"
#include "nsITlsHandshakeListener.h"
#include "nsNSSCertificate.h"
#include "nsTHashMap.h"
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

// Order matters for UpdateEchExtensioNStatus.
enum class EchExtensionStatus {
  kNotPresent,  // No ECH Extension was sent
  kGREASE,      // A GREASE ECH Extension was sent
  kReal         // A 'real' ECH Extension was sent
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
                               PRFileDesc** fd,
                               nsITLSSocketControl** tlsSocketControl,
                               bool forSTARTTLS, uint32_t flags,
                               uint32_t tlsFlags);

nsresult nsSSLIOLayerAddToSocket(int32_t family, const char* host, int32_t port,
                                 nsIProxyInfo* proxy,
                                 const OriginAttributes& originAttributes,
                                 PRFileDesc* fd,
                                 nsITLSSocketControl** tlsSocketControl,
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

#endif  // nsNSSIOLayer_h
