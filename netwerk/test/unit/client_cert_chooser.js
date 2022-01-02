// -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

"use strict";

const { ComponentUtils } = ChromeUtils.import(
  "resource://gre/modules/ComponentUtils.jsm"
);

var Prompter = {
  QueryInterface: ChromeUtils.generateQI(["nsIPrompt"]),
  alert() {}, // Do nothing when asked to show an alert
};

function WindowWatcherService() {}
WindowWatcherService.prototype = {
  classID: Components.ID("{01ae923c-81bb-45db-b860-d423b0fc4fe1}"),
  QueryInterface: ChromeUtils.generateQI(["nsIWindowWatcher"]),

  getNewPrompter() {
    return Prompter;
  },
};

this.NSGetFactory = ComponentUtils.generateNSGetFactory([WindowWatcherService]);
