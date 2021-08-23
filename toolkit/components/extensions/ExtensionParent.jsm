/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * This module contains code for managing APIs that need to run in the
 * parent process, and handles the parent side of operations that need
 * to be proxied from ExtensionChild.jsm.
 */

/* exported ExtensionParent */

var EXPORTED_SYMBOLS = ["ExtensionParent"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.jsm",
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  BroadcastConduit: "resource://gre/modules/ConduitsParent.jsm",
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  DevToolsShim: "chrome://devtools-startup/content/DevToolsShim.jsm",
  ExtensionData: "resource://gre/modules/Extension.jsm",
  ExtensionActivityLog: "resource://gre/modules/ExtensionActivityLog.jsm",
  GeckoViewConnection: "resource://gre/modules/GeckoViewWebExtension.jsm",
  MessageManagerProxy: "resource://gre/modules/MessageManagerProxy.jsm",
  NativeApp: "resource://gre/modules/NativeMessaging.jsm",
  PerformanceCounters: "resource://gre/modules/PerformanceCounters.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  aomStartup: [
    "@mozilla.org/addons/addon-manager-startup;1",
    "amIAddonManagerStartup",
  ],
});

// We're using the pref to avoid loading PerformanceCounters.jsm for nothing.
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "gTimingEnabled",
  "extensions.webextensions.enablePerformanceCounters",
  false
);
const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
const { ExtensionUtils } = ChromeUtils.import(
  "resource://gre/modules/ExtensionUtils.jsm"
);

var {
  BaseContext,
  CanOfAPIs,
  SchemaAPIManager,
  SpreadArgs,
  defineLazyGetter,
} = ExtensionCommon;

var {
  DefaultMap,
  DefaultWeakMap,
  ExtensionError,
  promiseDocumentLoaded,
  promiseEvent,
  promiseObserved,
} = ExtensionUtils;

const ERROR_NO_RECEIVERS =
  "Could not establish connection. Receiving end does not exist.";

const BASE_SCHEMA = "chrome://extensions/content/schemas/manifest.json";
const CATEGORY_EXTENSION_MODULES = "webextension-modules";
const CATEGORY_EXTENSION_SCHEMAS = "webextension-schemas";
const CATEGORY_EXTENSION_SCRIPTS = "webextension-scripts";

let schemaURLs = new Set();

schemaURLs.add("chrome://extensions/content/schemas/experiments.json");

let GlobalManager;
let ParentAPIManager;
let StartupCache;

const global = this;

// This object loads the ext-*.js scripts that define the extension API.
let apiManager = new (class extends SchemaAPIManager {
  constructor() {
    super("main", Schemas);
    this.initialized = null;

    /* eslint-disable mozilla/balanced-listeners */
    this.on("startup", (e, extension) => {
      return extension.apiManager.onStartup(extension);
    });

    this.on("update", async (e, { id, resourceURI }) => {
      let modules = this.eventModules.get("update");
      if (modules.size == 0) {
        return;
      }

      let extension = new ExtensionData(resourceURI);
      await extension.loadManifest();

      return Promise.all(
        Array.from(modules).map(async apiName => {
          let module = await this.asyncLoadModule(apiName);
          module.onUpdate(id, extension.manifest);
        })
      );
    });

    this.on("uninstall", (e, { id }) => {
      let modules = this.eventModules.get("uninstall");
      return Promise.all(
        Array.from(modules).map(async apiName => {
          let module = await this.asyncLoadModule(apiName);
          return module.onUninstall(id);
        })
      );
    });
    /* eslint-enable mozilla/balanced-listeners */

    // Handle any changes that happened during startup
    let disabledIds = AddonManager.getStartupChanges(
      AddonManager.STARTUP_CHANGE_DISABLED
    );
    if (disabledIds.length) {
      this._callHandlers(disabledIds, "disable", "onDisable");
    }

    let uninstalledIds = AddonManager.getStartupChanges(
      AddonManager.STARTUP_CHANGE_UNINSTALLED
    );
    if (uninstalledIds.length) {
      this._callHandlers(uninstalledIds, "uninstall", "onUninstall");
    }
  }

  getModuleJSONURLs() {
    return Array.from(
      Services.catMan.enumerateCategory(CATEGORY_EXTENSION_MODULES),
      ({ value }) => value
    );
  }

  // Loads all the ext-*.js scripts currently registered.
  lazyInit() {
    if (this.initialized) {
      return this.initialized;
    }

    let modulesPromise = StartupCache.other.get(["parentModules"], () =>
      this.loadModuleJSON(this.getModuleJSONURLs())
    );

    let scriptURLs = [];
    for (let { value } of Services.catMan.enumerateCategory(
      CATEGORY_EXTENSION_SCRIPTS
    )) {
      scriptURLs.push(value);
    }

    let promise = (async () => {
      let scripts = await Promise.all(
        scriptURLs.map(url => ChromeUtils.compileScript(url))
      );

      this.initModuleData(await modulesPromise);

      this.initGlobal();
      for (let script of scripts) {
        script.executeInGlobal(this.global);
      }

      // Load order matters here. The base manifest defines types which are
      // extended by other schemas, so needs to be loaded first.
      return Schemas.load(BASE_SCHEMA).then(() => {
        let promises = [];
        for (let { value } of Services.catMan.enumerateCategory(
          CATEGORY_EXTENSION_SCHEMAS
        )) {
          promises.push(Schemas.load(value));
        }
        for (let [url, { content }] of this.schemaURLs) {
          promises.push(Schemas.load(url, content));
        }
        for (let url of schemaURLs) {
          promises.push(Schemas.load(url));
        }
        return Promise.all(promises).then(() => {
          Schemas.updateSharedSchemas();
        });
      });
    })();

    /* eslint-disable mozilla/balanced-listeners */
    Services.mm.addMessageListener("Extension:GetTabAndWindowId", this);
    /* eslint-enable mozilla/balanced-listeners */

    this.initialized = promise;
    return this.initialized;
  }

  receiveMessage({ name, target, sync }) {
    if (name === "Extension:GetTabAndWindowId") {
      let result = this.global.tabTracker.getBrowserData(target);

      if (result.tabId) {
        if (sync) {
          return result;
        }
        target.messageManager.sendAsyncMessage(
          "Extension:SetFrameData",
          result
        );
      }
    }
  }

  // Call static handlers for the given event on the given extension ids,
  // and set up a shutdown blocker to ensure they all complete.
  _callHandlers(ids, event, method) {
    let promises = Array.from(this.eventModules.get(event))
      .map(async modName => {
        let module = await this.asyncLoadModule(modName);
        return ids.map(id => module[method](id));
      })
      .flat();
    if (event === "disable") {
      promises.push(...ids.map(id => this.emit("disable", id)));
    }
    if (event === "enabling") {
      promises.push(...ids.map(id => this.emit("enabling", id)));
    }

    AsyncShutdown.profileBeforeChange.addBlocker(
      `Extension API ${event} handlers for ${ids.join(",")}`,
      Promise.all(promises)
    );
  }
})();

// Receives messages related to the extension messaging API and forwards them
// to relevant child messengers.  Also handles Native messaging and GeckoView.
const ProxyMessenger = {
  /**
   * @typedef {object} ParentPort
   * @prop {function(StructuredCloneHolder)} onPortMessage
   * @prop {function()} onPortDisconnect
   */
  /** @type Map<number, ParentPort> */
  ports: new Map(),

  init() {
    this.conduit = new BroadcastConduit(ProxyMessenger, {
      id: "ProxyMessenger",
      reportOnClosed: "portId",
      recv: ["PortConnect", "PortMessage", "NativeMessage", "RuntimeMessage"],
      cast: ["PortConnect", "PortMessage", "PortDisconnect", "RuntimeMessage"],
    });
  },

  openNative(nativeApp, sender) {
    let context = ParentAPIManager.getContextById(sender.childId);
    let { extension } = context;
    if (extension.hasPermission("geckoViewAddons")) {
      let allowMessagingFromContent = extension.hasPermission(
        "nativeMessagingFromContent"
      );
      return new GeckoViewConnection(
        sender,
        nativeApp,
        allowMessagingFromContent
      );
    } else if (sender.verified) {
      return new NativeApp(context, nativeApp);
    }
    throw new Error(`Native messaging not allowed: ${JSON.stringify(sender)}`);
  },

  recvNativeMessage({ nativeApp, holder }, { sender }) {
    return this.openNative(nativeApp, sender).sendMessage(holder);
  },

  getSender(extension, source) {
    let { extensionId, envType, frameId, url, actor, id } = source;
    let sender = { id: extensionId, envType, frameId, url, contextId: id };
    let target = actor.browsingContext.top.embedderElement;
    apiManager.global.tabGetSender(extension, target, sender);
    return sender;
  },

  getTopBrowsingContextId(tabId) {
    // If a tab alredy has content scripts, no need to check private browsing.
    let tab = apiManager.global.tabTracker.getTab(tabId, null);
    if (!tab || (tab.browser || tab).getAttribute("pending") === "true") {
      // No receivers in discarded tabs, so bail early to keep the browser lazy.
      throw new ExtensionError(ERROR_NO_RECEIVERS);
    }
    let browser = tab.linkedBrowser || tab.browser;
    return browser.browsingContext.id;
  },

  // TODO: Rework/simplify this and getSender/getTopBC after bug 1580766.
  async normalizeArgs(arg, sender) {
    arg.extensionId = arg.extensionId || sender.extensionId;
    let extension = GlobalManager.extensionMap.get(arg.extensionId);
    if (!extension) {
      return Promise.reject({ message: ERROR_NO_RECEIVERS });
    }
    await extension.wakeupBackground?.();

    arg.sender = this.getSender(extension, sender);
    arg.topBC = arg.tabId && this.getTopBrowsingContextId(arg.tabId);
    return arg.tabId ? "tab" : "messenger";
  },

  async recvRuntimeMessage(arg, { sender }) {
    arg.firstResponse = true;
    let kind = await this.normalizeArgs(arg, sender);
    let result = await this.conduit.castRuntimeMessage(kind, arg);
    if (!result) {
      // "throw new ExtensionError" cannot be used because then the stack of the
      // sendMessage call would not be added to the error object generated by
      // context.normalizeError. Test coverage by test_ext_error_location.js.
      return Promise.reject({ message: ERROR_NO_RECEIVERS });
    }
    return result.value;
  },

  async recvPortConnect(arg, { sender }) {
    if (arg.native) {
      let port = this.openNative(arg.name, sender).onConnect(arg.portId, this);
      this.ports.set(arg.portId, port);
      return;
    }

    // PortMessages that follow will need to wait for the port to be opened.
    let resolvePort;
    this.ports.set(arg.portId, new Promise(res => (resolvePort = res)));

    let kind = await this.normalizeArgs(arg, sender);
    let all = await this.conduit.castPortConnect(kind, arg);
    resolvePort();

    // If there are no active onConnect listeners.
    if (!all.some(x => x.value)) {
      throw new ExtensionError(ERROR_NO_RECEIVERS);
    }
  },

  async recvPortMessage({ holder }, { sender }) {
    if (sender.native) {
      return this.ports.get(sender.portId).onPortMessage(holder);
    }
    await this.ports.get(sender.portId);
    this.sendPortMessage(sender.portId, holder, !sender.source);
  },

  recvConduitClosed(sender) {
    let app = this.ports.get(sender.portId);
    if (this.ports.delete(sender.portId) && sender.native) {
      return app.onPortDisconnect();
    }
    this.sendPortDisconnect(sender.portId, null, !sender.source);
  },

  sendPortMessage(portId, holder, source = true) {
    this.conduit.castPortMessage("port", { portId, source, holder });
  },

  sendPortDisconnect(portId, error, source = true) {
    this.conduit.castPortDisconnect("port", { portId, source, error });
    this.ports.delete(portId);
  },
};
ProxyMessenger.init();

// Responsible for loading extension APIs into the right globals.
GlobalManager = {
  // Map[extension ID -> Extension]. Determines which extension is
  // responsible for content under a particular extension ID.
  extensionMap: new Map(),
  initialized: false,

  init(extension) {
    if (this.extensionMap.size == 0) {
      apiManager.on("extension-browser-inserted", this._onExtensionBrowser);
      this.initialized = true;
      Services.ppmm.addMessageListener(
        "Extension:SendPerformanceCounter",
        this
      );
    }
    this.extensionMap.set(extension.id, extension);
  },

  uninit(extension) {
    this.extensionMap.delete(extension.id);

    if (this.extensionMap.size == 0 && this.initialized) {
      apiManager.off("extension-browser-inserted", this._onExtensionBrowser);
      this.initialized = false;
      Services.ppmm.removeMessageListener(
        "Extension:SendPerformanceCounter",
        this
      );
    }
  },

  async receiveMessage({ name, data }) {
    switch (name) {
      case "Extension:SendPerformanceCounter":
        PerformanceCounters.merge(data.counters);
        break;
    }
  },

  _onExtensionBrowser(type, browser, additionalData = {}) {
    browser.messageManager.loadFrameScript(
      "resource://gre/modules/onExtensionBrowser.js",
      false,
      true
    );

    let viewType = browser.getAttribute("webextension-view-type");
    if (viewType) {
      let data = { viewType };

      let { tabTracker } = apiManager.global;
      Object.assign(data, tabTracker.getBrowserData(browser), additionalData);

      browser.messageManager.sendAsyncMessage("Extension:SetFrameData", data);
    }
  },

  getExtension(extensionId) {
    return this.extensionMap.get(extensionId);
  },
};

/**
 * The proxied parent side of a context in ExtensionChild.jsm, for the
 * parent side of a proxied API.
 */
class ProxyContextParent extends BaseContext {
  constructor(envType, extension, params, xulBrowser, principal) {
    super(envType, extension);

    this.uri = Services.io.newURI(params.url);

    this.incognito = params.incognito;

    this.listenerPromises = new Set();

    // This message manager is used by ParentAPIManager to send messages and to
    // close the ProxyContext if the underlying message manager closes. This
    // message manager object may change when `xulBrowser` swaps docshells, e.g.
    // when a tab is moved to a different window.
    this.messageManagerProxy = new MessageManagerProxy(xulBrowser);

    Object.defineProperty(this, "principal", {
      value: principal,
      enumerable: true,
      configurable: true,
    });

    this.listenerProxies = new Map();

    this.pendingEventBrowser = null;

    apiManager.emit("proxy-context-load", this);
  }

  async withPendingBrowser(browser, callable) {
    let savedBrowser = this.pendingEventBrowser;
    this.pendingEventBrowser = browser;
    try {
      let result = await callable();
      return result;
    } finally {
      this.pendingEventBrowser = savedBrowser;
    }
  }

  logActivity(type, name, data) {
    // The base class will throw so we catch any subclasses that do not implement.
    // We do not want to throw here, but we also do not log here.
  }

  get cloneScope() {
    return this.sandbox;
  }

  applySafe(callback, args) {
    // There's no need to clone when calling listeners for a proxied
    // context.
    return this.applySafeWithoutClone(callback, args);
  }

  get xulBrowser() {
    return this.messageManagerProxy.eventTarget;
  }

  get parentMessageManager() {
    return this.messageManagerProxy.messageManager;
  }

  shutdown() {
    this.unload();
  }

  unload() {
    if (this.unloaded) {
      return;
    }
    this.messageManagerProxy.dispose();
    super.unload();
    apiManager.emit("proxy-context-unload", this);
  }
}

defineLazyGetter(ProxyContextParent.prototype, "apiCan", function() {
  let obj = {};
  let can = new CanOfAPIs(this, this.extension.apiManager, obj);
  return can;
});

defineLazyGetter(ProxyContextParent.prototype, "apiObj", function() {
  return this.apiCan.root;
});

defineLazyGetter(ProxyContextParent.prototype, "sandbox", function() {
  // NOTE: the required Blob and URL globals are used in the ext-registerContentScript.js
  // API module to convert JS and CSS data into blob URLs.
  return Cu.Sandbox(this.principal, {
    sandboxName: this.uri.spec,
    wantGlobalProperties: ["Blob", "URL"],
  });
});

/**
 * The parent side of proxied API context for extension content script
 * running in ExtensionContent.jsm.
 */
class ContentScriptContextParent extends ProxyContextParent {}

/**
 * The parent side of proxied API context for extension page, such as a
 * background script, a tab page, or a popup, running in
 * ExtensionChild.jsm.
 */
class ExtensionPageContextParent extends ProxyContextParent {
  constructor(envType, extension, params, xulBrowser) {
    super(envType, extension, params, xulBrowser, extension.principal);

    this.viewType = params.viewType;

    this.extension.views.add(this);

    extension.emit("extension-proxy-context-load", this);
  }

  // The window that contains this context. This may change due to moving tabs.
  get appWindow() {
    let win = this.xulBrowser.ownerGlobal;
    return win.browsingContext.topChromeWindow;
  }

  get currentWindow() {
    if (this.viewType !== "background") {
      return this.appWindow;
    }
  }

  get tabId() {
    let { tabTracker } = apiManager.global;
    let data = tabTracker.getBrowserData(this.xulBrowser);
    if (data.tabId >= 0) {
      return data.tabId;
    }
  }

  onBrowserChange(browser) {
    super.onBrowserChange(browser);
    this.xulBrowser = browser;
  }

  unload() {
    super.unload();
    this.extension.views.delete(this);
  }

  shutdown() {
    apiManager.emit("page-shutdown", this);
    super.shutdown();
  }
}

/**
 * The parent side of proxied API context for devtools extension page, such as a
 * devtools pages and panels running in ExtensionChild.jsm.
 */
class DevToolsExtensionPageContextParent extends ExtensionPageContextParent {
  constructor(...params) {
    super(...params);

    // Set all attributes that are lazily defined to `null` here.
    //
    // Note that we can't do that for `this._devToolsToolbox` because it will
    // be defined when calling our parent constructor and so would override it back to `null`.
    this._devToolsCommands = null;
    this._onNavigatedListeners = null;

    this._onResourceAvailable = this._onResourceAvailable.bind(this);
  }

  set devToolsToolbox(toolbox) {
    if (this._devToolsToolbox) {
      throw new Error("Cannot set the context DevTools toolbox twice");
    }

    this._devToolsToolbox = toolbox;
  }

  get devToolsToolbox() {
    return this._devToolsToolbox;
  }

  async addOnNavigatedListener(listener) {
    if (!this._onNavigatedListeners) {
      this._onNavigatedListeners = new Set();

      await this.devToolsToolbox.resourceCommand.watchResources(
        [this.devToolsToolbox.resourceCommand.TYPES.DOCUMENT_EVENT],
        {
          onAvailable: this._onResourceAvailable,
          ignoreExistingResources: true,
        }
      );
    }

    this._onNavigatedListeners.add(listener);
  }

  removeOnNavigatedListener(listener) {
    if (this._onNavigatedListeners) {
      this._onNavigatedListeners.delete(listener);
    }
  }

  /**
   * The returned "commands" object, exposing modules implemented from devtools/shared/commands.
   * Each attribute being a static interface to communicate with the server backend.
   *
   * @returns {Promise<Object>}
   */
  async getDevToolsCommands() {
    // Ensure that we try to instantiate a commands only once,
    // even if createCommandsForTabForWebExtension is async.
    if (this._devToolsCommandsPromise) {
      return this._devToolsCommandsPromise;
    }
    if (this._devToolsCommands) {
      return this._devToolsCommands;
    }

    this._devToolsCommandsPromise = (async () => {
      const commands = await DevToolsShim.createCommandsForTabForWebExtension(
        this.devToolsToolbox.descriptorFront.localTab
      );
      await commands.targetCommand.startListening();
      this._devToolsCommands = commands;
      this._devToolsCommandsPromise = null;
      return commands;
    })();
    return this._devToolsCommandsPromise;
  }

  unload() {
    // Bail if the toolbox reference was already cleared.
    if (!this.devToolsToolbox) {
      return;
    }

    if (this._onNavigatedListeners) {
      this.devToolsToolbox.resourceCommand.unwatchResources(
        [this.devToolsToolbox.resourceCommand.TYPES.DOCUMENT_EVENT],
        { onAvailable: this._onResourceAvailable }
      );
    }

    if (this._devToolsCommands) {
      this._devToolsCommands.destroy();
      this._devToolsCommands = null;
    }

    if (this._onNavigatedListeners) {
      this._onNavigatedListeners.clear();
      this._onNavigatedListeners = null;
    }

    this._devToolsToolbox = null;

    super.unload();
  }

  async _onResourceAvailable(resources) {
    for (const resource of resources) {
      const { targetFront } = resource;
      if (targetFront.isTopLevel && resource.name === "dom-complete") {
        const url = targetFront.localTab.linkedBrowser.currentURI.spec;
        for (const listener of this._onNavigatedListeners) {
          listener(url);
        }
      }
    }
  }
}

ParentAPIManager = {
  proxyContexts: new Map(),

  init() {
    // TODO: Bug 1595186 - remove/replace all usage of MessageManager below.
    Services.obs.addObserver(this, "message-manager-close");

    this.conduit = new BroadcastConduit(this, {
      id: "ParentAPIManager",
      reportOnClosed: "childId",
      recv: ["CreateProxyContext", "APICall", "AddListener", "RemoveListener"],
      send: ["CallResult"],
      query: ["RunListener"],
    });
  },

  attachMessageManager(extension, processMessageManager) {
    extension.parentMessageManager = processMessageManager;
  },

  async observe(subject, topic, data) {
    if (topic === "message-manager-close") {
      let mm = subject;
      for (let [childId, context] of this.proxyContexts) {
        if (context.parentMessageManager === mm) {
          this.closeProxyContext(childId);
        }
      }

      // Reset extension message managers when their child processes shut down.
      for (let extension of GlobalManager.extensionMap.values()) {
        if (extension.parentMessageManager === mm) {
          extension.parentMessageManager = null;
        }
      }
    }
  },

  shutdownExtension(extensionId, reason) {
    if (["ADDON_DISABLE", "ADDON_UNINSTALL"].includes(reason)) {
      apiManager._callHandlers([extensionId], "disable", "onDisable");
    }

    for (let [childId, context] of this.proxyContexts) {
      if (context.extension.id == extensionId) {
        context.shutdown();
        this.proxyContexts.delete(childId);
      }
    }
  },

  recvCreateProxyContext(data, { actor, sender }) {
    let { envType, extensionId, childId, principal } = data;
    let target = actor.browsingContext.top.embedderElement;

    if (this.proxyContexts.has(childId)) {
      throw new Error(
        "A WebExtension context with the given ID already exists!"
      );
    }

    let extension = GlobalManager.getExtension(extensionId);
    if (!extension) {
      throw new Error(`No WebExtension found with ID ${extensionId}`);
    }

    let context;
    if (envType == "addon_parent" || envType == "devtools_parent") {
      if (!sender.verified) {
        throw new Error(`Bad sender context envType: ${sender.envType}`);
      }

      let processMessageManager =
        target.messageManager.processMessageManager ||
        Services.ppmm.getChildAt(0);

      if (!extension.parentMessageManager) {
        if (target.remoteType === extension.remoteType) {
          this.attachMessageManager(extension, processMessageManager);
        }
      }

      if (processMessageManager !== extension.parentMessageManager) {
        throw new Error(
          "Attempt to create privileged extension parent from incorrect child process"
        );
      }

      if (envType == "addon_parent") {
        context = new ExtensionPageContextParent(
          envType,
          extension,
          data,
          target
        );
      } else if (envType == "devtools_parent") {
        context = new DevToolsExtensionPageContextParent(
          envType,
          extension,
          data,
          target
        );
      }
    } else if (envType == "content_parent") {
      context = new ContentScriptContextParent(
        envType,
        extension,
        data,
        target,
        principal
      );
    } else {
      throw new Error(`Invalid WebExtension context envType: ${envType}`);
    }
    this.proxyContexts.set(childId, context);
  },

  recvConduitClosed(sender) {
    this.closeProxyContext(sender.id);
  },

  closeProxyContext(childId) {
    let context = this.proxyContexts.get(childId);
    if (context) {
      context.unload();
      this.proxyContexts.delete(childId);
    }
  },

  async retrievePerformanceCounters() {
    // getting the parent counters
    return PerformanceCounters.getData();
  },

  /**
   * Call the given function and also log the call as appropriate
   * (i.e., with PerformanceCounters and/or activity logging)
   *
   * @param {BaseContext} context The context making this call.
   * @param {object} data Additional data about the call.
   * @param {function} callable The actual implementation to invoke.
   */
  async callAndLog(context, data, callable) {
    let { id } = context.extension;
    // If we were called via callParentAsyncFunction we don't want
    // to log again, check for the flag.
    const { alreadyLogged } = data.options || {};
    if (!alreadyLogged) {
      ExtensionActivityLog.log(id, context.viewType, "api_call", data.path, {
        args: data.args,
      });
    }

    let start = Cu.now();
    try {
      return callable();
    } finally {
      ChromeUtils.addProfilerMarker(
        "ExtensionParent",
        { startTime: start },
        `${id}, api_call: ${data.path}`
      );
      if (gTimingEnabled) {
        let end = Cu.now() * 1000;
        PerformanceCounters.storeExecutionTime(
          id,
          data.path,
          end - start * 1000
        );
      }
    }
  },

  async recvAPICall(data, { actor }) {
    let context = this.getContextById(data.childId);
    let target = actor.browsingContext.top.embedderElement;
    if (context.parentMessageManager !== target.messageManager) {
      throw new Error("Got message on unexpected message manager");
    }

    let reply = result => {
      if (!context.parentMessageManager) {
        Services.console.logStringMessage(
          "Cannot send function call result: other side closed connection " +
            `(call data: ${uneval({ path: data.path, args: data.args })})`
        );
        return;
      }

      this.conduit.sendCallResult(data.childId, {
        childId: data.childId,
        callId: data.callId,
        path: data.path,
        ...result,
      });
    };

    try {
      let args = data.args;
      let pendingBrowser = context.pendingEventBrowser;
      let fun = await context.apiCan.asyncFindAPIPath(data.path);
      let result = this.callAndLog(context, data, () => {
        return context.withPendingBrowser(pendingBrowser, () => fun(...args));
      });

      if (data.callId) {
        result = result || Promise.resolve();

        result.then(
          result => {
            result = result instanceof SpreadArgs ? [...result] : [result];

            let holder = new StructuredCloneHolder(result);

            reply({ result: holder });
          },
          error => {
            error = context.normalizeError(error);
            reply({
              error: { message: error.message, fileName: error.fileName },
            });
          }
        );
      }
    } catch (e) {
      if (data.callId) {
        let error = context.normalizeError(e);
        reply({ error: { message: error.message } });
      } else {
        Cu.reportError(e);
      }
    }
  },

  async recvAddListener(data, { actor }) {
    let context = this.getContextById(data.childId);
    let target = actor.browsingContext.top.embedderElement;
    if (context.parentMessageManager !== target.messageManager) {
      throw new Error("Got message on unexpected message manager");
    }

    let { childId, alreadyLogged = false } = data;
    let handlingUserInput = false;

    let listener = async (...listenerArgs) => {
      let startTime = Cu.now();
      // Extract urgentSend flag to avoid deserializing args holder later.
      let urgentSend = false;
      if (listenerArgs[0] && data.path.startsWith("webRequest.")) {
        urgentSend = listenerArgs[0].urgentSend;
        delete listenerArgs[0].urgentSend;
      }
      let result = await this.conduit.queryRunListener(childId, {
        childId,
        handlingUserInput,
        listenerId: data.listenerId,
        path: data.path,
        urgentSend,
        get args() {
          return new StructuredCloneHolder(listenerArgs);
        },
      });
      let rv = result && result.deserialize(global);
      ChromeUtils.addProfilerMarker(
        "ExtensionParent",
        { startTime },
        `${context.extension.id}, api_event: ${data.path}`
      );
      ExtensionActivityLog.log(
        context.extension.id,
        context.viewType,
        "api_event",
        data.path,
        { args: listenerArgs, result: rv }
      );
      return rv;
    };

    context.listenerProxies.set(data.listenerId, listener);

    let args = data.args;
    let promise = context.apiCan.asyncFindAPIPath(data.path);

    // Store pending listener additions so we can be sure they're all
    // fully initialize before we consider extension startup complete.
    if (context.viewType === "background" && context.listenerPromises) {
      const { listenerPromises } = context;
      listenerPromises.add(promise);
      let remove = () => {
        listenerPromises.delete(promise);
      };
      promise.then(remove, remove);
    }

    let handler = await promise;
    if (handler.setUserInput) {
      handlingUserInput = true;
    }
    handler.addListener(listener, ...args);
    if (!alreadyLogged) {
      ExtensionActivityLog.log(
        context.extension.id,
        context.viewType,
        "api_call",
        `${data.path}.addListener`,
        { args }
      );
    }
  },

  async recvRemoveListener(data) {
    let context = this.getContextById(data.childId);
    let listener = context.listenerProxies.get(data.listenerId);

    let handler = await context.apiCan.asyncFindAPIPath(data.path);
    handler.removeListener(listener);

    let { alreadyLogged = false } = data;
    if (!alreadyLogged) {
      ExtensionActivityLog.log(
        context.extension.id,
        context.viewType,
        "api_call",
        `${data.path}.removeListener`,
        { args: [] }
      );
    }
  },

  getContextById(childId) {
    let context = this.proxyContexts.get(childId);
    if (!context) {
      throw new Error("WebExtension context not found!");
    }
    return context;
  },
};

ParentAPIManager.init();

/**
 * A hidden window which contains the extension pages that are not visible
 * (i.e., background pages and devtools pages), and is also used by
 * ExtensionDebuggingUtils to contain the browser elements used by the
 * addon debugger to connect to the devtools actors running in the same
 * process of the target extension (and be able to stay connected across
 *  the addon reloads).
 */
class HiddenXULWindow {
  constructor() {
    this._windowlessBrowser = null;
    this.unloaded = false;
    this.waitInitialized = this.initWindowlessBrowser();
  }

  shutdown() {
    if (this.unloaded) {
      throw new Error(
        "Unable to shutdown an unloaded HiddenXULWindow instance"
      );
    }

    this.unloaded = true;

    this.waitInitialized = null;

    if (!this._windowlessBrowser) {
      Cu.reportError("HiddenXULWindow was shut down while it was loading.");
      // initWindowlessBrowser will close windowlessBrowser when possible.
      return;
    }

    this._windowlessBrowser.close();
    this._windowlessBrowser = null;
  }

  get chromeDocument() {
    return this._windowlessBrowser.document;
  }

  /**
   * Private helper that create a HTMLDocument in a windowless browser.
   *
   * @returns {Promise<void>}
   *          A promise which resolves when the windowless browser is ready.
   */
  async initWindowlessBrowser() {
    if (this.waitInitialized) {
      throw new Error("HiddenXULWindow already initialized");
    }

    // The invisible page is currently wrapped in a XUL window to fix an issue
    // with using the canvas API from a background page (See Bug 1274775).
    let windowlessBrowser = Services.appShell.createWindowlessBrowser(true);

    // The windowless browser is a thin wrapper around a docShell that keeps
    // its related resources alive. It implements nsIWebNavigation and
    // forwards its methods to the underlying docShell. That .docShell
    // needs `QueryInterface(nsIWebNavigation)` to give us access to the
    // webNav methods that are already available on the windowless browser.
    let chromeShell = windowlessBrowser.docShell;
    chromeShell.QueryInterface(Ci.nsIWebNavigation);

    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      let attrs = chromeShell.getOriginAttributes();
      attrs.privateBrowsingId = 1;
      chromeShell.setOriginAttributes(attrs);
    }

    windowlessBrowser.browsingContext.useGlobalHistory = false;
    chromeShell.loadURI("chrome://extensions/content/dummy.xhtml", {
      triggeringPrincipal: Services.scriptSecurityManager.getSystemPrincipal(),
    });

    await promiseObserved(
      "chrome-document-global-created",
      win => win.document == chromeShell.document
    );
    await promiseDocumentLoaded(windowlessBrowser.document);
    if (this.unloaded) {
      windowlessBrowser.close();
      return;
    }
    this._windowlessBrowser = windowlessBrowser;
  }

  /**
   * Creates the browser XUL element that will contain the WebExtension Page.
   *
   * @param {Object} xulAttributes
   *        An object that contains the xul attributes to set of the newly
   *        created browser XUL element.
   *
   * @returns {Promise<XULElement>}
   *          A Promise which resolves to the newly created browser XUL element.
   */
  async createBrowserElement(xulAttributes) {
    if (!xulAttributes || Object.keys(xulAttributes).length === 0) {
      throw new Error("missing mandatory xulAttributes parameter");
    }

    await this.waitInitialized;

    const chromeDoc = this.chromeDocument;

    const browser = chromeDoc.createXULElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    browser.setAttribute("messagemanagergroup", "webext-browsers");

    for (const [name, value] of Object.entries(xulAttributes)) {
      if (value != null) {
        browser.setAttribute(name, value);
      }
    }

    let awaitFrameLoader = Promise.resolve();

    if (browser.getAttribute("remote") === "true") {
      awaitFrameLoader = promiseEvent(browser, "XULFrameLoaderCreated");
    }

    chromeDoc.documentElement.appendChild(browser);

    // Forcibly flush layout so that we get a pres shell soon enough, see
    // bug 1274775.
    browser.getBoundingClientRect();

    await awaitFrameLoader;
    return browser;
  }
}

const SharedWindow = {
  _window: null,
  _count: 0,

  acquire() {
    if (this._window == null) {
      if (this._count != 0) {
        throw new Error(
          `Shared window already exists with count ${this._count}`
        );
      }

      this._window = new HiddenXULWindow();
    }

    this._count++;
    return this._window;
  },

  release() {
    if (this._count < 1) {
      throw new Error(`Releasing shared window with count ${this._count}`);
    }

    this._count--;
    if (this._count == 0) {
      this._window.shutdown();
      this._window = null;
    }
  },
};

/**
 * This is a base class used by the ext-backgroundPage and ext-devtools API implementations
 * to inherits the shared boilerplate code needed to create a parent document for the hidden
 * extension pages (e.g. the background page, the devtools page) in the BackgroundPage and
 * DevToolsPage classes.
 *
 * @param {Extension} extension
 *        The Extension which owns the hidden extension page created (used to decide
 *        if the hidden extension page parent doc is going to be a windowlessBrowser or
 *        a visible XUL window).
 * @param {string} viewType
 *        The viewType of the WebExtension page that is going to be loaded
 *        in the created browser element (e.g. "background" or "devtools_page").
 */
class HiddenExtensionPage {
  constructor(extension, viewType) {
    if (!extension || !viewType) {
      throw new Error("extension and viewType parameters are mandatory");
    }

    this.extension = extension;
    this.viewType = viewType;
    this.browser = null;
    this.unloaded = false;
  }

  /**
   * Destroy the created parent document.
   */
  shutdown() {
    if (this.unloaded) {
      throw new Error(
        "Unable to shutdown an unloaded HiddenExtensionPage instance"
      );
    }

    this.unloaded = true;

    if (this.browser) {
      this._releaseBrowser();
    }
  }

  _releaseBrowser() {
    this.browser.remove();
    this.browser = null;
    SharedWindow.release();
  }

  /**
   * Creates the browser XUL element that will contain the WebExtension Page.
   *
   * @returns {Promise<XULElement>}
   *          A Promise which resolves to the newly created browser XUL element.
   */
  async createBrowserElement() {
    if (this.browser) {
      throw new Error("createBrowserElement called twice");
    }

    let window = SharedWindow.acquire();
    try {
      this.browser = await window.createBrowserElement({
        "webextension-view-type": this.viewType,
        remote: this.extension.remote ? "true" : null,
        remoteType: this.extension.remoteType,
        initialBrowsingContextGroupId: this.extension.browsingContextGroupId,
      });
    } catch (e) {
      SharedWindow.release();
      throw e;
    }

    if (this.unloaded) {
      this._releaseBrowser();
      throw new Error("Extension shut down before browser element was created");
    }

    return this.browser;
  }
}

/**
 * This object provides utility functions needed by the devtools actors to
 * be able to connect and debug an extension (which can run in the main or in
 * a child extension process).
 */
const DebugUtils = {
  // A lazily created hidden XUL window, which contains the browser elements
  // which are used to connect the webextension patent actor to the extension process.
  hiddenXULWindow: null,

  // Map<extensionId, Promise<XULElement>>
  debugBrowserPromises: new Map(),
  // DefaultWeakMap<Promise<browser XULElement>, Set<WebExtensionParentActor>>
  debugActors: new DefaultWeakMap(() => new Set()),

  _extensionUpdatedWatcher: null,
  watchExtensionUpdated() {
    if (!this._extensionUpdatedWatcher) {
      // Watch the updated extension objects.
      this._extensionUpdatedWatcher = async (evt, extension) => {
        const browserPromise = this.debugBrowserPromises.get(extension.id);
        if (browserPromise) {
          const browser = await browserPromise;
          if (
            browser.isRemoteBrowser !== extension.remote &&
            this.debugBrowserPromises.get(extension.id) === browserPromise
          ) {
            // If the cached browser element is not anymore of the same
            // remote type of the extension, remove it.
            this.debugBrowserPromises.delete(extension.id);
            browser.remove();
          }
        }
      };

      apiManager.on("ready", this._extensionUpdatedWatcher);
    }
  },

  unwatchExtensionUpdated() {
    if (this._extensionUpdatedWatcher) {
      apiManager.off("ready", this._extensionUpdatedWatcher);
      delete this._extensionUpdatedWatcher;
    }
  },

  getExtensionManifestWarnings(id) {
    const addon = GlobalManager.extensionMap.get(id);
    if (addon) {
      return addon.warnings;
    }
    return [];
  },

  /**
   * Retrieve a XUL browser element which has been configured to be able to connect
   * the devtools actor with the process where the extension is running.
   *
   * @param {WebExtensionParentActor} webExtensionParentActor
   *        The devtools actor that is retrieving the browser element.
   *
   * @returns {Promise<XULElement>}
   *          A promise which resolves to the configured browser XUL element.
   */
  async getExtensionProcessBrowser(webExtensionParentActor) {
    const extensionId = webExtensionParentActor.addonId;
    const extension = GlobalManager.getExtension(extensionId);
    if (!extension) {
      throw new Error(`Extension not found: ${extensionId}`);
    }

    const createBrowser = () => {
      if (!this.hiddenXULWindow) {
        this.hiddenXULWindow = new HiddenXULWindow();
        this.watchExtensionUpdated();
      }

      return this.hiddenXULWindow.createBrowserElement({
        "webextension-addon-debug-target": extensionId,
        remote: extension.remote ? "true" : null,
        remoteType: extension.remoteType,
        initialBrowsingContextGroupId: extension.browsingContextGroupId,
      });
    };

    let browserPromise = this.debugBrowserPromises.get(extensionId);

    // Create a new promise if there is no cached one in the map.
    if (!browserPromise) {
      browserPromise = createBrowser();
      this.debugBrowserPromises.set(extensionId, browserPromise);
      browserPromise.then(browser => {
        browserPromise.browser = browser;
      });
      browserPromise.catch(e => {
        Cu.reportError(e);
        this.debugBrowserPromises.delete(extensionId);
      });
    }

    this.debugActors.get(browserPromise).add(webExtensionParentActor);

    return browserPromise;
  },

  getFrameLoader(extensionId) {
    let promise = this.debugBrowserPromises.get(extensionId);
    return promise && promise.browser && promise.browser.frameLoader;
  },

  /**
   * Given the devtools actor that has retrieved an addon debug browser element,
   * it destroys the XUL browser element, and it also destroy the hidden XUL window
   * if it is not currently needed.
   *
   * @param {WebExtensionParentActor} webExtensionParentActor
   *        The devtools actor that has retrieved an addon debug browser element.
   */
  async releaseExtensionProcessBrowser(webExtensionParentActor) {
    const extensionId = webExtensionParentActor.addonId;
    const browserPromise = this.debugBrowserPromises.get(extensionId);

    if (browserPromise) {
      const actorsSet = this.debugActors.get(browserPromise);
      actorsSet.delete(webExtensionParentActor);
      if (actorsSet.size === 0) {
        this.debugActors.delete(browserPromise);
        this.debugBrowserPromises.delete(extensionId);
        await browserPromise.then(browser => browser.remove());
      }
    }

    if (this.debugBrowserPromises.size === 0 && this.hiddenXULWindow) {
      this.hiddenXULWindow.shutdown();
      this.hiddenXULWindow = null;
      this.unwatchExtensionUpdated();
    }
  },
};

/**
 * Returns a Promise which resolves with the message data when the given message
 * was received by the message manager. The promise is rejected if the message
 * manager was closed before a message was received.
 *
 * @param {MessageListenerManager} messageManager
 *        The message manager on which to listen for messages.
 * @param {string} messageName
 *        The message to listen for.
 * @returns {Promise<*>}
 */
function promiseMessageFromChild(messageManager, messageName) {
  return new Promise((resolve, reject) => {
    let unregister;
    function listener(message) {
      unregister();
      resolve(message.data);
    }
    function observer(subject, topic, data) {
      if (subject === messageManager) {
        unregister();
        reject(
          new Error(
            `Message manager was disconnected before receiving ${messageName}`
          )
        );
      }
    }
    unregister = () => {
      Services.obs.removeObserver(observer, "message-manager-close");
      messageManager.removeMessageListener(messageName, listener);
    };
    messageManager.addMessageListener(messageName, listener);
    Services.obs.addObserver(observer, "message-manager-close");
  });
}

// This should be called before browser.loadURI is invoked.
async function promiseExtensionViewLoaded(browser) {
  let { childId } = await promiseMessageFromChild(
    browser.messageManager,
    "Extension:ExtensionViewLoaded"
  );
  if (childId) {
    return ParentAPIManager.getContextById(childId);
  }
}

/**
 * This helper is used to subscribe a listener (e.g. in the ext-devtools API implementation)
 * to be called for every ExtensionProxyContext created for an extension page given
 * its related extension, viewType and browser element (both the top level context and any context
 * created for the extension urls running into its iframe descendants).
 *
 * @param {object} params.extension
 *        The Extension on which we are going to listen for the newly created ExtensionProxyContext.
 * @param {string} params.viewType
 *        The viewType of the WebExtension page that we are watching (e.g. "background" or
 *        "devtools_page").
 * @param {XULElement} params.browser
 *        The browser element of the WebExtension page that we are watching.
 * @param {function} onExtensionProxyContextLoaded
 *        The callback that is called when a new context has been loaded (as `callback(context)`);
 *
 * @returns {function}
 *          Unsubscribe the listener.
 */
function watchExtensionProxyContextLoad(
  { extension, viewType, browser },
  onExtensionProxyContextLoaded
) {
  if (typeof onExtensionProxyContextLoaded !== "function") {
    throw new Error("Missing onExtensionProxyContextLoaded handler");
  }

  const listener = (event, context) => {
    if (context.viewType == viewType && context.xulBrowser == browser) {
      onExtensionProxyContextLoaded(context);
    }
  };

  extension.on("extension-proxy-context-load", listener);

  return () => {
    extension.off("extension-proxy-context-load", listener);
  };
}

// Manages icon details for toolbar buttons in the |pageAction| and
// |browserAction| APIs.
let IconDetails = {
  DEFAULT_ICON: "chrome://mozapps/skin/extensions/extensionGeneric.svg",

  // WeakMap<Extension -> Map<url-string -> Map<iconType-string -> object>>>
  iconCache: new DefaultWeakMap(() => {
    return new DefaultMap(() => new DefaultMap(() => new Map()));
  }),

  // Normalizes the various acceptable input formats into an object
  // with icon size as key and icon URL as value.
  //
  // If a context is specified (function is called from an extension):
  // Throws an error if an invalid icon size was provided or the
  // extension is not allowed to load the specified resources.
  //
  // If no context is specified, instead of throwing an error, this
  // function simply logs a warning message.
  normalize(details, extension, context = null) {
    if (!details.imageData && details.path != null) {
      // Pick a cache key for the icon paths. If the path is a string,
      // use it directly. Otherwise, stringify the path object.
      let key = details.path;
      if (typeof key !== "string") {
        key = uneval(key);
      }

      let icons = this.iconCache
        .get(extension)
        .get(context && context.uri.spec)
        .get(details.iconType);

      let icon = icons.get(key);
      if (!icon) {
        icon = this._normalize(details, extension, context);
        icons.set(key, icon);
      }
      return icon;
    }

    return this._normalize(details, extension, context);
  },

  _normalize(details, extension, context = null) {
    let result = {};

    try {
      let { imageData, path, themeIcons } = details;

      if (imageData) {
        if (typeof imageData == "string") {
          imageData = { "19": imageData };
        }

        for (let size of Object.keys(imageData)) {
          result[size] = imageData[size];
        }
      }

      let baseURI = context ? context.uri : extension.baseURI;

      if (path != null) {
        if (typeof path != "object") {
          path = { "19": path };
        }

        for (let size of Object.keys(path)) {
          let url = path[size];
          if (url) {
            url = baseURI.resolve(path[size]);

            // The Chrome documentation specifies these parameters as
            // relative paths. We currently accept absolute URLs as well,
            // which means we need to check that the extension is allowed
            // to load them. This will throw an error if it's not allowed.
            this._checkURL(url, extension);
          }
          result[size] = url || this.DEFAULT_ICON;
        }
      }

      if (themeIcons) {
        themeIcons.forEach(({ size, light, dark }) => {
          let lightURL = baseURI.resolve(light);
          let darkURL = baseURI.resolve(dark);

          this._checkURL(lightURL, extension);
          this._checkURL(darkURL, extension);

          let defaultURL = result[size] || result[19]; // always fallback to default first
          result[size] = {
            default: defaultURL || darkURL, // Fallback to the dark url if no default is specified.
            light: lightURL,
            dark: darkURL,
          };
        });
      }
    } catch (e) {
      // Function is called from extension code, delegate error.
      if (context) {
        throw e;
      }
      // If there's no context, it's because we're handling this
      // as a manifest directive. Log a warning rather than
      // raising an error.
      extension.manifestError(`Invalid icon data: ${e}`);
    }

    return result;
  },

  // Checks if the extension is allowed to load the given URL with the specified principal.
  // This will throw an error if the URL is not allowed.
  _checkURL(url, extension) {
    if (!extension.checkLoadURL(url, { allowInheritsPrincipal: true })) {
      throw new ExtensionError(`Illegal URL ${url}`);
    }
  },

  // Returns the appropriate icon URL for the given icons object and the
  // screen resolution of the given window.
  getPreferredIcon(icons, extension = null, size = 16) {
    const DEFAULT = "chrome://mozapps/skin/extensions/extensionGeneric.svg";

    let bestSize = null;
    if (icons[size]) {
      bestSize = size;
    } else if (icons[2 * size]) {
      bestSize = 2 * size;
    } else {
      let sizes = Object.keys(icons)
        .map(key => parseInt(key, 10))
        .sort((a, b) => a - b);

      bestSize = sizes.find(candidate => candidate > size) || sizes.pop();
    }

    if (bestSize) {
      return { size: bestSize, icon: icons[bestSize] || DEFAULT };
    }

    return { size, icon: DEFAULT };
  },

  // These URLs should already be properly escaped, but make doubly sure CSS
  // string escape characters are escaped here, since they could lead to a
  // sandbox break.
  escapeUrl(url) {
    return url.replace(/[\\\s"]/g, encodeURIComponent);
  },
};

// A cache to support faster initialization of extensions at browser startup.
// All cached data is removed when the browser is updated.
// Extension-specific data is removed when the add-on is updated.
StartupCache = {
  STORE_NAMES: Object.freeze([
    "general",
    "locales",
    "manifests",
    "other",
    "permissions",
    "schemas",
  ]),

  _ensureDirectoryPromise: null,
  _saveTask: null,

  _ensureDirectory() {
    if (this._ensureDirectoryPromise === null) {
      this._ensureDirectoryPromise = IOUtils.makeDirectory(
        PathUtils.parent(this.file),
        {
          ignoreExisting: true,
          createAncestors: true,
        }
      );
    }

    return this._ensureDirectoryPromise;
  },

  // When the application version changes, this file is removed by
  // RemoveComponentRegistries in nsAppRunner.cpp.
  file: PathUtils.join(
    Services.dirsvc.get("ProfLD", Ci.nsIFile).path,
    "startupCache",
    "webext.sc.lz4"
  ),

  async _saveNow() {
    let data = new Uint8Array(aomStartup.encodeBlob(this._data));
    await this._ensureDirectoryPromise;
    await IOUtils.write(this.file, data, { tmpPath: `${this.file}.tmp` });
  },

  save() {
    this._ensureDirectory();

    if (!this._saveTask) {
      this._saveTask = new DeferredTask(() => this._saveNow(), 5000);

      IOUtils.profileBeforeChange.addBlocker(
        "Flush WebExtension StartupCache",
        async () => {
          await this._saveTask.finalize();
          this._saveTask = null;
        }
      );
    }

    return this._saveTask.arm();
  },

  _data: null,
  async _readData() {
    let result = new Map();
    try {
      let { buffer } = await IOUtils.read(this.file);

      result = aomStartup.decodeBlob(buffer);
    } catch (e) {
      if (!(e instanceof DOMException) || e.name !== "NotFoundError") {
        Cu.reportError(e);
      }
    }

    this._data = result;
    return result;
  },

  get dataPromise() {
    if (!this._dataPromise) {
      this._dataPromise = this._readData();
    }
    return this._dataPromise;
  },

  clearAddonData(id) {
    return Promise.all([
      this.general.delete(id),
      this.locales.delete(id),
      this.manifests.delete(id),
      this.permissions.delete(id),
    ]).catch(e => {
      // Ignore the error. It happens when we try to flush the add-on
      // data after the AddonManager has flushed the entire startup cache.
    });
  },

  observe(subject, topic, data) {
    if (topic === "startupcache-invalidate") {
      this._data = new Map();
      this._dataPromise = Promise.resolve(this._data);
    }
  },

  get(extension, path, createFunc) {
    return this.general.get(
      [extension.id, extension.version, ...path],
      createFunc
    );
  },

  delete(extension, path) {
    return this.general.delete([extension.id, extension.version, ...path]);
  },
};

Services.obs.addObserver(StartupCache, "startupcache-invalidate");

class CacheStore {
  constructor(storeName) {
    this.storeName = storeName;
  }

  async getStore(path = null) {
    let data = await StartupCache.dataPromise;

    let store = data.get(this.storeName);
    if (!store) {
      store = new Map();
      data.set(this.storeName, store);
    }

    let key = path;
    if (Array.isArray(path)) {
      for (let elem of path.slice(0, -1)) {
        let next = store.get(elem);
        if (!next) {
          next = new Map();
          store.set(elem, next);
        }
        store = next;
      }
      key = path[path.length - 1];
    }

    return [store, key];
  }

  async get(path, createFunc) {
    let [store, key] = await this.getStore(path);

    let result = store.get(key);

    if (result === undefined) {
      result = await createFunc(path);
      store.set(key, result);
      StartupCache.save();
    }

    return result;
  }

  async set(path, value) {
    let [store, key] = await this.getStore(path);

    store.set(key, value);
    StartupCache.save();
  }

  async getAll() {
    let [store] = await this.getStore();

    return new Map(store);
  }

  async delete(path) {
    let [store, key] = await this.getStore(path);

    if (store.delete(key)) {
      StartupCache.save();
    }
  }
}

for (let name of StartupCache.STORE_NAMES) {
  StartupCache[name] = new CacheStore(name);
}

var ExtensionParent = {
  GlobalManager,
  HiddenExtensionPage,
  IconDetails,
  ParentAPIManager,
  StartupCache,
  WebExtensionPolicy,
  apiManager,
  promiseExtensionViewLoaded,
  watchExtensionProxyContextLoad,
  DebugUtils,
};

// browserPaintedPromise and browserStartupPromise are promises that
// resolve after the first browser window is painted and after browser
// windows have been restored, respectively. Alternatively,
// browserStartupPromise also resolves from the extensions-late-startup
// notification sent by Firefox Reality on desktop platforms, because it
// doesn't support SessionStore.
// _resetStartupPromises should only be called from outside this file in tests.
ExtensionParent._resetStartupPromises = () => {
  ExtensionParent.browserPaintedPromise = promiseObserved(
    "browser-delayed-startup-finished"
  ).then(() => {});
  ExtensionParent.browserStartupPromise = Promise.race([
    promiseObserved("sessionstore-windows-restored"),
    promiseObserved("extensions-late-startup"),
  ]).then(() => {});
};
ExtensionParent._resetStartupPromises();

XPCOMUtils.defineLazyGetter(ExtensionParent, "PlatformInfo", () => {
  return Object.freeze({
    os: (function() {
      let os = AppConstants.platform;
      if (os == "macosx") {
        os = "mac";
      }
      return os;
    })(),
    arch: (function() {
      let abi = Services.appinfo.XPCOMABI;
      let [arch] = abi.split("-");
      if (arch == "x86") {
        arch = "x86-32";
      } else if (arch == "x86_64") {
        arch = "x86-64";
      }
      return arch;
    })(),
  });
});

/**
 * Retreives the browser_style stylesheets needed for extension popups and sidebars.
 * @returns {Array<string>} an array of stylesheets needed for the current platform.
 */
XPCOMUtils.defineLazyGetter(ExtensionParent, "extensionStylesheets", () => {
  let stylesheets = ["chrome://browser/content/extension.css"];

  if (AppConstants.platform === "macosx") {
    stylesheets.push("chrome://browser/content/extension-mac.css");
  }

  return stylesheets;
});
