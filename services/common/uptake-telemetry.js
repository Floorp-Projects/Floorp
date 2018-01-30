/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";


this.EXPORTED_SYMBOLS = ["UptakeTelemetry"];

const { utils: Cu } = Components;
const { Services } = Cu.import("resource://gre/modules/Services.jsm", {});


// Telemetry report results.
const TELEMETRY_HISTOGRAM_ID = "UPTAKE_REMOTE_CONTENT_RESULT_1";

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
      UP_TO_DATE:            "up_to_date",
      SUCCESS:               "success",
      BACKOFF:               "backoff",
      PREF_DISABLED:         "pref_disabled",
      PARSE_ERROR:           "parse_error",
      CONTENT_ERROR:         "content_error",
      SIGNATURE_ERROR:       "sign_error",
      SIGNATURE_RETRY_ERROR: "sign_retry_error",
      CONFLICT_ERROR:        "conflict_error",
      SYNC_ERROR:            "sync_error",
      APPLY_ERROR:           "apply_error",
      SERVER_ERROR:          "server_error",
      CERTIFICATE_ERROR:     "certificate_error",
      DOWNLOAD_ERROR:        "download_error",
      TIMEOUT_ERROR:         "timeout_error",
      NETWORK_ERROR:         "network_error",
      NETWORK_OFFLINE_ERROR: "offline_error",
      CLEANUP_ERROR:         "cleanup_error",
      UNKNOWN_ERROR:         "unknown_error",
      CUSTOM_1_ERROR:        "custom_1_error",
      CUSTOM_2_ERROR:        "custom_2_error",
      CUSTOM_3_ERROR:        "custom_3_error",
      CUSTOM_4_ERROR:        "custom_4_error",
      CUSTOM_5_ERROR:        "custom_5_error",
    };
  }

  /**
   * Reports the uptake status for the specified source.
   *
   * @param {string} source  the identifier of the update source.
   * @param {string} status  the uptake status.
   */
  static report(source, status) {
    Services.telemetry
      .getKeyedHistogramById(TELEMETRY_HISTOGRAM_ID)
      .add(source, status);
  }
}

this.UptakeTelemetry = UptakeTelemetry;
