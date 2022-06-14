#filter substitution
#include @TOPOBJDIR@/source-repo.h
#include @TOPOBJDIR@/buildid.h
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const lazy = {};
const { Services } = ChromeUtils.import(
  "resource://gre/modules/Services.jsm"
);
ChromeUtils.defineModuleGetter(lazy, "AddonManager", "resource://gre/modules/AddonManager.jsm");

var EXPORTED_SYMBOLS = ["AppConstants"];

// Immutable for export.
var AppConstants = Object.freeze({
  // See this wiki page for more details about channel specific build
  // defines: https://wiki.mozilla.org/Platform/Channel-specific_build_defines
  NIGHTLY_BUILD:
#ifdef NIGHTLY_BUILD
  true,
#else
  false,
#endif

  RELEASE_OR_BETA:
#ifdef RELEASE_OR_BETA
  true,
#else
  false,
#endif

  EARLY_BETA_OR_EARLIER:
#ifdef EARLY_BETA_OR_EARLIER
  true,
#else
  false,
#endif

  IS_ESR:
#ifdef MOZ_ESR
  true,
#else
  false,
#endif

  ACCESSIBILITY:
#ifdef ACCESSIBILITY
  true,
#else
  false,
#endif

  // Official corresponds, roughly, to whether this build is performed
  // on Mozilla's continuous integration infrastructure. You should
  // disable developer-only functionality when this flag is set.
  MOZILLA_OFFICIAL:
#ifdef MOZILLA_OFFICIAL
  true,
#else
  false,
#endif

  MOZ_OFFICIAL_BRANDING:
#ifdef MOZ_OFFICIAL_BRANDING
  true,
#else
  false,
#endif

  MOZ_DEV_EDITION:
#ifdef MOZ_DEV_EDITION
  true,
#else
  false,
#endif

  MOZ_SERVICES_SYNC:
#ifdef MOZ_SERVICES_SYNC
  true,
#else
  false,
#endif

  MOZ_SERVICES_HEALTHREPORT:
#ifdef MOZ_SERVICES_HEALTHREPORT
  true,
#else
  false,
#endif

  MOZ_DATA_REPORTING:
#ifdef MOZ_DATA_REPORTING
  true,
#else
  false,
#endif

  MOZ_SANDBOX:
#ifdef MOZ_SANDBOX
  true,
#else
  false,
#endif

  MOZ_TELEMETRY_REPORTING:
#ifdef MOZ_TELEMETRY_REPORTING
  true,
#else
  false,
#endif

  MOZ_TELEMETRY_ON_BY_DEFAULT:
#ifdef MOZ_TELEMETRY_ON_BY_DEFAULT
  true,
#else
  false,
#endif

  MOZ_UPDATER:
#ifdef MOZ_UPDATER
  true,
#else
  false,
#endif

  MOZ_SWITCHBOARD:
#ifdef MOZ_SWITCHBOARD
  true,
#else
  false,
#endif

  MOZ_WEBRTC:
#ifdef MOZ_WEBRTC
  true,
#else
  false,
#endif

  MOZ_WIDGET_GTK:
#ifdef MOZ_WIDGET_GTK
  true,
#else
  false,
#endif

  XP_UNIX:
#ifdef XP_UNIX
  true,
#else
  false,
#endif

// NOTE! XP_LINUX has to go after MOZ_WIDGET_ANDROID otherwise Android
// builds will be misidentified as linux.
  platform:
#ifdef MOZ_WIDGET_GTK
  "linux",
#elif XP_WIN
  "win",
#elif XP_MACOSX
  "macosx",
#elif MOZ_WIDGET_ANDROID
  "android",
#elif XP_LINUX
  "linux",
#else
  "other",
#endif

// Most of our frontend code assumes that any desktop Unix platform
// is "linux". Add the distinction for code that needs it.
  unixstyle:
#ifdef XP_LINUX
    "linux",
#elif XP_OPENBSD
    "openbsd",
#elif XP_NETBSD
    "netbsd",
#elif XP_FREEBSD
    "freebsd",
#elif XP_SOLARIS
    "solaris",
#else
    "other",
#endif

  isPlatformAndVersionAtLeast(platform, version) {
    let platformVersion = Services.sysinfo.getProperty("version");
    return platform == this.platform &&
           Services.vc.compare(platformVersion, version) >= 0;
  },

  isPlatformAndVersionAtMost(platform, version) {
    let platformVersion = Services.sysinfo.getProperty("version");
    return platform == this.platform &&
           Services.vc.compare(platformVersion, version) <= 0;
  },

  MOZ_CRASHREPORTER:
#ifdef MOZ_CRASHREPORTER
  true,
#else
  false,
#endif

  MOZ_NORMANDY:
#ifdef MOZ_NORMANDY
  true,
#else
  false,
#endif

  MOZ_MAINTENANCE_SERVICE:
#ifdef MOZ_MAINTENANCE_SERVICE
  true,
#else
  false,
#endif

  MOZ_BACKGROUNDTASKS:
#ifdef MOZ_BACKGROUNDTASKS
  true,
#else
  false,
#endif

  MOZ_UPDATE_AGENT:
#ifdef MOZ_UPDATE_AGENT
  true,
#else
  false,
#endif

  MOZ_BITS_DOWNLOAD:
#ifdef MOZ_BITS_DOWNLOAD
  true,
#else
  false,
#endif

  DEBUG:
#ifdef DEBUG
  true,
#else
  false,
#endif

  ASAN:
#ifdef MOZ_ASAN
  true,
#else
  false,
#endif

  ASAN_REPORTER:
#ifdef MOZ_ASAN_REPORTER
  true,
#else
  false,
#endif

  TSAN:
#ifdef MOZ_TSAN
  true,
#else
  false,
#endif

  MOZ_SYSTEM_NSS:
#ifdef MOZ_SYSTEM_NSS
  true,
#else
  false,
#endif

  MOZ_PLACES:
#ifdef MOZ_PLACES
  true,
#else
  false,
#endif

  MOZ_REQUIRE_SIGNING:
#ifdef MOZ_REQUIRE_SIGNING
  true,
#else
  false,
#endif

  get MOZ_UNSIGNED_SCOPES() {
    let result = 0;
#ifdef MOZ_UNSIGNED_APP_SCOPE
    result |= lazy.AddonManager.SCOPE_APPLICATION;
#endif
#ifdef MOZ_UNSIGNED_SYSTEM_SCOPE
    result |= lazy.AddonManager.SCOPE_SYSTEM;
#endif
    return result;
  },

  MOZ_ALLOW_ADDON_SIDELOAD:
#ifdef MOZ_ALLOW_ADDON_SIDELOAD
  true,
#else
  false,
#endif

  MOZ_WEBEXT_WEBIDL_ENABLED:
#ifdef MOZ_WEBEXT_WEBIDL_ENABLED
  true,
#else
  false,
#endif

  MENUBAR_CAN_AUTOHIDE:
#ifdef MENUBAR_CAN_AUTOHIDE
  true,
#else
  false,
#endif

  MOZ_ANDROID_HISTORY:
#ifdef MOZ_ANDROID_HISTORY
  true,
#else
  false,
#endif

  MOZ_GECKO_PROFILER:
#ifdef MOZ_GECKO_PROFILER
  true,
#else
  false,
#endif

  DLL_PREFIX: "@DLL_PREFIX@",
  DLL_SUFFIX: "@DLL_SUFFIX@",

  MOZ_APP_NAME: "@MOZ_APP_NAME@",
  MOZ_APP_BASENAME: "@MOZ_APP_BASENAME@",
  // N.b.: you almost certainly want brandShortName/brand-short-name:
  // MOZ_APP_DISPLAYNAME should only be used for static user-visible
  // fields (e.g., DLL properties, Mac Bundle name, or similar).
  MOZ_APP_DISPLAYNAME_DO_NOT_USE: "@MOZ_APP_DISPLAYNAME@",
  MOZ_APP_VERSION: "@MOZ_APP_VERSION@",
  MOZ_APP_VERSION_DISPLAY: "@MOZ_APP_VERSION_DISPLAY@",
  MOZ_BUILDID: "@MOZ_BUILDID@",
  MOZ_BUILD_APP: "@MOZ_BUILD_APP@",
  MOZ_MACBUNDLE_ID: "@MOZ_MACBUNDLE_ID@",
  MOZ_MACBUNDLE_NAME: "@MOZ_MACBUNDLE_NAME@",
  MOZ_UPDATE_CHANNEL: "@MOZ_UPDATE_CHANNEL@",
  MOZ_WIDGET_TOOLKIT: "@MOZ_WIDGET_TOOLKIT@",
  ANDROID_PACKAGE_NAME: "@ANDROID_PACKAGE_NAME@",

  DEBUG_JS_MODULES: "@DEBUG_JS_MODULES@",

  MOZ_BING_API_CLIENTID: "@MOZ_BING_API_CLIENTID@",
  MOZ_BING_API_KEY: "@MOZ_BING_API_KEY@",
  MOZ_GOOGLE_LOCATION_SERVICE_API_KEY: "@MOZ_GOOGLE_LOCATION_SERVICE_API_KEY@",
  MOZ_GOOGLE_SAFEBROWSING_API_KEY: "@MOZ_GOOGLE_SAFEBROWSING_API_KEY@",
  MOZ_MOZILLA_API_KEY: "@MOZ_MOZILLA_API_KEY@",

  BROWSER_CHROME_URL: "@BROWSER_CHROME_URL@",

  OMNIJAR_NAME: "@OMNIJAR_NAME@",

  // URL to the hg revision this was built from (e.g.
  // "https://hg.mozilla.org/mozilla-central/rev/6256ec9113c1")
  // On unofficial builds, this is an empty string.
#ifndef MOZ_SOURCE_URL
#define MOZ_SOURCE_URL
#endif
  SOURCE_REVISION_URL: "@MOZ_SOURCE_URL@",

  HAVE_USR_LIB64_DIR:
#ifdef HAVE_USR_LIB64_DIR
    true,
#else
    false,
#endif

  HAVE_SHELL_SERVICE:
#ifdef HAVE_SHELL_SERVICE
    true,
#else
    false,
#endif

  MOZ_CODE_COVERAGE:
#ifdef MOZ_CODE_COVERAGE
    true,
#else
    false,
#endif

  TELEMETRY_PING_FORMAT_VERSION: @TELEMETRY_PING_FORMAT_VERSION@,

  MOZ_NEW_XULSTORE:
#ifdef MOZ_NEW_XULSTORE
    true,
#else
    false,
#endif

  MOZ_NEW_NOTIFICATION_STORE:
#ifdef MOZ_NEW_NOTIFICATION_STORE
    true,
#else
    false,
#endif

  ENABLE_WEBDRIVER:
#ifdef ENABLE_WEBDRIVER
    true,
#else
    false,
#endif

  REMOTE_SETTINGS_SERVER_URL:
#ifdef MOZ_THUNDERBIRD
    "https://thunderbird-settings.thunderbird.net/v1",
#else
    "https://firefox.settings.services.mozilla.com/v1",
#endif

  REMOTE_SETTINGS_VERIFY_SIGNATURE:
#ifdef MOZ_THUNDERBIRD
    false,
#else
    true,
#endif

  REMOTE_SETTINGS_DEFAULT_BUCKET:
#ifdef MOZ_THUNDERBIRD
    "thunderbird",
#else
    "main",
#endif

  MOZ_GLEAN_ANDROID:
#ifdef MOZ_GLEAN_ANDROID
    true,
#else
    false,
#endif

  MOZ_JXL:
#ifdef MOZ_JXL
    true,
#else
    false,
#endif

  MOZ_CAN_FOLLOW_SYSTEM_TIME:
#ifdef XP_WIN
    true,
#elif XP_MACOSX
    true,
#elif MOZ_WIDGET_GTK
  #ifdef MOZ_ENABLE_DBUS
    true,
  #else
    false,
  #endif
#else
    false,
#endif

  // Returns true for CN region build when distibution id set as 'MozillaOnline'
  isChinaRepack() {
    return (
      Services.prefs
      .getDefaultBranch("")
      .getCharPref("distribution.id", "default") === "MozillaOnline"
    );
  },
});
