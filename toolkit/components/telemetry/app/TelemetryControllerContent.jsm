/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { DeferredTask } = ChromeUtils.import(
  "resource://gre/modules/DeferredTask.jsm"
);
const { TelemetryControllerBase } = ChromeUtils.import(
  "resource://gre/modules/TelemetryControllerBase.jsm"
);

// Delay before intializing telemetry (ms)
const TELEMETRY_DELAY =
  Services.prefs.getIntPref("toolkit.telemetry.initDelay", 60) * 1000;
// Delay before initializing telemetry if we're testing (ms)
const TELEMETRY_TEST_DELAY = 1;

var EXPORTED_SYMBOLS = ["TelemetryController", "getTelemetryController"];

var TelemetryController = Object.freeze({
  /**
   * Used only for testing purposes.
   */
  testInitLogging() {
    TelemetryControllerBase.configureLogging();
  },

  /**
   * Used only for testing purposes.
   */
  testSetupContent() {
    return Impl.setupContentTelemetry(true);
  },

  /**
   * Send a notification.
   */
  observe(aSubject, aTopic, aData) {
    return Impl.observe(aSubject, aTopic, aData);
  },

  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
});

var Impl = {
  // This is true when running in the test infrastructure.
  _testMode: false,

  get _log() {
    return TelemetryControllerBase.log;
  },

  /**
   * This triggers basic telemetry initialization for content processes.
   * @param {Boolean} [testing=false] True if we are in test mode, false otherwise.
   */
  setupContentTelemetry(testing = false) {
    this._testMode = testing;

    // The thumbnail service also runs in a content process, even with e10s off.
    // We need to check if e10s is on so we don't submit child payloads for it.
    // We still need xpcshell child tests to work, so we skip this if test mode is enabled.
    if (testing || Services.appinfo.browserTabsRemoteAutostart) {
      // We call |enableTelemetryRecording| here to make sure that Telemetry.canRecord* flags
      // are in sync between chrome and content processes.
      if (!TelemetryControllerBase.enableTelemetryRecording()) {
        this._log.trace(
          "setupContentTelemetry - Content process recording disabled."
        );
        return;
      }
    }
    Services.telemetry.earlyInit();

    // FIXME: This is a terrible abuse of DeferredTask.
    let delayedTask = new DeferredTask(
      () => {
        Services.telemetry.delayedInit();
      },
      testing ? TELEMETRY_TEST_DELAY : TELEMETRY_DELAY,
      testing ? 0 : undefined
    );

    delayedTask.arm();
  },

  /**
   * This observer drives telemetry.
   */
  observe(aSubject, aTopic, aData) {
    if (aTopic == "content-process-ready-for-script") {
      TelemetryControllerBase.configureLogging();

      this._log.trace(`observe - ${aTopic} notified.`);

      this.setupContentTelemetry();
    }
  },
};

// Used by service registration, which requires a callable function.
function getTelemetryController() {
  return TelemetryController;
}
