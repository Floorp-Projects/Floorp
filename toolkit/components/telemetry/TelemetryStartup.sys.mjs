/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "TelemetryController",
  "resource://gre/modules/TelemetryController.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "TelemetryEnvironment",
  "resource://gre/modules/TelemetryEnvironment.jsm"
);

/**
 * TelemetryStartup is needed to forward the "profile-after-change" notification
 * to TelemetryController.jsm.
 */
export function TelemetryStartup() {}

TelemetryStartup.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIObserver",
]);
TelemetryStartup.prototype.observe = function(aSubject, aTopic, aData) {
  if (aTopic == "profile-after-change") {
    // In the content process, this is done in ContentProcessSingleton.js.
    lazy.TelemetryController.observe(null, aTopic, null);
  }
  if (aTopic == "profile-after-change") {
    annotateEnvironment();
    lazy.TelemetryEnvironment.registerChangeListener(
      "CrashAnnotator",
      annotateEnvironment
    );
    lazy.TelemetryEnvironment.onInitialized().then(() => annotateEnvironment());
  }
};

function annotateEnvironment() {
  try {
    let env = JSON.stringify(lazy.TelemetryEnvironment.currentEnvironment);
    Services.appinfo.annotateCrashReport("TelemetryEnvironment", env);
  } catch (e) {
    // crash reporting not built or disabled? Ignore errors
  }
}
