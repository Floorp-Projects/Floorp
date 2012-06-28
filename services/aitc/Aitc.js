/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-common/preferences.js");

function AitcService() {
  this.aitc = null;
  this.wrappedJSObject = this;
}
AitcService.prototype = {
  classID: Components.ID("{a3d387ca-fd26-44ca-93be-adb5fda5a78d}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver,
                                         Ci.nsISupportsWeakReference]),

  observe: function observe(subject, topic, data) {
    switch (topic) {
      case "app-startup":
        // We listen for this event beacause Aitc won't work until there is
        // atleast 1 visible top-level XUL window.
        Services.obs.addObserver(this, "sessionstore-windows-restored", true);
        break;
      case "sessionstore-windows-restored":
        Services.obs.removeObserver(this, "sessionstore-windows-restored");

        // Don't start AITC if classic sync is on.
        if (Preferences.get("services.sync.engine.apps", false)) {
          return;
        }
        // Start AITC only if it is enabled.
        if (!Preferences.get("services.aitc.enabled", false)) {
          return;
        }

        // Start AITC service only if apps.enabled is true. If false, setup
        // an observer in case the value changes as a result of an access to
        // the DOM API.
        if (Preferences.get("dom.mozApps.used", false)) {
          this.start();
          return;
        }

        // Wait and see if the user wants anything apps related.
        Preferences.observe("dom.mozApps.used", function checkIfEnabled() {
          if (Preferences.get("dom.mozApps.used", false)) {
            Preferences.ignore("dom.mozApps.used", checkIfEnabled, this);
            this.start();
          }
        }, this);
        break;
    }
  },

  start: function start() {
    if (this.aitc) {
      return;
    }

    // Log to stdout if enabled.
    Cu.import("resource://services-aitc/main.js");
    Cu.import("resource://services-common/log4moz.js");
    let root = Log4Moz.repository.getLogger("Service.AITC");
    root.level = Log4Moz.Level[Preferences.get("services.aitc.log.level")];
    if (Preferences.get("services.aitc.log.dump")) {
      root.addAppender(new Log4Moz.DumpAppender());
    }
    this.aitc = new Aitc();
    Services.obs.notifyObservers(null, "service:aitc:started", null);
  },

};

function AboutApps() {
}
AboutApps.prototype = {
  classID: Components.ID("{1de7cbe8-60f1-493e-b56b-9d099b3c018e}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports,
                                         Ci.nsIAboutModule]),

  getURIFlags: function(aURI) {
    return Ci.nsIAboutModule.ALLOW_SCRIPT;
  },

  newChannel: function(aURI) {
    let channel = Services.io.newChannel(
      Preferences.get("services.aitc.dashboard.url"), null, null
    );
    channel.originalURI = aURI;
    return channel;
  }
};

const components = [AitcService, AboutApps];
const NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
