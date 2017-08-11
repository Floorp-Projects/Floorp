/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

XPCOMUtils.defineLazyModuleGetter(this, "TelemetryController",
                                  "resource://gre/modules/TelemetryController.jsm");

function BHRTelemetryService() {
  Services.obs.addObserver(this, "profile-before-change");
  Services.obs.addObserver(this, "xpcom-shutdown");
  Services.obs.addObserver(this, "bhr-thread-hang");

  this.resetPayload();
};

BHRTelemetryService.prototype = Object.freeze({
  classID: Components.ID("{117c8cdf-69e6-4f31-a439-b8a654c67127}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  TRANSMIT_HANG_COUNT: 100,

  resetPayload() {
    this.payload = {
      modules: [],
      hangs: [],
    };
  },

  recordHang({duration, thread, runnableName, process, stack,
              modules, annotations, pseudoStack}) {
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

    // Create the hang object to record in the payload.
    this.payload.hangs.push({
      duration,
      thread,
      runnableName,
      process,
      annotations,
      pseudoStack,
      stack: stack.map(frame => {
        // Native stack frames are [modIdx, offset] arrays. If we have a valid
        // module index, we want to map it to the this.payload.modules array.
        if (Array.isArray(frame) && frame[0] !== -1) {
          return [moduleIdxs[frame[0]], frame[1]];
        }
        return frame;
      }),
    });

    // If we have collected enough hangs, we can submit the hangs we have
    // collected to telemetry.
    if (this.payload.hangs.length > this.SUBMIT_HANG_COUNT) {
      this.submit();
    }
  },

  submit() {
    if (!Services.telemetry.canRecordExtended) {
      return;
    }

    if (this.payload.hangs.length > 0) {
      TelemetryController.submitExternalPing("bhr", this.payload, {
        addEnvironment: true,
      });
    }
    this.resetPayload();
  },

  shutdown() {
    Services.obs.removeObserver(this, "profile-before-change");
    Services.obs.removeObserver(this, "xpcom-shutdown");
    Services.obs.removeObserver(this, "bhr-thread-hang");
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
      this.submit();
      break;
    case "xpcom-shutdown":
      this.shutdown();
      break;
    }
  },
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([BHRTelemetryService]);
