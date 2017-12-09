/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionTestUtils"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Components.utils.import("resource://gre/modules/ExtensionUtils.jsm");
Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AddonTestUtils",
                                  "resource://testing-common/AddonTestUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Extension",
                                  "resource://gre/modules/Extension.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "FileUtils",
                                  "resource://gre/modules/FileUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "TestUtils",
                                  "resource://testing-common/TestUtils.jsm");

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
  Components.utils.import("resource://gre/modules/Services.jsm");

  Services.obs.notifyObservers(this, "tab-content-frameloader-created");

  // eslint-disable-next-line mozilla/balanced-listeners
  addEventListener("MozHeapMinimize", () => {
    Services.obs.notifyObservers(null, "memory-pressure", "heap-minimize");
  }, true, true);
}

const FRAME_SCRIPT = `data:text/javascript,(${encodeURI(frameScript)}).call(this)`;

let kungFuDeathGrip = new Set();
function promiseBrowserLoaded(browser, url, redirectUrl) {
  return new Promise(resolve => {
    const listener = {
      QueryInterface: XPCOMUtils.generateQI([Ci.nsISupportsWeakReference, Ci.nsIWebProgressListener]),

      onStateChange(webProgress, request, stateFlags, statusCode) {
        let requestUrl = request.URI ? request.URI.spec : webProgress.DOMWindow.location.href;
        if (webProgress.isTopLevel &&
            (requestUrl === url || requestUrl === redirectUrl) &&
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
  constructor(remote = REMOTE_CONTENT_SCRIPTS, extension = null) {
    this.remote = remote;
    this.extension = extension;

    this.browserReady = this._initBrowser();
  }

  async _initBrowser() {
    this.windowlessBrowser = Services.appShell.createWindowlessBrowser(true);

    let system = Services.scriptSecurityManager.getSystemPrincipal();

    let chromeShell = this.windowlessBrowser.QueryInterface(Ci.nsIInterfaceRequestor)
                                            .getInterface(Ci.nsIDocShell)
                                            .QueryInterface(Ci.nsIWebNavigation);

    chromeShell.createAboutBlankContentViewer(system);
    chromeShell.useGlobalHistory = false;
    chromeShell.loadURI("chrome://extensions/content/dummy.xul", 0, null, null, null);

    await promiseObserved("chrome-document-global-created",
                          win => win.document == chromeShell.document);

    let chromeDoc = await promiseDocumentLoaded(chromeShell.document);

    let browser = chromeDoc.createElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");

    if (this.extension && this.extension.remote) {
      this.remote = true;
      browser.setAttribute("remote", "true");
      browser.setAttribute("remoteType", "extension");
      browser.sameProcessAsFrameLoader = this.extension.groupFrameLoader;
    }

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

  async loadURL(url, redirectUrl = undefined) {
    await this.browserReady;

    this.browser.loadURI(url);
    return promiseBrowserLoaded(this.browser, url, redirectUrl);
  }

  async close() {
    await this.browserReady;

    let {messageManager} = this.browser;

    this.browser = null;

    this.windowlessBrowser.close();
    this.windowlessBrowser = null;

    await TestUtils.topicObserved("message-manager-disconnect",
                                  subject => subject === messageManager);
  }
}

class ExtensionWrapper {
  constructor(testScope, extension = null) {
    this.testScope = testScope;

    this.extension = null;

    this.handleResult = this.handleResult.bind(this);
    this.handleMessage = this.handleMessage.bind(this);

    this.state = "uninitialized";

    this.testResolve = null;
    this.testDone = new Promise(resolve => { this.testResolve = resolve; });

    this.messageHandler = new Map();
    this.messageAwaiter = new Map();

    this.messageQueue = new Set();


    this.testScope.do_register_cleanup(() => {
      this.clearMessageQueues();

      if (this.state == "pending" || this.state == "running") {
        this.testScope.equal(this.state, "unloaded", "Extension left running at test shutdown");
        return this.unload();
      } else if (this.state == "unloading") {
        this.testScope.equal(this.state, "unloaded", "Extension not fully unloaded at test shutdown");
      }
      this.destroy();
    });

    if (extension) {
      this.id = extension.id;
      this.uuid = extension.uuid;
      this.attachExtension(extension);
    }
  }

  destroy() {
    // This method should be implemented in subclasses which need to
    // perform cleanup when destroyed.
  }

  attachExtension(extension) {
    if (extension === this.extension) {
      return;
    }

    if (this.extension) {
      this.extension.off("test-eq", this.handleResult);
      this.extension.off("test-log", this.handleResult);
      this.extension.off("test-result", this.handleResult);
      this.extension.off("test-done", this.handleResult);
      this.extension.off("test-message", this.handleMessage);
      this.clearMessageQueues();
    }
    this.extension = extension;

    extension.on("test-eq", this.handleResult);
    extension.on("test-log", this.handleResult);
    extension.on("test-result", this.handleResult);
    extension.on("test-done", this.handleResult);
    extension.on("test-message", this.handleMessage);

    this.testScope.do_print(`Extension attached`);
  }

  clearMessageQueues() {
    if (this.messageQueue.size) {
      let names = Array.from(this.messageQueue, ([msg]) => msg);
      this.testScope.equal(JSON.stringify(names), "[]", "message queue is empty");
      this.messageQueue.clear();
    }
    if (this.messageAwaiter.size) {
      let names = Array.from(this.messageAwaiter.keys());
      this.testScope.equal(JSON.stringify(names), "[]", "no tasks awaiting on messages");
      for (let promise of this.messageAwaiter.values()) {
        promise.reject();
      }
      this.messageAwaiter.clear();
    }
  }

  handleResult(kind, pass, msg, expected, actual) {
    switch (kind) {
      case "test-eq":
        this.testScope.ok(pass, `${msg} - Expected: ${expected}, Actual: ${actual}`);
        break;

      case "test-log":
        this.testScope.do_print(msg);
        break;

      case "test-result":
        this.testScope.ok(pass, msg);
        break;

      case "test-done":
        this.testScope.ok(pass, msg);
        this.testResolve(msg);
        break;
    }
  }

  handleMessage(kind, msg, ...args) {
    let handler = this.messageHandler.get(msg);
    if (handler) {
      handler(...args);
    } else {
      this.messageQueue.add([msg, ...args]);
      this.checkMessages();
    }
  }

  awaitStartup() {
    return this.startupPromise;
  }

  startup() {
    if (this.state != "uninitialized") {
      throw new Error("Extension already started");
    }
    this.state = "pending";

    this.startupPromise = this.extension.startup().then(
      result => {
        this.state = "running";

        return result;
      },
      error => {
        this.state = "failed";

        return Promise.reject(error);
      });

    return this.startupPromise;
  }

  async unload() {
    if (this.state != "running") {
      throw new Error("Extension not running");
    }
    this.state = "unloading";

    if (this.addon) {
      this.addon.uninstall();
    } else {
      await this.extension.shutdown();
    }

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
    return new Promise((resolve, reject) => {
      this.checkDuplicateListeners(msg);

      this.messageAwaiter.set(msg, {resolve, reject});
      this.checkMessages();
    });
  }

  onMessage(msg, callback) {
    this.checkDuplicateListeners(msg);
    this.messageHandler.set(msg, callback);
  }
}

class AOMExtensionWrapper extends ExtensionWrapper {
  constructor(testScope, xpiFile, installType) {
    super(testScope);

    this.onEvent = this.onEvent.bind(this);

    this.file = xpiFile;
    this.installType = installType;

    this.cleanupFiles = [xpiFile];

    Management.on("ready", this.onEvent);
    Management.on("shutdown", this.onEvent);
    Management.on("startup", this.onEvent);

    AddonTestUtils.on("addon-manager-shutdown", this.onEvent);
    AddonTestUtils.on("addon-manager-started", this.onEvent);

    AddonManager.addAddonListener(this);
  }

  destroy() {
    this.id = null;
    this.addon = null;

    Management.off("ready", this.onEvent);
    Management.off("shutdown", this.onEvent);
    Management.off("startup", this.onEvent);

    AddonTestUtils.off("addon-manager-shutdown", this.onEvent);
    AddonTestUtils.off("addon-manager-started", this.onEvent);

    AddonManager.removeAddonListener(this);

    for (let file of this.cleanupFiles.splice(0)) {
      try {
        Services.obs.notifyObservers(file, "flush-cache-entry");
        file.remove(false);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }

  setRestarting() {
    if (this.state !== "restarting") {
      this.startupPromise = new Promise(resolve => {
        this.resolveStartup = resolve;
      });
    }
    this.state = "restarting";
  }

  onEnabling(addon) {
    if (addon.id === this.id) {
      this.setRestarting();
    }
  }

  onInstalling(addon) {
    if (addon.id === this.id) {
      this.setRestarting();
    }
  }

  onInstalled(addon) {
    if (addon.id === this.id) {
      this.addon = addon;
    }
  }

  onUninstalled(addon) {
    if (addon.id === this.id) {
      this.destroy();
    }
  }

  onEvent(kind, ...args) {
    switch (kind) {
      case "addon-manager-started":
        AddonManager.getAddonByID(this.id).then(addon => {
          this.addon = addon;
        });
        // FALLTHROUGH
      case "addon-manager-shutdown":
        this.addon = null;

        this.setRestarting();
        break;

      case "startup": {
        let [extension] = args;
        if (extension.id === this.id) {
          this.attachExtension(extension);
          this.state = "pending";
        }
        break;
      }

      case "shutdown": {
        let [extension] = args;
        if (extension.id === this.id && this.state !== "restarting") {
          this.state = "unloaded";
        }
        break;
      }

      case "ready": {
        let [extension] = args;
        if (extension.id === this.id) {
          this.state = "running";
          this.resolveStartup(extension);
        }
        break;
      }
    }
  }

  _install(xpiFile) {
    if (this.installType === "temporary") {
      return AddonManager.installTemporaryAddon(xpiFile).then(addon => {
        this.id = addon.id;
        this.addon = addon;

        return this.startupPromise;
      }).catch(e => {
        this.state = "unloaded";
        return Promise.reject(e);
      });
    } else if (this.installType === "permanent") {
      return AddonManager.getInstallForFile(xpiFile).then(install => {
        let listener = {
          onInstallFailed: () => {
            this.state = "unloaded";
            this.resolveStartup(Promise.reject(new Error("Install failed")));
          },
          onInstallEnded: (install, newAddon) => {
            this.id = newAddon.id;
            this.addon = newAddon;
          },
        };

        install.addListener(listener);
        install.install();

        return this.startupPromise;
      });
    }
  }

  async _flushCache() {
    if (this.extension && this.extension.rootURI instanceof Ci.nsIJARURI) {
      let file = this.extension.rootURI.JARFile.QueryInterface(Ci.nsIFileURL).file;
      await Services.ppmm.broadcastAsyncMessage("Extension:FlushJarCache", {path: file.path});
    }
  }

  get version() {
    return this.addon && this.addon.version;
  }

  startup() {
    if (this.state != "uninitialized") {
      throw new Error("Extension already started");
    }

    this.state = "pending";
    this.startupPromise = new Promise(resolve => {
      this.resolveStartup = resolve;
    });

    return this._install(this.file);
  }

  async unload() {
    await this._flushCache();
    return super.unload();
  }

  async upgrade(data) {
    this.startupPromise = new Promise(resolve => {
      this.resolveStartup = resolve;
    });
    this.state = "restarting";

    await this._flushCache();

    let xpiFile = Extension.generateXPI(data);

    this.cleanupFiles.push(xpiFile);

    return this._install(xpiFile);
  }
}

var ExtensionTestUtils = {
  BASE_MANIFEST,

  async normalizeManifest(manifest, baseManifest = BASE_MANIFEST) {
    await Management.lazyInit();

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
  },

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
    if (data.useAddonManager) {
      let xpiFile = Extension.generateXPI(data);

      return new AOMExtensionWrapper(this.currentScope, xpiFile, data.useAddonManager);
    }

    let extension = Extension.generate(data);

    return new ExtensionWrapper(this.currentScope, extension);
  },

  get remoteContentScripts() {
    return REMOTE_CONTENT_SCRIPTS;
  },

  set remoteContentScripts(val) {
    REMOTE_CONTENT_SCRIPTS = !!val;
  },

  /**
   * Loads a content page into a hidden docShell.
   *
   * @param {string} url
   *        The URL to load.
   * @param {object} [options = {}]
   * @param {ExtensionWrapper} [options.extension]
   *        If passed, load the URL as an extension page for the given
   *        extension.
   * @param {boolean} [options.remote]
   *        If true, load the URL in a content process. If false, load
   *        it in the parent process.
   * @param {string} [options.redirectUrl]
   *        An optional URL that the initial page is expected to
   *        redirect to.
   *
   * @returns {ContentPage}
   */
  loadContentPage(url, {extension = undefined, remote = undefined, redirectUrl = undefined} = {}) {
    let contentPage = new ContentPage(remote, extension && extension.extension);

    return contentPage.loadURL(url, redirectUrl).then(() => {
      return contentPage;
    });
  },
};
