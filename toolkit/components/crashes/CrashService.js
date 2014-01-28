/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);

this.CrashService = function () {};

CrashService.prototype = Object.freeze({
  classID: Components.ID("{92668367-1b17-4190-86b2-1061b2179744}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function (subject, topic, data) {
    switch (topic) {
      case "profile-after-change":
        // Side-effect is the singleton is instantiated.
        let m = Services.crashmanager;
        break;
    }
  },
});

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([CrashService]);
