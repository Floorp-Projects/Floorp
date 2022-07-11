/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewRemoteDebugger"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);
const { GeckoViewUtils } = ChromeUtils.import(
  "resource://gre/modules/GeckoViewUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "require", () => {
  const { require } = ChromeUtils.import(
    "resource://devtools/shared/loader/Loader.jsm"
  );
  return require;
});

XPCOMUtils.defineLazyGetter(lazy, "DevToolsServer", () => {
  const { DevToolsServer } = lazy.require("devtools/server/devtools-server");
  return DevToolsServer;
});

XPCOMUtils.defineLazyGetter(lazy, "SocketListener", () => {
  const { SocketListener } = lazy.require("devtools/shared/security/socket");
  return SocketListener;
});

const { debug, warn } = GeckoViewUtils.initLogging("RemoteDebugger");

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
    debug`onInit`;
    this._isEnabled = false;
    this._usbDebugger = new USBRemoteDebugger();
  },

  onEnable() {
    if (this._isEnabled) {
      return;
    }

    debug`onEnable`;
    lazy.DevToolsServer.init();
    lazy.DevToolsServer.registerAllActors();
    const { createRootActor } = lazy.require(
      "resource://gre/modules/dbg-browser-actors.js"
    );
    lazy.DevToolsServer.setRootActor(createRootActor);
    lazy.DevToolsServer.allowChromeProcess = true;
    lazy.DevToolsServer.chromeWindowType = "navigator:geckoview";
    // Force the Server to stay alive even if there are no connections at the moment.
    lazy.DevToolsServer.keepAlive = true;

    // Socket address for USB remote debugger expects
    // @ANDROID_PACKAGE_NAME/firefox-debugger-socket.
    // In /proc/net/unix, it will be outputed as
    // @org.mozilla.geckoview_example/firefox-debugger-socket
    //
    // If package name isn't available, it will be "@firefox-debugger-socket".

    const env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    let packageName = env.get("MOZ_ANDROID_PACKAGE_NAME");
    if (packageName) {
      packageName = packageName + "/";
    } else {
      warn`Missing env MOZ_ANDROID_PACKAGE_NAME. Unable to get package name`;
    }

    this._isEnabled = true;
    this._usbDebugger.stop();

    const portOrPath = packageName + "firefox-debugger-socket";
    this._usbDebugger.start(portOrPath);
  },

  onDisable() {
    if (!this._isEnabled) {
      return;
    }

    debug`onDisable`;
    this._isEnabled = false;
    this._usbDebugger.stop();
  },
};

class USBRemoteDebugger {
  start(aPortOrPath) {
    try {
      const AuthenticatorType = lazy.DevToolsServer.Authenticators.get(
        "PROMPT"
      );
      const authenticator = new AuthenticatorType.Server();
      authenticator.allowConnection = this.allowConnection.bind(this);
      const socketOptions = {
        authenticator,
        portOrPath: aPortOrPath,
      };
      this._listener = new lazy.SocketListener(
        lazy.DevToolsServer,
        socketOptions
      );
      this._listener.open();
      debug`USB remote debugger - listening on ${aPortOrPath}`;
    } catch (e) {
      warn`Unable to start USB debugger server: ${e}`;
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
      warn`Unable to stop USB debugger server: ${e}`;
    }
  }

  allowConnection(aSession) {
    if (!this._listener) {
      return lazy.DevToolsServer.AuthenticationResult.DENY;
    }

    if (aSession.server.port) {
      return lazy.DevToolsServer.AuthenticationResult.DENY;
    }
    return lazy.DevToolsServer.AuthenticationResult.ALLOW;
  }
}
