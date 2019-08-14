/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* This code is loaded in every child process that is started by mochitest in
 * order to be used as a replacement for UniversalXPConnect
 */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var EXPORTED_SYMBOLS = ["SpecialPowersChild"];

const { bindDOMWindowUtils, SpecialPowersAPI } = ChromeUtils.import(
  "resource://specialpowers/SpecialPowersAPI.jsm"
);
const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

Cu.forcePermissiveCOWs();

class SpecialPowersChild extends SpecialPowersAPI {
  constructor() {
    super();

    this._windowID = null;
    this.DOMWindowUtils = null;

    this._encounteredCrashDumpFiles = [];
    this._unexpectedCrashDumpFiles = {};
    this._crashDumpDir = null;
    this._serviceWorkerRegistered = false;
    this._serviceWorkerCleanUpRequests = new Map();
    Object.defineProperty(this, "Components", {
      configurable: true,
      enumerable: true,
      value: this.getFullComponents(),
    });
    this._createFilesOnError = null;
    this._createFilesOnSuccess = null;

    this._messageListeners = new ExtensionUtils.DefaultMap(() => new Set());
  }

  handleEvent(aEvent) {
    // We don't actually care much about the "DOMWindowCreated" event.
    // We only listen to it to force creation of the actor.
  }

  actorCreated() {
    this.attachToWindow();
  }

  attachToWindow() {
    let window = this.contentWindow;
    if (!window.wrappedJSObject.SpecialPowers) {
      this._windowID = window.windowUtils.currentInnerWindowID;
      this.DOMWindowUtils = bindDOMWindowUtils(window);

      window.SpecialPowers = this;
      window.wrappedJSObject.SpecialPowers = this;
      if (this.IsInNestedFrame) {
        this.addPermission("allowXULXBL", true, window.document);
      }
    }
  }

  get window() {
    return this.contentWindow;
  }

  toString() {
    return "[SpecialPowers]";
  }
  sanityCheck() {
    return "foo";
  }

  _addMessageListener(msgname, listener) {
    this._messageListeners.get(msgname).add(listener);
  }

  _removeMessageListener(msgname, listener) {
    this._messageListeners.get(msgname).delete(listener);
  }

  registerProcessCrashObservers() {
    this.sendAsyncMessage("SPProcessCrashService", { op: "register-observer" });
  }

  unregisterProcessCrashObservers() {
    this.sendAsyncMessage("SPProcessCrashService", {
      op: "unregister-observer",
    });
  }

  receiveMessage(aMessage) {
    if (this._messageListeners.has(aMessage.name)) {
      for (let listener of this._messageListeners.get(aMessage.name)) {
        try {
          if (typeof listener === "function") {
            listener(aMessage);
          } else {
            listener.receiveMessage(aMessage);
          }
        } catch (e) {
          Cu.reportError(e);
        }
      }
    }

    switch (aMessage.name) {
      case "SPProcessCrashService":
        if (aMessage.json.type == "crash-observed") {
          for (let e of aMessage.json.dumpIDs) {
            this._encounteredCrashDumpFiles.push(e.id + "." + e.extension);
          }
        }
        break;

      case "SPServiceWorkerRegistered":
        this._serviceWorkerRegistered = aMessage.data.registered;
        break;

      case "SpecialPowers.FilesCreated":
        var createdHandler = this._createFilesOnSuccess;
        this._createFilesOnSuccess = null;
        this._createFilesOnError = null;
        if (createdHandler) {
          createdHandler(Cu.cloneInto(aMessage.data, this.contentWindow));
        }
        break;

      case "SpecialPowers.FilesError":
        var errorHandler = this._createFilesOnError;
        this._createFilesOnSuccess = null;
        this._createFilesOnError = null;
        if (errorHandler) {
          errorHandler(aMessage.data);
        }
        break;

      case "Spawn":
        let { task, args, caller, taskId } = aMessage.data;
        return this._spawnTask(task, args, caller, taskId);

      default:
        return super.receiveMessage(aMessage);
    }

    return true;
  }

  quit() {
    this.sendAsyncMessage("SpecialPowers.Quit", {});
  }

  // fileRequests is an array of file requests. Each file request is an object.
  // A request must have a field |name|, which gives the base of the name of the
  // file to be created in the profile directory. If the request has a |data| field
  // then that data will be written to the file.
  createFiles(fileRequests, onCreation, onError) {
    return this.sendQuery("SpecialPowers.CreateFiles", fileRequests).then(
      onCreation,
      onError
    );
  }

  // Remove the files that were created using |SpecialPowers.createFiles()|.
  // This will be automatically called by |SimpleTest.finish()|.
  removeFiles() {
    this.sendAsyncMessage("SpecialPowers.RemoveFiles", {});
  }

  executeAfterFlushingMessageQueue(aCallback) {
    return this.sendQuery("Ping").then(aCallback);
  }

  async registeredServiceWorkers() {
    // For the time being, if parent_intercept is false, we can assume that
    // ServiceWorkers registered by the current test are all known to the SWM in
    // this process.
    if (
      !Services.prefs.getBoolPref("dom.serviceWorkers.parent_intercept", false)
    ) {
      let swm = Cc["@mozilla.org/serviceworkers/manager;1"].getService(
        Ci.nsIServiceWorkerManager
      );
      let regs = swm.getAllRegistrations();

      // XXX This is shared with SpecialPowersAPIParent.jsm
      let workers = new Array(regs.length);
      for (let i = 0; i < workers.length; ++i) {
        let { scope, scriptSpec } = regs.queryElementAt(
          i,
          Ci.nsIServiceWorkerRegistrationInfo
        );
        workers[i] = { scope, scriptSpec };
      }

      return workers;
    }

    // Please see the comment in SpecialPowersObserver.jsm above
    // this._serviceWorkerListener's assignment for what this returns.
    if (this._serviceWorkerRegistered) {
      // This test registered at least one service worker. Send a synchronous
      // call to the parent to make sure that it called unregister on all of its
      // service workers.
      let { workers } = await this.sendQuery("SPCheckServiceWorkers");
      return workers;
    }

    return [];
  }
}
