/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TransportSecurityInfo_h
#define TransportSecurityInfo_h

#include "CertVerifier.h"  // For CertificateTransparencyInfo, EVStatus
#include "ScopedNSSTypes.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Components.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/ipc/TransportSecurityInfoUtils.h"
#include "mozpkix/pkixtypes.h"
#include "nsIObjectInputStream.h"
#include "nsITransportSecurityInfo.h"
#include "nsIX509Cert.h"
#include "nsString.h"

namespace mozilla {
namespace psm {

// TransportSecurityInfo implements nsITransportSecurityInfo, which is a
// collection of attributes describing the outcome of a TLS handshake. It is
// constant - once created, it cannot be modified.  It should probably not be
// instantiated directly, but rather accessed via
// nsITLSSocketControl.securityInfo.
class TransportSecurityInfo : public nsITransportSecurityInfo {
 public:
  TransportSecurityInfo(
      uint32_t aSecurityState, PRErrorCode aErrorCode,
      nsTArray<RefPtr<nsIX509Cert>>&& aFailedCertChain,
      nsCOMPtr<nsIX509Cert>& aServerCert,
      nsTArray<RefPtr<nsIX509Cert>>&& aSucceededCertChain,
      Maybe<uint16_t> aCipherSuite, Maybe<nsCString> aKeaGroupName,
      Maybe<nsCString> aSignatureSchemeName, Maybe<uint16_t> aProtocolVersion,
      uint16_t aCertificateTransparencyStatus, Maybe<bool> aIsAcceptedEch,
      Maybe<bool> aIsDelegatedCredential,
      Maybe<OverridableErrorCategory> aOverridableErrorCategory,
      bool aMadeOCSPRequests, bool aUsedPrivateDNS, Maybe<bool> aIsEV,
      bool aNPNCompleted, const nsCString& aNegotiatedNPN, bool aResumed,
      bool aIsBuiltCertChainRootBuiltInRoot, const nsCString& aPeerId);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORTSECURITYINFO

  static bool DeserializeFromIPC(IPC::MessageReader* aReader,
                                 RefPtr<nsITransportSecurityInfo>* aResult);
  static nsresult Read(const nsCString& aSerializedSecurityInfo,
                       nsITransportSecurityInfo** aResult);
  static uint16_t ConvertCertificateTransparencyInfoToStatus(
      const mozilla::psm::CertificateTransparencyInfo& info);

 private:
  virtual ~TransportSecurityInfo() = default;

  const uint32_t mSecurityState;
  const PRErrorCode mErrorCode;
  // Peer cert chain for failed connections.
  const nsTArray<RefPtr<nsIX509Cert>> mFailedCertChain;
  const nsCOMPtr<nsIX509Cert> mServerCert;
  const nsTArray<RefPtr<nsIX509Cert>> mSucceededCertChain;
  const mozilla::Maybe<uint16_t> mCipherSuite;
  const mozilla::Maybe<nsCString> mKeaGroupName;
  const mozilla::Maybe<nsCString> mSignatureSchemeName;
  const mozilla::Maybe<uint16_t> mProtocolVersion;
  const uint16_t mCertificateTransparencyStatus;
  const mozilla::Maybe<bool> mIsAcceptedEch;
  const mozilla::Maybe<bool> mIsDelegatedCredential;
  const mozilla::Maybe<OverridableErrorCategory> mOverridableErrorCategory;
  const bool mMadeOCSPRequests;
  const bool mUsedPrivateDNS;
  const mozilla::Maybe<bool> mIsEV;
  const bool mNPNCompleted;
  const nsCString mNegotiatedNPN;
  const bool mResumed;
  const bool mIsBuiltCertChainRootBuiltInRoot;
  const nsCString mPeerId;

  static nsresult ReadOldOverridableErrorBits(
      nsIObjectInputStream* aStream,
      OverridableErrorCategory& aOverridableErrorCategory);
  static nsresult ReadSSLStatus(
      nsIObjectInputStream* aStream, nsCOMPtr<nsIX509Cert>& aServerCert,
      Maybe<uint16_t>& aCipherSuite, Maybe<uint16_t>& aProtocolVersion,
      Maybe<OverridableErrorCategory>& aOverridableErrorCategory,
      Maybe<bool>& aIsEV, uint16_t& aCertificateTransparencyStatus,
      Maybe<nsCString>& aKeaGroupName, Maybe<nsCString>& aSignatureSchemeName,
      nsTArray<RefPtr<nsIX509Cert>>& aSucceededCertChain);

  // This function is used to read the binary that are serialized
  // by using nsIX509CertList
  static nsresult ReadCertList(nsIObjectInputStream* aStream,
                               nsTArray<RefPtr<nsIX509Cert>>& aCertList);
  static nsresult ReadCertificatesFromStream(
      nsIObjectInputStream* aStream, uint32_t aSize,
      nsTArray<RefPtr<nsIX509Cert>>& aCertList);
};

}  // namespace psm
}  // namespace mozilla

#endif  // TransportSecurityInfo_h
