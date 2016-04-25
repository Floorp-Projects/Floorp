/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRNameMatchingPolicy_h
#define BRNameMatchingPolicy_h

#include "pkix/pkixtypes.h"

namespace mozilla { namespace psm {

// According to the Baseline Requirements version 1.3.3 section 7.1.4.2.2.a,
// the requirements of the subject common name field are as follows:
// "If present, this field MUST contain a single IP address or Fully‐Qualified
// Domain Name that is one of the values contained in the Certificate’s
// subjectAltName extension". Consequently, since any name information present
// in the common name must be present in the subject alternative name extension,
// when performing name matching, it should not be necessary to fall back to the
// common name. Because this consequence has not commonly been enforced, this
// implementation provides a mechanism to start enforcing it gradually while
// maintaining some backwards compatibility. If configured with the mode
// "EnforceAfter23August2016", name matching will only fall back to using the
// subject common name for certificates where the notBefore field is before 23
// August 2016. Similarly, the mode "EnforceAfter23August2015" is also
// available. This is to provide a balance between allowing preexisting
// long-lived certificates and detecting newly-issued problematic certificates.
// Note that this implementation does not actually directly enforce that if the
// subject common name is present, its value corresponds to a dNSName or
// iPAddress entry in the subject alternative name extension.

class BRNameMatchingPolicy : public mozilla::pkix::NameMatchingPolicy
{
public:
  enum class Mode {
    DoNotEnforce = 0,
    EnforceAfter23August2016 = 1,
    EnforceAfter23August2015 = 2,
    Enforce = 3,
  };

  explicit BRNameMatchingPolicy(Mode mode)
    : mMode(mode)
  {
  }

  virtual mozilla::pkix::Result FallBackToCommonName(
    mozilla::pkix::Time notBefore,
    /*out*/ mozilla::pkix::FallBackToSearchWithinSubject& fallBacktoCommonName)
    override;

private:
  Mode mMode;
};

} } // namespace mozilla::psm

#endif // BRNameMatchingPolicy_h
