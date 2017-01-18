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
Cu.import("resource://gre/modules/Task.jsm");
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

Cu.import("resource://gre/modules/ExtensionCommon.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

var {
  BaseContext,
  SchemaAPIManager,
} = ExtensionCommon;

var {
  MessageManagerProxy,
  SpreadArgs,
  defineLazyGetter,
  findPathInObject,
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

if (!AppConstants.RELEASE_OR_BETA) {
  schemaURLs.add("chrome://extensions/content/schemas/experiments.json");
}

let GlobalManager;
let ParentAPIManager;
let ProxyMessenger;

// This object loads the ext-*.js scripts that define the extension API.
let apiManager = new class extends SchemaAPIManager {
  constructor() {
    super("main");
    this.initialized = null;
  }

  // Loads all the ext-*.js scripts currently registered.
  lazyInit() {
    if (this.initialized) {
      return this.initialized;
    }

    // Load order matters here. The base manifest defines types which are
    // extended by other schemas, so needs to be loaded first.
    let promise = Schemas.load(BASE_SCHEMA).then(() => {
      let promises = [];
      for (let [/* name */, url] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCHEMAS)) {
        promises.push(Schemas.load(url));
      }
      for (let url of schemaURLs) {
        promises.push(Schemas.load(url));
      }
      return Promise.all(promises);
    });

    for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS)) {
      this.loadScript(value);
    }

    this.initialized = promise;
    return this.initialized;
  }

  registerSchemaAPI(namespace, envType, getAPI) {
    if (envType == "addon_parent" || envType == "content_parent" ||
        envType == "devtools_parent") {
      super.registerSchemaAPI(namespace, envType, getAPI);
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
      // need to check whether `TabManager` exists.
      let tab = apiManager.global.TabManager.getTab(tabId, null, null);
      return tab && tab.linkedBrowser.messageManager;
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
      Components.utils.import("resource://gre/modules/ExtensionContent.jsm");
      ExtensionContent.init(this);
      addEventListener("unload", function() {
        ExtensionContent.uninit(this);
      });
    `, false);

    let viewType = browser.getAttribute("webextension-view-type");
    if (viewType) {
      let data = {viewType};

      let {getBrowserInfo} = apiManager.global;
      if (getBrowserInfo) {
        Object.assign(data, getBrowserInfo(browser), additionalData);
      }

      browser.messageManager.sendAsyncMessage("Extension:InitExtensionView",
                                              data);
    }
  },

  getExtension(extensionId) {
    return this.extensionMap.get(extensionId);
  },

  injectInObject(context, isChromeCompat, dest) {
    apiManager.generateAPIs(context, dest);
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

defineLazyGetter(ProxyContextParent.prototype, "apiObj", function() {
  let obj = {};
  GlobalManager.injectInObject(this, false, obj);
  return obj;
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
    return this.xulBrowser.ownerGlobal;
  }

  get windowId() {
    if (!apiManager.global.WindowManager || this.viewType == "background") {
      return;
    }
    // viewType popup or tab:
    return apiManager.global.WindowManager.getId(this.xulWindow);
  }

  get tabId() {
    let {getBrowserInfo} = apiManager.global;

    if (getBrowserInfo) {
      // This is currently only available on desktop Firefox.
      return getBrowserInfo(this.xulBrowser).tabId;
    }
    return undefined;
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
    if (!this._devToolsTarget) {
      throw new Error("no DevTools target is set during DevTools Context shutdown");
    }

    this._devToolsTarget.destroy();
    this._devToolsTarget = null;
    this._devToolsToolbox = null;
  }
}

ParentAPIManager = {
  proxyContexts: new Map(),

  init() {
    Services.obs.addObserver(this, "message-manager-close", false);

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

  call(data, target) {
    let context = this.getContextById(data.childId);
    if (context.parentMessageManager !== target.messageManager) {
      throw new Error("Got message on unexpected message manager");
    }

    let reply = result => {
      if (!context.parentMessageManager) {
        Cu.reportError("Cannot send function call result: other side closed connection");
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
      let result = findPathInObject(context.apiObj, data.path)(...args);

      if (data.callId) {
        result = result || Promise.resolve();

        result.then(result => {
          result = result instanceof SpreadArgs ? [...result] : [result];

          reply({result});
        }, error => {
          error = context.normalizeError(error);
          reply({error: {message: error.message}});
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

  addListener(data, target) {
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
    findPathInObject(context.apiObj, data.path).addListener(listener, ...args);
  },

  removeListener(data) {
    let context = this.getContextById(data.childId);
    let listener = context.listenerProxies.get(data.listenerId);
    findPathInObject(context.apiObj, data.path).removeListener(listener);
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
 * This is a base class used by the ext-backgroundPage and ext-devtools API implementations
 * to inherits the shared boilerplate code needed to create a parent document for the hidden
 * extension pages (e.g. the background page, the devtools page) in the BackgroundPage and
 * DevToolsPage classes.
 *
 * @param {Extension} extension
 *   the Extension which owns the hidden extension page created (used to decide
 *   if the hidden extension page parent doc is going to be a windowlessBrowser or
 *   a visible XUL window)
 * @param {string} viewType
 *  the viewType of the WebExtension page that is going to be loaded
 *  in the created browser element (e.g. "background" or "devtools_page").
 *
 */
class HiddenExtensionPage {
  constructor(extension, viewType) {
    if (!extension || !viewType) {
      throw new Error("extension and viewType parameters are mandatory");
    }
    this.extension = extension;
    this.viewType = viewType;
    this.parentWindow = null;
    this.windowlessBrowser = null;
    this.browser = null;
  }

  /**
   * Destroy the created parent document.
   */
  shutdown() {
    if (this.unloaded) {
      throw new Error("Unable to shutdown an unloaded HiddenExtensionPage instance");
    }

    this.unloaded = true;

    if (this.browser) {
      this.browser.remove();
      this.browser = null;
    }

    // Navigate away from the background page to invalidate any
    // setTimeouts or other callbacks.
    if (this.webNav) {
      this.webNav.loadURI("about:blank", 0, null, null, null);
      this.webNav = null;
    }

    if (this.parentWindow) {
      this.parentWindow.close();
      this.parentWindow = null;
    }

    if (this.windowlessBrowser) {
      this.windowlessBrowser.loadURI("about:blank", 0, null, null, null);
      this.windowlessBrowser.close();
      this.windowlessBrowser = null;
    }
  }

  /**
   * Creates the browser XUL element that will contain the WebExtension Page.
   *
   * @returns {Promise<XULElement>}
   *   a Promise which resolves to the newly created browser XUL element.
   */
  createBrowserElement() {
    if (this.browser) {
      throw new Error("createBrowserElement called twice");
    }

    let waitForParentDocument;
    if (this.extension.remote) {
      waitForParentDocument = this.createWindowedBrowser();
    } else {
      waitForParentDocument = this.createWindowlessBrowser();
    }

    return waitForParentDocument.then(chromeDoc => {
      const browser = this.browser = chromeDoc.createElement("browser");
      browser.setAttribute("type", "content");
      browser.setAttribute("disableglobalhistory", "true");
      browser.setAttribute("webextension-view-type", this.viewType);

      let awaitFrameLoader = Promise.resolve();

      if (this.extension.remote) {
        browser.setAttribute("remote", "true");
        browser.setAttribute("remoteType", E10SUtils.EXTENSION_REMOTE_TYPE);
        awaitFrameLoader = promiseEvent(browser, "XULFrameLoaderCreated");
      }

      chromeDoc.documentElement.appendChild(browser);
      return awaitFrameLoader.then(() => browser);
    });
  }

  /**
   * Private helper that create a XULDocument in a windowless browser.
   *
   * An hidden extension page (e.g. a background page or devtools page) is usually
   * loaded into a windowless browser, with no on-screen representation or graphical
   * display abilities.
   *
   * This currently does not support remote browsers, and therefore cannot
   * be used with out-of-process extensions.
   *
   * @returns {Promise<XULDocument>}
   *   a promise which resolves to the newly created XULDocument.
   */
  createWindowlessBrowser() {
    return Task.spawn(function* () {
      // The invisible page is currently wrapped in a XUL window to fix an issue
      // with using the canvas API from a background page (See Bug 1274775).
      let windowlessBrowser = Services.appShell.createWindowlessBrowser(true);
      this.windowlessBrowser = windowlessBrowser;

      // The windowless browser is a thin wrapper around a docShell that keeps
      // its related resources alive. It implements nsIWebNavigation and
      // forwards its methods to the underlying docShell, but cannot act as a
      // docShell itself. Calling `getInterface(nsIDocShell)` gives us the
      // underlying docShell, and `QueryInterface(nsIWebNavigation)` gives us
      // access to the webNav methods that are already available on the
      // windowless browser, but contrary to appearances, they are not the same
      // object.
      let chromeShell = windowlessBrowser.QueryInterface(Ci.nsIInterfaceRequestor)
                                         .getInterface(Ci.nsIDocShell)
                                         .QueryInterface(Ci.nsIWebNavigation);

      yield this.initParentWindow(chromeShell);

      return promiseDocumentLoaded(windowlessBrowser.document);
    }.bind(this));
  }

  /**
   * Private helper that create a XULDocument in a visible dialog window.
   *
   * Using this helper, the extension page is loaded into a visible dialog window.
   * Only to be used for debugging, and in temporary, test-only use for
   * out-of-process extensions.
   *
   * @returns {Promise<XULDocument>}
   *   a promise which resolves to the newly created XULDocument.
   */
  createWindowedBrowser() {
    return Task.spawn(function* () {
      let window = Services.ww.openWindow(null, "about:blank", "_blank",
                                          "chrome,alwaysLowered,dialog", null);

      this.parentWindow = window;

      let chromeShell = window.QueryInterface(Ci.nsIInterfaceRequestor)
                              .getInterface(Ci.nsIDocShell)
                              .QueryInterface(Ci.nsIWebNavigation);


      yield this.initParentWindow(chromeShell);

      window.minimize();

      return promiseDocumentLoaded(window.document);
    }.bind(this));
  }

  /**
   * Private helper that initialize the created parent document.
   *
   * @param {nsIDocShell} chromeShell
   *   the docShell related to initialize.
   *
   * @returns {Promise<nsIXULDocument>}
   *   the initialized parent chrome document.
   */
  initParentWindow(chromeShell) {
    if (PrivateBrowsingUtils.permanentPrivateBrowsing) {
      let attrs = chromeShell.getOriginAttributes();
      attrs.privateBrowsingId = 1;
      chromeShell.setOriginAttributes(attrs);
    }

    let system = Services.scriptSecurityManager.getSystemPrincipal();
    chromeShell.createAboutBlankContentViewer(system);
    chromeShell.useGlobalHistory = false;
    chromeShell.loadURI(XUL_URL, 0, null, null, null);

    return promiseObserved("chrome-document-global-created",
                           win => win.document == chromeShell.document);
  }
}

function promiseExtensionViewLoaded(browser) {
  return new Promise(resolve => {
    browser.messageManager.addMessageListener("Extension:ExtensionViewLoaded", function onLoad() {
      browser.messageManager.removeMessageListener("Extension:ExtensionViewLoaded", onLoad);
      resolve();
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
 *   the Extension on which we are going to listen for the newly created ExtensionProxyContext.
 * @param {string} params.viewType
 *  the viewType of the WebExtension page that we are watching (e.g. "background" or "devtools_page").
 * @param {XULElement} params.browser
 *  the browser element of the WebExtension page that we are watching.
 *
 * @param {Function} onExtensionProxyContextLoaded
 *  the callback that is called when a new context has been loaded (as `callback(context)`);
 *
 * @returns {Function}
 *   Unsubscribe the listener.
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

const ExtensionParent = {
  GlobalManager,
  HiddenExtensionPage,
  ParentAPIManager,
  apiManager,
  promiseExtensionViewLoaded,
  watchExtensionProxyContextLoad,
};
