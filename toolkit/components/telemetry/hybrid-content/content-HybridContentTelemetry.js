/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/frame-script */

"use strict";

ChromeUtils.import("resource://gre/modules/Log.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

const TelemetryPrefs = TelemetryUtils.Preferences;

// The permission to be granted to pages that want to use HCT.
const HCT_PERMISSION = "hc_telemetry";
// The message that chrome uses to communicate back to content, when the
// upload policy changes.
const HCT_POLICY_CHANGE_MSG = "HybridContentTelemetry:PolicyChanged";

var HybridContentTelemetryListener = {
  _logger: null,
  _hasListener: false,

  get _log() {
    if (!this._logger) {
      this._logger =
        Log.repository.getLoggerWithMessagePrefix("Toolkit.Telemetry",
                                                  "HybridContentTelemetryListener::");
    }

    return this._logger;
  },

  /**
   * Verifies that the hybrid content telemetry request comes
   * from a trusted origin.
   * @oaram {nsIDomEvent} event Optional object containing the data for the event coming from
   *                      the content. The "detail" section of this event holds the
   *                      hybrid content specific payload. It looks like:
   *                      {name: "apiEndpoint", data: { ... endpoint specific data ... }}
   * @return {Boolean} true if we are on a trusted page, false otherwise.
   */
  isTrustedOrigin(aEvent) {
    // Make sure that events only come from the toplevel frame. We need to check the
    // event's target as |content| always represents the current top level window in
    // the frame (or null). This means that a check like |content.top != content| will
    // always be false. See nsIMessageManager.idl for more info.
    if (aEvent &&
        (!("ownerGlobal" in aEvent.target) ||
          aEvent.target.ownerGlobal != content)) {
      return false;
    }

    const principal =
      aEvent ? aEvent.target.ownerGlobal.document.nodePrincipal : content.document.nodePrincipal;
    if (principal.isSystemPrincipal) {
      return true;
    }

    const allowedSchemes = ["https", "about"];
    if (!allowedSchemes.includes(principal.URI.scheme)) {
      return false;
    }

    // If this is not a system principal, it needs to have the
    // HCT_PERMISSION to use this API.
    let permission =
      Services.perms.testPermissionFromPrincipal(principal, HCT_PERMISSION);
    if (permission == Services.perms.ALLOW_ACTION) {
      return true;
    }

    return false;
  },

  /**
   * Handles incoming events from the content library.
   * This verifies that:
   *
   * - the hybrid content telemetry is on;
   * - the page is trusted and allowed to use it;
   * - the CustomEvent coming from the "content" has the expected format and
   *   is for a known API end-point.
   *
   * If the page is allowed to use the API and the event validates, the call
   * is passed on to Telemetry.
   *
   * @param {nsIDomEvent} event An object containing the data for the event coming from
   *                      the content. The "detail" section of this event holds the
   *                      hybrid content specific payload. It looks like:
   *                      {name: "apiEndpoint", data: { ... endpoint specific data ... }}
   */
  handleEvent(event) {
    if (!this._hybridContentEnabled) {
      this._log.trace("handleEvent - hybrid content telemetry is disabled.");
      return;
    }

    if (!this.isTrustedOrigin(event)) {
      this._log.warn("handleEvent - accessing telemetry from an untrusted origin.");
      return;
    }

    if (!event ||
        !("detail" in event) ||
        !("name" in event.detail) ||
        !("data" in event.detail) ||
        typeof event.detail.name != "string" ||
        typeof event.detail.data != "object") {
      this._log.error("handleEvent - received a malformed message.");
      return;
    }

    // We add the message listener here to guarantee it only gets added
    // for trusted origins.
    // Note that the name of the message must match the name of the one
    // in HybridContentTelemetry.jsm.
    if (!this._hasListener) {
      addMessageListener(HCT_POLICY_CHANGE_MSG, this);
      this._hasListener = true;
    }

    // Note that the name of the async message must match the name of
    // the message in the related listener in nsBrowserGlue.js.
    sendAsyncMessage("HybridContentTelemetry:onTelemetryMessage", {
      name: event.detail.name,
      data: event.detail.data,
    });
  },

  /**
   * Handles the HCT_POLICY_CHANGE_MSG message coming from chrome and
   * passes it back to the content.
   * @param {Object} aMessage The incoming message. See nsIMessageListener docs.
   */
  receiveMessage(aMessage) {
    if (!this.isTrustedOrigin()) {
      this._log.warn("receiveMessage - accessing telemetry from an untrusted origin.");
      return;
    }

    if (aMessage.name != HCT_POLICY_CHANGE_MSG ||
        !("data" in aMessage) ||
        !("canUpload" in aMessage.data) ||
        typeof aMessage.data.canUpload != "boolean") {
      this._log.warn("receiveMessage - received an unexpected message.");
      return;
    }

    // Finally send the message down to the page.
    let event = Cu.cloneInto({
      bubbles: true,
      detail: {canUpload: aMessage.data.canUpload}
    }, content);

    content.document.dispatchEvent(
      new content.document.defaultView.CustomEvent("mozTelemetryPolicyChange", event)
    );
  },
};

XPCOMUtils.defineLazyPreferenceGetter(HybridContentTelemetryListener, "_hybridContentEnabled",
                                      TelemetryUtils.Preferences.HybridContentEnabled,
                                      false /* aDefaultValue */);

// The following function installs the handler for "mozTelemetry"
// events in the chrome. Please note that the name of the message (i.e.
// "mozTelemetry") needs to match the one in HybridContentTelemetry-lib.js.
addEventListener("mozTelemetry", HybridContentTelemetryListener, false, true);
