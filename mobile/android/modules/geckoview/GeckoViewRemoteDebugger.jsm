/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewRemoteDebugger"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGetter(this, "DebuggerServer", () => {
  const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm", {});
  const { DebuggerServer } = require("devtools/server/main");
  return DebuggerServer;
});

GeckoViewUtils.initLogging("GeckoView.RemoteDebugger", this);

var GeckoViewRemoteDebugger = {
  observe(aSubject, aTopic, aData) {
    if (aTopic !== "nsPref:changed") {
      return;
    }

    if (Services.prefs.getBoolPref(aData, false)) {
      this.onEnable();
    } else {
      this.onDisable();
    }
  },

  onInit() {
    debug `onInit`;
    this._isEnabled = false;
    this._usbDebugger = new USBRemoteDebugger();
  },

  onEnable() {
    if (this._isEnabled) {
      return;
    }

    debug `onEnable`;
    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    DebuggerServer.registerModule("resource://gre/modules/dbg-browser-actors.js");
    DebuggerServer.allowChromeProcess = true;
    DebuggerServer.chromeWindowType = "navigator:geckoview";

    const env = Cc["@mozilla.org/process/environment;1"]
              .getService(Ci.nsIEnvironment);
    const dataDir = env.get("MOZ_ANDROID_DATA_DIR");

    if (!dataDir) {
      warn `Missing env MOZ_ANDROID_DATA_DIR - aborting debugger server start`;
      return;
    }

    this._isEnabled = true;
    this._usbDebugger.stop();

    const portOrPath = dataDir + "/firefox-debugger-socket";
    this._usbDebugger.start(portOrPath);
  },

  onDisable() {
    if (!this._isEnabled) {
      return;
    }

    debug `onDisable`;
    this._isEnabled = false;
    this._usbDebugger.stop();
  },
};

class USBRemoteDebugger {
  start(aPortOrPath) {
    try {
      let AuthenticatorType = DebuggerServer.Authenticators.get("PROMPT");
      let authenticator = new AuthenticatorType.Server();
      authenticator.allowConnection = this.allowConnection.bind(this);
      this._listener = DebuggerServer.createListener();
      this._listener.portOrPath = aPortOrPath;
      this._listener.authenticator = authenticator;
      this._listener.open();
      debug `USB remote debugger - listening on ${aPortOrPath}`;
    } catch (e) {
      warn `Unable to start USB debugger server: ${e}`;
    }
  }

  stop() {
    if (!this._listener) {
      return;
    }

    try {
      this._listener.close();
      this._listener = null;
    } catch (e) {
      warn `Unable to stop USB debugger server: ${e}`;
    }
  }

  allowConnection(aSession) {
    if (!this._listener) {
      return DebuggerServer.AuthenticationResult.DENY;
    }

    if (aSession.server.port) {
      return DebuggerServer.AuthenticationResult.DENY;
    }
    return DebuggerServer.AuthenticationResult.ALLOW;
  }
}
