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

#include "mozilla/StaticPrefs_general.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPrefs_widget.h"

#include "mozilla/LookAndFeel.h"
#include "mozilla/PreferenceSheet.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/dom/ScreenBinding.h"
#include "mozilla/intl/OSPreferences.h"
#include "mozilla/intl/TimeZone.h"
#include "mozilla/widget/ScreenManager.h"

#include "gfxPlatformFontList.h"
#include "prsystem.h"
#if defined(XP_WIN)
#  include "WinUtils.h"
#elif defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/java/GeckoAppShellWrappers.h"
#elif defined(XP_MACOSX)
#  include "nsMacUtilsImpl.h"
#endif

using namespace mozilla;

static LazyLogModule gUserCharacteristicsLog("UserCharacteristics");

// ==================================================================
namespace testing {
extern "C" {

int MaxTouchPoints() {
#if defined(XP_WIN)
  return widget::WinUtils::GetMaxTouchPoints();
#elif defined(MOZ_WIDGET_ANDROID)
  return java::GeckoAppShell::GetMaxTouchPoints();
#else
  return 0;
#endif
}

}  // extern "C"
};  // namespace testing

// ==================================================================
void PopulateCSSProperties() {
  glean::characteristics::prefers_reduced_transparency.Set(
      LookAndFeel::GetInt(LookAndFeel::IntID::PrefersReducedTransparency));
  glean::characteristics::prefers_reduced_motion.Set(
      LookAndFeel::GetInt(LookAndFeel::IntID::PrefersReducedMotion));
  glean::characteristics::inverted_colors.Set(
      LookAndFeel::GetInt(LookAndFeel::IntID::InvertedColors));
  glean::characteristics::color_scheme.Set(
      (int)PreferenceSheet::ContentPrefs().mColorScheme);

  StylePrefersContrast prefersContrast = [] {
    // Replicates Gecko_MediaFeatures_PrefersContrast but without a Document
    if (!PreferenceSheet::ContentPrefs().mUseAccessibilityTheme &&
        PreferenceSheet::ContentPrefs().mUseDocumentColors) {
      return StylePrefersContrast::NoPreference;
    }

    const auto& colors =
        PreferenceSheet::ContentPrefs().ColorsFor(ColorScheme::Light);
    float ratio = RelativeLuminanceUtils::ContrastRatio(
        colors.mDefaultBackground, colors.mDefault);
    // https://www.w3.org/TR/WCAG21/#contrast-minimum
    if (ratio < 4.5f) {
      return StylePrefersContrast::Less;
    }
    // https://www.w3.org/TR/WCAG21/#contrast-enhanced
    if (ratio >= 7.0f) {
      return StylePrefersContrast::More;
    }
    return StylePrefersContrast::Custom;
  }();
  glean::characteristics::prefers_contrast.Set((int)prefersContrast);
}

void PopulateScreenProperties() {
  auto& screenManager = widget::ScreenManager::GetSingleton();
  RefPtr<widget::Screen> screen = screenManager.GetPrimaryScreen();
  MOZ_ASSERT(screen);

  dom::ScreenColorGamut colorGamut;
  screen->GetColorGamut(&colorGamut);
  glean::characteristics::color_gamut.Set((int)colorGamut);

  int32_t colorDepth;
  screen->GetColorDepth(&colorDepth);
  glean::characteristics::color_depth.Set(colorDepth);

  glean::characteristics::color_gamut.Set((int)colorGamut);
  glean::characteristics::color_depth.Set(colorDepth);
  const LayoutDeviceIntRect rect = screen->GetRect();
  glean::characteristics::screen_height.Set(rect.Height());
  glean::characteristics::screen_width.Set(rect.Width());

  glean::characteristics::video_dynamic_range.Set(screen->GetIsHDR());
}

void PopulateMissingFonts() {
  nsCString aMissingFonts;
  gfxPlatformFontList::PlatformFontList()->GetMissingFonts(aMissingFonts);

  glean::characteristics::missing_fonts.Set(aMissingFonts);
}

void PopulatePrefs() {
  nsAutoCString acceptLang;
  Preferences::GetLocalizedCString("intl.accept_languages", acceptLang);
  glean::characteristics::prefs_intl_accept_languages.Set(acceptLang);

  glean::characteristics::prefs_media_eme_enabled.Set(
      StaticPrefs::media_eme_enabled());

  glean::characteristics::prefs_zoom_text_only.Set(
      !Preferences::GetBool("browser.zoom.full"));

  glean::characteristics::prefs_privacy_donottrackheader_enabled.Set(
      StaticPrefs::privacy_donottrackheader_enabled());
  glean::characteristics::prefs_privacy_globalprivacycontrol_enabled.Set(
      StaticPrefs::privacy_globalprivacycontrol_enabled());

  glean::characteristics::prefs_general_autoscroll.Set(
      Preferences::GetBool("general.autoScroll"));
  glean::characteristics::prefs_general_smoothscroll.Set(
      StaticPrefs::general_smoothScroll());
  glean::characteristics::prefs_overlay_scrollbars.Set(
      StaticPrefs::widget_gtk_overlay_scrollbars_enabled());
}

// ==================================================================
// The current schema of the data. Anytime you add a metric, or change how a
// metric is set, this variable should be incremented. It'll be a lot. It's
// okay. We're going to need it to know (including during development) what is
// the source of the data we are looking at.
const int kSubmissionSchema = 0;

/* static */
void nsUserCharacteristics::MaybeSubmitPing() {
  MOZ_LOG(gUserCharacteristicsLog, LogLevel::Debug, ("In MaybeSubmitPing()"));
  MOZ_ASSERT(XRE_IsParentProcess());

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

  auto lastSubmissionVersion = Preferences::GetInt(kLastVersionPref, 0);
  auto currentVersion = Preferences::GetInt(kCurrentVersionPref, 0);

  MOZ_ASSERT(currentVersion == -1 || lastSubmissionVersion <= currentVersion,
             "lastSubmissionVersion is somehow greater than currentVersion "
             "- did you edit prefs improperly?");

  if (lastSubmissionVersion < 0) {
    // This is a way for users to opt out of this ping specifically.
    MOZ_LOG(gUserCharacteristicsLog, LogLevel::Debug,
            ("Returning, User Opt-out"));
    return;
  }
  if (currentVersion == 0) {
    // Do nothing. We do not want any pings.
    MOZ_LOG(gUserCharacteristicsLog, LogLevel::Debug,
            ("Returning, currentVersion == 0"));
    return;
  }
  if (currentVersion == -1) {
    // currentVersion = -1 is a development value to force a ping submission
    MOZ_LOG(gUserCharacteristicsLog, LogLevel::Debug,
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
    Preferences::SetInt(kLastVersionPref, currentVersion);
    MOZ_LOG(gUserCharacteristicsLog, LogLevel::Warning,
            ("Returning, lastSubmissionVersion > currentVersion"));
    return;
  }
  if (lastSubmissionVersion == currentVersion) {
    // We are okay, we've already submitted the most recent ping
    MOZ_LOG(gUserCharacteristicsLog, LogLevel::Warning,
            ("Returning, lastSubmissionVersion == currentVersion"));
    return;
  }
  if (lastSubmissionVersion < currentVersion) {
    if (NS_SUCCEEDED(PopulateData())) {
      if (NS_SUCCEEDED(SubmitPing())) {
        Preferences::SetInt(kLastVersionPref, currentVersion);
      }
    }
  } else {
    MOZ_ASSERT_UNREACHABLE("Should never reach here");
  }
}

const auto* const kUUIDPref =
    "toolkit.telemetry.user_characteristics_ping.uuid";

/* static */
nsresult nsUserCharacteristics::PopulateData(bool aTesting /* = false */) {
  MOZ_LOG(gUserCharacteristicsLog, LogLevel::Warning, ("Populating Data"));
  MOZ_ASSERT(XRE_IsParentProcess());
  glean::characteristics::submission_schema.Set(kSubmissionSchema);

  nsAutoCString uuidString;
  nsresult rv = Preferences::GetCString(kUUIDPref, uuidString);
  if (NS_FAILED(rv) || uuidString.Length() == 0) {
    nsCOMPtr<nsIUUIDGenerator> uuidgen =
        do_GetService("@mozilla.org/uuid-generator;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);

    nsIDToCString id(nsID::GenerateUUID());
    uuidString = id.get();
    Preferences::SetCString(kUUIDPref, uuidString);
  }
  glean::characteristics::client_identifier.Set(uuidString);

  glean::characteristics::max_touch_points.Set(testing::MaxTouchPoints());

  if (aTesting) {
    // Many of the later peices of data do not work in a gtest
    // so just populate something, and return
    return NS_OK;
  }

  PopulateMissingFonts();
  PopulateCSSProperties();
  PopulateScreenProperties();
  PopulatePrefs();

  glean::characteristics::target_frame_rate.Set(gfxPlatform::TargetFrameRate());

  int32_t processorCount = 0;
#if defined(XP_MACOSX)
  if (nsMacUtilsImpl::IsTCSMAvailable()) {
    // On failure, zero is returned from GetPhysicalCPUCount()
    // and we fallback to PR_GetNumberOfProcessors below.
    processorCount = nsMacUtilsImpl::GetPhysicalCPUCount();
  }
#endif
  if (processorCount == 0) {
    processorCount = PR_GetNumberOfProcessors();
  }
  glean::characteristics::processor_count.Set(processorCount);

  AutoTArray<char16_t, 128> tzBuffer;
  auto result = intl::TimeZone::GetDefaultTimeZone(tzBuffer);
  if (result.isOk()) {
    NS_ConvertUTF16toUTF8 timeZone(
        nsDependentString(tzBuffer.Elements(), tzBuffer.Length()));
    glean::characteristics::timezone.Set(timeZone);
  } else {
    glean::characteristics::timezone.Set("<error>"_ns);
  }

  nsAutoCString locale;
  intl::OSPreferences::GetInstance()->GetSystemLocale(locale);
  glean::characteristics::system_locale.Set(locale);

  return NS_OK;
}

/* static */
nsresult nsUserCharacteristics::SubmitPing() {
  MOZ_LOG(gUserCharacteristicsLog, LogLevel::Warning, ("Submitting Ping"));
  glean_pings::UserCharacteristics.Submit();

  return NS_OK;
}
