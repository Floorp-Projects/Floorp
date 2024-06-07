/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "nsUserCharacteristics.h"

#include "nsID.h"
#include "nsIUUIDGenerator.h"
#include "nsIUserCharacteristicsPageService.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/Logging.h"
#include "mozilla/glean/GleanPings.h"
#include "mozilla/glean/GleanMetrics.h"

#include "jsapi.h"
#include "mozilla/Components.h"
#include "mozilla/dom/Promise-inl.h"

#include "mozilla/StaticPrefs_browser.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_general.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/StaticPrefs_widget.h"

#include "mozilla/LookAndFeel.h"
#include "mozilla/PreferenceSheet.h"
#include "mozilla/RelativeLuminanceUtils.h"
#include "mozilla/ServoStyleConsts.h"
#include "mozilla/dom/ScreenBinding.h"
#include "mozilla/intl/OSPreferences.h"
#include "mozilla/intl/TimeZone.h"
#include "mozilla/widget/ScreenManager.h"
#include "mozilla/dom/BrowsingContext.h"
#include "mozilla/dom/Document.h"
#include "nsPIDOMWindow.h"
#include "nsIAppWindow.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIBaseWindow.h"
#include "mozilla/MediaManager.h"
#include "mozilla/dom/MediaDeviceInfoBinding.h"
#include "mozilla/MozPromise.h"
#include "nsThreadUtils.h"
#include "CubebDeviceEnumerator.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/dom/Navigator.h"
#include "nsIGSettingsService.h"

#include "gfxPlatformFontList.h"
#include "prsystem.h"
#if defined(XP_WIN)
#  include "WinUtils.h"
#  include "mozilla/gfx/DisplayConfigWindows.h"
#  include "gfxWindowsPlatform.h"
#elif defined(MOZ_WIDGET_ANDROID)
#  include "mozilla/java/GeckoAppShellWrappers.h"
#elif defined(XP_MACOSX)
#  include "nsMacUtilsImpl.h"
#  include <CoreFoundation/CoreFoundation.h>
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

using VoidPromise = MozPromise<void_t, void_t, true>::Private;

// ==================================================================
// ==================================================================
RefPtr<VoidPromise> ContentPageStuff() {
  nsCOMPtr<nsIUserCharacteristicsPageService> ucp =
      do_GetService("@mozilla.org/user-characteristics-page;1");
  MOZ_ASSERT(ucp);

  RefPtr<VoidPromise> voidPromise = new VoidPromise(__func__);
  RefPtr<mozilla::dom::Promise> promise;
  nsresult rv = ucp->CreateContentPage(getter_AddRefs(promise));
  if (NS_FAILED(rv)) {
    MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Error,
            ("Could not create Content Page"));
    return nullptr;
  }
  MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Debug,
          ("Created Content Page"));

  if (promise) {
    promise->AddCallbacksWithCycleCollectedArgs(
        [=](JSContext*, JS::Handle<JS::Value>, mozilla::ErrorResult&) {
          voidPromise->Resolve(void_t(), __func__);
        },
        [=](JSContext*, JS::Handle<JS::Value>, mozilla::ErrorResult&) {
          voidPromise->Reject(void_t(), __func__);
        });
  } else {
    MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Error,
            ("Did not get a Promise back from ContentPageStuff"));
    voidPromise->Reject(void_t(), __func__);
  }

  return voidPromise;
}

void PopulateCSSProperties() {
  glean::characteristics::prefers_reduced_transparency.Set(
      LookAndFeel::GetInt(LookAndFeel::IntID::PrefersReducedTransparency));
  glean::characteristics::prefers_reduced_motion.Set(
      LookAndFeel::GetInt(LookAndFeel::IntID::PrefersReducedMotion));
  glean::characteristics::inverted_colors.Set(
      LookAndFeel::GetInt(LookAndFeel::IntID::InvertedColors));
  glean::characteristics::color_scheme.Set(
      (int)PreferenceSheet::ContentPrefs().mColorScheme);

  const auto& colors =
      PreferenceSheet::ContentPrefs().ColorsFor(ColorScheme::Light);

  StylePrefersContrast prefersContrast = [&colors] {
    // Replicates Gecko_MediaFeatures_PrefersContrast but without a Document
    if (!PreferenceSheet::ContentPrefs().mUseAccessibilityTheme &&
        PreferenceSheet::ContentPrefs().mUseDocumentColors) {
      return StylePrefersContrast::NoPreference;
    }

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

  glean::characteristics::use_document_colors.Set(
      PreferenceSheet::ContentPrefs().mUseDocumentColors);

  // These colors aren't using LookAndFeel, see Gecko_ComputeSystemColor.
  glean::characteristics::color_canvas.Set(colors.mDefaultBackground);
  glean::characteristics::color_canvastext.Set(colors.mDefault);

  // Similar to NS_TRANSPARENT and other special colors.
  constexpr nscolor kMissingColor = NS_RGBA(0x42, 0x00, 0x00, 0x00);

#define SYSTEM_COLOR(METRIC_NAME, COLOR_NAME)                                 \
  glean::characteristics::color_##METRIC_NAME.Set(                            \
      LookAndFeel::GetColor(LookAndFeel::ColorID::COLOR_NAME,                 \
                            ColorScheme::Light, LookAndFeel::UseStandins::No) \
          .valueOr(kMissingColor))

  SYSTEM_COLOR(accentcolor, Accentcolor);
  SYSTEM_COLOR(accentcolortext, Accentcolortext);
  SYSTEM_COLOR(highlight, Highlight);
  SYSTEM_COLOR(highlighttext, Highlighttext);
  SYSTEM_COLOR(selecteditem, Selecteditem);
  SYSTEM_COLOR(selecteditemtext, Selecteditemtext);

#undef SYSTEM_COLOR
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
  glean::characteristics::pixel_depth.Set(screen->GetPixelDepth());

  LayoutDeviceIntRect availRect = screen->GetAvailRect();
  glean::characteristics::avail_height.Set(availRect.Height());
  glean::characteristics::avail_width.Set(availRect.Width());
  glean::characteristics::orientation_angle.Set(screen->GetOrientationAngle());

  glean::characteristics::color_gamut.Set((int)colorGamut);
  glean::characteristics::color_depth.Set(colorDepth);
  const LayoutDeviceIntRect rect = screen->GetRect();
  glean::characteristics::screen_height.Set(rect.Height());
  glean::characteristics::screen_width.Set(rect.Width());

  nsCOMPtr<nsPIDOMWindowInner> innerWindow =
      do_QueryInterface(dom::GetEntryGlobal());

  double outerHeight, outerWidth;
  innerWindow->GetInnerHeight(&outerHeight);

  innerWindow->GetInnerWidth(&outerWidth);
  glean::characteristics::outer_height.Set(static_cast<int64_t>(outerHeight));
  glean::characteristics::outer_width.Set(static_cast<int64_t>(outerWidth));

  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  innerWindow->GetDocShell()->GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_QueryInterface(treeOwner));

  LayoutDeviceIntSize contentSize;
  treeOwner->GetPrimaryContentSize(&contentSize.width, &contentSize.height);

  CSSToLayoutDeviceScale cssToDevScale =
      treeOwnerAsWin->UnscaledDevicePixelsPerCSSPixel();
  CSSIntSize contentSizeCSS = RoundedToInt(contentSize / cssToDevScale);
  glean::characteristics::inner_height.Set(contentSizeCSS.height);
  glean::characteristics::inner_width.Set(contentSizeCSS.width);

  glean::characteristics::video_dynamic_range.Set(screen->GetIsHDR());

  glean::characteristics::posx.Set(rect.X());
  glean::characteristics::posy.Set(rect.Y());

  nsCOMPtr<nsIWidget> mainWidget;
  treeOwnerAsWin->GetMainWidget(getter_AddRefs(mainWidget));
  nsSizeMode sizeMode = mainWidget ? mainWidget->SizeMode() : nsSizeMode_Normal;
  glean::characteristics::size_mode.Set(sizeMode);

  mozilla::glean::characteristics::screen_orientation.Set(
      (int)screen->GetOrientationType());
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

  glean::characteristics::prefs_block_popups.Set(
      StaticPrefs::dom_disable_open_during_load());

  glean::characteristics::prefs_browser_display_use_document_fonts.Set(
      mozilla::StaticPrefs::browser_display_use_document_fonts());

  glean::characteristics::prefs_network_cookie_cookiebehavior.Set(
      StaticPrefs::network_cookie_cookieBehavior());
}

template <typename StringMetric, typename QuantityMetric>
static void CollectFontPrefValue(nsIPrefBranch* aPrefBranch,
                                 const nsACString& aDefaultLanguageGroup,
                                 const char* aStartingAt,
                                 StringMetric& aWesternMetric,
                                 StringMetric& aDefaultGroupMetric,
                                 QuantityMetric& aModifiedMetric) {
  nsTArray<nsCString> prefNames;
  if (NS_WARN_IF(
          NS_FAILED(aPrefBranch->GetChildList(aStartingAt, prefNames)))) {
    return;
  }

  nsCString westernPref(aStartingAt);
  westernPref.Append("x-western");
  nsCString defaultGroupPref(aStartingAt);
  defaultGroupPref.Append(aDefaultLanguageGroup);

  nsAutoCString westernPrefValue;
  Preferences::GetCString(westernPref.get(), westernPrefValue);
  aWesternMetric.Set(westernPrefValue);

  nsAutoCString defaultGroupPrefValue;
  if (!westernPref.Equals(defaultGroupPref)) {
    Preferences::GetCString(defaultGroupPref.get(), defaultGroupPrefValue);
  }
  aDefaultGroupMetric.Set(defaultGroupPrefValue);

  uint32_t modifiedCount = 0;
  for (const auto& prefName : prefNames) {
    if (!prefName.Equals(westernPref) && !prefName.Equals(defaultGroupPref)) {
      if (Preferences::HasUserValue(prefName.get())) {
        modifiedCount++;
      }
    }
  }
  aModifiedMetric.Set(modifiedCount);
}

template <typename QuantityMetric>
static void CollectFontPrefModified(nsIPrefBranch* aPrefBranch,
                                    const char* aStartingAt,
                                    QuantityMetric& aModifiedMetric) {
  nsTArray<nsCString> prefNames;
  if (NS_WARN_IF(
          NS_FAILED(aPrefBranch->GetChildList(aStartingAt, prefNames)))) {
    return;
  }

  uint32_t modifiedCount = 0;
  for (const auto& prefName : prefNames) {
    if (Preferences::HasUserValue(prefName.get())) {
      modifiedCount++;
    }
  }
  aModifiedMetric.Set(modifiedCount);
}

void PopulateFontPrefs() {
  nsIPrefBranch* prefRootBranch = Preferences::GetRootBranch();
  if (!prefRootBranch) {
    return;
  }

  nsCString defaultLanguageGroup;
  Preferences::GetLocalizedCString("font.language.group", defaultLanguageGroup);

#define FONT_PREF(PREF_NAME, METRIC_NAME)                                   \
  CollectFontPrefValue(prefRootBranch, defaultLanguageGroup, PREF_NAME,     \
                       glean::characteristics::METRIC_NAME##_western,       \
                       glean::characteristics::METRIC_NAME##_default_group, \
                       glean::characteristics::METRIC_NAME##_modified)

  // The following preferences can be modified using the advanced font options
  // on the about:preferences page. Every preference has a sub-branch per
  // script, so for example font.default.x-western or font.default.x-cyrillic
  // etc. For all of the 7 main preferences, we collect:
  // - The value for the x-western branch (if user modified)
  // - The value for the current default language group (~ script) based
  //   on the localized version of Firefox being used. (Only when not x-western)
  // - How many /other/ script that are not x-western or the default have been
  //   modified.

  FONT_PREF("font.default.", font_default);
  FONT_PREF("font.name.serif.", font_name_serif);
  FONT_PREF("font.name.sans-serif.", font_name_sans_serif);
  FONT_PREF("font.name.monospace.", font_name_monospace);
  FONT_PREF("font.size.variable.", font_size_variable);
  FONT_PREF("font.size.monospace.", font_size_monospace);
  FONT_PREF("font.minimum-size.", font_minimum_size);

#undef FONT_PREF

  CollectFontPrefModified(
      prefRootBranch, "font.name-list.serif.",
      glean::characteristics::font_name_list_serif_modified);
  CollectFontPrefModified(
      prefRootBranch, "font.name-list.sans-serif.",
      glean::characteristics::font_name_list_sans_serif_modified);
  CollectFontPrefModified(
      prefRootBranch, "font.name-list.monospace.",
      glean::characteristics::font_name_list_monospace_modified);
  CollectFontPrefModified(
      prefRootBranch, "font.name-list.cursive.",
      glean::characteristics::font_name_list_cursive_modified);
  // Exceptionally this pref has no variants per-script.
  glean::characteristics::font_name_list_emoji_modified.Set(
      Preferences::HasUserValue("font.name-list.emoji"));
}

void PopulateScaling() {
  nsCString output = "["_ns;

  auto& screenManager = widget::ScreenManager::GetSingleton();
  const auto& screens = screenManager.CurrentScreenList();
  for (const auto& screen : screens) {
    // Technically, not the same as (display resolution / shown resolution), but
    // this is the value the fingerprinters can access/compute.
    output.Append(std::to_string(screen->GetContentsScaleFactor()));
    if (&screen != &screens.LastElement()) {
      output.Append(",");
    }
  }

  output.Append("]");

  glean::characteristics::scalings.Set(output);
}

RefPtr<VoidPromise> PopulateMediaDevices() {
  RefPtr<VoidPromise> voidPromise = new VoidPromise(__func__);
  MediaManager::Get()->GetPhysicalDevices()->Then(
      GetCurrentSerialEventTarget(), __func__,
      [=](const RefPtr<const MediaManager::MediaDeviceSetRefCnt>& aDevices) {
        uint32_t cameraCount = 0;
        uint32_t microphoneCount = 0;
        uint32_t speakerCount = 0;
        std::set<nsString> groupIds;
        std::set<nsString> groupIdsWoSpeakers;

        for (const auto& device : *aDevices) {
          if (device->mKind == dom::MediaDeviceKind::Videoinput) {
            cameraCount++;
          } else if (device->mKind == dom::MediaDeviceKind::Audioinput) {
            microphoneCount++;
          } else if (device->mKind == dom::MediaDeviceKind::Audiooutput) {
            speakerCount++;
          }
          if (groupIds.find(device->mRawGroupID) == groupIds.end()) {
            groupIds.insert(device->mRawGroupID);
            if (device->mKind != dom::MediaDeviceKind::Audiooutput) {
              groupIdsWoSpeakers.insert(device->mRawGroupID);
            }
          }
        }

        nsCString json;
        json.AppendPrintf(
            R"({"cameraCount": %u, "microphoneCount": %u, "speakerCount": %u, "groupCount": %zu, "groupCountWoSpeakers": %zu})",
            cameraCount, microphoneCount, speakerCount, groupIds.size(),
            groupIdsWoSpeakers.size());
        glean::characteristics::media_devices.Set(json);
        voidPromise->Resolve(void_t(), __func__);
      },
      [=](RefPtr<MediaMgrError>&& reason) {
        voidPromise->Reject(void_t(), __func__);
      });
  return voidPromise;
}

RefPtr<VoidPromise> PopulateAudioDeviceProperties() {
  RefPtr<VoidPromise> voidPromise = new VoidPromise(__func__);

  NS_DispatchBackgroundTask(
      NS_NewRunnableFunction("PopulateAudioDeviceProperties", [=]() {
        RefPtr<CubebDeviceEnumerator> enumerator =
            CubebDeviceEnumerator::GetInstance();
        RefPtr<const CubebDeviceEnumerator::AudioDeviceSet> devices;

        nsCString output = "{"_ns;

        nsCString list = "["_ns;
        devices = enumerator->EnumerateAudioInputDevices();
        for (const auto& deviceInfo : *devices) {
          uint32_t maxChannels;
          deviceInfo->GetMaxChannels(&maxChannels);

          list.AppendPrintf(R"({"rate":%d,"channels":%d)",
                            deviceInfo->DefaultRate(), maxChannels);
          if (deviceInfo->Preferred()) {
            list.Append(",\"default\":1");
          }
          list.Append("}");

          if (&deviceInfo != &devices->LastElement()) {
            list.Append(',');
          }
        }
        list.Append(']');

        output.AppendPrintf(R"("devices":%s,)", list.get());

        double inputMean, inputStdDev, outputMean, outputStdDev;
        CubebUtils::EstimatedLatencyDefaultDevices(&inputMean, &inputStdDev,
                                                   CubebUtils::Side::Input);
        CubebUtils::EstimatedLatencyDefaultDevices(&outputMean, &outputStdDev,
                                                   CubebUtils::Side::Output);

        cubeb_stream_params output_params;
        output_params.format = CUBEB_SAMPLE_FLOAT32NE;
        output_params.rate = CubebUtils::PreferredSampleRate(false);
        output_params.channels = 2;
        output_params.layout = CUBEB_LAYOUT_UNDEFINED;
        output_params.prefs =
            CubebUtils::GetDefaultStreamPrefs(CUBEB_DEVICE_TYPE_OUTPUT);

        uint32_t latencyFrames =
            CubebUtils::GetCubebMTGLatencyInFrames(&output_params);
        RefPtr<AudioDeviceInfo> defaultOutputDevice =
            enumerator->DefaultDevice(CubebDeviceEnumerator::Side::OUTPUT);
        output.AppendPrintf(
            R"("latency":[%f,%f,%f,%f],"latFrames":%d,"rate":%u,"channels":%u)",
            inputMean, inputStdDev, outputMean, outputStdDev, latencyFrames,
            defaultOutputDevice->DefaultRate(),
            defaultOutputDevice->MaxChannels());

        output.Append("}");

        glean::characteristics::audio_devices.Set(output);

        NS_DispatchToMainThread(NS_NewRunnableFunction(
            "PopulateAudioDeviceProperties",
            [=]() { voidPromise->Resolve(void_t(), __func__); }));
      }));

  return voidPromise;
}

void PopulateLanguages() {
  // All navigator.languages, navigator.language, and Accept-Languages header
  // use Navigator::GetAcceptLanguages to create a language list. It is
  // sufficient to only collect this information as the other properties are
  // just reformats of Navigator::GetAcceptLanguages.
  nsTArray<nsString> languages;
  dom::Navigator::GetAcceptLanguages(languages);
  nsCString output = "["_ns;

  for (const auto& language : languages) {
    output.AppendPrintf(R"("%s")", NS_ConvertUTF16toUTF8(language).get());

    if (&language != &languages.LastElement()) {
      output.Append(",");
    }
  }

  output.Append("]");

  glean::characteristics::languages.Set(output);
}

void PopulateTextAntiAliasing() {
  nsCString output = "["_ns;
  nsTArray<int32_t> levels;

#if defined(XP_WIN)
  nsTArray<ClearTypeParameterInfo> params;
  gfxWindowsPlatform::GetCleartypeParams(params);
  for (const auto& param : params) {
    levels.AppendElement(param.clearTypeLevel);
  }
#elif defined(XP_MACOSX)
  uint32_t value = 2;  // default = medium
  CFNumberRef prefValue = (CFNumberRef)CFPreferencesCopyAppValue(
      CFSTR("AppleFontSmoothing"), kCFPreferencesAnyApplication);
  if (prefValue) {
    if (!CFNumberGetValue(prefValue, kCFNumberIntType, &value)) {
      value = 2;
    }
    CFRelease(prefValue);
  }
  levels.AppendElement(value);
#elif defined(XP_LINUX)
  nsAutoCString level;
  nsCOMPtr<nsIGSettingsService> gsettings =
      do_GetService("@mozilla.org/gsettings-service;1");
  if (gsettings) {
    nsCOMPtr<nsIGSettingsCollection> antiAliasing;
    gsettings->GetCollectionForSchema("org.gnome.desktop.interface"_ns,
                                      getter_AddRefs(antiAliasing));
    if (antiAliasing) {
      antiAliasing->GetString("font-antialiasing"_ns, level);
      if (level == "rgba") {  // Subpixel
        levels.AppendElement(2);
      } else if (level == "grayscale") {  // Standard
        levels.AppendElement(1);
      } else if (level == "none") {
        levels.AppendElement(0);
      }
    }
  }
#endif

  for (const auto& level : levels) {
    output.Append(std::to_string(level));

    if (&level != &levels.LastElement()) {
      output.Append(",");
    }
  }

  output.Append("]");

  glean::characteristics::text_anti_aliasing.Set(output);
}

// ==================================================================
// The current schema of the data. Anytime you add a metric, or change how a
// metric is set, this variable should be incremented. It'll be a lot. It's
// okay. We're going to need it to know (including during development) what is
// the source of the data we are looking at.
const int kSubmissionSchema = 3;

const auto* const kLastVersionPref =
    "toolkit.telemetry.user_characteristics_ping.last_version_sent";
const auto* const kCurrentVersionPref =
    "toolkit.telemetry.user_characteristics_ping.current_version";

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
    PopulateDataAndEventuallySubmit(false);
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
    MOZ_LOG(gUserCharacteristicsLog, LogLevel::Warning, ("Ping requested"));
    PopulateDataAndEventuallySubmit(true);
  } else {
    MOZ_ASSERT_UNREACHABLE("Should never reach here");
  }
}

const auto* const kUUIDPref =
    "toolkit.telemetry.user_characteristics_ping.uuid";

/* static */
void nsUserCharacteristics::PopulateDataAndEventuallySubmit(
    bool aUpdatePref /* = true */, bool aTesting /* = false */
) {
  MOZ_LOG(gUserCharacteristicsLog, LogLevel::Warning, ("Populating Data"));
  MOZ_ASSERT(XRE_IsParentProcess());

  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (!obs) {
    return;
  }

  // This notification tells us to register the actor
  obs->NotifyObservers(nullptr, "user-characteristics-populating-data",
                       nullptr);

  glean::characteristics::submission_schema.Set(kSubmissionSchema);

  nsAutoCString uuidString;
  nsresult rv = Preferences::GetCString(kUUIDPref, uuidString);
  if (NS_FAILED(rv) || uuidString.Length() == 0) {
    nsCOMPtr<nsIUUIDGenerator> uuidgen =
        do_GetService("@mozilla.org/uuid-generator;1", &rv);
    if (NS_FAILED(rv)) {
      return;
    }

    nsIDToCString id(nsID::GenerateUUID());
    uuidString = id.get();
    Preferences::SetCString(kUUIDPref, uuidString);
  }

  glean::characteristics::client_identifier.Set(uuidString);

  glean::characteristics::max_touch_points.Set(testing::MaxTouchPoints());

  // ------------------------------------------------------------------------

  nsTArray<RefPtr<MozPromise<mozilla::void_t, mozilla::void_t, true>>> promises;
  if (!aTesting) {
    // Many of the later peices of data do not work in a gtest
    // so skip populating them

    // ------------------------------------------------------------------------

    PopulateMissingFonts();
    PopulateCSSProperties();
    PopulateScreenProperties();
    PopulatePrefs();
    PopulateFontPrefs();
    PopulateScaling();
    promises.AppendElement(PopulateMediaDevices());
    promises.AppendElement(PopulateAudioDeviceProperties());
    PopulateLanguages();
    PopulateTextAntiAliasing();

    glean::characteristics::target_frame_rate.Set(
        gfxPlatform::TargetFrameRate());

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
  }

  promises.AppendElement(ContentPageStuff());

  // ------------------------------------------------------------------------

  auto fulfillSteps = [aUpdatePref, aTesting]() {
    MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Debug,
            ("ContentPageStuff Promise Resolved"));

    if (!aTesting) {
      nsUserCharacteristics::SubmitPing();
    }

    if (aUpdatePref) {
      MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Debug,
              ("Updating preference"));
      auto current_version =
          mozilla::Preferences::GetInt(kCurrentVersionPref, 0);
      mozilla::Preferences::SetInt(kLastVersionPref, current_version);
    }
  };

  VoidPromise::All(GetCurrentSerialEventTarget(), promises)
      ->Then(
          GetCurrentSerialEventTarget(), __func__, [=]() { fulfillSteps(); },
          []() {
            MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Error,
                    ("One of the promises rejected."));
          });
}

/* static */
void nsUserCharacteristics::SubmitPing() {
  MOZ_LOG(gUserCharacteristicsLog, mozilla::LogLevel::Warning,
          ("Submitting Ping"));
  glean_pings::UserCharacteristics.Submit();
}
