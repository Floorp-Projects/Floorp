/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ExtensionChild"];

/*
 * This file handles addon logic that is independent of the chrome process.
 * When addons run out-of-process, this is the main entry point.
 * Its primary function is managing addon globals.
 *
 * Don't put contentscript logic here, use ExtensionContent.jsm instead.
 */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "MessageChannel",
                                  "resource://gre/modules/MessageChannel.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");

const CATEGORY_EXTENSION_SCRIPTS_ADDON = "webextension-scripts-addon";

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  getInnerWindowID,
  BaseContext,
  ChildAPIManager,
  LocalAPIImplementation,
  Messenger,
  SchemaAPIManager,
} = ExtensionUtils;

// There is a circular dependency between Extension.jsm and us.
// Long-term this file should not reference Extension.jsm (because they would
// live in different processes), but for now use lazy getters.
XPCOMUtils.defineLazyGetter(this, "findPathInObject",
  () => Cu.import("resource://gre/modules/Extension.jsm", {}).findPathInObject);
XPCOMUtils.defineLazyGetter(this, "ParentAPIManager",
  () => Cu.import("resource://gre/modules/Extension.jsm", {}).ParentAPIManager);

var apiManager = new class extends SchemaAPIManager {
  constructor() {
    super("addon");
    this.initialized = false;
  }

  generateAPIs(...args) {
    if (!this.initialized) {
      this.initialized = true;
      for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS_ADDON)) {
        this.loadScript(value);
      }
    }
    return super.generateAPIs(...args);
  }

  registerSchemaAPI(namespace, envType, getAPI) {
    if (envType == "addon_child") {
      super.registerSchemaAPI(namespace, envType, getAPI);
    }
  }
}();

// A class that behaves identical to a ChildAPIManager, except
// 1) creation of the ProxyContext in the parent is synchronous, and
// 2) APIs without a local implementation and marked as incompatible with the
//    out-of-process model fall back to directly invoking the parent methods.
// TODO(robwu): Remove this when all APIs have migrated.
class WannabeChildAPIManager extends ChildAPIManager {
  createProxyContextInConstructor(data) {
    // Create a structured clone to simulate IPC.
    data = Object.assign({}, data);
    let {principal} = data;  // Not structurally cloneable.
    delete data.principal;
    data = Cu.cloneInto(data, {});
    data.principal = principal;
    let name = "API:CreateProxyContext";
    // The <browser> that receives messages from `this.messageManager`.
    let target = this.context.contentWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDocShell)
      .chromeEventHandler;
    ParentAPIManager.receiveMessage({name, data, target});

    let proxyContext = ParentAPIManager.proxyContexts.get(this.id);

    // Use an identical cloneScope in the parent as the child to have identical
    // behavior for proxied vs direct calls. If all APIs are proxied, then the
    // parent cloneScope does not really matter (because when the message
    // arrives locally, the object is cloned into the local clone scope).
    // If all calls are direct, then the parent cloneScope does matter, because
    // the objects are not cloned again.
    Object.defineProperty(proxyContext, "cloneScope", {
      get: () => this.cloneScope,
    });

    // Synchronously unload the ProxyContext because we synchronously create it.
    this.context.callOnClose({close: proxyContext.unload.bind(proxyContext)});
  }

  getFallbackImplementation(namespace, name) {
    // This is gross and should be removed ASAP.
    let shouldSynchronouslyUseParentAPI = false;
    // Incompatible APIs are listed here.
    if (namespace == "webNavigation" || // ChildAPIManager is oblivious to filters.
        namespace == "webRequest") { // Incompatible by design (synchronous).
      shouldSynchronouslyUseParentAPI = true;
    }
    if (shouldSynchronouslyUseParentAPI) {
      let proxyContext = ParentAPIManager.proxyContexts.get(this.id);
      let apiObj = findPathInObject(proxyContext.apiObj, namespace, false);
      if (apiObj && name in apiObj) {
        return new LocalAPIImplementation(apiObj, name, this.context);
      }
      // If we got here, then it means that the JSON schema claimed that the API
      // will be available, but no actual implementation is given.
      // You should either provide an implementation or rewrite the JSON schema.
    }

    return super.getFallbackImplementation(namespace, name);
  }
}

class ExtensionContext extends BaseContext {
  /**
   * This ExtensionContext represents a privileged addon execution environment
   * that has full access to the WebExtensions APIs (provided that the correct
   * permissions have been requested).
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object} params
   * @param {nsIDOMWindow} params.contentWindow The window where the addon runs.
   * @param {string} params.viewType One of "background", "popup" or "tab".
   *     "background" and "tab" are used by `browser.extension.getViews`.
   *     "popup" is only used internally to identify page action and browser
   *     action popups and options_ui pages.
   * @param {number} [params.tabId] This tab's ID, used if viewType is "tab".
   */
  constructor(extension, params) {
    super("addon_child", extension);
    if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_DEFAULT) {
      // This check is temporary. It should be removed once the proxy creation
      // is asynchronous.
      throw new Error("ExtensionContext cannot be created in child processes");
    }

    let {viewType, uri, contentWindow, tabId} = params;
    this.viewType = viewType;
    this.uri = uri || extension.baseURI;

    this.setContentWindow(contentWindow);

    // This is the MessageSender property passed to extension.
    // It can be augmented by the "page-open" hook.
    let sender = {id: extension.uuid};
    if (viewType == "tab") {
      sender.tabId = tabId;
      this.tabId = tabId;
    }
    if (uri) {
      sender.url = uri.spec;
    }

    let filter = {extensionId: extension.id};
    let optionalFilter = {};
    // Addon-generated messages (not necessarily from the same process as the
    // addon itself) are sent to the main process, which forwards them via the
    // parent process message manager. Specific replies can be sent to the frame
    // message manager.
    this.messenger = new Messenger(this, [Services.cpmm, this.messageManager], sender, filter, optionalFilter);

    let localApis = {};
    apiManager.generateAPIs(this, localApis);
    this.childManager = new WannabeChildAPIManager(this, this.messageManager, localApis, {
      envType: "addon_parent",
      viewType,
      url: uri.spec,
    });
    let chromeApiWrapper = Object.create(this.childManager);
    chromeApiWrapper.isChromeCompat = true;

    let browserObj = Cu.createObjectIn(contentWindow, {defineAs: "browser"});
    let chromeObj = Cu.createObjectIn(contentWindow, {defineAs: "chrome"});
    Schemas.inject(browserObj, this.childManager);
    Schemas.inject(chromeObj, chromeApiWrapper);

    if (viewType == "background") {
      apiManager.global.initializeBackgroundPage(contentWindow);
    }

    this.extension.views.add(this);
  }

  get cloneScope() {
    return this.contentWindow;
  }

  get principal() {
    return this.contentWindow.document.nodePrincipal;
  }

  get windowId() {
    if (this.viewType == "tab" || this.viewType == "popup") {
      let globalView = ExtensionChild.contentGlobals.get(this.messageManager);
      return globalView ? globalView.windowId : -1;
    }
  }

  // Called when the extension shuts down.
  shutdown() {
    this.unload();
  }

  // This method is called when an extension page navigates away or
  // its tab is closed.
  unload() {
    // Note that without this guard, we end up running unload code
    // multiple times for tab pages closed by the "page-unload" handlers
    // triggered below.
    if (this.unloaded) {
      return;
    }

    super.unload();
    this.childManager.close();
    this.extension.views.delete(this);
  }
}

// All subframes in a tab, background page, popup, etc. have the same view type.
// This class keeps track of such global state.
// Note that this is created even for non-extension tabs because at present we
// do not have a way to distinguish regular tabs from extension tabs at the
// initialization of a frame script.
class ContentGlobal {
  /**
   * @param {nsIContentFrameMessageManager} global The frame script's global.
   */
  constructor(global) {
    this.global = global;
    // Unless specified otherwise assume that the extension page is in a tab,
    // because the majority of all class instances are going to be a tab. Any
    // special views (background page, extension popup) will immediately send an
    // Extension:InitExtensionView message to change the viewType.
    this.viewType = "tab";
    this.tabId = -1;
    this.windowId = -1;
    this.initialized = false;
    this.global.addMessageListener("Extension:InitExtensionView", this);
    this.global.addMessageListener("Extension:SetTabAndWindowId", this);

    this.initialDocuments = new WeakSet();
  }

  uninit() {
    this.global.removeMessageListener("Extension:InitExtensionView", this);
    this.global.removeMessageListener("Extension:SetTabAndWindowId", this);
    this.global.removeEventListener("DOMContentLoaded", this);
  }

  ensureInitialized() {
    if (!this.initialized) {
      // Request tab and window ID in case "Extension:InitExtensionView" is not
      // sent (e.g. when `viewType` is "tab").
      let reply = this.global.sendSyncMessage("Extension:GetTabAndWindowId");
      this.handleSetTabAndWindowId(reply[0] || {});
    }
    return this;
  }

  receiveMessage({name, data}) {
    switch (name) {
      case "Extension:InitExtensionView":
        // The view type is initialized once and then fixed.
        this.global.removeMessageListener("Extension:InitExtensionView", this);
        let {viewType, url} = data;
        this.viewType = viewType;
        this.global.addEventListener("DOMContentLoaded", this);
        if (url) {
          // TODO(robwu): Remove this check. It is only here because the popup
          // implementation does not always load a URL at the initialization,
          // and the logic is too complex to fix at once.
          let {document} = this.global.content;
          this.initialDocuments.add(document);
          document.location.replace(url);
        }
        /* Falls through to allow these properties to be initialized at once */
      case "Extension:SetTabAndWindowId":
        this.handleSetTabAndWindowId(data);
        break;
    }
  }

  handleSetTabAndWindowId(data) {
    let {tabId, windowId} = data;
    if (tabId) {
      // Tab IDs are not expected to change.
      if (this.tabId !== -1 && tabId !== this.tabId) {
        throw new Error("Attempted to change a tabId after it was set");
      }
      this.tabId = tabId;
    }
    if (windowId !== undefined) {
      // Window IDs may change if a tab is moved to a different location.
      // Note: This is the ID of the browser window for the extension API.
      // Do not confuse it with the innerWindowID of DOMWindows!
      this.windowId = windowId;
    }
    this.initialized = true;
  }

  // "DOMContentLoaded" event.
  handleEvent(event) {
    let {document} = this.global.content;
    if (event.target === document) {
      // If the document was still being loaded at the time of navigation, then
      // the DOMContentLoaded event is fired for the old document. Ignore it.
      if (this.initialDocuments.has(document)) {
        this.initialDocuments.delete(document);
        return;
      }
      this.global.removeEventListener("DOMContentLoaded", this);
      this.global.sendAsyncMessage("Extension:ExtensionViewLoaded");
    }
  }
}

this.ExtensionChild = {
  // Map<nsIContentFrameMessageManager, ContentGlobal>
  contentGlobals: new Map(),

  // Map<innerWindowId, ExtensionContext>
  extensionContexts: new Map(),

  initOnce() {
    // This initializes the default message handler for messages targeted at
    // an addon process, in case the addon process receives a message before
    // its Messenger has been instantiated. For example, if a content script
    // sends a message while there is no background page.
    MessageChannel.setupMessageManagers([Services.cpmm]);
  },

  init(global) {
    this.contentGlobals.set(global, new ContentGlobal(global));
  },

  uninit(global) {
    this.contentGlobals.get(global).uninit();
    this.contentGlobals.delete(global);
  },

  /**
   * Create a privileged context at document-element-inserted.
   *
   * @param {BrowserExtensionContent} extension
   *     The extension for which the context should be created.
   * @param {nsIDOMWindow} contentWindow The global of the page.
   */
  createExtensionContext(extension, contentWindow) {
    let windowId = getInnerWindowID(contentWindow);
    let context = this.extensionContexts.get(windowId);
    if (context) {
      if (context.extension !== extension) {
        // Oops. This should never happen.
        Cu.reportError("A different extension context already exists in this frame!");
      } else {
        // This should not happen either.
        Cu.reportError("The extension context was already initialized in this frame.");
      }
      return;
    }

    let mm = contentWindow
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIDocShell)
      .QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIContentFrameMessageManager);
    let {viewType, tabId} = this.contentGlobals.get(mm).ensureInitialized();

    let uri = contentWindow.document.documentURIObject;

    context = new ExtensionContext(extension, {viewType, contentWindow, uri, tabId});
    this.extensionContexts.set(windowId, context);
  },

  /**
   * Close the ExtensionContext belonging to the given window, if any.
   *
   * @param {number} windowId The inner window ID of the destroyed context.
   */
  destroyExtensionContext(windowId) {
    let context = this.extensionContexts.get(windowId);
    if (context) {
      context.unload();
      this.extensionContexts.delete(windowId);
    }
  },

  shutdownExtension(extensionId) {
    for (let [windowId, context] of this.extensionContexts) {
      if (context.extension.id == extensionId) {
        context.shutdown();
        this.extensionContexts.delete(windowId);
      }
    }
  },
};

// TODO(robwu): Change this condition when addons move to a separate process.
if (Services.appinfo.processType != Services.appinfo.PROCESS_TYPE_DEFAULT) {
  Object.keys(ExtensionChild).forEach(function(key) {
    if (typeof ExtensionChild[key] == "function") {
      ExtensionChild[key] = () => {};
    }
  });
}
