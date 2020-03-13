/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/Log.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);

ChromeUtils.defineModuleGetter(
  this,
  "TelemetryController",
  "resource://gre/modules/TelemetryController.jsm"
);

const LOGGER_NAME = "Toolkit.Telemetry";
const PING_TYPE = "update";
const UPDATE_DOWNLOADED_TOPIC = "update-downloaded";
const UPDATE_STAGED_TOPIC = "update-staged";

var EXPORTED_SYMBOLS = ["UpdatePing"];

/**
 * This module is responsible for listening to all the relevant update
 * signals, gathering the needed information and assembling the "update"
 * ping.
 */
var UpdatePing = {
  _enabled: false,

  earlyInit() {
    this._log = Log.repository.getLoggerWithMessagePrefix(
      LOGGER_NAME,
      "UpdatePing::"
    );
    this._enabled = Services.prefs.getBoolPref(
      TelemetryUtils.Preferences.UpdatePing,
      false
    );

    this._log.trace("init - enabled: " + this._enabled);

    if (!this._enabled) {
      return;
    }

    Services.obs.addObserver(this, UPDATE_DOWNLOADED_TOPIC);
    Services.obs.addObserver(this, UPDATE_STAGED_TOPIC);
  },

  /**
   * Get the information about the update we're going to apply/was just applied
   * from the update manager.
   *
   * @return {nsIUpdate} The information about the update, if available, or null.
   */
  _getActiveUpdate() {
    let updateManager = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    if (!updateManager || !updateManager.activeUpdate) {
      return null;
    }

    return updateManager.activeUpdate;
  },

  /**
   * Generate an "update" ping with reason "success" and dispatch it
   * to the Telemetry system.
   *
   * @param {String} aPreviousVersion The browser version we updated from.
   * @param {String} aPreviousBuildId The browser build id we updated from.
   */
  handleUpdateSuccess(aPreviousVersion, aPreviousBuildId) {
    if (!this._enabled) {
      return;
    }

    this._log.trace("handleUpdateSuccess");

    // An update could potentially change the update channel. Moreover,
    // updates can only be applied if the update's channel matches with the build channel.
    // There's no way to pass this information from the caller nor the environment as,
    // in that case, the environment would report the "new" channel. However, the
    // update manager should still have information about the active update: given the
    // previous assumptions, we can simply get the channel from the update and assume
    // it matches with the state previous to the update.
    let update = this._getActiveUpdate();

    const payload = {
      reason: "success",
      previousChannel: update ? update.channel : null,
      previousVersion: aPreviousVersion,
      previousBuildId: aPreviousBuildId,
    };

    const options = {
      addClientId: true,
      addEnvironment: true,
      usePingSender: false,
    };

    TelemetryController.submitExternalPing(
      PING_TYPE,
      payload,
      options
    ).catch(e =>
      this._log.error("handleUpdateSuccess - failed to submit update ping", e)
    );
  },

  /**
   * Generate an "update" ping with reason "ready" and dispatch it
   * to the Telemetry system.
   *
   * @param {String} aUpdateState The state of the downloaded patch. See
   *        nsIUpdateService.idl for a list of possible values.
   */
  _handleUpdateReady(aUpdateState) {
    const ALLOWED_STATES = [
      "applied",
      "applied-service",
      "pending",
      "pending-service",
      "pending-elevate",
    ];
    if (!ALLOWED_STATES.includes(aUpdateState)) {
      this._log.trace("Unexpected update state: " + aUpdateState);
      return;
    }

    // Get the information about the update we're going to apply from the
    // update manager.
    let update = this._getActiveUpdate();
    if (!update) {
      this._log.trace(
        "Cannot get the update manager or no update is currently active."
      );
      return;
    }

    const payload = {
      reason: "ready",
      targetChannel: update.channel,
      targetVersion: update.appVersion,
      targetBuildId: update.buildID,
      targetDisplayVersion: update.displayVersion,
    };

    const options = {
      addClientId: true,
      addEnvironment: true,
      usePingSender: true,
    };

    TelemetryController.submitExternalPing(
      PING_TYPE,
      payload,
      options
    ).catch(e =>
      this._log.error("_handleUpdateReady - failed to submit update ping", e)
    );
  },

  /**
   * The notifications handler.
   */
  observe(aSubject, aTopic, aData) {
    this._log.trace("observe - aTopic: " + aTopic);
    if (aTopic == UPDATE_DOWNLOADED_TOPIC || aTopic == UPDATE_STAGED_TOPIC) {
      this._handleUpdateReady(aData);
    }
  },

  shutdown() {
    if (!this._enabled) {
      return;
    }
    Services.obs.removeObserver(this, UPDATE_DOWNLOADED_TOPIC);
    Services.obs.removeObserver(this, UPDATE_STAGED_TOPIC);
  },
};
