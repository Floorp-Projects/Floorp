/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/UserAgentOverrides.jsm");

function UserAgentOverridesComponent() {
}

UserAgentOverridesComponent.prototype = {
  init: function uaoc_init() {
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    UserAgentOverrides.init();
  },

  observe: function uaoc_observe(aSubject, aTopic, aData) {
    if (aTopic == "app-startup") {
      this.init();
    } else if (aTopic == "xpcom-shutdown") {
      Services.obs.removeObserver(this, "xpcom-shutdown");
      UserAgentOverrides.uninit();
    }
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
  classID: Components.ID("{965b0ca8-155b-11e7-93ae-92361f002671}")
};

const components = [UserAgentOverridesComponent];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
