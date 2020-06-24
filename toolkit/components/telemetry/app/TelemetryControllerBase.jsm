/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TelemetryControllerBase"];

const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryController::";

const PREF_BRANCH_LOG = "toolkit.telemetry.log.";

/**
 * Setup Telemetry logging. This function also gets called when loggin related
 * preferences change.
 */
var gLogger = null;
var gLogAppenderDump = null;

var TelemetryControllerBase = Object.freeze({
  // Whether the FHR/Telemetry unification features are enabled.
  // Changing this pref requires a restart.
  IS_UNIFIED_TELEMETRY: Services.prefs.getBoolPref(
    TelemetryUtils.Preferences.Unified,
    false
  ),

  get log() {
    return (
      gLogger ||
      Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, LOGGER_PREFIX)
    );
  },

  configureLogging() {
    if (!gLogger) {
      gLogger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX
      );

      // Log messages need to go to the browser console.
      let consoleAppender = new Log.ConsoleAppender(new Log.BasicFormatter());
      gLogger.addAppender(consoleAppender);

      Services.prefs.addObserver(PREF_BRANCH_LOG, this.configureLogging);
    }

    // Make sure the logger keeps up with the logging level preference.
    gLogger.level =
      Log.Level[
        Services.prefs.getStringPref(
          TelemetryUtils.Preferences.LogLevel,
          "Warn"
        )
      ];

    // If enabled in the preferences, add a dump appender.
    let logDumping = Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.LogDump,
      false
    );
    if (logDumping != !!gLogAppenderDump) {
      if (logDumping) {
        gLogAppenderDump = new Log.DumpAppender(new Log.BasicFormatter());
        gLogger.addAppender(gLogAppenderDump);
      } else {
        gLogger.removeAppender(gLogAppenderDump);
        gLogAppenderDump = null;
      }
    }
  },

  /**
   * Perform telemetry initialization for either chrome or content process.
   * @return {Boolean} True if Telemetry is allowed to record at least base (FHR) data,
   *                   false otherwise.
   */
  enableTelemetryRecording: function enableTelemetryRecording() {
    // Configure base Telemetry recording.
    // Unified Telemetry makes it opt-out. If extended Telemetry is enabled, base recording
    // is always on as well.
    if (this.IS_UNIFIED_TELEMETRY) {
      TelemetryUtils.setTelemetryRecordingFlags();
    } else {
      // We're not on unified Telemetry, stick to the old behaviour for
      // supporting Fennec.
      Services.telemetry.canRecordBase = Services.telemetry.canRecordExtended =
        TelemetryUtils.isTelemetryEnabled;
    }

    this.log.config(
      "enableTelemetryRecording - canRecordBase:" +
        Services.telemetry.canRecordBase +
        ", canRecordExtended: " +
        Services.telemetry.canRecordExtended
    );

    return Services.telemetry.canRecordBase;
  },
});
