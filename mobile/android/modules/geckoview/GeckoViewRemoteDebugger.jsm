/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["GeckoViewRemoteDebugger"];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/GeckoViewModule.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGetter(this, "dump", () =>
  Cu.import("resource://gre/modules/AndroidLog.jsm", {})
    .AndroidLog.d.bind(null, "ViewRemoteDebugger"));

XPCOMUtils.defineLazyGetter(this, "DebuggerServer", () => {
  const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
  const { DebuggerServer } = require("devtools/server/main");
  return DebuggerServer;
});

function debug(aMsg) {
  // dump(aMsg);
}

class GeckoViewRemoteDebugger extends GeckoViewModule {
  init() {
    this._isEnabled = false;
    this._usbDebugger = new USBRemoteDebugger();
  }

  onSettingsUpdate() {
    let enabled = this.settings.useRemoteDebugger;

    if (enabled && !this._isEnabled) {
      this.register();
    } else if (!enabled) {
      this.unregister();
    }
  }

  register() {
    if (!DebuggerServer.initialized) {
      DebuggerServer.init();
      DebuggerServer.addBrowserActors("navigator:geckoview");
      DebuggerServer.registerModule(
        "resource://gre/modules/dbg-browser-actors.js");
      DebuggerServer.allowChromeProcess = true;
    }
    this._isEnabled = true;
    this._usbDebugger.stop();

    let windowId = this.window.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDOMWindowUtils)
                              .outerWindowID;
    let portOrPath = this.settings.debuggerSocketDir +
                     "/firefox-debugger-socket-" +
                     windowId;
    this._usbDebugger.start(portOrPath);
  }

  unregister() {
    this._isEnabled = false;
    this._usbDebugger.stop();
  }
}

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
      debug(`USB remote debugger - listening on ${aPortOrPath}`);
    } catch (e) {
      debug("Unable to start USB debugger server: " + e);
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
      debug("Unable to stop USB debugger server: " + e);
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
