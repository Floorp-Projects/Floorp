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
