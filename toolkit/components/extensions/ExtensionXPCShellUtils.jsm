/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionTestUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Components.utils.import("resource://gre/modules/ExtensionUtils.jsm");
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

XPCOMUtils.defineLazyGetter(this, "Management", () => {
  const {Management} = Cu.import("resource://gre/modules/Extension.jsm", {});
  return Management;
});

/* exported ExtensionTestUtils */

const {
  promiseDocumentLoaded,
  promiseEvent,
  promiseObserved,
} = ExtensionUtils;

var REMOTE_CONTENT_SCRIPTS = false;

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


function frameScript() {
  Components.utils.import("resource://gre/modules/ExtensionContent.jsm");

  ExtensionContent.init(this);
  addEventListener("unload", () => {
    ExtensionContent.uninit(this);
  });
}

const FRAME_SCRIPT = `data:text/javascript,(${frameScript}).call(this)`;


const XUL_URL = "data:application/vnd.mozilla.xul+xml;charset=utf-8," + encodeURI(
  `<?xml version="1.0"?>
  <window id="documentElement"/>`);

let kungFuDeathGrip = new Set();
function promiseBrowserLoaded(browser, url) {
  return new Promise(resolve => {
    const listener = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference, Ci.nsIWebProgressListener]),

      onStateChange(webProgress, request, stateFlags, statusCode) {
        let requestUrl = request.URI ? request.URI.spec : webProgress.DOMWindow.location.href;

        if (webProgress.isTopLevel && requestUrl === url &&
            (stateFlags & Ci.nsIWebProgressListener.STATE_STOP)) {
          resolve();
          kungFuDeathGrip.delete(listener);
          browser.removeProgressListener(listener);
        }
      },
    };

    // addProgressListener only supports weak references, so we need to
    // use one. But we also need to make sure it stays alive until we're
    // done with it, so thunk away a strong reference to keep it alive.
    kungFuDeathGrip.add(listener);
    browser.addProgressListener(listener, Ci.nsIWebProgress.NOTIFY_STATE_WINDOW);
  });
}

class ContentPage {
  constructor(remote = REMOTE_CONTENT_SCRIPTS) {
    this.remote = remote;

    this.browserReady = this._initBrowser();
  }

  async _initBrowser() {
    this.windowlessBrowser = Services.appShell.createWindowlessBrowser(true);

    let system = Services.scriptSecurityManager.getSystemPrincipal();

    let chromeShell = this.windowlessBrowser.QueryInterface(Ci.nsIInterfaceRequestor)
                                            .getInterface(Ci.nsIDocShell)
                                            .QueryInterface(Ci.nsIWebNavigation);

    chromeShell.createAboutBlankContentViewer(system);
    chromeShell.loadURI(XUL_URL, 0, null, null, null);

    await promiseObserved("chrome-document-global-created",
                          win => win.document == chromeShell.document);

    let chromeDoc = await promiseDocumentLoaded(chromeShell.document);

    let browser = chromeDoc.createElement("browser");
    browser.setAttribute("type", "content");

    let awaitFrameLoader = Promise.resolve();
    if (this.remote) {
      awaitFrameLoader = promiseEvent(browser, "XULFrameLoaderCreated");
      browser.setAttribute("remote", "true");
    }

    chromeDoc.documentElement.appendChild(browser);

    await awaitFrameLoader;
    browser.messageManager.loadFrameScript(FRAME_SCRIPT, true);

    this.browser = browser;
    return browser;
  }

  async loadURL(url) {
    await this.browserReady;

    this.browser.loadURI(url);
    return promiseBrowserLoaded(this.browser, url);
  }

  async close() {
    await this.browserReady;

    this.browser = null;

    this.windowlessBrowser.close();
    this.windowlessBrowser = null;
  }
}

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

    this.attachListeners();

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

  attachListeners() {
    /* eslint-disable mozilla/balanced-listeners */
    this.extension.on("test-eq", (kind, pass, msg, expected, actual) => {
      this.testScope.ok(pass, `${msg} - Expected: ${expected}, Actual: ${actual}`);
    });
    this.extension.on("test-log", (kind, pass, msg) => {
      this.testScope.do_print(msg);
    });
    this.extension.on("test-result", (kind, pass, msg) => {
      this.testScope.ok(pass, msg);
    });
    this.extension.on("test-done", (kind, pass, msg, expected, actual) => {
      this.testScope.ok(pass, msg);
      this.testResolve(msg);
    });

    this.extension.on("test-message", (kind, msg, ...args) => {
      let handler = this.messageHandler.get(msg);
      if (handler) {
        handler(...args);
      } else {
        this.messageQueue.add([msg, ...args]);
        this.checkMessages();
      }
    });
    /* eslint-enable mozilla/balanced-listeners */
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

  async unload() {
    if (this.state != "running") {
      throw new Error("Extension not running");
    }
    this.state = "unloading";

    await this.extension.shutdown();

    this.state = "unloaded";
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

  mockAppInfo() {
    const {updateAppInfo} = Cu.import("resource://testing-common/AppInfo.jsm", {});
    updateAppInfo({
      ID: "xpcshell@tests.mozilla.org",
      name: "XPCShell",
      version: "48",
      platformVersion: "48",
    });
  },

  startAddonManager() {
    if (this.addonManagerStarted) {
      return;
    }
    this.addonManagerStarted = true;
    this.mockAppInfo();

    let manager = Cc["@mozilla.org/addons/integration;1"].getService(Ci.nsIObserver)
                                                         .QueryInterface(Ci.nsITimerCallback);
    manager.observe(null, "addons-startup", null);
  },

  loadExtension(data) {
    let extension = Extension.generate(data);

    return new ExtensionWrapper(extension, this.currentScope);
  },

  get remoteContentScripts() {
    return REMOTE_CONTENT_SCRIPTS;
  },

  set remoteContentScripts(val) {
    REMOTE_CONTENT_SCRIPTS = !!val;
  },

  loadContentPage(url, remote = undefined) {
    let contentPage = new ContentPage(remote);

    return contentPage.loadURL(url).then(() => {
      return contentPage;
    });
  },
};
