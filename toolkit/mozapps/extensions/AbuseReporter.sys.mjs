/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

// Maximum length of the string properties sent to the API endpoint.
const MAX_STRING_LENGTH = 255;

const AMO_SUPPORTED_ADDON_TYPES = [
  "extension",
  "theme",
  "sitepermission",
  // TODO(Bug 1789718): Remove after the deprecated XPIProvider-based implementation is also removed.
  "sitepermission-deprecated",
  "dictionary",
];

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
  ClientID: "resource://gre/modules/ClientID.sys.mjs",
});

/**
 * A singleton used to manage abuse reports for add-ons.
 */
export const AbuseReporter = {
  getAMOFormURL({ addonId }) {
    return Services.urlFormatter
      .formatURLPref("extensions.abuseReport.amoFormURL")
      .replace(/%addonID%/g, addonId);
  },

  isSupportedAddonType(addonType) {
    return AMO_SUPPORTED_ADDON_TYPES.includes(addonType);
  },

  /**
   * Helper function that retrieves from an addon object all the data to send
   * as part of the submission request, besides the `reason`, `message` which are
   * going to be received from the submit method of the report object returned
   * by `createAbuseReport`.
   * (See https://addons-server.readthedocs.io/en/latest/topics/api/abuse.html)
   *
   * @param {AddonWrapper} addon
   *        The addon object to collect the detail from.
   *
   * @return {object}
   *         An object that contains the collected details.
   */
  async getReportData(addon) {
    const truncateString = text =>
      typeof text == "string" ? text.slice(0, MAX_STRING_LENGTH) : text;

    // Normalize addon_install_source and addon_install_method values
    // as expected by the server API endpoint. Returns null if the
    // value is not a string.
    const normalizeValue = text =>
      typeof text == "string"
        ? text.toLowerCase().replace(/[- :]/g, "_")
        : null;

    const installInfo = addon.installTelemetryInfo || {};

    const data = {
      addon: addon.id,
      addon_version: addon.version,
      addon_name: truncateString(addon.name),
      addon_summary: truncateString(addon.description),
      addon_install_origin:
        addon.sourceURI && truncateString(addon.sourceURI.spec),
      install_date: addon.installDate && addon.installDate.toISOString(),
      addon_install_source: normalizeValue(installInfo.source),
      addon_install_source_url:
        installInfo.sourceURL && truncateString(installInfo.sourceURL),
      addon_install_method: normalizeValue(installInfo.method),
    };

    switch (addon.signedState) {
      case lazy.AddonManager.SIGNEDSTATE_BROKEN:
        data.addon_signature = "broken";
        break;
      case lazy.AddonManager.SIGNEDSTATE_UNKNOWN:
        data.addon_signature = "unknown";
        break;
      case lazy.AddonManager.SIGNEDSTATE_MISSING:
        data.addon_signature = "missing";
        break;
      case lazy.AddonManager.SIGNEDSTATE_PRELIMINARY:
        data.addon_signature = "preliminary";
        break;
      case lazy.AddonManager.SIGNEDSTATE_SIGNED:
        data.addon_signature = "signed";
        break;
      case lazy.AddonManager.SIGNEDSTATE_SYSTEM:
        data.addon_signature = "system";
        break;
      case lazy.AddonManager.SIGNEDSTATE_PRIVILEGED:
        data.addon_signature = "privileged";
        break;
      default:
        data.addon_signature = `unknown: ${addon.signedState}`;
    }

    // Set "curated" as addon_signature on recommended addons
    // (addon.isRecommended internally checks that the addon is also
    // signed correctly).
    if (addon.isRecommended) {
      data.addon_signature = "curated";
    }

    data.client_id = await lazy.ClientID.getClientIdHash();

    data.app = Services.appinfo.name.toLowerCase();
    data.appversion = Services.appinfo.version;
    data.lang = Services.locale.appLocaleAsBCP47;
    data.operating_system = AppConstants.platform;
    data.operating_system_version = Services.sysinfo.getProperty("version");

    return data;
  },
};
