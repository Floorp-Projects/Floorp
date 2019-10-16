/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// FxA Telemetry support. For hysterical raisins, the actual implementation
// is inside "sync". We should move the core implementation somewhere that's
// sanely shared (eg, services-common?), but let's wait and see where we end up
// first...

// We use this observers module because we leverage its support for richer
// "subject" data.
const { Observers } = ChromeUtils.import(
  "resource://services-common/observers.js"
);

class FxAccountsTelemetry {
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
}

var EXPORTED_SYMBOLS = ["FxAccountsTelemetry"];
