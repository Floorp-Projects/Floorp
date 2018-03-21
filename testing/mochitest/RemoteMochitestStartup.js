/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
function RemoteMochitestStartup() {}

RemoteMochitestStartup.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
  classID: Components.ID("{791cb384-ba12-464e-82cd-d0e269da1b8f}"),

  observe: function (subject, topic, data) {
    switch (topic) {
      case "profile-after-change":
        Services.obs.addObserver(this, "chrome-document-global-created");
        break;
      case "chrome-document-global-created":
        subject.addEventListener("DOMContentLoaded", function () {
            let win = this;
            if (win.location.href !== "chrome://browser/content/browser.xul") {
                return;
            }
            Services.scriptloader.loadSubScript("chrome://mochikit/content/chrome-harness.js", win);
            Services.scriptloader.loadSubScript("chrome://mochikit/content/mochitest-e10s-utils.js", win);
            Services.scriptloader.loadSubScript("chrome://mochikit/content/browser-test.js", win);
        }, {once: true});
        break;
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([RemoteMochitestStartup]);
