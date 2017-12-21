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

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

/* exported ExtensionParent */

this.EXPORTED_SYMBOLS = ["ExtensionParent"];

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetters(this, {
  AppConstants: "resource://gre/modules/AppConstants.jsm",
  AsyncShutdown: "resource://gre/modules/AsyncShutdown.jsm",
  DeferredTask: "resource://gre/modules/DeferredTask.jsm",
  E10SUtils: "resource://gre/modules/E10SUtils.jsm",
  ExtensionData: "resource://gre/modules/Extension.jsm",
  MessageChannel: "resource://gre/modules/MessageChannel.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  NativeApp: "resource://gre/modules/NativeMessaging.jsm",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.jsm",
  Schemas: "resource://gre/modules/Schemas.jsm",
});

XPCOMUtils.defineLazyServiceGetters(this, {
  aomStartup: ["@mozilla.org/addons/addon-manager-startup;1", "amIAddonManagerStartup"],
});

Cu.import("resource://gre/modules/ExtensionCommon.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  BaseContext,
  CanOfAPIs,
  SchemaAPIManager,
  SpreadArgs,
} = ExtensionCommon;

var {
  DefaultMap,
  DefaultWeakMap,
  ExtensionError,
  MessageManagerProxy,
  defineLazyGetter,
  promiseDocumentLoaded,
  promiseEvent,
  promiseObserved,
} = ExtensionUtils;

const BASE_SCHEMA = "chrome://extensions/content/schemas/manifest.json";
const CATEGORY_EXTENSION_MODULES = "webextension-modules";
const CATEGORY_EXTENSION_SCHEMAS = "webextension-schemas";
const CATEGORY_EXTENSION_SCRIPTS = "webextension-scripts";

let schemaURLs = new Set();

schemaURLs.add("chrome://extensions/content/schemas/experiments.json");

let GlobalManager;
let ParentAPIManager;
let ProxyMessenger;
let StartupCache;

const global = this;

// This object loads the ext-*.js scripts that define the extension API.
let apiManager = new class extends SchemaAPIManager {
  constructor() {
    super("main");
    this.initialized = null;

    /* eslint-disable mozilla/balanced-listeners */
    this.on("startup", (e, extension) => {
      let promises = [];
      for (let apiName of this.eventModules.get("startup")) {
        promises.push(this.asyncGetAPI(apiName, extension).then(api => {
          if (api) {
            api.onStartup();
          }
        }));
      }

      return Promise.all(promises);
    });

    this.on("update", async (e, {id, resourceURI}) => {
      let modules = this.eventModules.get("update");
      if (modules.size == 0) {
        return;
      }

      let extension = new ExtensionData(resourceURI);
      await extension.loadManifest();

      return Promise.all(Array.from(modules).map(async apiName => {
        let module = await this.asyncLoadModule(apiName);
        module.onUpdate(id, extension.manifest);
      }));
    });

    this.on("uninstall", (e, {id}) => {
      let modules = this.eventModules.get("uninstall");
      return Promise.all(Array.from(modules).map(async apiName => {
        let module = await this.asyncLoadModule(apiName);
        module.onUninstall(id);
      }));
    });
    /* eslint-enable mozilla/balanced-listeners */
  }

  getModuleJSONURLs() {
    return Array.from(XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_MODULES),
                      ([name, url]) => url);
  }

  // Loads all the ext-*.js scripts currently registered.
  lazyInit() {
    if (this.initialized) {
      return this.initialized;
    }

    let modulesPromise = StartupCache.other.get(
      ["parentModules"],
      () => this.loadModuleJSON(this.getModuleJSONURLs()));

    let scriptURLs = [];
    for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS)) {
      scriptURLs.push(value);
    }

    let promise = (async () => {
      let scripts = await Promise.all(scriptURLs.map(url => ChromeUtils.compileScript(url)));

      this.initModuleData(await modulesPromise);

      for (let script of scripts) {
        script.executeInGlobal(this.global);
      }

      // Load order matters here. The base manifest defines types which are
      // extended by other schemas, so needs to be loaded first.
      return Schemas.load(BASE_SCHEMA, AppConstants.DEBUG).then(() => {
        let promises = [];
        for (let [/* name */, url] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCHEMAS)) {
          promises.push(Schemas.load(url));
        }
        for (let [url, {content}] of this.schemaURLs) {
          promises.push(Schemas.load(url, content));
        }
        for (let url of schemaURLs) {
          promises.push(Schemas.load(url));
        }
        return Promise.all(promises);
      });
    })();

    /* eslint-disable mozilla/balanced-listeners */
    Services.mm.addMessageListener("Extension:GetTabAndWindowId", this);
    /* eslint-enable mozilla/balanced-listeners */

    this.initialized = promise;
    return this.initialized;
  }

  receiveMessage({name, target, sync}) {
    if (name === "Extension:GetTabAndWindowId") {
      let result = this.global.tabTracker.getBrowserData(target);

      if (result.tabId) {
        if (sync) {
          return result;
        }
        target.messageManager.sendAsyncMessage("Extension:SetFrameData", result);
      }
    }
  }
}();

// Subscribes to messages related to the extension messaging API and forwards it
// to the relevant message manager. The "sender" field for the `onMessage` and
// `onConnect` events are updated if needed.
ProxyMessenger = {
  _initialized: false,

  init() {
    if (this._initialized) {
      return;
    }
    this._initialized = true;

    // Listen on the global frame message manager because content scripts send
    // and receive extension messages via their frame.
    // Listen on the parent process message manager because `runtime.connect`
    // and `runtime.sendMessage` requests must be delivered to all frames in an
    // addon process (by the API contract).
    // And legacy addons are not associated with a frame, so that is another
    // reason for having a parent process manager here.
    let messageManagers = [Services.mm, Services.ppmm];

    MessageChannel.addListener(messageManagers, "Extension:Connect", this);
    MessageChannel.addListener(messageManagers, "Extension:Message", this);
    MessageChannel.addListener(messageManagers, "Extension:Port:Disconnect", this);
    MessageChannel.addListener(messageManagers, "Extension:Port:PostMessage", this);
  },

  async receiveMessage({target, messageName, channelId, sender, recipient, data, responseType}) {
    if (recipient.toNativeApp) {
      let {childId, toNativeApp} = recipient;
      if (messageName == "Extension:Message") {
        let context = ParentAPIManager.getContextById(childId);
        return new NativeApp(context, toNativeApp).sendMessage(data);
      }
      if (messageName == "Extension:Connect") {
        let context = ParentAPIManager.getContextById(childId);
        NativeApp.onConnectNative(context, target.messageManager, data.portId, sender, toNativeApp);
        return true;
      }
      // "Extension:Port:Disconnect" and "Extension:Port:PostMessage" for
      // native messages are handled by NativeApp.
      return;
    }

    const noHandlerError = {
      result: MessageChannel.RESULT_NO_HANDLER,
      message: "No matching message handler for the given recipient.",
    };

    let extension = GlobalManager.extensionMap.get(sender.extensionId);
    let receiverMM = this.getMessageManagerForRecipient(recipient);
    if (!extension || !receiverMM) {
      return Promise.reject(noHandlerError);
    }

    if ((messageName == "Extension:Message" ||
         messageName == "Extension:Connect") &&
        apiManager.global.tabGetSender) {
      // From ext-tabs.js, undefined on Android.
      apiManager.global.tabGetSender(extension, target, sender);
    }

    let promise1 = MessageChannel.sendMessage(receiverMM, messageName, data, {
      sender,
      recipient,
      responseType,
    });

    if (!(extension.isEmbedded || recipient.toProxyScript) || !extension.remote) {
      return promise1;
    }

    // If we have a proxy script sandbox or a remote, embedded extension, where
    // the legacy side is running in a different process than the WebExtension
    // side. As a result, we need to dispatch the message to both the parent and
    // extension processes, and manually merge the results.
    let promise2 = MessageChannel.sendMessage(Services.ppmm.getChildAt(0), messageName, data, {
      sender,
      recipient,
      responseType,
    });

    let result = undefined;
    let failures = 0;
    let tryPromise = async promise => {
      try {
        let res = await promise;
        if (result === undefined) {
          result = res;
        }
      } catch (e) {
        if (e.result === MessageChannel.RESULT_NO_RESPONSE) {
          // Ignore.
        } else if (e.result === MessageChannel.RESULT_NO_HANDLER) {
          failures++;
        } else {
          throw e;
        }
      }
    };

    await Promise.all([tryPromise(promise1), tryPromise(promise2)]);
    if (failures == 2) {
      return Promise.reject(noHandlerError);
    }
    return result;
  },

  /**
   * @param {object} recipient An object that was passed to
   *     `MessageChannel.sendMessage`.
   * @param {Extension} extension
   * @returns {object|null} The message manager matching the recipient if found.
   */
  getMessageManagerForRecipient(recipient) {
    // tabs.sendMessage / tabs.connect
    if ("tabId" in recipient) {
      // `tabId` being set implies that the tabs API is supported, so we don't
      // need to check whether `tabTracker` exists.
      let tab = apiManager.global.tabTracker.getTab(recipient.tabId, null);
      if (!tab) {
        return null;
      }

      // There can be no recipients in a tab pending restore,
      // So we bail early to avoid instantiating the lazy browser.
      let node = tab.browser || tab;
      if (node.getAttribute("pending") === "true") {
        return null;
      }

      let browser = tab.linkedBrowser || tab.browser;

      // Options panels in the add-on manager currently require
      // special-casing, since their message managers aren't currently
      // connected to the tab's top-level message manager. To deal with
      // this, we find the options <browser> for the tab, and use that
      // directly, insteead.
      if (browser.currentURI.cloneIgnoringRef().spec === "about:addons") {
        let optionsBrowser = browser.contentDocument.querySelector(".inline-options-browser");
        if (optionsBrowser) {
          browser = optionsBrowser;
        }
      }

      return browser.messageManager;
    }

    // runtime.sendMessage / runtime.connect
    let extension = GlobalManager.extensionMap.get(recipient.extensionId);
    if (extension) {
      return extension.parentMessageManager;
    }

    return null;
  },
};

// Responsible for loading extension APIs into the right globals.
GlobalManager = {
  // Map[extension ID -> Extension]. Determines which extension is
  // responsible for content under a particular extension ID.
  extensionMap: new Map(),
  initialized: false,

  init(extension) {
    if (this.extensionMap.size == 0) {
      ProxyMessenger.init();
      apiManager.on("extension-browser-inserted", this._onExtensionBrowser);
      this.initialized = true;
    }

    this.extensionMap.set(extension.id, extension);
  },

  uninit(extension) {
    this.extensionMap.delete(extension.id);

    if (this.extensionMap.size == 0 && this.initialized) {
      apiManager.off("extension-browser-inserted", this._onExtensionBrowser);
      this.initialized = false;
    }
  },

  _onExtensionBrowser(type, browser, additionalData = {}) {
    browser.messageManager.loadFrameScript(`data:,
      Components.utils.import("resource://gre/modules/Services.jsm");

      Services.obs.notifyObservers(this, "tab-content-frameloader-created", "");
    `, false);

    let viewType = browser.getAttribute("webextension-view-type");
    if (viewType) {
      let data = {viewType};

      let {tabTracker} = apiManager.global;
      Object.assign(data, tabTracker.getBrowserData(browser), additionalData);

      browser.messageManager.sendAsyncMessage("Extension:SetFrameData",
                                              data);
    }
  },

  getExtension(extensionId) {
    return this.extensionMap.get(extensionId);
  },

  injectInObject(context, isChromeCompat, dest) {
    SchemaAPIManager.generateAPIs(context, context.extension.apis, dest);
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
      value: principal, enumerable: true, configurable: true,
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
  let can = new CanOfAPIs(this, apiManager, obj);
  GlobalManager.injectInObject(this, false, obj);
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
class ContentScriptContextParent extends ProxyContextParent {
}

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
  get xulWindow() {
    let win = this.xulBrowser.ownerGlobal;
    return win.document.docShell.rootTreeItem
              .QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindow);
  }

  get currentWindow() {
    if (this.viewType !== "background") {
      return this.xulWindow;
    }
  }

  get windowId() {
    let {currentWindow} = this;
    let {windowTracker} = apiManager.global;

    if (currentWindow && windowTracker) {
      return windowTracker.getId(currentWindow);
    }
  }

  get tabId() {
    let {tabTracker} = apiManager.global;
    let data = tabTracker.getBrowserData(this.xulBrowser);
    if (data.tabId >= 0) {
      return data.tabId;
    }
  }

  onBrowserChange(browser) {
    super.onBrowserChange(browser);
    this.xulBrowser = browser;
  }

  shutdown() {
    apiManager.emit("page-shutdown", this);
    this.extension.views.delete(this);
    super.shutdown();
  }
}

/**
 * The parent side of proxied API context for devtools extension page, such as a
 * devtools pages and panels running in ExtensionChild.jsm.
 */
class DevToolsExtensionPageContextParent extends ExtensionPageContextParent {
  set devToolsToolbox(toolbox) {
    if (this._devToolsToolbox) {
      throw new Error("Cannot set the context DevTools toolbox twice");
    }

    this._devToolsToolbox = toolbox;

    return toolbox;
  }

  get devToolsToolbox() {
    return this._devToolsToolbox;
  }

  set devToolsTarget(contextDevToolsTarget) {
    if (this._devToolsTarget) {
      throw new Error("Cannot set the context DevTools target twice");
    }

    this._devToolsTarget = contextDevToolsTarget;

    return contextDevToolsTarget;
  }

  get devToolsTarget() {
    return this._devToolsTarget;
  }

  shutdown() {
    if (this._devToolsTarget) {
      this._devToolsTarget.destroy();
      this._devToolsTarget = null;
    }

    this._devToolsToolbox = null;

    super.shutdown();
  }
}

ParentAPIManager = {
  proxyContexts: new Map(),

  parentMessageManagers: new Set(),

  init() {
    Services.obs.addObserver(this, "message-manager-close");
    Services.obs.addObserver(this, "ipc:content-created");

    Services.mm.addMessageListener("API:CreateProxyContext", this);
    Services.mm.addMessageListener("API:CloseProxyContext", this, true);
    Services.mm.addMessageListener("API:Call", this);
    Services.mm.addMessageListener("API:AddListener", this);
    Services.mm.addMessageListener("API:RemoveListener", this);

    this.schemaHook = this.schemaHook.bind(this);
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

      this.parentMessageManagers.delete(mm);
    } else if (topic === "ipc:content-created") {
      let mm = subject.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIMessageSender);
      if (mm.remoteType === E10SUtils.EXTENSION_REMOTE_TYPE) {
        this.parentMessageManagers.add(mm);
        mm.sendAsyncMessage("Schema:Add", Schemas.schemaJSON);

        Schemas.schemaHook = this.schemaHook;
      }
    }
  },

  schemaHook(schemas) {
    for (let mm of this.parentMessageManagers) {
      mm.sendAsyncMessage("Schema:Add", schemas);
    }
  },

  shutdownExtension(extensionId) {
    for (let [childId, context] of this.proxyContexts) {
      if (context.extension.id == extensionId) {
        context.shutdown();
        this.proxyContexts.delete(childId);
      }
    }
  },

  receiveMessage({name, data, target}) {
    try {
      switch (name) {
        case "API:CreateProxyContext":
          this.createProxyContext(data, target);
          break;

        case "API:CloseProxyContext":
          this.closeProxyContext(data.childId);
          break;

        case "API:Call":
          this.call(data, target);
          break;

        case "API:AddListener":
          this.addListener(data, target);
          break;

        case "API:RemoveListener":
          this.removeListener(data);
          break;
      }
    } catch (e) {
      Cu.reportError(e);
    }
  },

  createProxyContext(data, target) {
    let {envType, extensionId, childId, principal} = data;
    if (this.proxyContexts.has(childId)) {
      throw new Error("A WebExtension context with the given ID already exists!");
    }

    let extension = GlobalManager.getExtension(extensionId);
    if (!extension) {
      throw new Error(`No WebExtension found with ID ${extensionId}`);
    }

    let context;
    if (envType == "addon_parent" || envType == "devtools_parent") {
      let processMessageManager = (target.messageManager.processMessageManager ||
                                   Services.ppmm.getChildAt(0));

      if (!extension.parentMessageManager) {
        let expectedRemoteType = extension.remote ? E10SUtils.EXTENSION_REMOTE_TYPE : null;
        if (target.remoteType === expectedRemoteType) {
          this.attachMessageManager(extension, processMessageManager);
        }
      }

      if (processMessageManager !== extension.parentMessageManager) {
        throw new Error("Attempt to create privileged extension parent from incorrect child process");
      }

      if (envType == "addon_parent") {
        context = new ExtensionPageContextParent(envType, extension, data, target);
      } else if (envType == "devtools_parent") {
        context = new DevToolsExtensionPageContextParent(envType, extension, data, target);
      }
    } else if (envType == "content_parent") {
      context = new ContentScriptContextParent(envType, extension, data, target, principal);
    } else {
      throw new Error(`Invalid WebExtension context envType: ${envType}`);
    }
    this.proxyContexts.set(childId, context);
  },

  closeProxyContext(childId) {
    let context = this.proxyContexts.get(childId);
    if (context) {
      context.unload();
      this.proxyContexts.delete(childId);
    }
  },

  async call(data, target) {
    let context = this.getContextById(data.childId);
    if (context.parentMessageManager !== target.messageManager) {
      throw new Error("Got message on unexpected message manager");
    }

    let reply = result => {
      if (!context.parentMessageManager) {
        Services.console.logStringMessage("Cannot send function call result: other side closed connection " +
                                          `(call data: ${uneval({path: data.path, args: data.args})})`);
        return;
      }

      context.parentMessageManager.sendAsyncMessage(
        "API:CallResult",
        Object.assign({
          childId: data.childId,
          callId: data.callId,
        }, result));
    };

    try {
      let args = data.args;
      let pendingBrowser = context.pendingEventBrowser;
      let fun = await context.apiCan.asyncFindAPIPath(data.path);
      let result = context.withPendingBrowser(pendingBrowser,
                                              () => fun(...args));
      if (data.callId) {
        result = result || Promise.resolve();

        result.then(result => {
          result = result instanceof SpreadArgs ? [...result] : [result];

          let holder = new StructuredCloneHolder(result);

          reply({result: holder});
        }, error => {
          error = context.normalizeError(error);
          reply({error: {message: error.message, fileName: error.fileName}});
        });
      }
    } catch (e) {
      if (data.callId) {
        let error = context.normalizeError(e);
        reply({error: {message: error.message}});
      } else {
        Cu.reportError(e);
      }
    }
  },

  async addListener(data, target) {
    let context = this.getContextById(data.childId);
    if (context.parentMessageManager !== target.messageManager) {
      throw new Error("Got message on unexpected message manager");
    }

    let {childId} = data;
    let handlingUserInput = false;
    let lowPriority = data.path.startsWith("webRequest.");

    function listener(...listenerArgs) {
      return context.sendMessage(
        context.parentMessageManager,
        "API:RunListener",
        {
          childId,
          handlingUserInput,
          listenerId: data.listenerId,
          path: data.path,
          get args() {
            return new StructuredCloneHolder(listenerArgs);
          },
        },
        {
          lowPriority,
          recipient: {childId},
        })
        .then(result => {
          return result && result.deserialize(global);
        });
    }

    context.listenerProxies.set(data.listenerId, listener);

    let args = data.args;
    let promise = context.apiCan.asyncFindAPIPath(data.path);

    // Store pending listener additions so we can be sure they're all
    // fully initialize before we consider extension startup complete.
    if (context.viewType === "background" && context.listenerPromises) {
      const {listenerPromises} = context;
      listenerPromises.add(promise);
      let remove = () => { listenerPromises.delete(promise); };
      promise.then(remove, remove);
    }

    let handler = await promise;
    if (handler.setUserInput) {
      handlingUserInput = true;
    }
    handler.addListener(listener, ...args);
  },

  async removeListener(data) {
    let context = this.getContextById(data.childId);
    let listener = context.listenerProxies.get(data.listenerId);

    let handler = await context.apiCan.asyncFindAPIPath(data.path);
    handler.removeListener(listener);
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
 * This utility class is used to create hidden XUL windows, which are used to
 * contains the extension pages that are not visible (e.g. the background page and
 * the devtools page), and it is also used by the ExtensionDebuggingUtils to
 * contains the browser elements that are used by the addon debugger to be able
 * to connect to the devtools actors running in the same process of the target
 * extension (and be able to stay connected across the addon reloads).
 */
class HiddenXULWindow {
  constructor() {
    this._windowlessBrowser = null;
    this.waitInitialized = this.initWindowlessBrowser();
  }

  shutdown() {
    if (this.unloaded) {
      throw new Error("Unable to shutdown an unloaded HiddenXULWindow instance");
    }

    this.unloaded = true;

    this.chromeShell = null;
    this.waitInitialized = null;

    this._windowlessBrowser.close();
    this._windowlessBrowser = null;
  }

  get chromeDocument() {
    return this._windowlessBrowser.document;
  }

  /**
   * Private helper that create a XULDocument in a windowless browser.
   *
   * @returns {Promise<XULDocument>}
   *          A promise which resolves to the newly created XULDocument.
   */
  async initWindowlessBrowser() {
    if (this.waitInitialized) {
      throw new Error("HiddenXULWindow already initialized");
    }

    // The invisible page is currently wrapped in a XUL window to fix an issue
    // with using the canvas API from a background page (See Bug 1274775).
    let windowlessBrowser = Services.appShell.createWindowlessBrowser(true);
    this._windowlessBrowser = windowlessBrowser;

    // The windowless browser is a thin wrapper around a docShell that keeps
    // its related resources alive. It implements nsIWebNavigation and
    // forwards its methods to the underlying docShell, but cannot act as a
    // docShell itself. Calling `getInterface(nsIDocShell)` gives us the
    // underlying docShell, and `QueryInterface(nsIWebNavigation)` gives us
    // access to the webNav methods that are already available on the
    // windowless browser, but contrary to appearances, they are not the same
    // object.
    this.chromeShell = this._windowlessBrowser
                           .QueryInterface(Ci.nsIInterfaceRequestor)
                           .getInterface(Ci.nsIDocShell)
                           .QueryInterface(Ci.nsIWebNavigation);

    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      let attrs = this.chromeShell.getOriginAttributes();
      attrs.privateBrowsingId = 1;
      this.chromeShell.setOriginAttributes(attrs);
    }

    let system = Services.scriptSecurityManager.getSystemPrincipal();
    this.chromeShell.createAboutBlankContentViewer(system);
    this.chromeShell.useGlobalHistory = false;
    this.chromeShell.loadURI("chrome://extensions/content/dummy.xul", 0, null, null, null);

    await promiseObserved("chrome-document-global-created",
                          win => win.document == this.chromeShell.document);
    return promiseDocumentLoaded(windowlessBrowser.document);
  }

  /**
   * Creates the browser XUL element that will contain the WebExtension Page.
   *
   * @param {Object} xulAttributes
   *        An object that contains the xul attributes to set of the newly
   *        created browser XUL element.
   * @param {nsIFrameLoader} [groupFrameLoader]
   *        The frame loader to load this browser into the same process
   *        and tab group as.
   *
   * @returns {Promise<XULElement>}
   *          A Promise which resolves to the newly created browser XUL element.
   */
  async createBrowserElement(xulAttributes, groupFrameLoader = null) {
    if (!xulAttributes || Object.keys(xulAttributes).length === 0) {
      throw new Error("missing mandatory xulAttributes parameter");
    }

    await this.waitInitialized;

    const chromeDoc = this.chromeDocument;

    const browser = chromeDoc.createElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");
    browser.sameProcessAsFrameLoader = groupFrameLoader;

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
    await awaitFrameLoader;

    return browser;
  }
}


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
class HiddenExtensionPage extends HiddenXULWindow {
  constructor(extension, viewType) {
    if (!extension || !viewType) {
      throw new Error("extension and viewType parameters are mandatory");
    }

    super();
    this.extension = extension;
    this.viewType = viewType;
    this.browser = null;
  }

  /**
   * Destroy the created parent document.
   */
  shutdown() {
    if (this.unloaded) {
      throw new Error("Unable to shutdown an unloaded HiddenExtensionPage instance");
    }

    if (this.browser) {
      this.browser.remove();
      this.browser = null;
    }

    super.shutdown();
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

    this.browser = await super.createBrowserElement({
      "webextension-view-type": this.viewType,
      "remote": this.extension.remote ? "true" : null,
      "remoteType": this.extension.remote ?
        E10SUtils.EXTENSION_REMOTE_TYPE : null,
    }, this.extension.groupFrameLoader);

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
          if (browser.isRemoteBrowser !== extension.remote &&
              this.debugBrowserPromises.get(extension.id) === browserPromise) {
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
        "remote": extension.remote ? "true" : null,
        "remoteType": extension.remote ?
          E10SUtils.EXTENSION_REMOTE_TYPE : null,
      }, extension.groupFrameLoader);
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
        await browserPromise.then((browser) => browser.remove());
      }
    }

    if (this.debugBrowserPromises.size === 0 && this.hiddenXULWindow) {
      this.hiddenXULWindow.shutdown();
      this.hiddenXULWindow = null;
      this.unwatchExtensionUpdated();
    }
  },
};


function promiseExtensionViewLoaded(browser) {
  return new Promise(resolve => {
    browser.messageManager.addMessageListener("Extension:ExtensionViewLoaded", function onLoad({data}) {
      browser.messageManager.removeMessageListener("Extension:ExtensionViewLoaded", onLoad);
      resolve(data.childId && ParentAPIManager.getContextById(data.childId));
    });
  });
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
function watchExtensionProxyContextLoad({extension, viewType, browser}, onExtensionProxyContextLoaded) {
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

      let icons = this.iconCache.get(extension)
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
      let {imageData, path, themeIcons} = details;

      if (imageData) {
        if (typeof imageData == "string") {
          imageData = {"19": imageData};
        }

        for (let size of Object.keys(imageData)) {
          result[size] = imageData[size];
        }
      }

      let baseURI = context ? context.uri : extension.baseURI;

      if (path != null) {
        if (typeof path != "object") {
          path = {"19": path};
        }

        for (let size of Object.keys(path)) {
          let url = baseURI.resolve(path[size]);

          // The Chrome documentation specifies these parameters as
          // relative paths. We currently accept absolute URLs as well,
          // which means we need to check that the extension is allowed
          // to load them. This will throw an error if it's not allowed.
          this._checkURL(url, extension);

          result[size] = url;
        }
      }

      if (themeIcons) {
        themeIcons.forEach(({size, light, dark}) => {
          let lightURL = baseURI.resolve(light);
          let darkURL = baseURI.resolve(dark);

          this._checkURL(lightURL, extension);
          this._checkURL(darkURL, extension);

          let defaultURL = result[size] || result[19]; // always fallback to default first
          result[size] = {
            "default": defaultURL || darkURL, // Fallback to the dark url if no default is specified.
            "light": lightURL,
            "dark": darkURL,
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
    if (!extension.checkLoadURL(url, {allowInheritsPrincipal: true})) {
      throw new ExtensionError(`Illegal URL ${url}`);
    }
  },

  // Returns the appropriate icon URL for the given icons object and the
  // screen resolution of the given window.
  getPreferredIcon(icons, extension = null, size = 16) {
    const DEFAULT = "chrome://browser/content/extension.svg";

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
      return {size: bestSize, icon: icons[bestSize]};
    }

    return {size, icon: DEFAULT};
  },

  convertImageURLToDataURL(imageURL, contentWindow, browserWindow, size = 18) {
    return new Promise((resolve, reject) => {
      let image = new contentWindow.Image();
      image.onload = function() {
        let canvas = contentWindow.document.createElement("canvas");
        let ctx = canvas.getContext("2d");
        let dSize = size * browserWindow.devicePixelRatio;

        // Scales the image while maintaing width to height ratio.
        // If the width and height differ, the image is centered using the
        // smaller of the two dimensions.
        let dWidth, dHeight, dx, dy;
        if (this.width > this.height) {
          dWidth = dSize;
          dHeight = image.height * (dSize / image.width);
          dx = 0;
          dy = (dSize - dHeight) / 2;
        } else {
          dWidth = image.width * (dSize / image.height);
          dHeight = dSize;
          dx = (dSize - dWidth) / 2;
          dy = 0;
        }

        canvas.width = dSize;
        canvas.height = dSize;
        ctx.drawImage(this, 0, 0, this.width, this.height, dx, dy, dWidth, dHeight);
        resolve(canvas.toDataURL("image/png"));
      };
      image.onerror = reject;
      image.src = imageURL;
    });
  },

  // These URLs should already be properly escaped, but make doubly sure CSS
  // string escape characters are escaped here, since they could lead to a
  // sandbox break.
  escapeUrl(url) {
    return url.replace(/[\\\s"]/g, encodeURIComponent);
  },
};

StartupCache = {
  DB_NAME: "ExtensionStartupCache",

  STORE_NAMES: Object.freeze(["general", "locales", "manifests", "other", "permissions", "schemas"]),

  file: OS.Path.join(OS.Constants.Path.localProfileDir, "startupCache", "webext.sc.lz4"),

  async _saveNow() {
    let data = new Uint8Array(aomStartup.encodeBlob(this._data));
    await OS.File.writeAtomic(this.file, data, {tmpPath: `${this.file}.tmp`});
  },

  async save() {
    if (!this._saveTask) {
      OS.File.makeDir(OS.Path.dirname(this.file), {
        ignoreExisting: true,
        from: OS.Constants.Path.localProfileDir,
      });

      this._saveTask = new DeferredTask(() => this._saveNow(), 5000);

      AsyncShutdown.profileBeforeChange.addBlocker(
        "Flush WebExtension StartupCache",
        async () => {
          await this._saveTask.finalize();
          this._saveTask = null;
        });
    }
    return this._saveTask.arm();
  },

  _data: null,
  async _readData() {
    let result = new Map();
    try {
      let {buffer} = await OS.File.read(this.file);

      result = aomStartup.decodeBlob(buffer);
    } catch (e) {
      if (!e.becauseNoSuchFile) {
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
    return this.general.get([extension.id, extension.version, ...path],
                            createFunc);
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

    store.delete(key);
    StartupCache.save();
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
