"use strict";

/* globals ExtensionAPI */

const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

this.quitter = class extends ExtensionAPI {
  getAPI(context) {
    return {
      quitter: {
        quit() {
          Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
        },
      },
    };
  }
};
