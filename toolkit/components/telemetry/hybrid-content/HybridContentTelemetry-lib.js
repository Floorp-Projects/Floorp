/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

if (typeof Mozilla == "undefined") {
  var Mozilla = {};
}

(function($) {
  "use strict";

  var _canUpload = false;
  var _initPromise = null;

  if (typeof Mozilla.ContentTelemetry == "undefined") {
    /**
     * Library that exposes an event-based Web API for communicating with the
     * desktop browser chrome. It can be used for recording Telemetry data from
     * authorized web content pages.
     *
     * <p>For security/privacy reasons `Mozilla.ContentTelemetry` will only work
     * on a list of allowed secure origins. The list of allowed origins can be
     * found in
     * {@link https://dxr.mozilla.org/mozilla-central/source/browser/app/permissions|
     * browser/app/permissions}.</p>
     *
     * @since 59
     * @namespace
     */
    Mozilla.ContentTelemetry = {};
  }

  function _sendMessageToChrome(name, data) {
    var event = new CustomEvent("mozTelemetry", {
      bubbles: true,
      detail: {
        name,
        data: data || {}
      }
    });

    document.dispatchEvent(event);
  }

  /**
   * This internal function is used to register the policy handler. This is
   * needed by pages that do not want to use Telemetry but still need to
   * respect user Privacy choices.
   */
  function _registerInternalPolicyHandler() {
    // Create a promise to wait on for HCT to be completely initialized.
    _initPromise = new Promise(resolveInit => {
      // Register the handler that will update the policy boolean.
      function policyChangeHandler(updatedPref) {
        if (!("detail" in updatedPref) ||
            !("canUpload" in updatedPref.detail) ||
            typeof updatedPref.detail.canUpload != "boolean") {
          return;
        }
        _canUpload = updatedPref.detail.canUpload;
        // Resolve the setup promise the first time we receive a message
        // from the chrome.
        resolveInit();
      }
      document.addEventListener("mozTelemetryPolicyChange", policyChangeHandler);
    });

    // Make sure the chrome is initialized.
    _sendMessageToChrome("init");
  }

  Mozilla.ContentTelemetry.canUpload = function() {
    return _canUpload;
  };

  Mozilla.ContentTelemetry.initPromise = function() {
    return _initPromise;
  };

  Mozilla.ContentTelemetry.registerEvents = function(category, eventData) {
    _sendMessageToChrome("registerEvents", { category, eventData });
  };

  Mozilla.ContentTelemetry.recordEvent = function(category, method, object, value, extra) {
    _sendMessageToChrome("recordEvent", { category, method, object, value, extra });
  };

  // Register the policy handler so that |canUpload| is always up to date.
  _registerInternalPolicyHandler();
})();

// Make this library Require-able.
/* eslint-env commonjs */
if (typeof module !== "undefined" && module.exports) {
  module.exports = Mozilla.ContentTelemetry;
}
