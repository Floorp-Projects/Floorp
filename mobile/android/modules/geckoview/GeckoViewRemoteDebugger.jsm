/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["GeckoViewRemoteDebugger"];

const {XPCOMUtils} = ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
const {GeckoViewUtils} = ChromeUtils.import("resource://gre/modules/GeckoViewUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  Services: "resource://gre/modules/Services.jsm",
});

XPCOMUtils.defineLazyGetter(this, "require", () => {
  const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");
  return require;
});

XPCOMUtils.defineLazyGetter(this, "DebuggerServer", () => {
  const { DebuggerServer } = require("devtools/server/main");
  return DebuggerServer;
});

XPCOMUtils.defineLazyGetter(this, "SocketListener", () => {
  const { SocketListener } = require("devtools/shared/security/socket");
  return SocketListener;
});

const {debug, warn} = GeckoViewUtils.initLogging("RemoteDebugger"); // eslint-disable-line no-unused-vars

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

    // This lets Marionette start listening (when it's enabled).  Both
    // GeckoView and Marionette do most of their initialization in
    // "profile-after-change", and there is no order enforced between
    // them.  Therefore we defer asking Marionette to startup until
    // after all "profile-after-change" handlers (including this one)
    // have completed.
    Services.tm.dispatchToMainThread(() => {
      Services.obs.notifyObservers(null, "marionette-startup-requested");
    });
  },

  onEnable() {
    if (this._isEnabled) {
      return;
    }

    debug `onEnable`;
    DebuggerServer.init();
    DebuggerServer.registerAllActors();
    const { createRootActor } = require("resource://gre/modules/dbg-browser-actors.js");
    DebuggerServer.setRootActor(createRootActor);
    DebuggerServer.allowChromeProcess = true;
    DebuggerServer.chromeWindowType = "navigator:geckoview";
    // Force the Server to stay alive even if there are no connections at the moment.
    DebuggerServer.keepAlive = true;

    // Socket address for USB remote debugger expects
    // @ANDROID_PACKAGE_NAME/firefox-debugger-socket.
    // In /proc/net/unix, it will be outputed as
    // @org.mozilla.geckoview_example/firefox-debugger-socket
    //
    // If package name isn't available, it will be "@firefox-debugger-socket".

    const env = Cc["@mozilla.org/process/environment;1"]
              .getService(Ci.nsIEnvironment);
    let packageName = env.get("MOZ_ANDROID_PACKAGE_NAME");
    if (packageName) {
      packageName = packageName + "/";
    } else {
      warn `Missing env MOZ_ANDROID_PACKAGE_NAME. Unable to get package name`;
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

    debug `onDisable`;
    this._isEnabled = false;
    this._usbDebugger.stop();
  },
};

class USBRemoteDebugger {
  start(aPortOrPath) {
    try {
      const AuthenticatorType = DebuggerServer.Authenticators.get("PROMPT");
      const authenticator = new AuthenticatorType.Server();
      authenticator.allowConnection = this.allowConnection.bind(this);
      const socketOptions = {
        authenticator,
        portOrPath: aPortOrPath,
      };
      this._listener = new SocketListener(DebuggerServer, socketOptions);
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
