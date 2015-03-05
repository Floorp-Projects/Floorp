/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm");

this.EXPORTED_SYMBOLS = ["ContentCollector"];

// This listens for the message "browser-test:collect-request". When it gets it,
// it runs some GCs and CCs, then prints out a message indicating the collections
// are complete. Mochitest uses this information to determine when windows and
// docshells should be destroyed.

var ContentCollector = {
  init: function() {
      let processType = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).processType;
      if (processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
        // In the main process, we handle triggering collections in browser-test.js
        return;
      }

    let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                 .getService(Ci.nsISyncMessageSender);
    cpmm.addMessageListener("browser-test:collect-request", this);
  },

  receiveMessage: function(aMessage) {
    switch (aMessage.name) {
      case "browser-test:collect-request":
        Services.obs.notifyObservers(null, "memory-pressure", "heap-minimize");

        Cu.forceGC();
        Cu.forceCC();

        Cu.schedulePreciseShrinkingGC(() => {
          Cu.forceCC();

          let pid = Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).processID;
          dump("Completed ShutdownLeaks collections in process " + pid + "\n")});

          let cpmm = Cc["@mozilla.org/childprocessmessagemanager;1"]
                       .getService(Ci.nsISyncMessageSender);
          cpmm.removeMessageListener("browser-test:collect-request", this);

        break;
    }
  }

};
ContentCollector.init();
