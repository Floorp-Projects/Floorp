/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef mozilla_pkix__pkixutil_h
#define mozilla_pkix__pkixutil_h

#include "pkixder.h"

namespace mozilla { namespace pkix {

// During path building and verification, we build a linked list of BackCerts
// from the current cert toward the end-entity certificate. The linked list
// is used to verify properties that aren't local to the current certificate
// and/or the direct link between the current certificate and its issuer,
// such as name constraints.
//
// Each BackCert contains pointers to all the given certificate's extensions
// so that we can parse the extension block once and then process the
// extensions in an order that may be different than they appear in the cert.
class BackCert
{
public:
  // certDER and childCert must be valid for the lifetime of BackCert.
  BackCert(Input certDER, EndEntityOrCA endEntityOrCA,
           const BackCert* childCert)
    : der(certDER)
    , endEntityOrCA(endEntityOrCA)
    , childCert(childCert)
  {
  }

  Result Init();

  const Input GetDER() const { return der; }
  const der::Version GetVersion() const { return version; }
  const SignedDataWithSignature& GetSignedData() const { return signedData; }
  const Input GetIssuer() const { return issuer; }
  // XXX: "validity" is a horrible name for the structure that holds
  // notBefore & notAfter, but that is the name used in RFC 5280 and we use the
  // RFC 5280 names for everything.
  const Input GetValidity() const { return validity; }
  const Input GetSerialNumber() const { return serialNumber; }
  const Input GetSubject() const { return subject; }
  const Input GetSubjectPublicKeyInfo() const
  {
    return subjectPublicKeyInfo;
  }
  const Input* GetAuthorityInfoAccess() const
  {
    return MaybeInput(authorityInfoAccess);
  }
  const Input* GetBasicConstraints() const
  {
    return MaybeInput(basicConstraints);
  }
  const Input* GetCertificatePolicies() const
  {
    return MaybeInput(certificatePolicies);
  }
  const Input* GetExtKeyUsage() const
  {
    return MaybeInput(extKeyUsage);
  }
  const Input* GetKeyUsage() const
  {
    return MaybeInput(keyUsage);
  }
  const Input* GetInhibitAnyPolicy() const
  {
    return MaybeInput(inhibitAnyPolicy);
  }
  const Input* GetNameConstraints() const
  {
    return MaybeInput(nameConstraints);
  }

private:
  const Input der;

public:
  const EndEntityOrCA endEntityOrCA;
  BackCert const* const childCert;

private:
  der::Version version;

  // When parsing certificates in BackCert::Init, we don't accept empty
  // extensions. Consequently, we don't have to store a distinction between
  // empty extensions and extensions that weren't included. However, when
  // *processing* extensions, we distinguish between whether an extension was
  // included or not based on whetehr the GetXXX function for the extension
  // returns nullptr.
  static inline const Input* MaybeInput(const Input& item)
  {
    return item.GetLength() > 0 ? &item : nullptr;
  }

  SignedDataWithSignature signedData;
  Input issuer;
  // XXX: "validity" is a horrible name for the structure that holds
  // notBefore & notAfter, but that is the name used in RFC 5280 and we use the
  // RFC 5280 names for everything.
  Input validity;
  Input serialNumber;
  Input subject;
  Input subjectPublicKeyInfo;

  Input authorityInfoAccess;
  Input basicConstraints;
  Input certificatePolicies;
  Input extKeyUsage;
  Input inhibitAnyPolicy;
  Input keyUsage;
  Input nameConstraints;
  Input subjectAltName;

  Result RememberExtension(Reader& extnID, const Input& extnValue,
                           /*out*/ bool& understood);

  BackCert(const BackCert&) /* = delete */;
  void operator=(const BackCert&); /* = delete */;
};

class NonOwningDERArray : public DERArray
{
public:
  NonOwningDERArray()
    : numItems(0)
  {
    // we don't need to initialize the items array because we always check
    // numItems before accessing i.
  }

  virtual size_t GetLength() const { return numItems; }

  virtual const Input* GetDER(size_t i) const
  {
    return i < numItems ? &items[i] : nullptr;
  }

  Result Append(Input der)
  {
    if (numItems >= MAX_LENGTH) {
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
    Result rv = items[numItems].Init(der); // structure assignment
    if (rv != Success) {
      return rv;
    }
    ++numItems;
    return Success;
  }

  // Public so we can static_assert on this. Keep in sync with MAX_SUBCA_COUNT.
  static const size_t MAX_LENGTH = 8;
private:
  Input items[MAX_LENGTH]; // avoids any heap allocations
  size_t numItems;

  NonOwningDERArray(const NonOwningDERArray&) /* = delete*/;
  void operator=(const NonOwningDERArray&) /* = delete*/;
};

inline unsigned int
DaysBeforeYear(unsigned int year)
{
  assert(year >= 1);
  assert(year <= 9999);
  return ((year - 1u) * 365u)
       + ((year - 1u) / 4u)    // leap years are every 4 years,
       - ((year - 1u) / 100u)  // except years divisible by 100,
       + ((year - 1u) / 400u); // except years divisible by 400.
}

} } // namespace mozilla::pkix

#endif // mozilla_pkix__pkixutil_h
