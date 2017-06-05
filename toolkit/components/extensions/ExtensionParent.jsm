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

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "E10SUtils",
                                  "resource:///modules/E10SUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NativeApp",
                                  "resource://gre/modules/NativeMessaging.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "PrivateBrowsingUtils",
                                  "resource://gre/modules/PrivateBrowsingUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");

XPCOMUtils.defineLazyServiceGetter(this, "gAddonPolicyService",
                                   "@mozilla.org/addons/policy-service;1",
                                   "nsIAddonPolicyService");

Cu.import("resource://gre/modules/ExtensionCommon.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  BaseContext,
  CanOfAPIs,
  SchemaAPIManager,
} = ExtensionCommon;

var {
  DefaultWeakMap,
  MessageManagerProxy,
  SpreadArgs,
  defineLazyGetter,
  promiseDocumentLoaded,
  promiseEvent,
  promiseObserved,
} = ExtensionUtils;

const BASE_SCHEMA = "chrome://extensions/content/schemas/manifest.json";
const CATEGORY_EXTENSION_SCHEMAS = "webextension-schemas";
const CATEGORY_EXTENSION_SCRIPTS = "webextension-scripts";

const XUL_URL = "data:application/vnd.mozilla.xul+xml;charset=utf-8," + encodeURI(
  `<?xml version="1.0"?>
  <window id="documentElement"/>`);

let schemaURLs = new Set();

schemaURLs.add("chrome://extensions/content/schemas/experiments.json");

let GlobalManager;
let ParentAPIManager;
let ProxyMessenger;

// This object loads the ext-*.js scripts that define the extension API.
let apiManager = new class extends SchemaAPIManager {
  constructor() {
    super("main");
    this.initialized = null;

    this.on("startup", (event, extension) => { // eslint-disable-line mozilla/balanced-listeners
      let promises = [];
      for (let apiName of this.eventModules.get("startup")) {
        promises.push(this.asyncGetAPI(apiName, extension).then(api => {
          api.onStartup(extension.startupReason);
        }));
      }

      return Promise.all(promises);
    });
  }

  // Loads all the ext-*.js scripts currently registered.
  lazyInit() {
    if (this.initialized) {
      return this.initialized;
    }

    let scripts = [];
    for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS)) {
      scripts.push(value);
    }

    let promise = Promise.all(scripts.map(url => ChromeUtils.compileScript(url))).then(scripts => {
      for (let script of scripts) {
        script.executeInGlobal(this.global);
      }

      // Load order matters here. The base manifest defines types which are
      // extended by other schemas, so needs to be loaded first.
      return Schemas.load(BASE_SCHEMA).then(() => {
        let promises = [];
        for (let [/* name */, url] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCHEMAS)) {
          promises.push(Schemas.load(url));
        }
        for (let url of this.schemaURLs) {
          promises.push(Schemas.load(url));
        }
        for (let url of schemaURLs) {
          promises.push(Schemas.load(url));
        }
        return Promise.all(promises);
      });
    });

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
        target.messageManager.sendAsyncMessage("Extension:SetTabAndWindowId", result);
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

  receiveMessage({target, messageName, channelId, sender, recipient, data, responseType}) {
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

    let extension = GlobalManager.extensionMap.get(sender.extensionId);
    let receiverMM = this.getMessageManagerForRecipient(recipient);
    if (!extension || !receiverMM) {
      return Promise.reject({
        result: MessageChannel.RESULT_NO_HANDLER,
        message: "No matching message handler for the given recipient.",
      });
    }

    if ((messageName == "Extension:Message" ||
         messageName == "Extension:Connect") &&
        apiManager.global.tabGetSender) {
      // From ext-tabs.js, undefined on Android.
      apiManager.global.tabGetSender(extension, target, sender);
    }
    return MessageChannel.sendMessage(receiverMM, messageName, data, {
      sender,
      recipient,
      responseType,
    });
  },

  /**
   * @param {object} recipient An object that was passed to
   *     `MessageChannel.sendMessage`.
   * @param {Extension} extension
   * @returns {object|null} The message manager matching the recipient if found.
   */
  getMessageManagerForRecipient(recipient) {
    let {tabId} = recipient;
    // tabs.sendMessage / tabs.connect
    if (tabId) {
      // `tabId` being set implies that the tabs API is supported, so we don't
      // need to check whether `tabTracker` exists.
      let tab = apiManager.global.tabTracker.getTab(tabId, null);
      return tab && (tab.linkedBrowser || tab.browser).messageManager;
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

      browser.messageManager.sendAsyncMessage("Extension:InitExtensionView",
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

    this.uri = NetUtil.newURI(params.url);

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

    apiManager.emit("proxy-context-load", this);
  }

  get cloneScope() {
    return this.sandbox;
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
  return Cu.Sandbox(this.principal);
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

    extension.emit("extension-proxy-context-load", this);
  }

  // The window that contains this context. This may change due to moving tabs.
  get xulWindow() {
    let win = this.xulBrowser.ownerGlobal;
    return win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDocShell)
              .QueryInterface(Ci.nsIDocShellTreeItem).rootTreeItem
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

  init() {
    Services.obs.addObserver(this, "message-manager-close");

    Services.mm.addMessageListener("API:CreateProxyContext", this);
    Services.mm.addMessageListener("API:CloseProxyContext", this, true);
    Services.mm.addMessageListener("API:Call", this);
    Services.mm.addMessageListener("API:AddListener", this);
    Services.mm.addMessageListener("API:RemoveListener", this);
  },

  observe(subject, topic, data) {
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
          extension.parentMessageManager = processMessageManager;
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
      let args = Cu.cloneInto(data.args, context.sandbox);
      let fun = await context.apiCan.asyncFindAPIPath(data.path);
      let result = fun(...args);

      if (data.callId) {
        result = result || Promise.resolve();

        result.then(result => {
          result = result instanceof SpreadArgs ? [...result] : [result];

          reply({result});
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

    function listener(...listenerArgs) {
      return context.sendMessage(
        context.parentMessageManager,
        "API:RunListener",
        {
          childId,
          listenerId: data.listenerId,
          path: data.path,
          args: listenerArgs,
        },
        {
          recipient: {childId},
        });
    }

    context.listenerProxies.set(data.listenerId, listener);

    let args = Cu.cloneInto(data.args, context.sandbox);
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
    this.chromeShell.loadURI(XUL_URL, 0, null, null, null);

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

    const browser = chromeDoc.createElement("browser");
    browser.setAttribute("type", "content");
    browser.setAttribute("disableglobalhistory", "true");

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
    });

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
      });
    };

    let browserPromise = this.debugBrowserPromises.get(extensionId);

    // Create a new promise if there is no cached one in the map.
    if (!browserPromise) {
      browserPromise = createBrowser();
      this.debugBrowserPromises.set(extensionId, browserPromise);
      browserPromise.catch(() => {
        this.debugBrowserPromises.delete(extensionId);
      });
    }

    this.debugActors.get(browserPromise).add(webExtensionParentActor);

    return browserPromise;
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

// Used to cache the list of WebExtensionManifest properties defined in the BASE_SCHEMA.
let gBaseManifestProperties = null;

/**
 * Function to obtain the extension name from a moz-extension URI without exposing GlobalManager.
 *
 * @param {Object} uri The URI for the extension to look up.
 * @returns {string} the name of the extension.
 */
function extensionNameFromURI(uri) {
  let id = null;
  try {
    id = gAddonPolicyService.extensionURIToAddonId(uri);
  } catch (ex) {
    if (ex.name != "NS_ERROR_XPC_BAD_CONVERT_JS") {
      Cu.reportError("Extension cannot be found in AddonPolicyService.");
    }
  }
  return GlobalManager.getExtension(id).name;
}

const ExtensionParent = {
  extensionNameFromURI,
  GlobalManager,
  HiddenExtensionPage,
  ParentAPIManager,
  WebExtensionPolicy,
  apiManager,
  get baseManifestProperties() {
    if (gBaseManifestProperties) {
      return gBaseManifestProperties;
    }

    let types = Schemas.schemaJSON.get(BASE_SCHEMA)[0].types;
    let manifest = types.find(type => type.id === "WebExtensionManifest");
    if (!manifest) {
      throw new Error("Unable to find base manifest properties");
    }

    gBaseManifestProperties = Object.getOwnPropertyNames(manifest.properties);
    return gBaseManifestProperties;
  },
  promiseExtensionViewLoaded,
  watchExtensionProxyContextLoad,
  DebugUtils,
};
