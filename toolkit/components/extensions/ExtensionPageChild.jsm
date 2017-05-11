/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* exported ExtensionPageChild */

this.EXPORTED_SYMBOLS = ["ExtensionPageChild"];

/**
 * This file handles privileged extension page logic that runs in the
 * child process.
 */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ExtensionChildDevToolsUtils",
                                  "resource://gre/modules/ExtensionChildDevToolsUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "ExtensionManagement",
                                  "resource://gre/modules/ExtensionManagement.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Schemas",
                                  "resource://gre/modules/Schemas.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "WebNavigationFrames",
                                  "resource://gre/modules/WebNavigationFrames.jsm");

const CATEGORY_EXTENSION_SCRIPTS_ADDON = "webextension-scripts-addon";
const CATEGORY_EXTENSION_SCRIPTS_DEVTOOLS = "webextension-scripts-devtools";

Cu.import("resource://gre/modules/ExtensionCommon.jsm");
Cu.import("resource://gre/modules/ExtensionChild.jsm");
Cu.import("resource://gre/modules/ExtensionUtils.jsm");

const {
  defineLazyGetter,
  getInnerWindowID,
  promiseEvent,
} = ExtensionUtils;

const {
  BaseContext,
  CanOfAPIs,
  SchemaAPIManager,
} = ExtensionCommon;

const {
  ChildAPIManager,
  Messenger,
} = ExtensionChild;

var ExtensionPageChild;

var apiManager = new class extends SchemaAPIManager {
  constructor() {
    super("addon");
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS_ADDON)) {
        this.loadScript(value);
      }
    }
  }
}();

var devtoolsAPIManager = new class extends SchemaAPIManager {
  constructor() {
    super("devtools");
    this.initialized = false;
  }

  lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      for (let [/* name */, value] of XPCOMUtils.enumerateCategoryEntries(CATEGORY_EXTENSION_SCRIPTS_DEVTOOLS)) {
        this.loadScript(value);
      }
    }
  }
}();

class ExtensionBaseContextChild extends BaseContext {
  /**
   * This ExtensionBaseContextChild represents an addon execution environment
   * that is running in an addon or devtools child process.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object} params
   * @param {string} params.envType One of "addon_child" or "devtools_child".
   * @param {nsIDOMWindow} params.contentWindow The window where the addon runs.
   * @param {string} params.viewType One of "background", "popup", "tab",
   *   "sidebar", "devtools_page" or "devtools_panel".
   * @param {number} [params.tabId] This tab's ID, used if viewType is "tab".
   */
  constructor(extension, params) {
    if (!params.envType) {
      throw new Error("Missing envType");
    }

    super(params.envType, extension);
    let {viewType, uri, contentWindow, tabId} = params;
    this.viewType = viewType;
    this.uri = uri || extension.baseURI;

    this.setContentWindow(contentWindow);

    // This is the MessageSender property passed to extension.
    let sender = {id: extension.id};
    if (viewType == "tab") {
      sender.frameId = WebNavigationFrames.getFrameId(contentWindow);
      sender.tabId = tabId;
      Object.defineProperty(this, "tabId",
        {value: tabId, enumerable: true, configurable: true});
    }
    if (uri) {
      sender.url = uri.spec;
    }
    this.sender = sender;

    Schemas.exportLazyGetter(contentWindow, "browser", () => {
      let browserObj = Cu.createObjectIn(contentWindow);
      Schemas.inject(browserObj, this.childManager);
      return browserObj;
    });

    Schemas.exportLazyGetter(contentWindow, "chrome", () => {
      let chromeApiWrapper = Object.create(this.childManager);
      chromeApiWrapper.isChromeCompat = true;

      let chromeObj = Cu.createObjectIn(contentWindow);
      Schemas.inject(chromeObj, chromeApiWrapper);
      return chromeObj;
    });
  }

  get cloneScope() {
    return this.contentWindow;
  }

  get principal() {
    return this.contentWindow.document.nodePrincipal;
  }

  get windowId() {
    if (["tab", "popup", "sidebar"].includes(this.viewType)) {
      let globalView = ExtensionPageChild.contentGlobals.get(this.messageManager);
      return globalView ? globalView.windowId : -1;
    }
    return -1;
  }

  get tabId() {
    // Will be overwritten in the constructor if necessary.
    return -1;
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

    if (this.contentWindow) {
      this.contentWindow.close();
    }

    super.unload();
  }
}

defineLazyGetter(ExtensionBaseContextChild.prototype, "messenger", function() {
  let filter = {extensionId: this.extension.id};
  let optionalFilter = {};
  // Addon-generated messages (not necessarily from the same process as the
  // addon itself) are sent to the main process, which forwards them via the
  // parent process message manager. Specific replies can be sent to the frame
  // message manager.
  return new Messenger(this, [Services.cpmm, this.messageManager], this.sender,
                       filter, optionalFilter);
});

class ExtensionPageContextChild extends ExtensionBaseContextChild {
  /**
   * This ExtensionPageContextChild represents a privileged addon
   * execution environment that has full access to the WebExtensions
   * APIs (provided that the correct permissions have been requested).
   *
   * This is the child side of the ExtensionPageContextParent class
   * defined in ExtensionParent.jsm.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object} params
   * @param {nsIDOMWindow} params.contentWindow The window where the addon runs.
   * @param {string} params.viewType One of "background", "popup", "sidebar" or "tab".
   *     "background", "sidebar" and "tab" are used by `browser.extension.getViews`.
   *     "popup" is only used internally to identify page action and browser
   *     action popups and options_ui pages.
   * @param {number} [params.tabId] This tab's ID, used if viewType is "tab".
   */
  constructor(extension, params) {
    super(extension, Object.assign(params, {envType: "addon_child"}));

    this.extension.views.add(this);
  }

  unload() {
    super.unload();
    this.extension.views.delete(this);
  }
}

defineLazyGetter(ExtensionPageContextChild.prototype, "childManager", function() {
  apiManager.lazyInit();

  let localApis = {};
  let can = new CanOfAPIs(this, apiManager, localApis);

  let childManager = new ChildAPIManager(this, this.messageManager, can, {
    envType: "addon_parent",
    viewType: this.viewType,
    url: this.uri.spec,
    incognito: this.incognito,
  });

  this.callOnClose(childManager);

  if (this.viewType == "background") {
    apiManager.global.initializeBackgroundPage(this.contentWindow);
  }

  return childManager;
});

class DevToolsContextChild extends ExtensionBaseContextChild {
  /**
   * This DevToolsContextChild represents a devtools-related addon execution
   * environment that has access to the devtools API namespace and to the same subset
   * of APIs available in a content script execution environment.
   *
   * @param {BrowserExtensionContent} extension This context's owner.
   * @param {object} params
   * @param {nsIDOMWindow} params.contentWindow The window where the addon runs.
   * @param {string} params.viewType One of "devtools_page" or "devtools_panel".
   * @param {object} [params.devtoolsToolboxInfo] This devtools toolbox's information,
   *   used if viewType is "devtools_page" or "devtools_panel".
   */
  constructor(extension, params) {
    super(extension, Object.assign(params, {envType: "devtools_child"}));

    this.devtoolsToolboxInfo = params.devtoolsToolboxInfo;
    ExtensionChildDevToolsUtils.initThemeChangeObserver(
      params.devtoolsToolboxInfo.themeName, this);

    this.extension.devtoolsViews.add(this);
  }

  unload() {
    super.unload();
    this.extension.devtoolsViews.delete(this);
  }
}

defineLazyGetter(DevToolsContextChild.prototype, "childManager", function() {
  devtoolsAPIManager.lazyInit();

  let localApis = {};
  let can = new CanOfAPIs(this, devtoolsAPIManager, localApis);

  let childManager = new ChildAPIManager(this, this.messageManager, can, {
    envType: "devtools_parent",
    viewType: this.viewType,
    url: this.uri.spec,
    incognito: this.incognito,
  });

  this.callOnClose(childManager);

  return childManager;
});

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
  }

  uninit() {
    this.global.removeMessageListener("Extension:InitExtensionView", this);
    this.global.removeMessageListener("Extension:SetTabAndWindowId", this);
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
        this.viewType = data.viewType;

        // Force external links to open in tabs.
        if (["popup", "sidebar"].includes(this.viewType)) {
          this.global.docShell.isAppTab = true;
        }

        if (data.devtoolsToolboxInfo) {
          this.devtoolsToolboxInfo = data.devtoolsToolboxInfo;
        }

        promiseEvent(this.global, "DOMContentLoaded", true).then(() => {
          let windowId = getInnerWindowID(this.global.content);
          let context = ExtensionPageChild.extensionContexts.get(windowId);

          this.global.sendAsyncMessage("Extension:ExtensionViewLoaded",
                                       {childId: context && context.childManager.id});
        });

        /* FALLTHROUGH */
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
}

ExtensionPageChild = {
  // Map<nsIContentFrameMessageManager, ContentGlobal>
  contentGlobals: new Map(),

  // Map<innerWindowId, ExtensionPageContextChild>
  extensionContexts: new Map(),

  initialized: false,

  _init() {
    if (this.initialized) {
      return;
    }
    this.initialized = true;

    Services.obs.addObserver(this, "inner-window-destroyed"); // eslint-ignore-line mozilla/balanced-listeners
  },

  init(global) {
    if (!ExtensionManagement.isExtensionProcess) {
      throw new Error("Cannot init extension page global in current process");
    }

    if (!this.contentGlobals.has(global)) {
      this.contentGlobals.set(global, new ContentGlobal(global));
    }
  },

  uninit(global) {
    if (this.contentGlobals.has(global)) {
      this.contentGlobals.get(global).uninit();
      this.contentGlobals.delete(global);
    }
  },

  observe(subject, topic, data) {
    if (topic === "inner-window-destroyed") {
      let windowId = subject.QueryInterface(Ci.nsISupportsPRUint64).data;

      this.destroyExtensionContext(windowId);
    }
  },

  /**
   * Create a privileged context at document-element-inserted.
   *
   * @param {BrowserExtensionContent} extension
   *     The extension for which the context should be created.
   * @param {nsIDOMWindow} contentWindow The global of the page.
   */
  initExtensionContext(extension, contentWindow) {
    this._init();

    if (!ExtensionManagement.isExtensionProcess) {
      throw new Error("Cannot create an extension page context in current process");
    }

    let windowId = getInnerWindowID(contentWindow);
    let context = this.extensionContexts.get(windowId);
    if (context) {
      if (context.extension !== extension) {
        throw new Error("A different extension context already exists for this frame");
      }
      throw new Error("An extension context was already initialized for this frame");
    }

    let mm = contentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIDocShell)
                          .QueryInterface(Ci.nsIInterfaceRequestor)
                          .getInterface(Ci.nsIContentFrameMessageManager);

    let {viewType, tabId, devtoolsToolboxInfo} = this.contentGlobals.get(mm).ensureInitialized();

    let uri = contentWindow.document.documentURIObject;

    if (devtoolsToolboxInfo) {
      context = new DevToolsContextChild(extension, {
        viewType, contentWindow, uri, tabId, devtoolsToolboxInfo,
      });
    } else {
      context = new ExtensionPageContextChild(extension, {viewType, contentWindow, uri, tabId});
    }

    this.extensionContexts.set(windowId, context);
  },

  /**
   * Close the ExtensionPageContextChild belonging to the given window, if any.
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
