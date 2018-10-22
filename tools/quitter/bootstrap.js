/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/Services.jsm");

const CHILD_SCRIPT = "chrome://quitter/content/contentscript.js";

const quitterObserver = {
  init() {
    Services.mm.addMessageListener("Quitter.Quit", this);
    Services.mm.loadFrameScript(CHILD_SCRIPT, true);
  },

  uninit() {
    Services.mm.removeMessageListener("Quitter.Quit", this);
    Services.mm.removeDelayedFrameScript(CHILD_SCRIPT, true);
  },

  /**
   * messageManager callback function
   * This will get requests from our API in the window and process them in chrome for it
   **/
  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "Quitter.Quit":
        Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
        break;
    }
  },
};

function startup(data, reason) {
  quitterObserver.init();
}

function shutdown(data, reason) {
  quitterObserver.uninit();
}

function install(data, reason) {}
function uninstall(data, reason) {}
