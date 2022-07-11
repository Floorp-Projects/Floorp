/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

var EXPORTED_SYMBOLS = ["ContentCollector"];

// This listens for the message "browser-test:collect-request". When it gets it,
// it runs some GCs and CCs, then prints out a message indicating the collections
// are complete. Mochitest uses this information to determine when windows and
// docshells should be destroyed.

var ContentCollector = {
  init() {
    let processType = Services.appinfo.processType;
    if (processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT) {
      // In the main process, we handle triggering collections in browser-test.js
      return;
    }

    Services.cpmm.addMessageListener("browser-test:collect-request", this);
  },

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "browser-test:collect-request":
        Services.obs.notifyObservers(null, "memory-pressure", "heap-minimize");

        Cu.forceGC();
        Cu.forceCC();

        let shutdownCleanup = aCallback => {
          Cu.schedulePreciseShrinkingGC(() => {
            // Run the GC and CC a few times to make sure that as much
            // as possible is freed.
            Cu.forceGC();
            Cu.forceCC();
            aCallback();
          });
        };

        shutdownCleanup(() => {
          setTimeout(() => {
            shutdownCleanup(() => {
              this.finish();
            });
          }, 1000);
        });

        break;
    }
  },

  finish() {
    let pid = Services.appinfo.processID;
    dump("Completed ShutdownLeaks collections in process " + pid + "\n");

    Services.cpmm.removeMessageListener("browser-test:collect-request", this);
  },
};
ContentCollector.init();
