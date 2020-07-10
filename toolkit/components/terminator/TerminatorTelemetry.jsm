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

const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);

ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

function nsTerminatorTelemetry() {}

var HISTOGRAMS = {
  "quit-application": "SHUTDOWN_PHASE_DURATION_TICKS_QUIT_APPLICATION",
  "profile-change-teardown":
    "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_CHANGE_TEARDOWN",
  "profile-before-change":
    "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_BEFORE_CHANGE",
  "xpcom-will-shutdown": "SHUTDOWN_PHASE_DURATION_TICKS_XPCOM_WILL_SHUTDOWN",
};

nsTerminatorTelemetry.prototype = {
  classID: Components.ID("{3f78ada1-cba2-442a-82dd-d5fb300ddea7}"),

  _xpcom_factory: ComponentUtils.generateSingletonFactory(
    nsTerminatorTelemetry
  ),

  // nsISupports

  QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

  // nsIObserver

  observe: function DS_observe(aSubject, aTopic, aData) {
    (async function() {
      //
      // This data is hardly critical, reading it can wait for a few seconds.
      //
      await new Promise(resolve => setTimeout(resolve, 3000));

      let PATH = OS.Path.join(
        OS.Constants.Path.localProfileDir,
        "ShutdownDuration.json"
      );
      let raw;
      try {
        raw = await OS.File.read(PATH, { encoding: "utf-8" });
      } catch (ex) {
        if (!ex.becauseNoSuchFile) {
          throw ex;
        }
        return;
      }
      // Let other errors be reported by Promise's error-reporting.

      // Clean up
      OS.File.remove(PATH);
      OS.File.remove(PATH + ".tmp");

      let data = JSON.parse(raw);
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
