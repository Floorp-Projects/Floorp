/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { TelemetryControllerBase } from "resource://gre/modules/TelemetryControllerBase.sys.mjs";

export var TelemetryController = Object.freeze({
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

    let options = testing ? { timeout: 0 } : {};
    ChromeUtils.idleDispatch(() => Services.telemetry.delayedInit(), options);
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
export function getTelemetryController() {
  return TelemetryController;
}
