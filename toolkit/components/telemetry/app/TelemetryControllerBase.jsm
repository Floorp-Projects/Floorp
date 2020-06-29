/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["TelemetryControllerBase"];

ChromeUtils.defineModuleGetter(
  this,
  "AppConstants",
  "resource://gre/modules/AppConstants.jsm"
);
const { Log } = ChromeUtils.import("resource://gre/modules/Log.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const LOGGER_NAME = "Toolkit.Telemetry";
const LOGGER_PREFIX = "TelemetryController::";

const PREF_BRANCH_LOG = "toolkit.telemetry.log.";
const PREF_LOG_LEVEL = "toolkit.telemetry.log.level";
const PREF_LOG_DUMP = "toolkit.telemetry.log.dump";

const PREF_TELEMETRY_ENABLED = "toolkit.telemetry.enabled";

const Preferences = Object.freeze({
  OverridePreRelease: "toolkit.telemetry.testing.overridePreRelease",
  Unified: "toolkit.telemetry.unified",
});

/**
 * Setup Telemetry logging. This function also gets called when loggin related
 * preferences change.
 */
var gLogger = null;
var gPrefixLogger = null;
var gLogAppenderDump = null;

var TelemetryControllerBase = Object.freeze({
  // Whether the FHR/Telemetry unification features are enabled.
  // Changing this pref requires a restart.
  IS_UNIFIED_TELEMETRY: Services.prefs.getBoolPref(Preferences.Unified, false),

  Preferences,

  /**
   * Returns the state of the Telemetry enabled preference, making sure
   * it correctly evaluates to a boolean type.
   */
  get isTelemetryEnabled() {
    return Services.prefs.getBoolPref(PREF_TELEMETRY_ENABLED, false) === true;
  },

  get log() {
    if (!gPrefixLogger) {
      gPrefixLogger = Log.repository.getLoggerWithMessagePrefix(
        LOGGER_NAME,
        LOGGER_PREFIX
      );
    }
    return gPrefixLogger;
  },

  configureLogging() {
    if (!gLogger) {
      gLogger = Log.repository.getLogger(LOGGER_NAME);

      // Log messages need to go to the browser console.
      let consoleAppender = new Log.ConsoleAppender(new Log.BasicFormatter());
      gLogger.addAppender(consoleAppender);

      Services.prefs.addObserver(PREF_BRANCH_LOG, this.configureLogging);
    }

    // Make sure the logger keeps up with the logging level preference.
    gLogger.level =
      Log.Level[Services.prefs.getStringPref(PREF_LOG_LEVEL, "Warn")];

    // If enabled in the preferences, add a dump appender.
    let logDumping = Services.prefs.getBoolPref(PREF_LOG_DUMP, false);
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
   * Set the Telemetry core recording flag for Unified Telemetry.
   */
  setTelemetryRecordingFlags() {
    // Enable extended Telemetry on pre-release channels and disable it
    // on Release/ESR.
    let prereleaseChannels = ["nightly", "aurora", "beta"];
    if (!AppConstants.MOZILLA_OFFICIAL) {
      // Turn extended telemetry for local developer builds.
      prereleaseChannels.push("default");
    }
    const isPrereleaseChannel = prereleaseChannels.includes(
      AppConstants.MOZ_UPDATE_CHANNEL
    );
    const isReleaseCandidateOnBeta =
      AppConstants.MOZ_UPDATE_CHANNEL === "release" &&
      Services.prefs.getCharPref("app.update.channel", null) === "beta";
    Services.telemetry.canRecordBase = true;
    Services.telemetry.canRecordExtended =
      isPrereleaseChannel ||
      isReleaseCandidateOnBeta ||
      Services.prefs.getBoolPref(this.Preferences.OverridePreRelease, false);
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
      this.setTelemetryRecordingFlags();
    } else {
      // We're not on unified Telemetry, stick to the old behaviour for
      // supporting Fennec.
      Services.telemetry.canRecordBase = Services.telemetry.canRecordExtended = this.isTelemetryEnabled;
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
