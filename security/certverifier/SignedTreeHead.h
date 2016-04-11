/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SignedTreeHead_h
#define SignedTreeHead_h

#include "SignedCertificateTimestamp.h"

namespace mozilla { namespace ct {

// Signed Tree Head as defined in section 3.5. of RFC-6962.
struct SignedTreeHead
{
  // Version enum in RFC 6962, Section 3.2. Note that while in the current
  // RFC the STH and SCT share the versioning scheme, there are plans in
  // RFC-6962-bis to use separate versions, so using a separate scheme here.
  enum class Version { V1 = 0, };

  Version version;
  uint64_t timestamp;
  uint64_t treeSize;
  Buffer sha256RootHash;
  DigitallySigned signature;
};

} } // namespace mozilla::ct

#endif // SignedTreeHead_h
