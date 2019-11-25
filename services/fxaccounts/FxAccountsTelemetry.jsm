/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// FxA Telemetry support. For hysterical raisins, the actual implementation
// is inside "sync". We should move the core implementation somewhere that's
// sanely shared (eg, services-common?), but let's wait and see where we end up
// first...

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  log: "resource://gre/modules/FxAccountsCommon.js",
  // We use this observers module because we leverage its support for richer
  // "subject" data.
  Observers: "resource://services-common/observers.js",
  Services: "resource://gre/modules/Services.jsm",
});

class FxAccountsTelemetry {
  constructor(fxai) {
    this._fxai = fxai;
    Services.telemetry.setEventRecordingEnabled("fxa", true);
  }

  // Records an event *in the Fxa/Sync ping*.
  recordEvent(object, method, value, extra = undefined) {
    // We need to ensure the telemetry module is loaded.
    ChromeUtils.import("resource://services-sync/telemetry.js");
    // Now it will be listening for the notifications...
    Observers.notify("fxa:telemetry:event", { object, method, value, extra });
  }

  // A flow ID can be anything that's "probably" unique, so for now use a UUID.
  generateFlowID() {
    return Cc["@mozilla.org/uuid-generator;1"]
      .getService(Ci.nsIUUIDGenerator)
      .generateUUID()
      .toString()
      .slice(1, -1);
  }

  // Sanitize the ID of a device into something suitable for including in the
  // ping. Returns null if no transformation is possible.
  sanitizeDeviceId(deviceId) {
    // We only know how to hash it for sync users, which kinda sucks.
    let xps =
      this._weaveXPCOM ||
      Cc["@mozilla.org/weave/service;1"].getService(Ci.nsISupports)
        .wrappedJSObject;
    if (!xps.enabled) {
      return null;
    }
    try {
      return xps.Weave.Service.identity.hashedDeviceID(deviceId);
    } catch {
      // sadly this can happen in various scenarios, so don't complain.
    }
    return null;
  }

  // Record the connection of FxA or one of its services.
  // Note that you must call this before performing the actual connection
  // or we may record incorrect data - for example, we will not be able to
  // determine whether FxA itself was connected before this call.
  //
  // Currently sends an event in the main telemetry event ping rather than the
  // FxA/Sync ping (although this might change in the future)
  //
  // @param services - An array of service names which should be recorded. FxA
  //  itself is not counted as a "service" - ie, an empty array should be passed
  //  if the account is connected without anything else .
  //
  // @param how - How the connection was done.
  async recordConnection(services, how = null) {
    try {
      let extra = {};
      // Record that fxa was connected if it isn't currently - it will be soon.
      if (!(await this._fxai.getUserAccountData())) {
        extra.fxa = "true";
      }
      // Events.yaml only declares "sync" as a valid service.
      if (services.includes("sync")) {
        extra.sync = "true";
      }
      Services.telemetry.recordEvent("fxa", "connect", "account", how, extra);
    } catch (ex) {
      log.error("Failed to record connection telemetry", ex);
      console.error("Failed to record connection telemetry", ex);
    }
  }

  // Record the disconnection of FxA or one of its services.
  // Note that you must call this before performing the actual disconnection
  // or we may record incomplete data - for example, if this is called after
  // disconnection, we've almost certainly lost the ability to record what
  // services were enabled prior to disconnection.
  //
  // Currently sends an event in the main telemetry event ping rather than the
  // FxA/Sync ping (although this might change in the future)
  //
  // @param service - the service being disconnected. If null, the account
  // itself is being disconnected, so all connected services are too.
  //
  // @param how - how the disconnection was done.
  async recordDisconnection(service = null, how = null) {
    try {
      let extra = {};
      if (!service) {
        extra.fxa = "true";
        // We need a way to enumerate all services - but for now we just hard-code
        // all possibilities here.
        if (Services.prefs.prefHasUserValue("services.sync.username")) {
          extra.sync = "true";
        }
      } else if (service == "sync") {
        extra[service] = "true";
      } else {
        // Events.yaml only declares "sync" as a valid service.
        log.warn(
          `recordDisconnection has invalid value for service: ${service}`
        );
      }
      Services.telemetry.recordEvent(
        "fxa",
        "disconnect",
        "account",
        how,
        extra
      );
    } catch (ex) {
      log.error("Failed to record disconnection telemetry", ex);
      console.error("Failed to record disconnection telemetry", ex);
    }
  }
}

var EXPORTED_SYMBOLS = ["FxAccountsTelemetry"];
