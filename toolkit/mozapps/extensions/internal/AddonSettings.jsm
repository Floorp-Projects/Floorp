/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["AddonSettings"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { AddonManager } = ChromeUtils.import(
  "resource://gre/modules/AddonManager.jsm"
);

const PREF_SIGNATURES_REQUIRED = "xpinstall.signatures.required";
const PREF_LANGPACK_SIGNATURES = "extensions.langpacks.signatures.required";
const PREF_ALLOW_EXPERIMENTS = "extensions.experiments.enabled";
const PREF_EM_SIDELOAD_SCOPES = "extensions.sideloadScopes";
const PREF_IS_EMBEDDED = "extensions.isembedded";
const PREF_UPDATE_REQUIREBUILTINCERTS = "extensions.update.requireBuiltInCerts";
const PREF_INSTALL_REQUIREBUILTINCERTS =
  "extensions.install.requireBuiltInCerts";

var AddonSettings = {};

// Make a non-changable property that can't be manipulated from other
// code in the app.
function makeConstant(name, value) {
  Object.defineProperty(AddonSettings, name, {
    configurable: false,
    enumerable: false,
    writable: false,
    value,
  });
}

if (AppConstants.MOZ_REQUIRE_SIGNING && !Cu.isInAutomation) {
  makeConstant("REQUIRE_SIGNING", true);
  makeConstant("LANGPACKS_REQUIRE_SIGNING", true);
} else {
  XPCOMUtils.defineLazyPreferenceGetter(
    AddonSettings,
    "REQUIRE_SIGNING",
    PREF_SIGNATURES_REQUIRED,
    false
  );
  XPCOMUtils.defineLazyPreferenceGetter(
    AddonSettings,
    "LANGPACKS_REQUIRE_SIGNING",
    PREF_LANGPACK_SIGNATURES,
    false
  );
}

/**
 * Require the use of certs shipped with Firefox for
 * addon install and update, if the distribution does
 * not require addon signing and is not ESR.
 */
XPCOMUtils.defineLazyPreferenceGetter(
  AddonSettings,
  "INSTALL_REQUIREBUILTINCERTS",
  PREF_INSTALL_REQUIREBUILTINCERTS,
  !AppConstants.MOZ_REQUIRE_SIGNING &&
    !AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")
);

XPCOMUtils.defineLazyPreferenceGetter(
  AddonSettings,
  "UPDATE_REQUIREBUILTINCERTS",
  PREF_UPDATE_REQUIREBUILTINCERTS,
  !AppConstants.MOZ_REQUIRE_SIGNING &&
    !AppConstants.MOZ_APP_VERSION_DISPLAY.endsWith("esr")
);

// Whether or not we're running in GeckoView embedded in an Android app
if (Cu.isInAutomation) {
  XPCOMUtils.defineLazyPreferenceGetter(
    AddonSettings,
    "IS_EMBEDDED",
    PREF_IS_EMBEDDED,
    false
  );
} else {
  makeConstant("IS_EMBEDDED", AppConstants.platform === "android");
}

/**
 * AddonSettings.EXPERIMENTS_ENABLED
 *
 * Experimental APIs are always available to privileged signed addons.
 * This constant makes an optional preference available to enable experimental
 * APIs for developement purposes.
 *
 * Two features are toggled with this preference:
 *
 *   1. The ability to load an extension that contains an experimental
 *      API but is not privileged.
 *   2. The ability to load an unsigned extension that gains privilege
 *      if it is temporarily loaded (e.g. via about:debugging).
 *
 * MOZ_REQUIRE_SIGNING is set to zero in unbranded builds, we also
 * ensure nightly, dev-ed and our test infrastructure have access to
 * the preference.
 *
 * Official releases ignore this preference.
 */
if (
  !AppConstants.MOZ_REQUIRE_SIGNING ||
  AppConstants.NIGHTLY_BUILD ||
  AppConstants.MOZ_DEV_EDITION ||
  Cu.isInAutomation
) {
  XPCOMUtils.defineLazyPreferenceGetter(
    AddonSettings,
    "EXPERIMENTS_ENABLED",
    PREF_ALLOW_EXPERIMENTS,
    true
  );
} else {
  makeConstant("EXPERIMENTS_ENABLED", false);
}

if (AppConstants.MOZ_DEV_EDITION) {
  makeConstant("DEFAULT_THEME_ID", "firefox-compact-dark@mozilla.org");
} else {
  makeConstant("DEFAULT_THEME_ID", "default-theme@mozilla.org");
}

// SCOPES_SIDELOAD is a bitflag for what scopes we will load new extensions from when we scan the directories.
// If a build allows sideloading, or we're in automation, we'll also allow use of the preference.
if (AppConstants.MOZ_ALLOW_ADDON_SIDELOAD || Cu.isInAutomation) {
  XPCOMUtils.defineLazyPreferenceGetter(
    AddonSettings,
    "SCOPES_SIDELOAD",
    PREF_EM_SIDELOAD_SCOPES,
    AppConstants.MOZ_ALLOW_ADDON_SIDELOAD
      ? AddonManager.SCOPE_ALL
      : AddonManager.SCOPE_PROFILE
  );
} else {
  makeConstant("SCOPES_SIDELOAD", AddonManager.SCOPE_PROFILE);
}
