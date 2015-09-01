/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["MessagesBridge"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SystemAppProxy.jsm");

// This module receives messages from Launcher.java as observer notifications.

function debug() {
  dump("-*- MessagesBridge " + Array.slice(arguments) + "\n");
}

this.MessagesBridge = {
  init: function() {
    Services.obs.addObserver(this, "Android:Launcher", false);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic == "xpcom-shutdown") {
      Services.obs.removeObserver(this, "Android:Launcher");
      Services.obs.removeObserver(this, "xpcom-shutdown");
    }

    if (aTopic != "Android:Launcher") {
      return;
    }

    let data = JSON.parse(aData);
    debug(`Got Android:Launcher message ${data.action}`);

    switch (data.action) {
      case "screen_on":
      case "screen_off":
        // In both cases, make it look like pressing the power button
        // by dispatching keydown & keyup on the system app window.
        let window = SystemAppProxy.getFrame().contentWindow;
        window.dispatchEvent(new window.KeyboardEvent("keydown", { key: "Power" }));
        window.dispatchEvent(new window.KeyboardEvent("keyup", { key: "Power" }));
        break;
    }
  }
}

this.MessagesBridge.init();
