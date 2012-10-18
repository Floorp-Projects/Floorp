/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _NSSSLSTATUS_H
#define _NSSSLSTATUS_H

#include "nsISSLStatus.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsIX509Cert.h"
#include "nsISerializable.h"
#include "nsIClassInfo.h"

class nsSSLStatus
  : public nsISSLStatus
  , public nsISerializable
  , public nsIClassInfo
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISSLSTATUS
  NS_DECL_NSISERIALIZABLE
  NS_DECL_NSICLASSINFO

  nsSSLStatus();
  virtual ~nsSSLStatus();

  /* public for initilization in this file */
  nsCOMPtr<nsIX509Cert> mServerCert;

  uint32_t mKeyLength;
  uint32_t mSecretKeyLength;
  nsXPIDLCString mCipherName;

  bool mIsDomainMismatch;
  bool mIsNotValidAtThisTime;
  bool mIsUntrusted;

  bool mHaveKeyLengthAndCipher;

  /* mHaveCertErrrorBits is relied on to determine whether or not a SPDY
     connection is eligible for joining in nsNSSSocketInfo::JoinConnection() */
  bool mHaveCertErrorBits;
};

// 2c3837af-8b85-4a68-b0d8-0aed88985b32
#define NS_SSLSTATUS_CID \
{ 0x2c3837af, 0x8b85, 0x4a68, \
  { 0xb0, 0xd8, 0x0a, 0xed, 0x88, 0x98, 0x5b, 0x32 } }

#endif
