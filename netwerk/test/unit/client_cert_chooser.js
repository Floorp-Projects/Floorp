// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

const { XPCOMUtils } = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm", {});

var Prompter = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrompt]),
  alert: function() {} // Do nothing when asked to show an alert
};

function WindowWatcherService() {}
WindowWatcherService.prototype = {
  classID: Components.ID("{01ae923c-81bb-45db-b860-d423b0fc4fe1}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIWindowWatcher]),

  getNewPrompter: function() {
    return Prompter;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([
  WindowWatcherService
]);
