/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict;"

// Note that this module used to supervise the step-by-step migration from
// a legacy Sync account to a FxA-based Sync account. In bug 1205928, this
// changed to automatically disconnect the legacy Sync account.

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "WeaveService", function() {
  return Cc["@mozilla.org/weave/service;1"]
         .getService(Components.interfaces.nsISupports)
         .wrappedJSObject;
});

XPCOMUtils.defineLazyModuleGetter(this, "Weave",
  "resource://services-sync/main.js");

// We send this notification when we perform the disconnection. The browser
// window will show a one-off notification bar.
const OBSERVER_STATE_CHANGE_TOPIC = "fxa-migration:state-changed";

const OBSERVER_TOPICS = [
  "xpcom-shutdown",
  "weave:eol",
];

function Migrator() {
  // Leave the log-level as Debug - Sync will setup log appenders such that
  // these messages generally will not be seen unless other log related
  // prefs are set.
  this.log.level = Log.Level.Debug;

  for (let topic of OBSERVER_TOPICS) {
    Services.obs.addObserver(this, topic, false);
  }
}

Migrator.prototype = {
  log: Log.repository.getLogger("Sync.SyncMigration"),

  finalize() {
    for (let topic of OBSERVER_TOPICS) {
      Services.obs.removeObserver(this, topic);
    }
  },

  observe(subject, topic, data) {
    this.log.debug("observed " + topic);
    switch (topic) {
      case "xpcom-shutdown":
        this.finalize();
        break;

      default:
        // this notification when configured with legacy Sync means we want to
        // disconnect
        if (!WeaveService.fxAccountsEnabled) {
          this.log.info("Disconnecting from legacy Sync");
          // Set up an observer for when the disconnection is complete.
          let observe;
          Services.obs.addObserver(observe = () => {
            this.log.info("observed that startOver is complete");
            Services.obs.removeObserver(observe, "weave:service:start-over:finish");
            // Send the notification for the UI.
            Services.obs.notifyObservers(null, OBSERVER_STATE_CHANGE_TOPIC, null);
          }, "weave:service:start-over:finish", false);

          // Do the disconnection.
          Weave.Service.startOver();
        }
    }
  },

  get learnMoreLink() {
    try {
      var url = Services.prefs.getCharPref("app.support.baseURL");
    } catch (err) {
      return null;
    }
    url += "sync-upgrade";
    let sb = Services.strings.createBundle("chrome://weave/locale/services/sync.properties");
    return {
      text: sb.GetStringFromName("sync.eol.learnMore.label"),
      href: Services.urlFormatter.formatURL(url),
    };
  },
};

// We expose a singleton
this.EXPORTED_SYMBOLS = ["fxaMigrator"];
var fxaMigrator = new Migrator();
