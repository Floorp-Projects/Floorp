/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals ExtensionAPI */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

this.quitter = class extends ExtensionAPI {
  getAPI(context) {
    return {
      quitter: {
        async quit() {
          let browserWindow = Services.wm.getMostRecentWindow(
            "navigator:browser"
          );
          if (browserWindow && browserWindow.gBrowserInit) {
            await browserWindow.gBrowserInit.idleTasksFinishedPromise;
          }
          Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
        },
      },
    };
  }
};
