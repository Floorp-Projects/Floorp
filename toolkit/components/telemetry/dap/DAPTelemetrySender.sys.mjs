/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { HPKEConfigManager } from "resource://gre/modules/HPKEConfigManager.sys.mjs";

let lazy = {};

ChromeUtils.defineLazyGetter(lazy, "logConsole", function () {
  return console.createInstance({
    prefix: "DAPTelemetrySender",
    maxLogLevelPref: "toolkit.telemetry.dap.logLevel",
  });
});
ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  DAPVisitCounter: "resource://gre/modules/DAPVisitCounter.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
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
    lazy.logConsole.debug("Performing DAP startup");

    if (lazy.NimbusFeatures.dapTelemetry.getVariable("visitCountingEnabled")) {
      lazy.DAPVisitCounter.startup();
    }

    if (lazy.NimbusFeatures.dapTelemetry.getVariable("task1Enabled")) {
      let tasks = [];
      lazy.logConsole.debug("Task 1 is enabled.");
      let task1_id =
        lazy.NimbusFeatures.dapTelemetry.getVariable("task1TaskId");
      if (task1_id !== undefined && task1_id != "") {
        /** @typedef { 'u8' | 'vecu8' | 'vecu16' } measurementtype */

        /**
         * @typedef {object} Task
         * @property {string} id - The task ID, base 64 encoded.
         * @property {string} leader_endpoint - Base URL for the leader.
         * @property {string} helper_endpoint - Base URL for the helper.
         * @property {number} time_precision - Timestamps (in s) are rounded to the nearest multiple of this.
         * @property {measurementtype} measurement_type - Defines measurements and aggregations used by this task. Effectively specifying the VDAF.
         */
        let task = {
          // this is testing task 1
          id: task1_id,
          leader_endpoint: null,
          helper_endpoint: null,
          time_precision: 300,
          measurement_type: "vecu8",
        };
        tasks.push(task);

        lazy.setTimeout(
          () => this.timedSendTestReports(tasks),
          this.timeout_value()
        );

        lazy.NimbusFeatures.dapTelemetry.onUpdate(async (event, reason) => {
          if (typeof this.counters !== "undefined") {
            await this.sendTestReports(tasks, 30 * 1000);
          }
        });
      }

      this._asyncShutdownBlocker = async () => {
        lazy.logConsole.debug(`Sending on shutdown.`);
        // Shorter timeout to prevent crashing due to blocking shutdown
        await this.sendTestReports(tasks, 2 * 1000);
      };

      lazy.AsyncShutdown.quitApplicationGranted.addBlocker(
        "DAPTelemetrySender: sending data",
        this._asyncShutdownBlocker
      );
    }
  }

  async sendTestReports(tasks, timeout) {
    for (let task of tasks) {
      let measurement;
      if (task.measurement_type == "u8") {
        measurement = 3;
      } else if (task.measurement_type == "vecu8") {
        measurement = new Uint8Array(20);
        let r = Math.floor(Math.random() * 10);
        measurement[r] += 1;
        measurement[19] += 1;
      }

      await this.sendDAPMeasurement(task, measurement, timeout);
    }
  }

  async timedSendTestReports(tasks) {
    lazy.logConsole.debug("Sending on timer.");
    await this.sendTestReports(tasks, 30 * 1000);
    lazy.setTimeout(
      () => this.timedSendTestReports(tasks),
      this.timeout_value()
    );
  }

  timeout_value() {
    const MINUTE = 60 * 1000;
    return MINUTE * (9 + Math.random() * 2); // 9 - 11 minutes
  }

  /**
   * Creates a DAP report for a specific task from a measurement and sends it.
   *
   * @param {Task} task
   *   Definition of the task for which the measurement was taken.
   * @param {number} measurement
   *   The measured value for which a report is generated.
   */
  async sendDAPMeasurement(task, measurement, timeout) {
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
      const controller = new AbortController();
      lazy.setTimeout(() => controller.abort(), timeout);
      let report = await this.generateReport(
        task,
        measurement,
        controller.signal
      );
      Glean.dap.reportGenerationStatus.success.add(1);
      await this.sendReport(
        task.leader_endpoint,
        task.id,
        report,
        controller.signal
      );
    } catch (e) {
      if (e.name === "AbortError") {
        Glean.dap.reportGenerationStatus.abort.add(1);
        lazy.logConsole.error("Aborted DAP report generation.", e);
      } else {
        Glean.dap.reportGenerationStatus.failure.add(1);
        lazy.logConsole.error("DAP report generation failed: " + e);
      }
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
  async generateReport(task, measurement, abortSignal) {
    let [leader_config_bytes, helper_config_bytes] = await Promise.all([
      this.getHpkeConfig(
        task.leader_endpoint + "/hpke_config?task_id=" + task.id,
        abortSignal
      ),
      this.getHpkeConfig(
        task.helper_endpoint + "/hpke_config?task_id=" + task.id,
        abortSignal
      ),
    ]);
    if (abortSignal.aborted) {
      throw new DOMException("HPKE config download was aborted", "AbortError");
    }
    let task_id = new Uint8Array(
      ChromeUtils.base64URLDecode(task.id, { padding: "ignore" })
    );
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
    } else if (task.measurement_type == "vecu8") {
      Services.DAPTelemetry.GetReportVecU8(
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
        `Unknown measurement type for task ${task.id}: ${task.measurement_type}`
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
  async getHpkeConfig(endpoint, abortSignal) {
    // Use HPKEConfigManager to cache config for up to 24 hr. This reduces
    // unecessary requests while limiting how long a stale config can be stuck
    // if a server change is made ungracefully.
    let buffer = await HPKEConfigManager.get(endpoint, {
      maxAge: 24 * 60 * 60 * 1000,
      abortSignal,
    });
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
  async sendReport(leader_endpoint, task_id, report, abortSignal) {
    const upload_path = leader_endpoint + "/tasks/" + task_id + "/reports";
    try {
      let response = await fetch(upload_path, {
        method: "PUT",
        headers: { "Content-Type": "application/dap-report" },
        body: report,
        signal: abortSignal,
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
        lazy.logConsole.debug("DAP report sent");
        Glean.dap.uploadStatus.success.add(1);
      }
    } catch (err) {
      if (err.name === "AbortError") {
        lazy.logConsole.error("Aborted DAP report sending.", err);
        Glean.dap.uploadStatus.abort.add(1);
      } else {
        lazy.logConsole.error("Failed to send report. fetch failed", err);
        Glean.dap.uploadStatus.failure.add(1);
      }
    }
  }
})();
