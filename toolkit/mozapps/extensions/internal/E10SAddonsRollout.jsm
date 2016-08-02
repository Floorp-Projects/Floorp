/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "isAddonPartOfE10SRollout" ];

const Cu = Components.utils;
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const PREF_E10S_ADDON_BLOCKLIST = "extensions.e10s.rollout.blocklist";
const PREF_E10S_ADDON_POLICY    = "extensions.e10s.rollout.policy";

const ADDONS = {
  "Greasemonkey": { // Greasemonkey
    id: "{e4a8a97b-f2ed-450b-b12d-ee082ba24781}", minVersion: "3.8",
  },

  "DYTV": { // Download YouTube Videos as MP4
    id: "{b9bfaf1c-a63f-47cd-8b9a-29526ced9060}", minVersion: "1.8.7",
  },

  "VDH": { // Video Download Helper
    id: "{b9db16a4-6edc-47ec-a1f4-b86292ed211d}", minVersion: "5.6.1",
  },

  "Lightbeam": { // Lightbeam
    id: "jid1-F9UJ2thwoAm5gQ@jetpack", minVersion: "1.3.0.1",
  },

  "ABP": { // Adblock Plus
    id: "{d10d0bf8-f5b5-c8b4-a8b2-2b9879e08c5d}", minVersion: "2.7.3",
  },

  "uBlockOrigin": { // uBlock Origin
    id: "uBlock0@raymondhill.net", minVersion: "1.7.6",
  },

  "Emoji": { // Emoji Cheatsheet
    id: "jid1-Xo5SuA6qc1DFpw@jetpack", minVersion: "1.1.1",
  },

  "ASP": { // Awesome Screenshot Plus
    id: "jid0-GXjLLfbCoAx0LcltEdFrEkQdQPI@jetpack", minVersion: "3.0.10",
  },

  "PersonasPlus": {
    id: "personas@christopher.beard", minVersion: "1.8.0",
  },

  // Add-ons used for testing
  "test1": {
    id: "bootstrap1@tests.mozilla.org", minVersion: "1.0",
  },

  "test2": {
    id: "bootstrap2@tests.mozilla.org", minVersion: "1.0",
  },
};

// NOTE: Do not modify sets or policies after they have already been
// published to users. They must remain unchanged to provide valid data.
const set1 = [ADDONS.Emoji,
              ADDONS.ASP,
              ADDONS.DYTV];

const set2 = [ADDONS.Greasemonkey,
              ADDONS.DYTV,
              ADDONS.VDH,
              ADDONS.Lightbeam,
              ADDONS.ABP,
              ADDONS.uBlockOrigin,
              ADDONS.Emoji,
              ADDONS.ASP,
              ADDONS.PersonasPlus];

// We use these named policies to correlate the telemetry
// data with them, in order to understand how each set
// is behaving in the wild.
const RolloutPolicy = {
  "1a": { addons: set1, webextensions: true },
  "2a": { addons: set2, webextensions: true },

  "1b": { addons: set1, webextensions: false },
  "2b": { addons: set2, webextensions: false },

  "xpcshell-test": { addons: [ADDONS.test1, ADDONS.test2], webextensions: false },
};

Object.defineProperty(this, "isAddonPartOfE10SRollout", {
  configurable: false,
  enumerable: false,
  writable: false,
  value: function isAddonPartOfE10SRollout(aAddon) {
    let blocklist = Preferences.get(PREF_E10S_ADDON_BLOCKLIST, "");
    let policyId = Preferences.get(PREF_E10S_ADDON_POLICY, "");

    if (!policyId || !RolloutPolicy.hasOwnProperty(policyId)) {
      return false;
    }

    if (blocklist && blocklist.indexOf(aAddon.id) > -1) {
      return false;
    }

    let policy = RolloutPolicy[policyId];

    if (policy.webextensions && aAddon.type == "webextension") {
      return true;
    }

    for (let rolloutAddon of policy.addons) {
      if (aAddon.id == rolloutAddon.id &&
          Services.vc.compare(aAddon.version, rolloutAddon.minVersion) >= 0) {
        return true;
      }
    }

    return false;
  },
});
