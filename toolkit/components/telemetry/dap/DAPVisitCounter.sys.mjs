/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";
import { DAPTelemetrySender } from "./DAPTelemetrySender.sys.mjs";

let lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "logConsole", function () {
  return console.createInstance({
    prefix: "DAPVisitCounter",
    maxLogLevelPref: "toolkit.telemetry.dap.logLevel",
  });
});
ChromeUtils.defineESModuleGetters(lazy, {
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.sys.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
  setTimeout: "resource://gre/modules/Timer.sys.mjs",
});

export const DAPVisitCounter = new (class {
  startup() {
    this._asyncShutdownBlocker = async () => {
      lazy.logConsole.debug(`Sending on shutdown.`);
      await this.send(2 * 1000);
    };

    lazy.AsyncShutdown.quitApplicationGranted.addBlocker(
      "DAPVisitCounter: sending data",
      this._asyncShutdownBlocker
    );

    const listener = events => {
      // Even using the event.hidden flag there mayb be some double counting
      // here. It would have to be fixed in the Places API.
      for (const event of events) {
        lazy.logConsole.debug(`Visited: ${event.url}`);
        if (event.hidden) {
          continue;
        }
        for (const counter of this.counters) {
          for (const pattern of counter.patterns) {
            if (pattern.matches(event.url)) {
              lazy.logConsole.debug(`${pattern.pattern} matched!`);
              counter.count += 1;
            }
          }
        }
      }
    };

    lazy.NimbusFeatures.dapTelemetry.onUpdate(async (event, reason) => {
      if (typeof this.counters !== "undefined") {
        await this.send(30 * 1000);
      }
      this.initialize_counters();
    });

    if (typeof this.counters === "undefined") {
      this.initialize_counters();
    }

    lazy.PlacesUtils.observers.addListener(["page-visited"], listener);

    lazy.setTimeout(() => this.timed_send(), this.timeout_value());
  }

  initialize_counters() {
    let experiments = lazy.NimbusFeatures.dapTelemetry.getVariable(
      "visitCountingExperimentList"
    );

    this.counters = [];
    for (const experiment of experiments) {
      let counter = { experiment, count: 0, patterns: [] };
      this.counters.push(counter);
      for (const url of experiment.urls) {
        let mpattern = new MatchPattern(url);
        counter.patterns.push(mpattern);
      }
    }
  }

  async timed_send() {
    lazy.logConsole.debug("Sending on timer.");
    await this.send(30 * 1000);
    lazy.setTimeout(() => this.timed_send(), this.timeout_value());
  }

  timeout_value() {
    const MINUTE = 60 * 1000;
    return MINUTE * (9 + Math.random() * 2); // 9 - 11 minutes
  }

  async send(timeout) {
    let collected_measurements = new Map();
    for (const counter of this.counters) {
      lazy.logConsole.debug(`Summing up experiment ${counter.experiment.name}`);

      if (!collected_measurements.has(counter.experiment.task_id)) {
        collected_measurements.set(
          counter.experiment.task_id,
          new Uint8Array(counter.experiment.task_veclen)
        );
      }
      collected_measurements.get(counter.experiment.task_id)[
        counter.experiment.bucket
      ] = counter.count;
      counter.count = 0;
    }

    for (const [task_id, measurement] of collected_measurements) {
      let task = {
        id: task_id,
        time_precision: 60,
        measurement_type: "vecu8",
      };

      await DAPTelemetrySender.sendDAPMeasurement(task, measurement, timeout);
    }
  }

  show() {
    for (const counter of this.counters) {
      lazy.logConsole.info(
        `Experiment: ${counter.experiment.name} -> ${counter.count}`
      );
    }
    return this.counters;
  }
})();
