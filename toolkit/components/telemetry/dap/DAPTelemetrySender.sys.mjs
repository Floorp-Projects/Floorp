/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

let lazy = {};

ChromeUtils.defineLazyGetter(lazy, "logConsole", function () {
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

  async sendTestReports() {
    /** @typedef { 'u8' | 'vecu16'} measurementtype */

    /**
     * @typedef {object} Task
     * @property {string} id_hexstring - The task's ID hex encoded.
     * @property {string} id_base64 - The same ID base 64 encoded.
     * @property {string} leader_endpoint - Base URL for the leader.
     * @property {string} helper_endpoint - Base URL for the helper.
     * @property {number} time_precision - Timestamps (in s) are rounded to the nearest multiple of this.
     * @property {measurementtype} measurement_type - Defines measurements and aggregations used by this task. Effectively specifying the VDAF.
     */

    // For now tasks are hardcoded here.
    const tasks = [
      {
        // this is load testing task 1
        id_hexstring:
          "423303e27f25fcc1c1a0badb09f2d3162f210b6eb87c2e7d48a1cfbe23c5d2af",
        id_base64: "QjMD4n8l_MHBoLrbCfLTFi8hC264fC59SKHPviPF0q8",
        leader_endpoint: null,
        helper_endpoint: null,
        time_precision: 60, // TODO what is a reasonable value
        measurement_type: "u8",
      },
      {
        // this is load testing task 2
        id_hexstring:
          "0d2646305876ea10585cd68abe12ff3780070373f99439f5f689f5bc53c1c493",
        id_base64: "DSZGMFh26hBYXNaKvhL_N4AHA3P5lDn19on1vFPBxJM",
        leader_endpoint: null,
        helper_endpoint: null,
        time_precision: 60,
        measurement_type: "vecu16",
      },
    ];

    for (let task of tasks) {
      let measurement;
      if (task.measurement_type == "u8") {
        measurement = 3;
      } else if (task.measurement_type == "vecu16") {
        measurement = new Uint16Array(1024);
        let r = Math.floor(Math.random() * 10);
        measurement[r] += 1;
        measurement[1000] += 1;
      }

      await this.sendTestReport(task, measurement);
    }
  }

  /**
   * Creates a DAP report for a specific task from a measurement and sends it.
   *
   * @param {Task} task
   *   Definition of the task for which the measurement was taken.
   * @param {number} measurement
   *   The measured value for which a report is generated.
   */
  async sendTestReport(task, measurement) {
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
      await this.sendReport(task.leader_endpoint, task.id_base64, report);
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
    if (task.measurement_type == "u8") {
      Services.DAPTelemetry.GetReportU8(
        leader_config_bytes,
        helper_config_bytes,
        measurement,
        task_id,
        task.time_precision,
        report
      );
    } else if (task.measurement_type == "vecu16") {
      Services.DAPTelemetry.GetReportVecU16(
        leader_config_bytes,
        helper_config_bytes,
        measurement,
        task_id,
        task.time_precision,
        report
      );
    } else {
      throw new Error(
        `Unknown measurement type for task ${task.id_base64}: ${task.measurement_type}`
      );
    }
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
  async sendReport(leader_endpoint, task_id, report) {
    const upload_path = leader_endpoint + "/tasks/" + task_id + "/reports";
    try {
      let response = await fetch(upload_path, {
        method: "PUT",
        headers: { "Content-Type": "application/dap-report" },
        body: report,
      });

      if (response.status != 200) {
        const content_type = response.headers.get("content-type");
        if (content_type && content_type === "application/json") {
          // A JSON error from the DAP server.
          let error = await response.json();
          lazy.logConsole.error(
            `Sending failed. HTTP response: ${response.status} ${response.statusText}. Error: ${error.type} ${error.title}`
          );
        } else {
          // A different error, e.g. from a load-balancer.
          let error = await response.text();
          lazy.logConsole.error(
            `Sending failed. HTTP response: ${response.status} ${response.statusText}. Error: ${error}`
          );
        }

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
