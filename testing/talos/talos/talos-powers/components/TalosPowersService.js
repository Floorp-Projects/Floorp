/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");

const FRAME_SCRIPT = "chrome://talos-powers/content/talos-powers-content.js";

function TalosPowersService() {};

TalosPowersService.prototype = {
  classDescription: "Talos Powers",
  classID: Components.ID("{f5d53443-d58d-4a2f-8df0-98525d4f91ad}"),
  contractID: "@mozilla.org/talos/talos-powers-service;1",
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe(subject, topic, data) {
    switch(topic) {
      case "profile-after-change":
        // Note that this observation is registered in the chrome.manifest
        // for this add-on.
        this.init();
        break;
      case "sessionstore-windows-restored":
        this.inject();
        break;
      case "xpcom-shutdown":
        this.uninit();
        break;
    }
  },

  init() {
    // We want to defer any kind of work until after sessionstore has
    // finished in order not to skew sessionstore test numbers.
    Services.obs.addObserver(this, "sessionstore-windows-restored", false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  uninit() {
    Services.obs.removeObserver(this, "sessionstore-windows-restored", false);
    Services.obs.removeObserver(this, "xpcom-shutdown", false);
  },

  inject() {
    Services.mm.loadFrameScript(FRAME_SCRIPT, true);
    Services.mm.addMessageListener("Talos:ForceQuit", this);
  },

  receiveMessage(message) {
    if (message.name == "Talos:ForceQuit") {
      this.forceQuit();
    }
  },

  forceQuit() {
    let enumerator = Services.wm.getEnumerator(null);
    while (enumerator.hasMoreElements()) {
      let domWindow = enumerator.getNext();
      domWindow.close();
    }

    try {
      Services.startup.quit(Services.startup.eForceQuit);
    } catch(e) {
      dump('Force Quit failed: ' + e);
    }
  },
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([TalosPowersService]);
