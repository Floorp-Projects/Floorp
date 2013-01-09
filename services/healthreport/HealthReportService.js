/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/preferences.js");


const BRANCH = "healthreport.";
const DEFAULT_LOAD_DELAY_MSEC = 10 * 1000;

/**
 * The Firefox Health Report XPCOM service.
 *
 * External consumers will be interested in the "reporter" property of this
 * service. This property is a `HealthReporter` instance that powers the
 * service. The property may be null if the Health Report service is not
 * enabled.
 *
 * EXAMPLE USAGE
 * =============
 *
 * let reporter = Cc["@mozilla.org/healthreport/service;1"]
 *                  .getService(Ci.nsISupports)
 *                  .wrappedJSObject
 *                  .reporter;
 *
 * if (reporter.haveRemoteData) {
 *   // ...
 * }
 *
 * IMPLEMENTATION NOTES
 * ====================
 *
 * In order to not adversely impact application start time, the `HealthReporter`
 * instance is not initialized until a few seconds after "final-ui-startup."
 * The exact delay is configurable via preferences so it can be adjusted with
 * a hotfix extension if the default value is ever problematic.
 *
 * Shutdown of the `HealthReporter` instance is handled completely within the
 * instance (it registers observers on initialization). See the notes on that
 * type for more.
 */
this.HealthReportService = function HealthReportService() {
  this.wrappedJSObject = this;

  this._prefs = new Preferences(BRANCH);

  this._reporter = null;
}

HealthReportService.prototype = {
  classID: Components.ID("{e354c59b-b252-4040-b6dd-b71864e3e35c}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function observe(subject, topic, data) {
    // If the background service is disabled, don't do anything.
    if (!this._prefs.get("service.enabled", true)) {
      return;
    }

    let os = Cc["@mozilla.org/observer-service;1"]
               .getService(Ci.nsIObserverService);

    switch (topic) {
      case "app-startup":
        os.addObserver(this, "sessionstore-windows-restored", true);
        break;

      case "sessionstore-windows-restored":
        os.removeObserver(this, "sessionstore-windows-restored");

        let delayInterval = this._prefs.get("service.loadDelayMsec") ||
                            DEFAULT_LOAD_DELAY_MSEC;

        // Delay service loading a little more so things have an opportunity
        // to cool down first.
        this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        this.timer.initWithCallback({
          notify: function notify() {
            // Side effect: instantiates the reporter instance if not already
            // accessed.
            let reporter = this.reporter;
            delete this.timer;
          }.bind(this),
        }, delayInterval, this.timer.TYPE_ONE_SHOT);

        break;
    }
  },

  /**
   * The HealthReporter instance associated with this service.
   *
   * If the service is disabled, this will return null.
   *
   * The obtained instance may not be fully initialized.
   */
  get reporter() {
    if (!this._prefs.get("service.enabled", true)) {
      return null;
    }

    if (this._reporter) {
      return this._reporter;
    }

    let ns = {};
    // Lazy import so application startup isn't adversely affected.
    Cu.import("resource://gre/modules/Task.jsm", ns);
    Cu.import("resource://gre/modules/services/healthreport/healthreporter.jsm", ns);
    Cu.import("resource://services-common/log4moz.js", ns);

    // How many times will we rewrite this code before rolling it up into a
    // generic module? See also bug 451283.
    const LOGGERS = [
      "Services.HealthReport",
      "Services.Metrics",
      "Services.BagheeraClient",
      "Sqlite.Connection.healthreport",
    ];

    let prefs = new Preferences(BRANCH + "logging.");
    if (prefs.get("consoleEnabled", true)) {
      let level = prefs.get("consoleLevel", "Warn");
      let appender = new ns.Log4Moz.ConsoleAppender();
      appender.level = ns.Log4Moz.Level[level] || ns.Log4Moz.Level.Warn;

      for (let name of LOGGERS) {
        let logger = ns.Log4Moz.repository.getLogger(name);
        logger.addAppender(appender);
      }
    }

    // The reporter initializes in the background.
    this._reporter = new ns.HealthReporter(BRANCH);

    return this._reporter;
  },
};

Object.freeze(HealthReportService.prototype);

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HealthReportService]);

