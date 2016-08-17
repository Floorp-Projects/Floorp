/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionTestUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Components.utils.import("resource://gre/modules/Task.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Extension",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

/* exported ExtensionTestUtils */

let BASE_MANIFEST = Object.freeze({
  "applications": Object.freeze({
    "gecko": Object.freeze({
      "id": "test@web.ext",
    }),
  }),

  "manifest_version": 2,

  "name": "name",
  "version": "0",
});

class ExtensionWrapper {
  constructor(extension, testScope) {
    this.extension = extension;
    this.testScope = testScope;

    this.state = "uninitialized";

    this.testResolve = null;
    this.testDone = new Promise(resolve => { this.testResolve = resolve; });

    this.messageHandler = new Map();
    this.messageAwaiter = new Map();

    this.messageQueue = new Set();

    this.testScope.do_register_cleanup(() => {
      if (this.messageQueue.size) {
        let names = Array.from(this.messageQueue, ([msg]) => msg);
        this.testScope.equal(JSON.stringify(names), "[]", "message queue is empty");
      }
      if (this.messageAwaiter.size) {
        let names = Array.from(this.messageAwaiter.keys());
        this.testScope.equal(JSON.stringify(names), "[]", "no tasks awaiting on messages");
      }
    });

    /* eslint-disable mozilla/balanced-listeners */
    extension.on("test-eq", (kind, pass, msg, expected, actual) => {
      this.testScope.ok(pass, `${msg} - Expected: ${expected}, Actual: ${actual}`);
    });
    extension.on("test-log", (kind, pass, msg) => {
      this.testScope.do_print(msg);
    });
    extension.on("test-result", (kind, pass, msg) => {
      this.testScope.ok(pass, msg);
    });
    extension.on("test-done", (kind, pass, msg, expected, actual) => {
      this.testScope.ok(pass, msg);
      this.testResolve(msg);
    });

    extension.on("test-message", (kind, msg, ...args) => {
      let handler = this.messageHandler.get(msg);
      if (handler) {
        handler(...args);
      } else {
        this.messageQueue.add([msg, ...args]);
        this.checkMessages();
      }
    });
    /* eslint-enable mozilla/balanced-listeners */

    this.testScope.do_register_cleanup(() => {
      if (this.state == "pending" || this.state == "running") {
        this.testScope.equal(this.state, "unloaded", "Extension left running at test shutdown");
        return this.unload();
      } else if (extension.state == "unloading") {
        this.testScope.equal(this.state, "unloaded", "Extension not fully unloaded at test shutdown");
      }
    });

    this.testScope.do_print(`Extension loaded`);
  }

  startup() {
    if (this.state != "uninitialized") {
      throw new Error("Extension already started");
    }
    this.state = "pending";

    return this.extension.startup().then(
      result => {
        this.state = "running";

        return result;
      },
      error => {
        this.state = "failed";

        return Promise.reject(error);
      });
  }

  unload() {
    if (this.state != "running") {
      throw new Error("Extension not running");
    }
    this.state = "unloading";

    this.extension.shutdown();

    this.state = "unloaded";

    return Promise.resolve();
  }

  /*
   * This method marks the extension unloading without actually calling
   * shutdown, since shutting down a MockExtension causes it to be uninstalled.
   *
   * Normally you shouldn't need to use this unless you need to test something
   * that requires a restart, such as updates.
   */
  markUnloaded() {
    if (this.state != "running") {
      throw new Error("Extension not running");
    }
    this.state = "unloaded";

    return Promise.resolve();
  }

  sendMessage(...args) {
    this.extension.testMessage(...args);
  }

  awaitFinish(msg) {
    return this.testDone.then(actual => {
      if (msg) {
        this.testScope.equal(actual, msg, "test result correct");
      }
      return actual;
    });
  }

  checkMessages() {
    for (let message of this.messageQueue) {
      let [msg, ...args] = message;

      let listener = this.messageAwaiter.get(msg);
      if (listener) {
        this.messageQueue.delete(message);
        this.messageAwaiter.delete(msg);

        listener.resolve(...args);
        return;
      }
    }
  }

  checkDuplicateListeners(msg) {
    if (this.messageHandler.has(msg) || this.messageAwaiter.has(msg)) {
      throw new Error("only one message handler allowed");
    }
  }

  awaitMessage(msg) {
    return new Promise(resolve => {
      this.checkDuplicateListeners(msg);

      this.messageAwaiter.set(msg, {resolve});
      this.checkMessages();
    });
  }

  onMessage(msg, callback) {
    this.checkDuplicateListeners(msg);
    this.messageHandler.set(msg, callback);
  }
}

var ExtensionTestUtils = {
  BASE_MANIFEST,

  normalizeManifest: Task.async(function* (manifest, baseManifest = BASE_MANIFEST) {
    const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});

    yield Management.lazyInit();

    let errors = [];
    let context = {
      url: null,

      logError: error => {
        errors.push(error);
      },

      preprocessors: {},
    };

    manifest = Object.assign({}, baseManifest, manifest);

    let normalized = Schemas.normalize(manifest, "manifest.WebExtensionManifest", context);
    normalized.errors = errors;

    return normalized;
  }),

  currentScope: null,

  profileDir: null,

  init(scope) {
    this.currentScope = scope;

    this.profileDir = scope.do_get_profile();

    // We need to load at least one frame script into every message
    // manager to ensure that the scriptable wrapper for its global gets
    // created before we try to access it externally. If we don't, we
    // fail sanity checks on debug builds the first time we try to
    // create a wrapper, because we should never have a global without a
    // cached wrapper.
    Services.mm.loadFrameScript("data:text/javascript,//", true);


    let tmpD = this.profileDir.clone();
    tmpD.append("tmp");
    tmpD.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

    let dirProvider = {
      getFile(prop, persistent) {
        persistent.value = false;
        if (prop == "TmpD") {
          return tmpD.clone();
        }
        return null;
      },

      QueryInterface: XPCOMUtils.generateQI([Ci.nsIDirectoryServiceProvider]),
    };
    Services.dirsvc.registerProvider(dirProvider);


    scope.do_register_cleanup(() => {
      tmpD.remove(true);
      Services.dirsvc.unregisterProvider(dirProvider);

      this.currentScope = null;
    });
  },

  addonManagerStarted: false,

  startAddonManager() {
    if (this.addonManagerStarted) {
      return;
    }
    this.addonManagerStarted = true;

    let appInfo = {};
    Cu.import("resource://testing-common/AppInfo.jsm", appInfo);

    appInfo.updateAppInfo({
      ID: "xpcshell@tests.mozilla.org",
      name: "XPCShell",
      version: "48",
      platformVersion: "48",
    });


    let manager = Cc["@mozilla.org/addons/integration;1"].getService(Ci.nsIObserver)
                                                         .QueryInterface(Ci.nsITimerCallback);
    manager.observe(null, "addons-startup", null);
  },

  loadExtension(data) {
    let extension = Extension.generate(data);

    return new ExtensionWrapper(extension, this.currentScope);
  },
};
