/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

this.EXPORTED_SYMBOLS = [ "AddonSettings" ];

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/AppConstants.jsm");

const PREF_SIGNATURES_REQUIRED = "xpinstall.signatures.required";
const PREF_ALLOW_LEGACY = "extensions.legacy.enabled";

this.AddonSettings = {};

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

makeConstant("ADDON_SIGNING", AppConstants.MOZ_ADDON_SIGNING);

if (AppConstants.MOZ_REQUIRE_SIGNING && !Cu.isInAutomation) {
  makeConstant("REQUIRE_SIGNING", true);
} else {
  XPCOMUtils.defineLazyPreferenceGetter(AddonSettings, "REQUIRE_SIGNING",
                                        PREF_SIGNATURES_REQUIRED, false);
}

if (AppConstants.MOZ_ALLOW_LEGACY_EXTENSIONS || Cu.isInAutomation) {
  XPCOMUtils.defineLazyPreferenceGetter(AddonSettings, "ALLOW_LEGACY_EXTENSIONS",
                                        PREF_ALLOW_LEGACY, true);
} else {
  makeConstant("ALLOW_LEGACY_EXTENSIONS", false);
}
