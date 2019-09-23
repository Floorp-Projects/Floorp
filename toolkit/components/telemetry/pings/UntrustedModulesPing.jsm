/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This module periodically sends a Telemetry ping containing information
 * about untrusted module loads on Windows.
 *
 * https://firefox-source-docs.mozilla.org/toolkit/components/telemetry/telemetry/data/third-party-modules-ping.html
 */

"use strict";

var EXPORTED_SYMBOLS = ["TelemetryUntrustedModulesPing"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetters(this, {
  Log: "resource://gre/modules/Log.jsm",
  Services: "resource://gre/modules/Services.jsm",
  TelemetryController: "resource://gre/modules/TelemetryController.jsm",
  TelemetryUtils: "resource://gre/modules/TelemetryUtils.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  Telemetry: ["@mozilla.org/base/telemetry;1", "nsITelemetry"],
  UpdateTimerManager: [
    "@mozilla.org/updates/timer-manager;1",
    "nsIUpdateTimerManager",
  ],
});

const DEFAULT_INTERVAL_SECONDS = 24 * 60 * 60; // 1 day

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryUntrustedModulesPing::";
const TIMER_NAME = "telemetry_untrustedmodules_ping";
const PING_SUBMISSION_NAME = "third-party-modules";

var TelemetryUntrustedModulesPing = Object.freeze({
  _log: Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX),

  start() {
    UpdateTimerManager.registerTimer(
      TIMER_NAME,
      this,
      Services.prefs.getIntPref(
        TelemetryUtils.Preferences.UntrustedModulesPingFrequency,
        DEFAULT_INTERVAL_SECONDS
      )
    );
  },

  notify() {
    try {
      Telemetry.getUntrustedModuleLoadEvents().then(payload => {
        try {
          if (payload) {
            TelemetryController.submitExternalPing(
              PING_SUBMISSION_NAME,
              payload,
              {
                addClientId: true,
                addEnvironment: true,
              }
            );
          }
        } catch (ex) {
          this._log.error("payload handler caught an exception", ex);
        }
      });
    } catch (ex) {
      this._log.error("notify() caught an exception", ex);
    }
  },
});
