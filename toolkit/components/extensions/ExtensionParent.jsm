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
XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NativeApp",
                                  "resource://gre/modules/NativeMessaging.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "NetUtil",
                                  "resource://gre/modules/NetUtil.jsm");
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
} = ExtensionUtils;

const BASE_SCHEMA = "chrome://extensions/content/schemas/manifest.json";
const CATEGORY_EXTENSION_SCHEMAS = "webextension-schemas";
const CATEGORY_EXTENSION_SCRIPTS = "webextension-scripts";

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
    if (envType == "addon_parent" || envType == "content_parent") {
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

    // TODO(robwu): When addons move to a separate process, we should use the
    // parent process manager(s) of the addon process(es) instead of the
    // in-process one.
    let pipmm = Services.ppmm.getChildAt(0);
    // Listen on the global frame message manager because content scripts send
    // and receive extension messages via their frame.
    // Listen on the parent process message manager because `runtime.connect`
    // and `runtime.sendMessage` requests must be delivered to all frames in an
    // addon process (by the API contract).
    // And legacy addons are not associated with a frame, so that is another
    // reason for having a parent process manager here.
    let messageManagers = [Services.mm, pipmm];

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
    let receiverMM = this._getMessageManagerForRecipient(recipient);
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
   * @returns {object|null} The message manager matching the recipient if found.
   */
  _getMessageManagerForRecipient(recipient) {
    let {extensionId, tabId} = recipient;
    // tabs.sendMessage / tabs.connect
    if (tabId) {
      // `tabId` being set implies that the tabs API is supported, so we don't
      // need to check whether `TabManager` exists.
      let tab = apiManager.global.TabManager.getTab(tabId, null, null);
      return tab && tab.linkedBrowser.messageManager;
    }

    // runtime.sendMessage / runtime.connect
    if (extensionId) {
      // TODO(robwu): map the extensionId to the addon parent process's message
      // manager when they run in a separate process.
      return Services.ppmm.getChildAt(0);
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

  _onExtensionBrowser(type, browser) {
    browser.messageManager.loadFrameScript(`data:,
      Components.utils.import("resource://gre/modules/ExtensionContent.jsm");
      ExtensionContent.init(this);
      addEventListener("unload", function() {
        ExtensionContent.uninit(this);
      });
    `, false);
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
    if (envType == "addon_parent") {
      // Privileged addon contexts can only be loaded in documents whose main
      // frame is also the same addon.
      if (principal.URI.prePath !== extension.baseURI.prePath ||
          !target.contentPrincipal.subsumes(principal)) {
        throw new Error(`Refused to create privileged WebExtension context for ${principal.URI.spec}`);
      }
      context = new ExtensionPageContextParent(envType, extension, data, target);
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
      Cu.reportError("WebExtension warning: Message manager unexpectedly changed");
    }

    try {
      let args = Cu.cloneInto(data.args, context.sandbox);
      let result = findPathInObject(context.apiObj, data.path)(...args);

      if (data.callId) {
        result = result || Promise.resolve();

        result.then(result => {
          result = result instanceof SpreadArgs ? [...result] : [result];

          context.parentMessageManager.sendAsyncMessage("API:CallResult", {
            childId: data.childId,
            callId: data.callId,
            result,
          });
        }, error => {
          error = context.normalizeError(error);
          context.parentMessageManager.sendAsyncMessage("API:CallResult", {
            childId: data.childId,
            callId: data.callId,
            error: {message: error.message},
          });
        });
      }
    } catch (e) {
      if (data.callId) {
        let error = context.normalizeError(e);
        context.parentMessageManager.sendAsyncMessage("API:CallResult", {
          childId: data.childId,
          callId: data.callId,
          error: {message: error.message},
        });
      } else {
        Cu.reportError(e);
      }
    }
  },

  addListener(data, target) {
    let context = this.getContextById(data.childId);
    if (context.parentMessageManager !== target.messageManager) {
      Cu.reportError("WebExtension warning: Message manager unexpectedly changed");
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

    context.listenerProxies.set(data.path, listener);

    let args = Cu.cloneInto(data.args, context.sandbox);
    findPathInObject(context.apiObj, data.path).addListener(listener, ...args);
  },

  removeListener(data) {
    let context = this.getContextById(data.childId);
    let listener = context.listenerProxies.get(data.path);
    findPathInObject(context.apiObj, data.path).removeListener(listener);
  },

  getContextById(childId) {
    let context = this.proxyContexts.get(childId);
    if (!context) {
      let error = new Error("WebExtension context not found!");
      Cu.reportError(error);
      throw error;
    }
    return context;
  },
};

ParentAPIManager.init();


const ExtensionParent = {
  GlobalManager,
  ParentAPIManager,
  apiManager,
};
