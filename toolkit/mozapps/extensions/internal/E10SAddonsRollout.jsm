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

  "PersonasPlus": { // PersonasPlus
    id: "personas@christopher.beard", minVersion: "1.8.0",
  },

  "ACR": { // Add-on Compatibility Reporter
    id: "compatibility@addons.mozilla.org", minVersion: "2.2.0",
  },

  // Test Pilot add-ons for Firefox 49
  "TestPilot": { // Test Pilot Addon
    id: "@testpilot-addon", minVersion: "0.8.5",
  },

  "TabCenter": { // Tab Center
    id: "tabcentertest1@mozilla.com", minVersion: "1.24",
  },

  "ActivityStream": { // Activity Stream
    id: "@activity-streams", minVersion: "1.1.3",
  },

  "PageShot": { // Page Shot
    id: "jid1-NeEaf3sAHdKHPA@jetpack", minVersion: "0.0.1",
  },

  "MinVid": { // Min Vid
    id: "@min-vid", minVersion: "0.0.1",
  },

  "TrackingProtection": { // Tracking Protection Web Extension
    id: "blok@mozilla.org", minVersion: "1.0.0",
  },

  "NoMore404s": { // No More 404s Web Extension
    id: "wayback_machine@mozilla.org", minVersion: "1.5.6",
  },

  "UniversalSearch": { // Universal Search Extension
    id: "universal-search@mozilla.com", minVersion: "1.0.10",
  },

  // (ENDS) Test Pilot add-ons for Firefox 49

  "YouTubeHD": {
    id: "{7b1bf0b6-a1b9-42b0-b75d-252036438bdc}", minVersion: "48.3",
  },

  "WeatherUnderground": {
    id: "jid1-w3xH9kJhd3KJUp@jetpack", minVersion: "0.2.1",
  },

  "DictionaryAnywhere": {
    id: "jid0-fbHwsGfb6kJyq2hj65KnbGte3yT@jetpack", minVersion: "0.1.9",
  },

  "GmailNotifier": {
    id: "jid0-GjwrPchS3Ugt7xydvqVK4DQk8Ls@jetpack", minVersion: "0.6.5",
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

// Set 2 used during 48 Beta cycle. Kept here for historical reasons.
const set2 = [ADDONS.Greasemonkey,
              ADDONS.DYTV,
              ADDONS.VDH,
              ADDONS.Lightbeam,
              ADDONS.ABP,
              ADDONS.uBlockOrigin,
              ADDONS.Emoji,
              ADDONS.ASP,
              ADDONS.PersonasPlus];

const set49Release = [
  ADDONS.Greasemonkey,
  ADDONS.DYTV,
  ADDONS.VDH,
  ADDONS.Lightbeam,
  ADDONS.ABP,
  ADDONS.uBlockOrigin,
  ADDONS.Emoji,
  ADDONS.ASP,
  ADDONS.PersonasPlus,
  ADDONS.ACR
];

// These are only the add-ons in the Add-Ons Manager Discovery
// pane. This set is here in case we need to reduce add-ons
// exposure live on Release.
const set49PaneOnly = [
  ADDONS.ABP,
  ADDONS.VDH,
  ADDONS.Emoji,
  ADDONS.ASP,
  ADDONS.ACR
];

// Main Test Pilot add-on + test add-ons for Firefox 50
const setTestPilotAddons50 = [
  ADDONS.TestPilot,
  ADDONS.TabCenter,
  ADDONS.ActivityStream,
  ADDONS.MinVid,
  ADDONS.PageShot,
  ADDONS.TrackingProtection,
  ADDONS.NoMore404s,
  ADDONS.UniversalSearch,
];

const setDiscoveryPane50 = [
  ADDONS.YouTubeHD,
  ADDONS.WeatherUnderground,
  ADDONS.DictionaryAnywhere,
  ADDONS.GmailNotifier,
];

const set50Release = [
  ...set49Release,
  ...setTestPilotAddons50,
  ...setDiscoveryPane50,
];

// We use these named policies to correlate the telemetry
// data with them, in order to understand how each set
// is behaving in the wild.
const RolloutPolicy = {
  // Used during 48 Beta cycle
  "2a": { addons: set2, webextensions: true },
  "2b": { addons: set2, webextensions: false },

  // Set agreed for Release 49
  "49a": { addons: set49Release, webextensions: true },
  "49b": { addons: set49Release, webextensions: false },

  // Smaller set that can be used for Release 49
  "49limiteda": { addons: set49PaneOnly, webextensions: true },
  "49limitedb": { addons: set49PaneOnly, webextensions: false },

  // Beta testing on 50
  "50allmpc": { addons: [], webextensions: true, mpc: true },

  "50a": { addons: set50Release, webextensions: true },
  "50b": { addons: set50Release, webextensions: false },

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

    if (policy.mpc && aAddon.multiprocessCompatible) {
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
