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

// NOTE: Do not modify policies after they have already been
// published to users. They must remain unchanged to provide valid data.

// We use these named policies to correlate the telemetry
// data with them, in order to understand how each set
// is behaving in the wild.
const RolloutPolicy = {
  // Beta testing on 50
  "50allmpc": { webextensions: true, mpc: true },

  // ESR
  "esrA": { mpc: true, webextensions: true },
  "esrB": { mpc: true, webextensions: false },
  "esrC": { mpc: false, webextensions: true },

  "xpcshell-test": { mpc: true, webextensions: false },
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

    if (aAddon.mpcOptedOut == true) {
      return false;
    }

    if (policy.webextensions && (aAddon.type == "webextension" || aAddon.type == "webextension-theme")) {
      return true;
    }

    if (policy.mpc && aAddon.multiprocessCompatible) {
      return true;
    }

    return false;
  },
});
