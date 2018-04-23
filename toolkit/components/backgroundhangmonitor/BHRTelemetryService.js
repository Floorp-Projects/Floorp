/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);

ChromeUtils.defineModuleGetter(this, "TelemetryController",
                               "resource://gre/modules/TelemetryController.jsm");

function BHRTelemetryService() {
  // Allow tests to get access to this object to verify it works correctly.
  this.wrappedJSObject = this;

  Services.obs.addObserver(this, "profile-before-change");
  Services.obs.addObserver(this, "bhr-thread-hang");
  Services.obs.addObserver(this, "idle-daily");

  this.resetPayload();
}

BHRTelemetryService.prototype = Object.freeze({
  classID: Components.ID("{117c8cdf-69e6-4f31-a439-b8a654c67127}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  TRANSMIT_HANG_COUNT: 50,

  resetPayload() {
    this.startTime = +new Date();
    this.payload = {
      modules: [],
      hangs: [],
    };
  },

  recordHang({duration, thread, runnableName, process, stack,
              remoteType, modules, annotations}) {
    if (!Services.telemetry.canRecordExtended) {
      return;
    }

    // Create a mapping from module indicies in the original nsIHangDetails
    // object to this.payload.modules indicies.
    let moduleIdxs = modules.map(module => {
      let idx = this.payload.modules.findIndex(m => {
        return m[0] === module[0] && m[1] === module[1];
      });
      if (idx === -1) {
        idx = this.payload.modules.length;
        this.payload.modules.push(module);
      }
      return idx;
    });

    // Native stack frames are [modIdx, offset] arrays. If we have a valid
    // module index, we want to map it to the this.payload.modules array.
    for (let i = 0; i < stack.length; ++i) {
      if (Array.isArray(stack[i]) && stack[i][0] !== -1) {
        stack[i][0] = moduleIdxs[stack[i][0]];
      }
    }

    // Create the hang object to record in the payload.
    this.payload.hangs.push({
      duration,
      thread,
      runnableName,
      process,
      remoteType,
      annotations,
      stack,
    });

    // If we have collected enough hangs, we can submit the hangs we have
    // collected to telemetry.
    if (this.payload.hangs.length > this.TRANSMIT_HANG_COUNT) {
      this.submit();
    }
  },

  submit() {
    if (!Services.telemetry.canRecordExtended) {
      return;
    }

    // NOTE: We check a separate bhrPing.enabled pref here. This pref is unset
    // when running tests so that we run as much of BHR as possible (to catch
    // errors) while avoiding timeouts caused by invoking `pingsender` during
    // testing.
    if (Services.prefs.getBoolPref("toolkit.telemetry.bhrPing.enabled", false)) {
      this.payload.timeSinceLastPing = new Date() - this.startTime;
      TelemetryController.submitExternalPing("bhr", this.payload, {
        addEnvironment: true,
      });
    }
    this.resetPayload();
  },

  shutdown() {
    Services.obs.removeObserver(this, "profile-before-change");
    Services.obs.removeObserver(this, "bhr-thread-hang");
    Services.obs.removeObserver(this, "idle-daily");
    this.submit();
  },

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
    case "profile-after-change":
      this.resetPayload();
      break;
    case "bhr-thread-hang":
      this.recordHang(aSubject.QueryInterface(Ci.nsIHangDetails));
      break;
    case "profile-before-change":
      this.shutdown();
      break;
    case "idle-daily":
      this.submit();
      break;
    }
  },
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([BHRTelemetryService]);
