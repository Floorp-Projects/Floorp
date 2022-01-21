/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CRLiteTimestamp_h
#define CRLiteTimestamp_h

#include "nsICertStorage.h"
#include "SignedCertificateTimestamp.h"

namespace mozilla::psm {

class CRLiteTimestamp final : public nsICRLiteTimestamp {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSICRLITETIMESTAMP

  CRLiteTimestamp() : mTimestamp(0) {}
  explicit CRLiteTimestamp(const ct::SignedCertificateTimestamp& sct)
      : mLogID(Span(sct.logId)), mTimestamp(sct.timestamp) {}

 private:
  ~CRLiteTimestamp() = default;

  nsTArray<uint8_t> mLogID;
  uint64_t mTimestamp;
};

}  // namespace mozilla::psm

#endif  // CRLiteTimestamp_h
