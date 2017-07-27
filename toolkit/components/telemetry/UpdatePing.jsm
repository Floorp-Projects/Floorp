/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Log.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryController",
                                  "resource://gre/modules/TelemetryController.jsm");

const LOGGER_NAME = "Toolkit.Telemetry";
const PING_TYPE = "update";
const UPDATE_DOWNLOADED_TOPIC = "update-downloaded";

this.EXPORTED_SYMBOLS = ["UpdatePing"];

/**
 * This module is responsible for listening to all the relevant update
 * signals, gathering the needed information and assembling the "update"
 * ping.
 */
this.UpdatePing = {
  earlyInit() {
    this._log = Log.repository.getLoggerWithMessagePrefix(LOGGER_NAME, "UpdatePing::");
    this._enabled = Preferences.get(TelemetryUtils.Preferences.UpdatePing, false);

    this._log.trace("init - enabled: " + this._enabled);

    if (!this._enabled) {
      return;
    }

    Services.obs.addObserver(this, UPDATE_DOWNLOADED_TOPIC);
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
      "applied", "applied-service", "pending", "pending-service", "pending-elevate"
    ];
    if (!ALLOWED_STATES.includes(aUpdateState)) {
      this._log.trace("Unexpected update state: " + aUpdateState);
      return;
    }

    // Get the information about the update we're going to apply from the
    // update manager.
    let updateManager =
      Cc["@mozilla.org/updates/update-manager;1"].getService(Ci.nsIUpdateManager);
    if (!updateManager || !updateManager.activeUpdate) {
      this._log.trace("Cannot get the update manager or no update is currently active.");
      return;
    }

    let update = updateManager.activeUpdate;

    const payload = {
      reason: "ready",
      targetChannel: update.channel,
      targetVersion: update.appVersion,
      targetBuildId: update.buildID,
    };

    const options = {
      addClientId: true,
      addEnvironment: true,
      usePingSender: true,
    };

    TelemetryController.submitExternalPing(PING_TYPE, payload, options)
                       .catch(e => this._log.error("_handleUpdateReady - failed to submit update ping", e));
  },

  /**
   * The notifications handler.
   */
  observe(aSubject, aTopic, aData) {
    this._log.trace("observe - aTopic: " + aTopic);
    if (aTopic == UPDATE_DOWNLOADED_TOPIC) {
      this._handleUpdateReady(aData);
    }
  },

  shutdown() {
    if (!this._enabled) {
      return;
    }
    Services.obs.removeObserver(this, UPDATE_DOWNLOADED_TOPIC);
  },
};
