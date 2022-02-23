/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

var EXPORTED_SYMBOLS = ["ExtensionTestUtils"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "AddonTestUtils",
  "resource://testing-common/AddonTestUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "ExtensionTestCommon",
  "resource://testing-common/ExtensionTestCommon.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "FileUtils",
  "resource://gre/modules/FileUtils.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Management",
  "resource://gre/modules/Extension.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Schemas",
  "resource://gre/modules/Schemas.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Services",
  "resource://gre/modules/Services.jsm"
);

let BASE_MANIFEST = Object.freeze({
  applications: Object.freeze({
    gecko: Object.freeze({
      id: "test@web.ext",
    }),
  }),

  manifest_version: 2,

  name: "name",
  version: "0",
});

class ExtensionWrapper {
  constructor(testScope, extension = null) {
    this.testScope = testScope;

    this.extension = null;

    this.handleResult = this.handleResult.bind(this);
    this.handleMessage = this.handleMessage.bind(this);

    this.state = "uninitialized";

    this.testResolve = null;
    this.testDone = new Promise(resolve => {
      this.testResolve = resolve;
    });

    this.messageHandler = new Map();
    this.messageAwaiter = new Map();

    this.messageQueue = new Set();

    this.testScope.registerCleanupFunction(() => {
      this.clearMessageQueues();

      if (this.state == "pending" || this.state == "running") {
        this.testScope.equal(
          this.state,
          "unloaded",
          "Extension left running at test shutdown"
        );
        return this.unload();
      } else if (this.state == "unloading") {
        this.testScope.equal(
          this.state,
          "unloaded",
          "Extension not fully unloaded at test shutdown"
        );
      }
      this.destroy();
    });

    if (extension) {
      this.id = extension.id;
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
    this.uuid = extension.uuid;
    this.extension = extension;

    extension.on("test-eq", this.handleResult);
    extension.on("test-log", this.handleResult);
    extension.on("test-result", this.handleResult);
    extension.on("test-done", this.handleResult);
    extension.on("test-message", this.handleMessage);

    this.testScope.info(`Extension attached`);
  }

  clearMessageQueues() {
    if (this.messageQueue.size) {
      let names = Array.from(this.messageQueue, ([msg]) => msg);
      this.testScope.equal(
        JSON.stringify(names),
        "[]",
        "message queue is empty"
      );
      this.messageQueue.clear();
    }
    if (this.messageAwaiter.size) {
      let names = Array.from(this.messageAwaiter.keys());
      this.testScope.equal(
        JSON.stringify(names),
        "[]",
        "no tasks awaiting on messages"
      );
      for (let promise of this.messageAwaiter.values()) {
        promise.reject();
      }
      this.messageAwaiter.clear();
    }
  }

  handleResult(kind, pass, msg, expected, actual) {
    switch (kind) {
      case "test-eq":
        this.testScope.ok(
          pass,
          `${msg} - Expected: ${expected}, Actual: ${actual}`
        );
        break;

      case "test-log":
        this.testScope.info(msg);
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

  async startup() {
    if (this.state != "uninitialized") {
      throw new Error("Extension already started");
    }
    this.state = "pending";

    await ExtensionTestCommon.setIncognitoOverride(this.extension);

    this.startupPromise = this.extension.startup().then(
      result => {
        this.state = "running";

        return result;
      },
      error => {
        this.state = "failed";

        return Promise.reject(error);
      }
    );

    return this.startupPromise;
  }

  async unload() {
    if (this.state != "running") {
      throw new Error("Extension not running");
    }
    this.state = "unloading";

    if (this.addonPromise) {
      // If addonPromise is still pending resolution, wait for it to make sure
      // that add-ons that are installed through the AddonManager are properly
      // uninstalled.
      await this.addonPromise;
    }

    if (this.addon) {
      await this.addon.uninstall();
    } else {
      await this.extension.shutdown();
    }

    if (AppConstants.platform === "android") {
      // We need a way to notify the embedding layer that an extension has been
      // uninstalled, so that the java layer can be updated too.
      Services.obs.notifyObservers(
        null,
        "testing-uninstalled-addon",
        this.addon ? this.addon.id : this.extension.id
      );
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

      this.messageAwaiter.set(msg, { resolve, reject });
      this.checkMessages();
    });
  }

  onMessage(msg, callback) {
    this.checkDuplicateListeners(msg);
    this.messageHandler.set(msg, callback);
  }
}

class AOMExtensionWrapper extends ExtensionWrapper {
  constructor(testScope) {
    super(testScope);

    this.onEvent = this.onEvent.bind(this);

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
  }

  setRestarting() {
    if (this.state !== "restarting") {
      this.startupPromise = new Promise(resolve => {
        this.resolveStartup = resolve;
      }).then(async result => {
        await this.addonPromise;
        return result;
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
        if (this.state === "uninitialized") {
          // startup() not called yet, ignore AddonManager startup notification.
          return;
        }
        this.addonPromise = AddonManager.getAddonByID(this.id).then(addon => {
          this.addon = addon;
          this.addonPromise = null;
        });
      // FALLTHROUGH
      case "addon-manager-shutdown":
        if (this.state === "uninitialized") {
          return;
        }
        this.addon = null;

        this.setRestarting();
        break;

      case "startup": {
        let [extension] = args;

        this.maybeSetID(extension.rootURI, extension.id);

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
          if (AppConstants.platform === "android") {
            // We need a way to notify the embedding layer that a new extension
            // has been installed, so that the java layer can be updated too.
            Services.obs.notifyObservers(
              null,
              "testing-installed-addon",
              extension.id
            );
          }
          this.resolveStartup(extension);
        }
        break;
      }
    }
  }

  async _flushCache() {
    if (this.extension && this.extension.rootURI instanceof Ci.nsIJARURI) {
      let file = this.extension.rootURI.JARFile.QueryInterface(Ci.nsIFileURL)
        .file;
      await Services.ppmm.broadcastAsyncMessage("Extension:FlushJarCache", {
        path: file.path,
      });
    }
  }

  get version() {
    return this.addon && this.addon.version;
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

    let xpiFile = ExtensionTestCommon.generateXPI(data);

    this.cleanupFiles.push(xpiFile);

    return this._install(xpiFile);
  }
}

class InstallableWrapper extends AOMExtensionWrapper {
  constructor(testScope, xpiFile, addonData = {}) {
    super(testScope);

    this.file = xpiFile;
    this.addonData = addonData;
    this.installType = addonData.useAddonManager || "temporary";
    this.installTelemetryInfo = addonData.amInstallTelemetryInfo;

    this.cleanupFiles = [xpiFile];
  }

  destroy() {
    super.destroy();

    for (let file of this.cleanupFiles.splice(0)) {
      try {
        Services.obs.notifyObservers(file, "flush-cache-entry");
        file.remove(false);
      } catch (e) {
        Cu.reportError(e);
      }
    }
  }

  maybeSetID(uri, id) {
    if (
      !this.id &&
      uri instanceof Ci.nsIJARURI &&
      uri.JARFile.QueryInterface(Ci.nsIFileURL).file.equals(this.file)
    ) {
      this.id = id;
    }
  }

  _setIncognitoOverride() {
    // this.id is not set yet so grab it from the manifest data to set
    // the incognito permission.
    let { addonData } = this;
    if (addonData && addonData.incognitoOverride) {
      try {
        let { id } = addonData.manifest.applications.gecko;
        if (id) {
          return ExtensionTestCommon.setIncognitoOverride({ id, addonData });
        }
      } catch (e) {}
      throw new Error(
        "Extension ID is required for setting incognito permission."
      );
    }
  }

  async _install(xpiFile) {
    await this._setIncognitoOverride();

    if (this.installType === "temporary") {
      return AddonManager.installTemporaryAddon(xpiFile)
        .then(addon => {
          this.id = addon.id;
          this.addon = addon;

          return this.startupPromise;
        })
        .catch(e => {
          this.state = "unloaded";
          return Promise.reject(e);
        });
    } else if (this.installType === "permanent") {
      return AddonManager.getInstallForFile(
        xpiFile,
        null,
        this.installTelemetryInfo
      ).then(install => {
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
}

class ExternallyInstalledWrapper extends AOMExtensionWrapper {
  constructor(testScope, id) {
    super(testScope);

    this.id = id;
    this.startupPromise = new Promise(resolve => {
      this.resolveStartup = resolve;
    });

    this.state = "restarting";
  }

  maybeSetID(uri, id) {}
}

var ExtensionTestUtils = {
  BASE_MANIFEST,

  async normalizeManifest(
    manifest,
    manifestType = "manifest.WebExtensionManifest",
    baseManifest = BASE_MANIFEST
  ) {
    await Management.lazyInit();

    manifest = Object.assign({}, baseManifest, manifest);

    let errors = [];
    let context = {
      url: null,
      manifestVersion: manifest.manifest_version,

      logError: error => {
        errors.push(error);
      },

      preprocessors: {},
    };

    let normalized = Schemas.normalize(manifest, manifestType, context);
    normalized.errors = errors;

    return normalized;
  },

  currentScope: null,

  profileDir: null,

  init(scope) {
    XPCShellContentUtils.init(scope);

    this.currentScope = scope;

    this.profileDir = scope.do_get_profile();

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

      QueryInterface: ChromeUtils.generateQI(["nsIDirectoryServiceProvider"]),
    };
    Services.dirsvc.registerProvider(dirProvider);

    scope.registerCleanupFunction(() => {
      try {
        tmpD.remove(true);
      } catch (e) {
        Cu.reportError(e);
      }
      Services.dirsvc.unregisterProvider(dirProvider);

      this.currentScope = null;
    });
  },

  addonManagerStarted: false,

  mockAppInfo() {
    AddonTestUtils.createAppInfo(
      "xpcshell@tests.mozilla.org",
      "XPCShell",
      "48",
      "48"
    );
  },

  startAddonManager() {
    if (this.addonManagerStarted) {
      return;
    }
    this.addonManagerStarted = true;
    this.mockAppInfo();

    return AddonTestUtils.promiseStartupManager();
  },

  loadExtension(data) {
    if (data.useAddonManager) {
      // If we're using incognitoOverride, we'll need to ensure
      // an ID is available before generating the XPI.
      if (data.incognitoOverride) {
        ExtensionTestCommon.setExtensionID(data);
      }
      let xpiFile = ExtensionTestCommon.generateXPI(data);

      return this.loadExtensionXPI(xpiFile, data);
    }

    let extension = ExtensionTestCommon.generate(data);

    return new ExtensionWrapper(this.currentScope, extension);
  },

  loadExtensionXPI(xpiFile, data) {
    return new InstallableWrapper(this.currentScope, xpiFile, data);
  },

  // Create a wrapper for a webextension that will be installed
  // by some external process (e.g., Normandy)
  expectExtension(id) {
    return new ExternallyInstalledWrapper(this.currentScope, id);
  },

  failOnSchemaWarnings(warningsAsErrors = true) {
    let prefName = "extensions.webextensions.warnings-as-errors";
    Services.prefs.setBoolPref(prefName, warningsAsErrors);
    if (!warningsAsErrors) {
      this.currentScope.registerCleanupFunction(() => {
        Services.prefs.setBoolPref(prefName, true);
      });
    }
  },

  get remoteContentScripts() {
    return XPCShellContentUtils.remoteContentScripts;
  },

  set remoteContentScripts(val) {
    XPCShellContentUtils.remoteContentScripts = val;
  },

  async fetch(...args) {
    return XPCShellContentUtils.fetch(...args);
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
   * @param {boolean} [options.remoteSubframes]
   *        If true, load cross-origin frames in separate content processes.
   *        This is ignored if |options.remote| is false.
   * @param {string} [options.redirectUrl]
   *        An optional URL that the initial page is expected to
   *        redirect to.
   *
   * @returns {ContentPage}
   */
  loadContentPage(...args) {
    return XPCShellContentUtils.loadContentPage(...args);
  },
};
