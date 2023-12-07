/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

// The following constants represent the different possible privacy levels that
// can be set by the user and that we need to consider when collecting text
// data, and cookies.
//
// Collect data from all sites (http and https).
const PRIVACY_NONE = 0;
// Collect data from unencrypted sites (http), only.
const PRIVACY_ENCRYPTED = 1;
// Collect no data.
const PRIVACY_FULL = 2;

export var PrivacyLevel = {
  /**
   * Returns whether the current privacy level allows saving data for the given
   * |url|.
   *
   * @param url The URL we want to save data for.
   * @return bool
   */
  check(url) {
    return PrivacyLevel.canSave(url.startsWith("https:"));
  },

  /**
   * Checks whether we're allowed to save data for a specific site.
   *
   * @param isHttps A boolean that tells whether the site uses TLS.
   * @return {bool} Whether we can save data for the specified site.
   */
  canSave(isHttps) {
    // Never save any data when full privacy is requested.
    if (this.privacyLevel == PRIVACY_FULL) {
      return false;
    }

    // Don't save data for encrypted sites when requested.
    if (isHttps && this.privacyLevel == PRIVACY_ENCRYPTED) {
      return false;
    }

    return true;
  },

  canSaveAnything() {
    return this.privacyLevel != PRIVACY_FULL;
  },

  shouldSaveEverything() {
    return this.privacyLevel == PRIVACY_NONE;
  },
};

XPCOMUtils.defineLazyPreferenceGetter(
  PrivacyLevel,
  "privacyLevel",
  "browser.sessionstore.privacy_level"
);
