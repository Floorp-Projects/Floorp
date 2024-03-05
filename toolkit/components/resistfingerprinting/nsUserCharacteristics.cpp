/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUserCharacteristics.h"

#include "nsID.h"
#include "nsIUUIDGenerator.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/Logging.h"
#include "mozilla/glean/GleanPings.h"
#include "mozilla/glean/GleanMetrics.h"

#include "prsystem.h"
#if defined(XP_WIN)
#  include "WinUtils.h"
#elif defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/java/GeckoAppShellWrappers.h"
#endif

static mozilla::LazyLogModule gUserCharacteristicsLog("UserCharacteristics");

// ==================================================================
namespace testing {
extern "C" {

int MaxTouchPoints() {
#if defined(XP_WIN)
  return widget::WinUtils::GetMaxTouchPoints();
#elif defined(MOZ_WIDGET_ANDROID)
  return java::GeckoAppShell::GetMaxTouchPoints();
#endif
  return 0;
}

}  // extern "C"
};  // namespace testing

// ==================================================================
// The current schema of the data. Anytime you add a metric, or change how a
// metric is set, this variable should be incremented. It'll be a lot. It's
// okay. We're going to need it to know (including during development) what is
// the source of the data we are looking at.
const int kSubmissionSchema = 0;

/* static */
void nsUserCharacteristics::MaybeSubmitPing() {
  MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Debug,
          ("In MaybeSubmitPing()"));

  /**
   * There are two preferences at play here:
   *  - Last Version Sent - preference containing the last version sent by the
   * user to Mozilla
   *  - Current Version - preference containing the version Mozilla would like
   * the user to send
   *
   * A point of complexity arises in that these two values _may_ be changed
   * by the user, even though neither is intended to be.
   *
   * When Current Version > Last Version Sent, we intend for the user to submit
   * a new ping, which will include the schema version. Then update Last Version
   * Sent = Current Version.
   *
   */
  const auto* const kLastVersionPref =
      "toolkit.telemetry.user_characteristics_ping.last_version_sent";
  const auto* const kCurrentVersionPref =
      "toolkit.telemetry.user_characteristics_ping.current_version";

  auto lastSubmissionVersion =
      mozilla::Preferences::GetInt(kLastVersionPref, 0);
  auto currentVersion = mozilla::Preferences::GetInt(kCurrentVersionPref, 0);

  MOZ_ASSERT(currentVersion == -1 || lastSubmissionVersion <= currentVersion,
             "lastSubmissionVersion is somehow greater than currentVersion "
             "- did you edit prefs improperly?");

  if (lastSubmissionVersion < 0) {
    // This is a way for users to opt out of this ping specifically.
    MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Debug,
            ("Returning, User Opt-out"));
    return;
  }
  if (currentVersion == 0) {
    // Do nothing. We do not want any pings.
    MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Debug,
            ("Returning, currentVersion == 0"));
    return;
  }
  if (currentVersion == -1) {
    // currentVersion = -1 is a development value to force a ping submission
    MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Debug,
            ("Force-Submitting Ping"));
    if (NS_SUCCEEDED(PopulateData())) {
      SubmitPing();
    }
    return;
  }
  if (lastSubmissionVersion > currentVersion) {
    // This is an unexpected scneario that indicates something is wrong. We
    // asserted against it (in debug, above) We will try to sanity-correct
    // ourselves by setting it to the current version.
    mozilla::Preferences::SetInt(kLastVersionPref, currentVersion);
    MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Warning,
            ("Returning, lastSubmissionVersion > currentVersion"));
    return;
  }
  if (lastSubmissionVersion == currentVersion) {
    // We are okay, we've already submitted the most recent ping
    MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Warning,
            ("Returning, lastSubmissionVersion == currentVersion"));
    return;
  }
  if (lastSubmissionVersion < currentVersion) {
    if (NS_SUCCEEDED(PopulateData())) {
      if (NS_SUCCEEDED(SubmitPing())) {
        mozilla::Preferences::SetInt(kLastVersionPref, currentVersion);
      }
    }
  } else {
    MOZ_ASSERT_UNREACHABLE("Should never reach here");
  }
}

const auto* const kUUIDPref =
    "toolkit.telemetry.user_characteristics_ping.uuid";

/* static */
nsresult nsUserCharacteristics::PopulateData() {
  MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Warning,
          ("Populating Data"));
  mozilla::glean::characteristics::submission_schema.Set(kSubmissionSchema);

  nsAutoCString uuidString;
  nsresult rv = mozilla::Preferences::GetCString(kUUIDPref, uuidString);
  if (NS_FAILED(rv) || uuidString.Length() == 0) {
    nsCOMPtr<nsIUUIDGenerator> uuidgen =
        do_GetService("@mozilla.org/uuid-generator;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIDToCString id(nsID::GenerateUUID());
    uuidString = id.get();
    mozilla::Preferences::SetCString(kUUIDPref, uuidString);
  }
  mozilla::glean::characteristics::client_identifier.Set(uuidString);

  mozilla::glean::characteristics::max_touch_points.Set(
      testing::MaxTouchPoints());

  return NS_OK;
}

/* static */
nsresult nsUserCharacteristics::SubmitPing() {
  MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Warning,
          ("Submitting Ping"));
  mozilla::glean_pings::UserCharacteristics.Submit();

  return NS_OK;
}
