/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  ClientID: "resource://gre/modules/ClientID.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "CryptoHash", () => {
  return Components.Constructor(
    "@mozilla.org/security/hash;1",
    "nsICryptoHash",
    "initWithString"
  );
});

XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "gSampleRate",
  "services.common.uptake.sampleRate"
);

// Telemetry events id (see Events.yaml).
const TELEMETRY_EVENTS_ID = "uptake.remotecontent.result";

/**
 * A wrapper around certain low-level operations that can be substituted for testing.
 */
export var Policy = {
  _clientIDHash: null,

  getClientID() {
    return lazy.ClientID.getClientID();
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
    let hash = new lazy.CryptoHash("sha256");
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
export class UptakeTelemetry {
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
   * - `CORRUPTION_ERROR`: Error related to corrupted local data.
   * - `SYNC_ERROR`: Synchronization of remote changes has failed.
   * - `APPLY_ERROR`: Application of changes locally has failed.
   * - `SERVER_ERROR`: Server failed to respond.
   * - `CERTIFICATE_ERROR`: Server certificate verification has failed.
   * - `DOWNLOAD_ERROR`: Data could not be fully retrieved.
   * - `TIMEOUT_ERROR`: Server response has timed out.
   * - `NETWORK_ERROR`: Communication with server has failed.
   * - `NETWORK_OFFLINE_ERROR`: Network not available.
   * - `SHUTDOWN_ERROR`: Error occuring during shutdown.
   * - `UNKNOWN_ERROR`: Uncategorized error.
   * - `CLEANUP_ERROR`: Clean-up of temporary files has failed.
   * - `SYNC_BROKEN_ERROR`: Synchronization is broken.
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
      PARSE_ERROR: "parse_error",
      CONTENT_ERROR: "content_error",
      PREF_DISABLED: "pref_disabled",
      SIGNATURE_ERROR: "sign_error",
      SIGNATURE_RETRY_ERROR: "sign_retry_error",
      CONFLICT_ERROR: "conflict_error",
      CORRUPTION_ERROR: "corruption_error",
      SYNC_ERROR: "sync_error",
      APPLY_ERROR: "apply_error",
      SERVER_ERROR: "server_error",
      CERTIFICATE_ERROR: "certificate_error",
      DOWNLOAD_ERROR: "download_error",
      TIMEOUT_ERROR: "timeout_error",
      NETWORK_ERROR: "network_error",
      NETWORK_OFFLINE_ERROR: "offline_error",
      SHUTDOWN_ERROR: "shutdown_error",
      UNKNOWN_ERROR: "unknown_error",
      CLEANUP_ERROR: "cleanup_error",
      SYNC_BROKEN_ERROR: "sync_broken_error",
      CUSTOM_1_ERROR: "custom_1_error",
      CUSTOM_2_ERROR: "custom_2_error",
      CUSTOM_3_ERROR: "custom_3_error",
      CUSTOM_4_ERROR: "custom_4_error",
      CUSTOM_5_ERROR: "custom_5_error",
    };
  }

  static get Policy() {
    return Policy;
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

    if (!Object.values(UptakeTelemetry.STATUS).includes(status)) {
      throw new Error(`Unknown status '${status}'`);
    }

    // Report event for real-time monitoring. See Events.yaml for registration.
    // Contrary to histograms, Telemetry Events are not enabled by default.
    // Enable them on first call to `report()`.
    if (!this._eventsEnabled) {
      Services.telemetry.setEventRecordingEnabled(TELEMETRY_EVENTS_ID, true);
      this._eventsEnabled = true;
    }

    const hash = await UptakeTelemetry.Policy.getClientIDHash();
    const channel = UptakeTelemetry.Policy.getChannel();
    const shouldSendEvent =
      !["release", "esr"].includes(channel) || hash < lazy.gSampleRate;
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
  }
}
