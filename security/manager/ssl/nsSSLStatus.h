/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSSSLSTATUS_H
#define _NSSSLSTATUS_H

#include "CertVerifier.h" // For CertificateTransparencyInfo
#include "nsISSLStatus.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIX509Cert.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"
#include "nsNSSCertificate.h" // For EVStatus

class nsSSLStatus final
  : public nsISSLStatus
  , public nsISerializable
  , public nsIClassInfo
{
protected:
  virtual ~nsSSLStatus();
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSISSLSTATUS
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  nsSSLStatus();

  void SetServerCert(nsNSSCertificate* aServerCert,
                     nsNSSCertificate::EVStatus aEVStatus);

  bool HasServerCert() {
    return mServerCert != nullptr;
  }

  void SetCertificateTransparencyInfo(
    const mozilla::psm::CertificateTransparencyInfo& info);

  /* public for initilization in this file */
  uint16_t mCipherSuite;
  uint16_t mProtocolVersion;
  uint16_t mCertificateTransparencyStatus;

  bool mIsDomainMismatch;
  bool mIsNotValidAtThisTime;
  bool mIsUntrusted;
  bool mIsEV;

  bool mHasIsEVStatus;
  bool mHaveCipherSuiteAndProtocol;

  /* mHaveCertErrrorBits is relied on to determine whether or not a SPDY
     connection is eligible for joining in nsNSSSocketInfo::JoinConnection() */
  bool mHaveCertErrorBits;

private:
  nsCOMPtr<nsIX509Cert> mServerCert;
};

#define NS_SSLSTATUS_CID \
{ 0xe2f14826, 0x9e70, 0x4647, \
  { 0xb2, 0x3f, 0x10, 0x10, 0xf5, 0x12, 0x46, 0x28 } }

#endif
