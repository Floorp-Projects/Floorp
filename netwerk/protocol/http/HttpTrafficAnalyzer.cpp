/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HttpTrafficAnalyzer.h"
#include "HttpLog.h"

#include "mozilla/StaticPrefs.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace net {

#define DEFINE_CATEGORY(_name, _idx) NS_LITERAL_CSTRING("Y" #_idx "_" #_name),
static const nsCString gKeyName[] = {
#include "HttpTrafficAnalyzer.inc"
};
#undef DEFINE_CATEGORY

#define DEFINE_CATEGORY(_name, _idx) \
  Telemetry::LABELS_HTTP_TRAFFIC_ANALYSIS_3::Y##_idx##_##_name,
static const Telemetry::LABELS_HTTP_TRAFFIC_ANALYSIS_3 gTelemetryLabel[] = {
#include "HttpTrafficAnalyzer.inc"
};
#undef DEFINE_CATEGORY

// ----------------------------------------------------
// | Flags                           |   Load Type    |
// ----------------------------------------------------
// | nsIClassOfService::Leader       |       A        |
// | w/o nsIRequest::LOAD_BACKGROUND |       B        |
// | w/ nsIRequest::LOAD_BACKGROUND  |       C        |
// ----------------------------------------------------
// | Category                        | List Category  |
// ----------------------------------------------------
// | Basic Disconnected List         |       I        |
// | Content                         |      II        |
// | Fingerprinting                  |     III        |
// ----------------------------------------------------
// ====================================================
// | Normal Mode                                      |
// ----------------------------------------------------
// | Y = 0 for system principals                      |
// | Y = 1 for first party                            |
// | Y = 2 for non-listed third party type            |
// ----------------------------------------------------
// |          \Y\          | Type A | Type B | Type C |
// ----------------------------------------------------
// | Category I            |    3   |    4   |    5   |
// | Category II           |    6   |    7   |    8   |
// | Category III          |    9   |   10   |   11   |
// ====================================================
// | Private Mode                                     |
// ----------------------------------------------------
// | Y = 12 for system principals                     |
// | Y = 13 for first party                           |
// | Y = 14 for non-listed third party type           |
// ----------------------------------------------------
// |          \Y\          | Type A | Type B | Type C |
// ----------------------------------------------------
// | Category I            |   15   |   16   |   17   |
// | Category II           |   18   |   19   |   20   |
// | Category III          |   21   |   22   |   23   |
// ====================================================

HttpTrafficCategory HttpTrafficAnalyzer::CreateTrafficCategory(
    bool aIsPrivateMode, bool aIsSystemPrincipal, bool aIsThirdParty,
    ClassOfService aClassOfService, TrackingClassification aClassification) {
  uint8_t category = aIsPrivateMode ? 12 : 0;
  if (aIsSystemPrincipal) {
    MOZ_ASSERT_IF(!aIsPrivateMode,
                  gKeyName[category].EqualsLiteral("Y0_N1Sys"));
    MOZ_ASSERT_IF(aIsPrivateMode,
                  gKeyName[category].EqualsLiteral("Y12_P1Sys"));
    return static_cast<HttpTrafficCategory>(category);
  }
  ++category;

  if (!aIsThirdParty) {
    MOZ_ASSERT_IF(!aIsPrivateMode, gKeyName[category].EqualsLiteral("Y1_N1"));
    MOZ_ASSERT_IF(aIsPrivateMode, gKeyName[category].EqualsLiteral("Y13_P1"));
    return static_cast<HttpTrafficCategory>(category);
  }

  switch (aClassification) {
    case TrackingClassification::eNone:
      ++category;
      MOZ_ASSERT_IF(!aIsPrivateMode,
                    gKeyName[category].EqualsLiteral("Y2_N3Oth"));
      MOZ_ASSERT_IF(aIsPrivateMode,
                    gKeyName[category].EqualsLiteral("Y14_P3Oth"));
      return static_cast<HttpTrafficCategory>(category);
    case TrackingClassification::eBasic:
      category += 2;
      break;
    case TrackingClassification::eContent:
      category += 5;
      break;
    case TrackingClassification::eFingerprinting:
      category += 8;
      break;
    default:
      MOZ_ASSERT(false, "incorrect classification");
      return HttpTrafficCategory::eInvalid;
  }

  switch (aClassOfService) {
    case ClassOfService::eLeader:
      MOZ_ASSERT_IF(
          !aIsPrivateMode,
          (aClassification == TrackingClassification::eBasic &&
           gKeyName[category].EqualsLiteral("Y3_N3BasicLead")) ||
              (aClassification == TrackingClassification::eContent &&
               gKeyName[category].EqualsLiteral("Y6_N3ContentLead")) ||
              (aClassification == TrackingClassification::eFingerprinting &&
               gKeyName[category].EqualsLiteral("Y9_N3FpLead")));
      MOZ_ASSERT_IF(
          aIsPrivateMode,
          (aClassification == TrackingClassification::eBasic &&
           gKeyName[category].EqualsLiteral("Y15_P3BasicLead")) ||
              (aClassification == TrackingClassification::eContent &&
               gKeyName[category].EqualsLiteral("Y18_P3ContentLead")) ||
              (aClassification == TrackingClassification::eFingerprinting &&
               gKeyName[category].EqualsLiteral("Y21_P3FpLead")));
      return static_cast<HttpTrafficCategory>(category);
    case ClassOfService::eBackground:
      ++category;

      MOZ_ASSERT_IF(
          !aIsPrivateMode,
          (aClassification == TrackingClassification::eBasic &&
           gKeyName[category].EqualsLiteral("Y4_N3BasicBg")) ||
              (aClassification == TrackingClassification::eContent &&
               gKeyName[category].EqualsLiteral("Y7_N3ContentBg")) ||
              (aClassification == TrackingClassification::eFingerprinting &&
               gKeyName[category].EqualsLiteral("Y10_N3FpBg")));
      MOZ_ASSERT_IF(
          aIsPrivateMode,
          (aClassification == TrackingClassification::eBasic &&
           gKeyName[category].EqualsLiteral("Y16_P3BasicBg")) ||
              (aClassification == TrackingClassification::eContent &&
               gKeyName[category].EqualsLiteral("Y19_P3ContentBg")) ||
              (aClassification == TrackingClassification::eFingerprinting &&
               gKeyName[category].EqualsLiteral("Y22_P3FpBg")));

      return static_cast<HttpTrafficCategory>(category);
    case ClassOfService::eOther:
      category += 2;

      MOZ_ASSERT_IF(
          !aIsPrivateMode,
          (aClassification == TrackingClassification::eBasic &&
           gKeyName[category].EqualsLiteral("Y5_N3BasicOth")) ||
              (aClassification == TrackingClassification::eContent &&
               gKeyName[category].EqualsLiteral("Y8_N3ContentOth")) ||
              (aClassification == TrackingClassification::eFingerprinting &&
               gKeyName[category].EqualsLiteral("Y11_N3FpOth")));
      MOZ_ASSERT_IF(
          aIsPrivateMode,
          (aClassification == TrackingClassification::eBasic &&
           gKeyName[category].EqualsLiteral("Y17_P3BasicOth")) ||
              (aClassification == TrackingClassification::eContent &&
               gKeyName[category].EqualsLiteral("Y20_P3ContentOth")) ||
              (aClassification == TrackingClassification::eFingerprinting &&
               gKeyName[category].EqualsLiteral("Y23_P3FpOth")));

      return static_cast<HttpTrafficCategory>(category);
  }

  MOZ_ASSERT(false, "incorrect class of service");
  return HttpTrafficCategory::eInvalid;
}

nsresult HttpTrafficAnalyzer::IncrementHttpTransaction(
    HttpTrafficCategory aCategory) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(StaticPrefs::network_traffic_analyzer_enabled());
  MOZ_ASSERT(aCategory != HttpTrafficCategory::eInvalid, "invalid category");

  LOG(("HttpTrafficAnalyzer::IncrementHttpTransaction [%s] [this=%p]\n",
       gKeyName[aCategory].get(), this));

  Telemetry::AccumulateCategoricalKeyed(NS_LITERAL_CSTRING("Transaction"),
                                        gTelemetryLabel[aCategory]);
  return NS_OK;
}

nsresult HttpTrafficAnalyzer::IncrementHttpConnection(
    HttpTrafficCategory aCategory) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(StaticPrefs::network_traffic_analyzer_enabled());
  MOZ_ASSERT(aCategory != HttpTrafficCategory::eInvalid, "invalid category");

  LOG(("HttpTrafficAnalyzer::IncrementHttpConnection [%s] [this=%p]\n",
       gKeyName[aCategory].get(), this));

  Telemetry::AccumulateCategoricalKeyed(NS_LITERAL_CSTRING("Connection"),
                                        gTelemetryLabel[aCategory]);
  return NS_OK;
}

nsresult HttpTrafficAnalyzer::IncrementHttpConnection(
    nsTArray<HttpTrafficCategory>&& aCategories) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(StaticPrefs::network_traffic_analyzer_enabled());
  MOZ_ASSERT(!aCategories.IsEmpty(), "empty category");

  nsTArray<HttpTrafficCategory> categories(std::move(aCategories));

  LOG(("HttpTrafficAnalyzer::IncrementHttpConnection size=%" PRIuPTR
       " [this=%p]\n",
       categories.Length(), this));

  // divide categories into 4 parts:
  //   1) normal 1st-party (Y = 0)
  //   2) normal 3rd-party (0 < Y < 11)
  //   3) private 1st-party (Y = 11)
  //   4) private 3rd-party (11 < Y < 22)
  // Normal and private transaction should not share the same connection,
  // and we choose 3rd-party prior than 1st-party.
  HttpTrafficCategory best = categories[0];
  if ((best == 0 || best == 11) && categories.Length() > 1) {
    best = categories[1];
  }
  Unused << IncrementHttpConnection(best);

  return NS_OK;
}

#define CLAMP_U32(num) \
  Clamp<uint32_t>(num, 0, std::numeric_limits<uint32_t>::max())

nsresult HttpTrafficAnalyzer::AccumulateHttpTransferredSize(
    HttpTrafficCategory aCategory, uint64_t aBytesRead, uint64_t aBytesSent) {
  MOZ_ASSERT(OnSocketThread(), "not on socket thread");
  MOZ_ASSERT(StaticPrefs::network_traffic_analyzer_enabled());
  MOZ_ASSERT(aCategory != HttpTrafficCategory::eInvalid, "invalid category");

  LOG(("HttpTrafficAnalyzer::AccumulateHttpTransferredSize [%s] rb=%" PRIu64 " "
       "sb=%" PRIu64 " [this=%p]\n",
       gKeyName[aCategory].get(), aBytesRead, aBytesSent, this));

  // Telemetry supports uint32_t only, and we send KB here.
  auto total = CLAMP_U32((aBytesRead >> 10) + (aBytesSent >> 10));
  if (aBytesRead || aBytesSent) {
    Telemetry::ScalarAdd(Telemetry::ScalarID::NETWORKING_DATA_TRANSFERRED_V3_KB,
                         NS_ConvertUTF8toUTF16(gKeyName[aCategory]), total);
  }
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
