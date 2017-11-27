/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["HybridContentTelemetry"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/TelemetryUtils.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

let HybridContentTelemetry = {
  _logger: null,
  _observerInstalled: false,

  get _log() {
    if (!this._logger) {
      this._logger =
        Log.repository.getLoggerWithMessagePrefix("Toolkit.Telemetry",
                                                  "HybridContentTelemetry::");
    }

    return this._logger;
  },

  /**
   * Lazily initialized observer for the Telemetry upload preference. This is
   * only ever executed if a page uses hybrid content telemetry and has enough
   * privileges to run it.
   */
  _lazyObserverInit() {
    if (this._observerInstalled) {
      // We only want to install the observers once, if needed.
      return;
    }
    this._log.trace("_lazyObserverInit - installing the pref observers.");
    XPCOMUtils.defineLazyPreferenceGetter(this, "_uploadEnabled",
                                          TelemetryUtils.Preferences.FhrUploadEnabled,
                                          false, /* aDefaultValue */
                                          () => this._broadcastPolicyUpdate());
    this._observerInstalled = true;
  },

  /**
   * This is the handler for the async "HybridContentTelemetry:onTelemetryMessage"
   * message. This function is getting called by the listener in nsBrowserGlue.js.
   */
  onTelemetryMessage(aMessage, aData) {
    if (!this._hybridContentEnabled) {
      this._log.trace("onTelemetryMessage - hybrid content telemetry is disabled.");
      return;
    }

    this._log.trace("onTelemetryMessage - Message received, dispatching API call.");
    if (!aData ||
        !("data" in aData) ||
        !("name" in aData) ||
        typeof aData.name != "string" ||
        typeof aData.data != "object") {
      this._log.error("onTelemetryMessage - received a malformed message.");
      return;
    }
    this._dispatchAPICall(aData.name, aData.data, aMessage);
  },

  /**
   * Broadcast the upload policy state to the pages using hybrid
   * content telemetry.
   */
  _broadcastPolicyUpdate() {
    this._log.trace(`_broadcastPolicyUpdate - New value is ${this._uploadEnabled}.`);
    Services.mm.broadcastAsyncMessage("HybridContentTelemetry:PolicyChanged",
                                      {canUpload: this._uploadEnabled});
  },

  /**
   * Dispatches the calls to the Telemetry service.
   * @param {String} aEndpoint The name of the api endpoint to call.
   * @param {Object} aData An object containing the data to pass to the API.
   * @param {Object} aOriginalMessage The message object coming from the listener.
   */
  _dispatchAPICall(aEndpoint, aData, aOriginalMessage) {
    this._log.info(`_dispatchAPICall - processing "${aEndpoint}".`);

    // We don't really care too much about validating the passed parameters.
    // Telemetry will take care of that for us so there is little gain from
    // duplicating that logic here. Just make sure we don't throw and report
    // any error.
    try {
      switch (aEndpoint) {
        case "init":
            this._lazyObserverInit();
            this._broadcastPolicyUpdate();
            break;
        case "registerEvents":
            Services.telemetry.registerEvents(aData.category, aData.eventData);
          break;
        case "recordEvent":
            // Don't pass "undefined" for the optional |value| and |extra|:
            // the Telemetry API expects them to be "null" if something is being
            // passed.
            let check = (data, key) => (key in data && typeof data[key] != "undefined");
            Services.telemetry.recordEvent(aData.category,
                                           aData.method,
                                           aData.object,
                                           check(aData, "value") ? aData.value : null,
                                           check(aData, "extra") ? aData.extra : null);
          break;
        default:
          this._log.error(`_dispatchAPICall - unknown "${aEndpoint}"" API call.`);
      }
    } catch (e) {
      this._log.error(`_dispatchAPICall - error executing "${aEndpoint}".`, e);
    }
  },
};

XPCOMUtils.defineLazyPreferenceGetter(HybridContentTelemetry, "_hybridContentEnabled",
                                      TelemetryUtils.Preferences.HybridContentEnabled,
                                      false /* aDefaultValue */);
