/* This Source Code Form is subject to the terms of the Mozilla Public
 *  * License, v. 2.0. If a copy of the MPL was not distributed with this
 *   * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SBTelemetryUtils.h"
#include "mozilla/Assertions.h"

namespace mozilla {
namespace safebrowsing {

uint8_t
MatchResultToUint(const MatchResult& aResult)
{
  MOZ_ASSERT(!(aResult & MatchResult::eTelemetryDisabled));
  switch (aResult) {
  case MatchResult::eNoMatch:          return 0;
  case MatchResult::eV2Prefix:         return 1;
  case MatchResult::eV4Prefix:         return 2;
  case MatchResult::eBothPrefix:       return 3;
  case MatchResult::eAll:              return 4;
  case MatchResult::eV2PreAndCom:      return 5;
  case MatchResult::eV4PreAndCom:      return 6;
  case MatchResult::eBothPreAndV2Com:  return 7;
  case MatchResult::eBothPreAndV4Com:  return 8;
  default:
    MOZ_ASSERT_UNREACHABLE("Unexpected match result");
    return 9;
  }
}

MatchThreatType
TableNameToThreatType(bool aIsV2, const nsACString& aTable)
{
  if (FindInReadable(NS_LITERAL_CSTRING("-phish-"), aTable)) {
    return aIsV2 ? MatchThreatType::eV2Phishing : MatchThreatType::eV4Phishing;
  } else if (FindInReadable(NS_LITERAL_CSTRING("-malware-"), aTable)) {
    return aIsV2 ? MatchThreatType::eV2Malware : MatchThreatType::eV4Malware;
  } else if (FindInReadable(NS_LITERAL_CSTRING("-unwanted-"), aTable)) {
    return aIsV2 ? MatchThreatType::eV2Unwanted : MatchThreatType::eV4Unwanted;
  }
  return MatchThreatType::eIdentical;
}

} // namespace safebrowsing
} // namespace mozilla
