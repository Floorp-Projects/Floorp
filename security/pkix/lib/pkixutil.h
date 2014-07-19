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
  BackCert(InputBuffer certDER, EndEntityOrCA endEntityOrCA,
           const BackCert* childCert)
    : der(certDER)
    , endEntityOrCA(endEntityOrCA)
    , childCert(childCert)
  {
  }

  Result Init();

  const InputBuffer GetDER() const { return der; }
  const der::Version GetVersion() const { return version; }
  const SignedDataWithSignature& GetSignedData() const { return signedData; }
  const InputBuffer GetIssuer() const { return issuer; }
  // XXX: "validity" is a horrible name for the structure that holds
  // notBefore & notAfter, but that is the name used in RFC 5280 and we use the
  // RFC 5280 names for everything.
  const InputBuffer GetValidity() const { return validity; }
  const InputBuffer GetSerialNumber() const { return serialNumber; }
  const InputBuffer GetSubject() const { return subject; }
  const InputBuffer GetSubjectPublicKeyInfo() const
  {
    return subjectPublicKeyInfo;
  }
  const InputBuffer* GetAuthorityInfoAccess() const
  {
    return MaybeInputBuffer(authorityInfoAccess);
  }
  const InputBuffer* GetBasicConstraints() const
  {
    return MaybeInputBuffer(basicConstraints);
  }
  const InputBuffer* GetCertificatePolicies() const
  {
    return MaybeInputBuffer(certificatePolicies);
  }
  const InputBuffer* GetExtKeyUsage() const
  {
    return MaybeInputBuffer(extKeyUsage);
  }
  const InputBuffer* GetKeyUsage() const
  {
    return MaybeInputBuffer(keyUsage);
  }
  const InputBuffer* GetInhibitAnyPolicy() const
  {
    return MaybeInputBuffer(inhibitAnyPolicy);
  }
  const InputBuffer* GetNameConstraints() const
  {
    return MaybeInputBuffer(nameConstraints);
  }

private:
  const InputBuffer der;

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
  static inline const InputBuffer* MaybeInputBuffer(const InputBuffer& item)
  {
    return item.GetLength() > 0 ? &item : nullptr;
  }

  SignedDataWithSignature signedData;
  InputBuffer issuer;
  // XXX: "validity" is a horrible name for the structure that holds
  // notBefore & notAfter, but that is the name used in RFC 5280 and we use the
  // RFC 5280 names for everything.
  InputBuffer validity;
  InputBuffer serialNumber;
  InputBuffer subject;
  InputBuffer subjectPublicKeyInfo;

  InputBuffer authorityInfoAccess;
  InputBuffer basicConstraints;
  InputBuffer certificatePolicies;
  InputBuffer extKeyUsage;
  InputBuffer inhibitAnyPolicy;
  InputBuffer keyUsage;
  InputBuffer nameConstraints;
  InputBuffer subjectAltName;

  Result RememberExtension(Input& extnID, const InputBuffer& extnValue,
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

  virtual const InputBuffer* GetDER(size_t i) const
  {
    return i < numItems ? &items[i] : nullptr;
  }

  Result Append(InputBuffer der)
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
  InputBuffer items[MAX_LENGTH]; // avoids any heap allocations
  size_t numItems;

  NonOwningDERArray(const NonOwningDERArray&) /* = delete*/;
  void operator=(const NonOwningDERArray&) /* = delete*/;
};

} } // namespace mozilla::pkix

#endif // mozilla_pkix__pkixutil_h
