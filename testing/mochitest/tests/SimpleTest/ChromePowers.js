/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://specialpowers/SpecialPowersAPI.jsm", this);

class ChromePowers extends SpecialPowersAPI {
  constructor(window) {
    super();

    this.window = Cu.getWeakReference(window);

    this.chromeWindow = window;

    this.DOMWindowUtils = bindDOMWindowUtils(window);

    this.spObserver = new SpecialPowersObserverAPI();
    this.spObserver._sendReply = this._sendReply.bind(this);
    this.listeners = new Map();
  }

  toString() { return "[ChromePowers]"; }
  sanityCheck() { return "foo"; }

  _sendReply(aOrigMsg, aType, aMsg) {
    var msg = {'name':aType, 'json': aMsg, 'data': aMsg};
    if (!this.listeners.has(aType)) {
      throw new Error(`No listener for ${aType}`);
    }
    this.listeners.get(aType)(msg);
  }

  _sendSyncMessage(aType, aMsg) {
    var msg = {'name':aType, 'json': aMsg, 'data': aMsg};
    return [this._receiveMessage(msg)];
  }

  _sendAsyncMessage(aType, aMsg) {
    var msg = {'name':aType, 'json': aMsg, 'data': aMsg};
    this._receiveMessage(msg);
  }

  _addMessageListener(aType, aCallback) {
    if (this.listeners.has(aType)) {
      throw new Error(`unable to handle multiple listeners for ${aType}`);
    }
    this.listeners.set(aType, aCallback);
  }
  _removeMessageListener(aType, aCallback) {
    this.listeners.delete(aType);
  }

  registerProcessCrashObservers() {
    this._sendSyncMessage("SPProcessCrashService", { op: "register-observer" });
  }

  unregisterProcessCrashObservers() {
    this._sendSyncMessage("SPProcessCrashService", { op: "unregister-observer" });
  }

  _receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "SpecialPowers.Quit":
        let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
        appStartup.quit(Ci.nsIAppStartup.eForceQuit);
        break;
      case "SPProcessCrashService":
        if (aMessage.json.op == "register-observer" || aMessage.json.op == "unregister-observer") {
          // Hack out register/unregister specifically for browser-chrome leaks
          break;
        } else if (aMessage.type == "crash-observed") {
          for (let e of msg.dumpIDs) {
            this._encounteredCrashDumpFiles.push(e.id + "." + e.extension);
          }
        }
      default:
        // All calls go here, because we need to handle SPProcessCrashService calls as well
        return this.spObserver._receiveMessageAPI(aMessage);
    }
    return undefined;		// Avoid warning.
  }

  quit() {
    // We come in here as SpecialPowers.quit, but SpecialPowers is really ChromePowers.
    // For some reason this.<func> resolves to TestRunner, so using SpecialPowers
    // allows us to use the ChromePowers object which we defined below.
    SpecialPowers._sendSyncMessage("SpecialPowers.Quit", {});
  }

  focus(aWindow) {
    // We come in here as SpecialPowers.focus, but SpecialPowers is really ChromePowers.
    // For some reason this.<func> resolves to TestRunner, so using SpecialPowers
    // allows us to use the ChromePowers object which we defined below.
    if (aWindow)
      aWindow.focus();
  }

  executeAfterFlushingMessageQueue(aCallback) {
    aCallback();
  }
}

if (window.parent.SpecialPowers && !window.SpecialPowers) {
  window.SpecialPowers = window.parent.SpecialPowers;
} else {
  ChromeUtils.import("resource://specialpowers/SpecialPowersObserverAPI.jsm", this);

  window.SpecialPowers = new ChromePowers(window);
}

