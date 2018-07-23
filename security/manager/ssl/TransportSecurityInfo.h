/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TransportSecurityInfo_h
#define TransportSecurityInfo_h

#include "ScopedNSSTypes.h"
#include "certt.h"
#include "mozilla/Assertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"
#include "nsDataHashtable.h"
#include "nsIAssociatedContentSecurity.h"
#include "nsIInterfaceRequestor.h"
#include "nsISSLStatusProvider.h"
#include "nsITransportSecurityInfo.h"
#include "nsSSLStatus.h"
#include "nsString.h"
#include "pkix/pkixtypes.h"

namespace mozilla { namespace psm {

class TransportSecurityInfo : public nsITransportSecurityInfo
                            , public nsIInterfaceRequestor
                            , public nsISSLStatusProvider
                            , public nsIAssociatedContentSecurity
                            , public nsISerializable
                            , public nsIClassInfo
{
protected:
  virtual ~TransportSecurityInfo() {}
public:
  TransportSecurityInfo();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITRANSPORTSECURITYINFO
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSISSLSTATUSPROVIDER
  NS_DECL_NSIASSOCIATEDCONTENTSECURITY
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  void SetSecurityState(uint32_t aState);

  const nsACString & GetHostName() const { return mHostName; }

  void SetHostName(const char* host);

  int32_t GetPort() const { return mPort; }
  void SetPort(int32_t aPort);

  const OriginAttributes& GetOriginAttributes() const {
    return mOriginAttributes;
  }
  void SetOriginAttributes(const OriginAttributes& aOriginAttributes);

  void SetCanceled(PRErrorCode errorCode);

  /* Set SSL Status values */
  void SetSSLStatus(nsSSLStatus* aSSLStatus);
  nsSSLStatus* SSLStatus() { return mSSLStatus; }
  void SetStatusErrorBits(nsNSSCertificate* cert, uint32_t collected_errors);

  nsresult SetFailedCertChain(UniqueCERTCertList certList);

private:
  mutable ::mozilla::Mutex mMutex;

protected:
  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;

private:
  uint32_t mSecurityState;
  int32_t mSubRequestsBrokenSecurity;
  int32_t mSubRequestsNoSecurity;

  PRErrorCode mErrorCode;

  int32_t mPort;
  nsCString mHostName;
  OriginAttributes mOriginAttributes;

  /* SSL Status */
  RefPtr<nsSSLStatus> mSSLStatus;

  /* Peer cert chain for failed connections (for error reporting) */
  nsCOMPtr<nsIX509CertList> mFailedCertChain;
};

class RememberCertErrorsTable
{
private:
  RememberCertErrorsTable();

  struct CertStateBits
  {
    bool mIsDomainMismatch;
    bool mIsNotValidAtThisTime;
    bool mIsUntrusted;
  };
  nsDataHashtable<nsCStringHashKey, CertStateBits> mErrorHosts;

public:
  void RememberCertHasError(TransportSecurityInfo * infoobject,
                            nsSSLStatus * status,
                            SECStatus certVerificationResult);
  void LookupCertErrorBits(TransportSecurityInfo * infoObject,
                           nsSSLStatus* status);

  static void Init()
  {
    sInstance = new RememberCertErrorsTable();
  }

  static RememberCertErrorsTable & GetInstance()
  {
    MOZ_ASSERT(sInstance);
    return *sInstance;
  }

  static void Cleanup()
  {
    delete sInstance;
    sInstance = nullptr;
  }
private:
  Mutex mMutex;

  static RememberCertErrorsTable * sInstance;
};

} } // namespace mozilla::psm

// 16786594-0296-4471-8096-8f84497ca428
#define TRANSPORTSECURITYINFO_CID \
{ 0x16786594, 0x0296, 0x4471, \
    { 0x80, 0x96, 0x8f, 0x84, 0x49, 0x7c, 0xa4, 0x28 } }

#endif // TransportSecurityInfo_h
