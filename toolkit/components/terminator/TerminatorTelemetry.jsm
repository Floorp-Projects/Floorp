/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Read the data saved by nsTerminator during shutdown and feed it to the
 * relevant telemetry histograms.
 */

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

function nsTerminatorTelemetry() {}

var HISTOGRAMS = {
  "quit-application": "SHUTDOWN_PHASE_DURATION_TICKS_QUIT_APPLICATION",
  "profile-change-net-teardown":
    "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_CHANGE_NET_TEARDOWN",
  "profile-change-teardown":
    "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_CHANGE_TEARDOWN",
  "profile-before-change":
    "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_BEFORE_CHANGE",
  "profile-before-change-qm":
    "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_BEFORE_CHANGE_QM",
  "xpcom-will-shutdown": "SHUTDOWN_PHASE_DURATION_TICKS_XPCOM_WILL_SHUTDOWN",
  "xpcom-shutdown": "SHUTDOWN_PHASE_DURATION_TICKS_XPCOM_SHUTDOWN",
};

nsTerminatorTelemetry.prototype = {
  classID: Components.ID("{3f78ada1-cba2-442a-82dd-d5fb300ddea7}"),

  // nsISupports

  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  // nsIObserver

  observe: function DS_observe(aSubject, aTopic, aData) {
    (async function() {
      //
      // This data is hardly critical, reading it can wait for a few seconds.
      //
      await new Promise(resolve => lazy.setTimeout(resolve, 3000));

      let PATH = PathUtils.join(
        Services.dirsvc.get("ProfLD", Ci.nsIFile).path,
        "ShutdownDuration.json"
      );
      let data;
      try {
        data = await IOUtils.readJSON(PATH);
      } catch (ex) {
        if (DOMException.isInstance(ex) && ex.name == "NotFoundError") {
          return;
        }
        // Let other errors be reported by Promise's error-reporting.
        throw ex;
      }

      // Clean up
      await IOUtils.remove(PATH);
      await IOUtils.remove(PATH + ".tmp");

      for (let k of Object.keys(data)) {
        let id = HISTOGRAMS[k];
        try {
          let histogram = Services.telemetry.getHistogramById(id);
          if (!histogram) {
            throw new Error("Unknown histogram " + id);
          }

          histogram.add(Number.parseInt(data[k]));
        } catch (ex) {
          // Make sure that the error is reported and causes test failures,
          // but otherwise, ignore it.
          Promise.reject(ex);
          continue;
        }
      }

      // Inform observers that we are done.
      Services.obs.notifyObservers(
        null,
        "shutdown-terminator-telemetry-updated"
      );
    })();
  },
};

// Module

var EXPORTED_SYMBOLS = ["nsTerminatorTelemetry"];
