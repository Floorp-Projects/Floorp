/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

let lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "logConsole", function() {
  return console.createInstance({
    prefix: "DAPTelemetrySender",
    maxLogLevelPref: "toolkit.telemetry.dap.logLevel",
  });
});
ChromeUtils.defineESModuleGetters(lazy, {
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
});

const PREF_LEADER = "toolkit.telemetry.dap_leader";
const PREF_HELPER = "toolkit.telemetry.dap_helper";

XPCOMUtils.defineLazyPreferenceGetter(lazy, "LEADER", PREF_LEADER, undefined);
XPCOMUtils.defineLazyPreferenceGetter(lazy, "HELPER", PREF_HELPER, undefined);

/**
 * The purpose of this singleton is to handle sending of DAP telemetry data.
 * The current DAP draft standard is available here:
 * https://github.com/ietf-wg-ppm/draft-ietf-ppm-dap
 *
 * The specific purpose of this singleton is to make the necessary calls to fetch to do networking.
 */

export const DAPTelemetrySender = new (class {
  startup() {
    lazy.logConsole.info("Performing DAP startup");

    if (lazy.NimbusFeatures.dapTelemetry.getVariable("task1Enabled")) {
      // For now we are sending a constant value because it simplifies verification.
      let measurement = 3;
      this.sendVerificationTaskReport(measurement);
    }
  }

  /**
   * For testing: sends a hard coded report for a hard coded task.
   *
   * @param {number} measurement
   */
  async sendVerificationTaskReport(measurement) {
    lazy.logConsole.info("Trying to send verification task.");

    // For now there is only a single task which is hardcoded here.
    /**
     * @typedef {object} Task
     * @property {string} id_hexstring - The task's ID hex encoded.
     * @property {string} id_base64 - The same ID base 64 encoded.
     * @property {string} leader_endpoint - Base URL for the leader.
     * @property {string} helper_endpoint - Base URL for the helper.
     * @property {number} time_precision - Timestamps (in s) are rounded to the nearest multiple of this.
     */
    const task = {
      // Note that this does not exactly match the task definition from the standard linked-to above.
      id_hexstring:
        "4ad95d3b67332ff89a505da296315b88b88d4f1c5535d3c780fbae1162c79ec9",
      id_base64: "StldO2czL_iaUF2iljFbiLiNTxxVNdPHgPuuEWLHnsk",
      leader_endpoint: null,
      helper_endpoint: null,
      time_precision: 600,
    };

    task.leader_endpoint = lazy.LEADER;
    if (!task.leader_endpoint) {
      lazy.logConsole.error('Preference "' + PREF_LEADER + '" not set');
      return;
    }

    task.helper_endpoint = lazy.HELPER;
    if (!task.helper_endpoint) {
      lazy.logConsole.error('Preference "' + PREF_HELPER + '" not set');
      return;
    }

    try {
      let report = await this.generateReport(task, measurement);
      Glean.dap.reportGenerationStatus.success.add(1);
      await this.sendReport(task.leader_endpoint, report);
    } catch (e) {
      Glean.dap.reportGenerationStatus.failure.add(1);
      lazy.logConsole.error("DAP report generation failed: " + e.message);
    }
  }

  /**
   * Downloads HPKE configs for endpoints and generates report.
   *
   * @param {Task} task
   *   Definition of the task for which the measurement was taken.
   * @param {number} measurement
   *   The measured value for which a report is generated.
   * @returns Promise
   * @resolves {Uint8Array} The generated binary report data.
   * @rejects {Error} If an exception is thrown while generating the report.
   */
  async generateReport(task, measurement) {
    let [leader_config_bytes, helper_config_bytes] = await Promise.all([
      this.getHpkeConfig(
        task.leader_endpoint + "/hpke_config?task_id=" + task.id_base64
      ),
      this.getHpkeConfig(
        task.helper_endpoint + "/hpke_config?task_id=" + task.id_base64
      ),
    ]);
    let task_id = hexString2Binary(task.id_hexstring);
    let report = {};
    Services.DAPTelemetry.GetReport(
      leader_config_bytes,
      helper_config_bytes,
      measurement,
      task_id,
      task.time_precision,
      report
    );
    let reportData = new Uint8Array(report.value);
    return reportData;
  }

  /**
   * Fetches TLS encoded HPKE config from a URL.
   *
   * @param {string} endpoint
   *   The URL from where to get the data.
   * @returns Promise
   * @resolves {Uint8Array} The binary representation of the endpoint configuration.
   * @rejects {Error} If an exception is thrown while fetching the configuration.
   */
  async getHpkeConfig(endpoint) {
    let response = await fetch(endpoint);
    if (!response.ok) {
      throw new Error(
        `Failed to retrieve HPKE config for DAP from: ${endpoint}. Response: ${
          response.status
        }: ${await response.text()}.`
      );
    }
    let buffer = await response.arrayBuffer();
    let hpke_config_bytes = new Uint8Array(buffer);
    return hpke_config_bytes;
  }

  /**
   * Sends a report to the leader.
   *
   * @param {string} leader_endpoint
   *   The URL for the leader.
   * @param {Uint8Array} report
   *   Raw bytes of the TLS encoded report.
   * @returns Promise
   * @resolves {undefined} Once the attempt to send the report completes, whether or not it was successful.
   */
  async sendReport(leader_endpoint, report) {
    const upload_path = leader_endpoint + "/upload";
    try {
      let response = await fetch(upload_path, {
        method: "POST",
        headers: { "Content-Type": "application/dap-report" },
        body: report,
      });

      if (response.status != 200) {
        let error = await response.json();
        lazy.logConsole.error(
          `Sending failed. HTTP response: ${response.status} ${response.statusText}. Error: ${error.type} ${error.title}`
        );

        Glean.dap.uploadStatus.failure.add(1);
      } else {
        lazy.logConsole.info("DAP report sent");
        Glean.dap.uploadStatus.success.add(1);
      }
    } catch (err) {
      lazy.logConsole.error("Failed to send report. fetch failed", err);
      Glean.dap.uploadStatus.failure.add(1);
    }
  }
})();

/**
 * Converts a hex representation of a byte string into an array
 * @param {string} hexstring - a list of bytes represented as a hex string two characters per bytes
 * @return {Uint8Array} - the input byte list as an array
 */
function hexString2Binary(hexstring) {
  const binlen = hexstring.length / 2;
  let binary = new Uint8Array(binlen);
  for (var i = 0; i < binlen; i++) {
    binary[i] = parseInt(hexstring.substring(2 * i, 2 * (i + 1)), 16);
  }
  return binary;
}
