//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SBTelemetryUtils_h__
#define SBTelemetryUtils_h__

#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace safebrowsing {

enum class MatchResult : uint8_t
{
  eNoMatch           = 0x00,
  eV2Prefix          = 0x01,
  eV4Prefix          = 0x02,
  eV2Completion      = 0x04,
  eV4Completion      = 0x08,
  eTelemetryDisabled = 0x10,

  eBothPrefix      = eV2Prefix     | eV4Prefix,
  eBothCompletion  = eV2Completion | eV4Completion,
  eV2PreAndCom     = eV2Prefix     | eV2Completion,
  eV4PreAndCom     = eV4Prefix     | eV4Completion,
  eBothPreAndV2Com = eBothPrefix   | eV2Completion,
  eBothPreAndV4Com = eBothPrefix   | eV4Completion,
  eAll             = eBothPrefix   | eBothCompletion,
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(MatchResult)

enum class MatchThreatType : uint8_t
{
  eIdentical        = 0x00,
  eV2Phishing       = 0x01,
  eV2Malware        = 0x02,
  eV2Unwanted       = 0x04,
  eV4Phishing       = 0x08,
  eV4Malware        = 0x10,
  eV4Unwanted       = 0x20,
  ePhishingMask     = eV2Phishing | eV4Phishing,
  eMalwareMask      = eV2Malware  | eV4Malware,
  eUnwantedMask     = eV2Unwanted | eV4Unwanted,
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(MatchThreatType)

uint8_t
MatchResultToUint(const MatchResult& aResult);

MatchThreatType
TableNameToThreatType(bool aIsV2, const nsACString& aTable);

enum UpdateTimeout {
  eNoTimeout = 0,
  eResponseTimeout = 1,
  eDownloadTimeout = 2,
};

} // namespace safebrowsing
} // namespace mozilla

#endif //SBTelemetryUtils_h__
