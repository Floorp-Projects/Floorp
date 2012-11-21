/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://services-common/preferences.js");


const INITIAL_STARTUP_DELAY_MSEC = 10 * 1000;
const BRANCH = "healthreport.";
const JS_PROVIDERS_CATEGORY = "healthreport-js-provider";


/**
 * The Firefox Health Report XPCOM service.
 *
 * This instantiates an instance of HealthReporter (assuming it is enabled)
 * and starts it upon application startup.
 *
 * One can obtain a reference to the underlying HealthReporter instance by
 * accessing .reporter. If this property is null, the reporter isn't running
 * yet or has been disabled.
 */
this.HealthReportService = function HealthReportService() {
  this.wrappedJSObject = this;

  this.prefs = new Preferences(BRANCH);
  this._reporter = null;
}

HealthReportService.prototype = {
  classID: Components.ID("{e354c59b-b252-4040-b6dd-b71864e3e35c}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function observe(subject, topic, data) {
    // If the background service is disabled, don't do anything.
    if (!this.prefs.get("serviceEnabled", true)) {
      return;
    }

    let os = Cc["@mozilla.org/observer-service;1"]
               .getService(Ci.nsIObserverService);

    switch (topic) {
      case "app-startup":
        os.addObserver(this, "final-ui-startup", true);
        break;

      case "final-ui-startup":
        os.removeObserver(this, "final-ui-startup");
        os.addObserver(this, "quit-application", true);

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
        }, INITIAL_STARTUP_DELAY_MSEC, this.timer.TYPE_ONE_SHOT);

        break;

      case "quit-application-granted":
        if (this.reporter) {
          this.reporter.stop();
        }

        os.removeObserver(this, "quit-application");
        break;
    }
  },

  /**
   * The HealthReporter instance associated with this service.
   */
  get reporter() {
    if (!this.prefs.get("serviceEnabled", true)) {
      return null;
    }

    if (this._reporter) {
      return this._reporter;
    }

    // Lazy import so application startup isn't adversely affected.
    let ns = {};
    Cu.import("resource://services-common/log4moz.js", ns);
    Cu.import("resource://gre/modules/services/healthreport/healthreporter.jsm", ns);

    // How many times will we rewrite this code before rolling it up into a
    // generic module? See also bug 451283.
    const LOGGERS = [
      "Metrics",
      "Services.HealthReport",
      "Services.Metrics",
      "Services.BagheeraClient",
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

    this._reporter = new ns.HealthReporter(BRANCH);
    this._reporter.registerProvidersFromCategoryManager(JS_PROVIDERS_CATEGORY);
    this._reporter.start();

    return this._reporter;
  },
};

Object.freeze(HealthReportService.prototype);

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([HealthReportService]);

