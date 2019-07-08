/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UptakeTelemetry"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ClientID",
  "resource://gre/modules/ClientID.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

XPCOMUtils.defineLazyGetter(this, "CryptoHash", () => {
  return Components.Constructor(
    "@mozilla.org/security/hash;1",
    "nsICryptoHash",
    "initWithString"
  );
});

XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gSampleRate",
  "services.common.uptake.sampleRate"
);

// Telemetry histogram id (see Histograms.json).
const TELEMETRY_HISTOGRAM_ID = "UPTAKE_REMOTE_CONTENT_RESULT_1";

// Telemetry events id (see Events.yaml).
const TELEMETRY_EVENTS_ID = "uptake.remotecontent.result";

/**
 * A wrapper around certain low-level operations that can be substituted for testing.
 */
var Policy = {
  _clientIDHash: null,

  getClientID() {
    return ClientID.getClientID();
  },

  /**
   * Compute an integer in the range [0, 100) using a hash of the
   * client ID.
   *
   * This is useful for sampling clients when trying to report
   * telemetry only for a sample of clients.
   */
  async getClientIDHash() {
    if (this._clientIDHash === null) {
      this._clientIDHash = this._doComputeClientIDHash();
    }
    return this._clientIDHash;
  },

  async _doComputeClientIDHash() {
    const clientID = await this.getClientID();
    let byteArr = new TextEncoder().encode(clientID);
    let hash = new CryptoHash("sha256");
    hash.update(byteArr, byteArr.length);
    const bytes = hash.finish(false);
    let rem = 0;
    for (let i = 0, len = bytes.length; i < len; i++) {
      rem = ((rem << 8) + (bytes[i].charCodeAt(0) & 0xff)) % 100;
    }
    return rem;
  },

  getChannel() {
    return AppConstants.MOZ_UPDATE_CHANNEL;
  },
};

/**
 * A Telemetry helper to report uptake of remote content.
 */
class UptakeTelemetry {
  /**
   * Supported uptake statuses:
   *
   * - `UP_TO_DATE`: Local content was already up-to-date with remote content.
   * - `SUCCESS`: Local content was updated successfully.
   * - `BACKOFF`: Remote server asked clients to backoff.
   * - `PARSE_ERROR`: Parsing server response has failed.
   * - `CONTENT_ERROR`: Server response has unexpected content.
   * - `PREF_DISABLED`: Update is disabled in user preferences.
   * - `SIGNATURE_ERROR`: Signature verification after diff-based sync has failed.
   * - `SIGNATURE_RETRY_ERROR`: Signature verification after full fetch has failed.
   * - `CONFLICT_ERROR`: Some remote changes are in conflict with local changes.
   * - `SYNC_ERROR`: Synchronization of remote changes has failed.
   * - `APPLY_ERROR`: Application of changes locally has failed.
   * - `SERVER_ERROR`: Server failed to respond.
   * - `CERTIFICATE_ERROR`: Server certificate verification has failed.
   * - `DOWNLOAD_ERROR`: Data could not be fully retrieved.
   * - `TIMEOUT_ERROR`: Server response has timed out.
   * - `NETWORK_ERROR`: Communication with server has failed.
   * - `NETWORK_OFFLINE_ERROR`: Network not available.
   * - `UNKNOWN_ERROR`: Uncategorized error.
   * - `CLEANUP_ERROR`: Clean-up of temporary files has failed.
   * - `CUSTOM_1_ERROR`: Update source specific error #1.
   * - `CUSTOM_2_ERROR`: Update source specific error #2.
   * - `CUSTOM_3_ERROR`: Update source specific error #3.
   * - `CUSTOM_4_ERROR`: Update source specific error #4.
   * - `CUSTOM_5_ERROR`: Update source specific error #5.
   *
   * @type {Object}
   */
  static get STATUS() {
    return {
      UP_TO_DATE: "up_to_date",
      SUCCESS: "success",
      BACKOFF: "backoff",
      PREF_DISABLED: "pref_disabled",
      PARSE_ERROR: "parse_error",
      CONTENT_ERROR: "content_error",
      SIGNATURE_ERROR: "sign_error",
      SIGNATURE_RETRY_ERROR: "sign_retry_error",
      CONFLICT_ERROR: "conflict_error",
      SYNC_ERROR: "sync_error",
      APPLY_ERROR: "apply_error",
      SERVER_ERROR: "server_error",
      CERTIFICATE_ERROR: "certificate_error",
      DOWNLOAD_ERROR: "download_error",
      TIMEOUT_ERROR: "timeout_error",
      NETWORK_ERROR: "network_error",
      NETWORK_OFFLINE_ERROR: "offline_error",
      CLEANUP_ERROR: "cleanup_error",
      UNKNOWN_ERROR: "unknown_error",
      CUSTOM_1_ERROR: "custom_1_error",
      CUSTOM_2_ERROR: "custom_2_error",
      CUSTOM_3_ERROR: "custom_3_error",
      CUSTOM_4_ERROR: "custom_4_error",
      CUSTOM_5_ERROR: "custom_5_error",
    };
  }

  /**
   * Reports the uptake status for the specified source.
   *
   * @param {string} component     the component reporting the uptake (eg. "normandy").
   * @param {string} status        the uptake status (eg. "network_error")
   * @param {Object} extra         extra values to report
   * @param {string} extra.source  the update source (eg. "recipe-42").
   * @param {string} extra.trigger what triggered the polling/fetching (eg. "broadcast", "timer").
   * @param {int}    extra.age     age of pulled data in seconds
   */
  static async report(component, status, extra = {}) {
    const { source } = extra;

    if (!source) {
      throw new Error("`source` value is mandatory.");
    }

    // Report event for real-time monitoring. See Events.yaml for registration.
    // Contrary to histograms, Telemetry Events are not enabled by default.
    // Enable them on first call to `report()`.
    if (!this._eventsEnabled) {
      Services.telemetry.setEventRecordingEnabled(TELEMETRY_EVENTS_ID, true);
      this._eventsEnabled = true;
    }

    const hash = await Policy.getClientIDHash();
    const channel = Policy.getChannel();
    const shouldSendEvent =
      !["release", "esr"].includes(channel) || hash < gSampleRate;
    if (shouldSendEvent) {
      // The Event API requires `extra` values to be of type string. Force it!
      const extraStr = Object.keys(extra).reduce((acc, k) => {
        acc[k] = extra[k].toString();
        return acc;
      }, {});
      Services.telemetry.recordEvent(
        TELEMETRY_EVENTS_ID,
        "uptake",
        component,
        status,
        extraStr
      );
    }

    // Report via histogram in main ping.
    // Note: this is the legacy equivalent of the above event. We keep it for continuity.
    Services.telemetry
      .getKeyedHistogramById(TELEMETRY_HISTOGRAM_ID)
      .add(source, status);
  }
}

this.UptakeTelemetry = UptakeTelemetry;
