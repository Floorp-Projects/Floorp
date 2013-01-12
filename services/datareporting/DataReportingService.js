/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/services/datareporting/policy.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/observers.js");
Cu.import("resource://services-common/preferences.js");


const ROOT_BRANCH = "datareporting.";
const POLICY_BRANCH = ROOT_BRANCH + "policy.";
const HEALTHREPORT_BRANCH = ROOT_BRANCH + "healthreport.";
const HEALTHREPORT_LOGGING_BRANCH = HEALTHREPORT_BRANCH + "logging.";
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
 *                  .healthReporter;
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
this.DataReportingService = function () {
  this.wrappedJSObject = this;

  this._os = Cc["@mozilla.org/observer-service;1"]
               .getService(Ci.nsIObserverService);
}

DataReportingService.prototype = Object.freeze({
  classID: Components.ID("{41f6ae36-a79f-4613-9ac3-915e70f83789}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  //---------------------------------------------
  // Start of policy listeners.
  //---------------------------------------------

  /**
   * Called when policy requests data upload.
   */
  onRequestDataUpload: function (request) {
    if (!this.healthReporter) {
      return;
    }

    this.healthReporter.requestDataUpload(request);
  },

  onNotifyDataPolicy: function (request) {
    Observers.notify("datareporting:notify-data-policy:request", request);
  },

  onRequestRemoteDelete: function (request) {
    if (!this.healthReporter) {
      return;
    }

    this.healthReporter.deleteRemoteData(request);
  },

  //---------------------------------------------
  // End of policy listeners.
  //---------------------------------------------

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "app-startup":
        this._os.addObserver(this, "profile-after-change", true);
        break;

      case "profile-after-change":
        this._os.removeObserver(this, "profile-after-change");
        this._os.addObserver(this, "sessionstore-windows-restored", true);

        // We can't interact with prefs until after the profile is present.
        let policyPrefs = new Preferences(POLICY_BRANCH);
        this._prefs = new Preferences(HEALTHREPORT_BRANCH);
        this.policy = new DataReportingPolicy(policyPrefs, this._prefs, this);
        break;

      case "sessionstore-windows-restored":
        this._os.removeObserver(this, "sessionstore-windows-restored");
        this._os.addObserver(this, "quit-application", false);

        this.policy.startPolling();

        // Don't initialize Firefox Health Reporter collection and submission
        // service unless it is enabled.
        if (!this._prefs.get("service.enabled", true)) {
          return;
        }

        let delayInterval = this._prefs.get("service.loadDelayMsec") ||
                            DEFAULT_LOAD_DELAY_MSEC;

        // Delay service loading a little more so things have an opportunity
        // to cool down first.
        this.timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        this.timer.initWithCallback({
          notify: function notify() {
            // Side effect: instantiates the reporter instance if not already
            // accessed.
            let reporter = this.healthReporter;
            delete this.timer;
          }.bind(this),
        }, delayInterval, this.timer.TYPE_ONE_SHOT);

        break;

      case "quit-application":
        this._os.removeObserver(this, "quit-application");
        this.policy.stopPolling();
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
  get healthReporter() {
    if (!this._prefs.get("service.enabled", true)) {
      return null;
    }

    if ("_healthReporter" in this) {
      return this._healthReporter;
    }

    try {
      this._loadHealthReporter();
    } catch (ex) {
      this._healthReporter = null;
    }

    return this._healthReporter;
  },

  _loadHealthReporter: function () {
    let ns = {};
    // Lazy import so application startup isn't adversely affected.

    Cu.import("resource://gre/modules/Task.jsm", ns);
    Cu.import("resource://gre/modules/services/healthreport/healthreporter.jsm", ns);
    Cu.import("resource://services-common/log4moz.js", ns);

    // How many times will we rewrite this code before rolling it up into a
    // generic module? See also bug 451283.
    const LOGGERS = [
      "Services.DataReporting",
      "Services.HealthReport",
      "Services.Metrics",
      "Services.BagheeraClient",
      "Sqlite.Connection.healthreport",
    ];

    let loggingPrefs = new Preferences(HEALTHREPORT_LOGGING_BRANCH);
    if (loggingPrefs.get("consoleEnabled", true)) {
      let level = loggingPrefs.get("consoleLevel", "Warn");
      let appender = new ns.Log4Moz.ConsoleAppender();
      appender.level = ns.Log4Moz.Level[level] || ns.Log4Moz.Level.Warn;

      for (let name of LOGGERS) {
        let logger = ns.Log4Moz.repository.getLogger(name);
        logger.addAppender(appender);
      }
    }

    // The reporter initializes in the background.
    this._healthReporter = new ns.HealthReporter(HEALTHREPORT_BRANCH,
                                                 this.policy);
  },
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([DataReportingService]);

