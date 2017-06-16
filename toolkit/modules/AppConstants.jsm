#filter substitution
#include @TOPOBJDIR@/source-repo.h
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services", "resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["AppConstants"];

// Immutable for export.
this.AppConstants = Object.freeze({
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

  MOZ_CONTENT_SANDBOX:
#ifdef MOZ_CONTENT_SANDBOX
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

# MOZ_B2G covers both device and desktop b2g
  MOZ_B2G:
#ifdef MOZ_B2G
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

# NOTE! XP_LINUX has to go after MOZ_WIDGET_ANDROID otherwise Android
# builds will be misidentified as linux.
  platform:
#ifdef MOZ_WIDGET_GTK
  "linux",
#elif XP_WIN
  "win",
#elif XP_MACOSX
  "macosx",
#elif MOZ_WIDGET_ANDROID
  "android",
#elif MOZ_WIDGET_GONK
  "gonk",
#elif XP_LINUX
  "linux",
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

  MOZ_MAINTENANCE_SERVICE:
#ifdef MOZ_MAINTENANCE_SERVICE
  true,
#else
  false,
#endif

  E10S_TESTING_ONLY:
#ifdef E10S_TESTING_ONLY
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

  MOZ_B2G_RIL:
#ifdef MOZ_B2G_RIL
  true,
#else
  false,
#endif

  MOZ_GRAPHENE:
#ifdef MOZ_GRAPHENE
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

  MOZ_ADDON_SIGNING:
#ifdef MOZ_ADDON_SIGNING
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

  MOZ_ALLOW_LEGACY_EXTENSIONS:
#ifdef MOZ_ALLOW_LEGACY_EXTENSIONS
  true,
#else
  false,
#endif

  INSTALL_COMPACT_THEMES:
#ifdef INSTALL_COMPACT_THEMES
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

  CAN_DRAW_IN_TITLEBAR:
#ifdef CAN_DRAW_IN_TITLEBAR
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

  MOZ_TOOLKIT_SEARCH:
#ifdef MOZ_TOOLKIT_SEARCH
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

  MOZ_ANDROID_ACTIVITY_STREAM:
#ifdef MOZ_ANDROID_ACTIVITY_STREAM
  true,
#else
  false,
#endif

  MOZ_ANDROID_MOZILLA_ONLINE:
#ifdef MOZ_ANDROID_MOZILLA_ONLINE
  true,
#else
  false,
#endif

  DLL_PREFIX: "@DLL_PREFIX@",
  DLL_SUFFIX: "@DLL_SUFFIX@",

  MOZ_APP_NAME: "@MOZ_APP_NAME@",
  MOZ_APP_VERSION: "@MOZ_APP_VERSION@",
  MOZ_APP_VERSION_DISPLAY: "@MOZ_APP_VERSION_DISPLAY@",
  MOZ_BUILD_APP: "@MOZ_BUILD_APP@",
  MOZ_MACBUNDLE_NAME: "@MOZ_MACBUNDLE_NAME@",
  MOZ_UPDATE_CHANNEL: "@MOZ_UPDATE_CHANNEL@",
  INSTALL_LOCALE: "@AB_CD@",
  MOZ_WIDGET_TOOLKIT: "@MOZ_WIDGET_TOOLKIT@",
  ANDROID_PACKAGE_NAME: "@ANDROID_PACKAGE_NAME@",
  MOZ_B2G_VERSION: @MOZ_B2G_VERSION@,
  MOZ_B2G_OS_NAME: @MOZ_B2G_OS_NAME@,

  DEBUG_JS_MODULES: "@DEBUG_JS_MODULES@",

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

  MOZ_PHOTON_ANIMATIONS:
#ifdef MOZ_PHOTON_ANIMATIONS
    true,
#else
    false,
#endif

  MOZ_PHOTON_THEME:
#ifdef MOZ_PHOTON_THEME
    true,
#else
    false,
#endif

});
