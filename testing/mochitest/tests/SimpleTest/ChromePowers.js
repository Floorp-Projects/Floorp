/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const {SpecialPowersAPI, bindDOMWindowUtils} = ChromeUtils.import("resource://specialpowers/SpecialPowersAPI.jsm");
const {SpecialPowersAPIParent} = ChromeUtils.import("resource://specialpowers/SpecialPowersAPIParent.jsm");

class ChromePowers extends SpecialPowersAPI {
  constructor(window) {
    super();

    this.window = Cu.getWeakReference(window);

    this.chromeWindow = window;

    this.DOMWindowUtils = bindDOMWindowUtils(window);

    this.parentActor = new SpecialPowersAPIParent();
    this.parentActor.sendAsyncMessage = this.sendReply.bind(this);

    this.listeners = new Map();
  }

  toString() { return "[ChromePowers]"; }
  sanityCheck() { return "foo"; }

  get contentWindow() {
    return window;
  }

  get document() {
    return window.document;
  }

  get docShell() {
    return window.docShell;
  }

  sendReply(aType, aMsg) {
    var msg = {name: aType, json: aMsg, data: aMsg};
    if (!this.listeners.has(aType)) {
      throw new Error(`No listener for ${aType}`);
    }
    this.listeners.get(aType)(msg);
  }

  sendAsyncMessage(aType, aMsg) {
    var msg = {name: aType, json: aMsg, data: aMsg};
    this.receiveMessage(msg);
  }

  async sendQuery(aType, aMsg) {
    var msg = {name: aType, json: aMsg, data: aMsg};
    return this.receiveMessage(msg);
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

  receiveMessage(aMessage) {
    switch (aMessage.name) {
      case "SpecialPowers.Quit":
        let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
        appStartup.quit(Ci.nsIAppStartup.eForceQuit);
        break;
      case "SPProcessCrashService":
        if (aMessage.json.op == "register-observer" || aMessage.json.op == "unregister-observer") {
          // Hack out register/unregister specifically for browser-chrome leaks
          break;
        }
      default:
        // All calls go here, because we need to handle SPProcessCrashService calls as well
        return this.parentActor.receiveMessage(aMessage);
    }
    return undefined;
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
  ChromeUtils.import("resource://specialpowers/SpecialPowersAPIParent.jsm", this);

  window.SpecialPowers = new ChromePowers(window);
}

